param(
    [string]$Branch = "",
    [switch]$Setup
)

$ErrorActionPreference = "Stop"
$stateDir = Join-Path $env:LOCALAPPDATA "FLOWER-CI"
$configPath = Join-Path $stateDir "config.json"
$tokenPath = Join-Path $stateDir "token.bin"
$defaultBranch = "feature/AN-17-ci-emulator-smoke"

function Read-Required([string]$Prompt) {
    while ($true) {
        $value = Read-Host $Prompt
        if (-not [string]::IsNullOrWhiteSpace($value)) {
            return $value.Trim()
        }
        Write-Host "Value is required." -ForegroundColor Yellow
    }
}

function Save-EncryptedToken([string]$Path) {
    $secure = Read-Host "CircleCI Personal API Token" -AsSecureString
    $ptr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
    try {
        $plain = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($ptr)
        if ([string]::IsNullOrWhiteSpace($plain)) {
            throw "Token was empty."
        }
        $bytes = [Text.Encoding]::UTF8.GetBytes($plain)
        $protected = [Security.Cryptography.ProtectedData]::Protect(
            $bytes,
            $null,
            [Security.Cryptography.DataProtectionScope]::CurrentUser
        )
        [IO.File]::WriteAllBytes($Path, $protected)
    }
    finally {
        if ($ptr -ne [IntPtr]::Zero) {
            [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr)
        }
    }
}

function Load-EncryptedToken([string]$Path) {
    $protected = [IO.File]::ReadAllBytes($Path)
    $bytes = [Security.Cryptography.ProtectedData]::Unprotect(
        $protected,
        $null,
        [Security.Cryptography.DataProtectionScope]::CurrentUser
    )
    return [Text.Encoding]::UTF8.GetString($bytes)
}

function Save-Config($Config) {
    $Config | ConvertTo-Json -Depth 5 | Set-Content -Path $configPath -Encoding UTF8
}

function Invoke-Circle([string]$Method, [string]$Uri, $Headers, $Body = $null) {
    if ($null -eq $Body) {
        return Invoke-RestMethod -Method $Method -Uri $Uri -Headers $Headers
    }

    $json = $Body | ConvertTo-Json -Depth 8
    return Invoke-RestMethod -Method $Method -Uri $Uri -Headers $Headers -ContentType "application/json" -Body $json
}

New-Item -ItemType Directory -Force -Path $stateDir | Out-Null

if ($Setup -or -not (Test-Path $configPath) -or -not (Test-Path $tokenPath)) {
    Write-Host "FLOWER CircleCI first-time setup" -ForegroundColor Cyan
    Write-Host "CircleCI Project Settings > Overview: copy Project Slug"
    Write-Host "CircleCI Project Settings > Project Setup: copy Pipeline Definition ID"
    Write-Host ""

    $projectSlug = Read-Required "Project Slug"
    $definitionId = Read-Required "Pipeline Definition ID"
    Save-EncryptedToken $tokenPath

    $cfg = [ordered]@{
        project_slug = $projectSlug
        definition_id = $definitionId
        last_branch = $defaultBranch
    }
    Save-Config $cfg
    Write-Host "Setup saved under $stateDir" -ForegroundColor Green

    if ($Setup) {
        Write-Host "Run FLOWER_CI_RUN.bat to start a pipeline."
        exit 0
    }
}

$cfg = Get-Content -Path $configPath -Raw | ConvertFrom-Json
$token = Load-EncryptedToken $tokenPath

if ([string]::IsNullOrWhiteSpace($Branch)) {
    if (-not [string]::IsNullOrWhiteSpace([string]$cfg.last_branch)) {
        $Branch = [string]$cfg.last_branch
    } else {
        $Branch = $defaultBranch
    }
} else {
    $cfg.last_branch = $Branch
    Save-Config $cfg
}

Write-Host ""
Write-Host "FLOWER CircleCI" -ForegroundColor Cyan
Write-Host "Branch : $Branch"
Write-Host "Project: $($cfg.project_slug)"
Write-Host "Mode   : build + Android emulator startup smoke"
Write-Host ""

$headers = @{
    "Circle-Token" = $token
    "Accept" = "application/json"
}

$triggerBody = @{
    definition_id = [string]$cfg.definition_id
    config = @{ branch = $Branch }
    checkout = @{ branch = $Branch }
    parameters = @{ run_build = $true }
}

$projectSlug = [string]$cfg.project_slug
$triggerUri = "https://circleci.com/api/v2/project/$projectSlug/pipeline/run"

Write-Host "[1/4] Triggering pipeline..."
$response = Invoke-Circle "Post" $triggerUri $headers $triggerBody

if (-not $response.id) {
    if ($response.message) {
        throw "CircleCI did not create a pipeline: $($response.message)"
    }
    throw "CircleCI response did not contain a pipeline ID."
}

$pipelineId = [string]$response.id
Write-Host "Pipeline ID: $pipelineId" -ForegroundColor Green

Write-Host "[2/4] Waiting for workflow..."
$workflow = $null
for ($i = 0; $i -lt 90; $i++) {
    Start-Sleep -Seconds 2
    $wfResponse = Invoke-Circle "Get" "https://circleci.com/api/v2/pipeline/$pipelineId/workflow" $headers
    if ($wfResponse.items -and $wfResponse.items.Count -gt 0) {
        $workflow = $wfResponse.items | Select-Object -First 1
        break
    }
}

if ($null -eq $workflow) {
    throw "Workflow did not appear within 3 minutes."
}

$workflowId = [string]$workflow.id
Write-Host "Workflow: $($workflow.name) [$workflowId]"

$terminal = @("success", "failed", "error", "canceled", "unauthorized", "not_run")
$workflowStatus = [string]$workflow.status

Write-Host "[3/4] Waiting for build and emulator smoke..."
$deadline = (Get-Date).AddMinutes(90)

while ($terminal -notcontains $workflowStatus) {
    if ((Get-Date) -gt $deadline) {
        throw "Timed out waiting for CircleCI after 90 minutes."
    }

    Start-Sleep -Seconds 10
    $workflow = Invoke-Circle "Get" "https://circleci.com/api/v2/workflow/$workflowId" $headers
    $workflowStatus = [string]$workflow.status
    Write-Host ("  {0:HH:mm:ss}  {1}" -f (Get-Date), $workflowStatus)
}

$jobs = Invoke-Circle "Get" "https://circleci.com/api/v2/workflow/$workflowId/job" $headers
Write-Host ""
Write-Host "Jobs:"
$jobs.items | Select-Object name,status,job_number | Format-Table -AutoSize

$passed = ($workflowStatus -eq "success")
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$downloadRoot = Join-Path ([Environment]::GetFolderPath("UserProfile")) "Downloads\FLOWER-CI\$stamp"
New-Item -ItemType Directory -Force -Path $downloadRoot | Out-Null

Write-Host "[4/4] Downloading artifacts to:"
Write-Host "  $downloadRoot"

foreach ($job in $jobs.items) {
    if (-not $job.job_number) {
        continue
    }

    $jobDir = Join-Path $downloadRoot ([string]$job.name)
    New-Item -ItemType Directory -Force -Path $jobDir | Out-Null

    try {
        $slugForJob = if ($job.project_slug) { [string]$job.project_slug } else { $projectSlug }
        $artifactResponse = Invoke-Circle "Get" "https://circleci.com/api/v2/project/$slugForJob/$($job.job_number)/artifacts" $headers

        foreach ($artifact in $artifactResponse.items) {
            $leaf = Split-Path ([string]$artifact.path) -Leaf

            # Do not hand out an APK when the emulator gate failed.
            if (-not $passed -and $leaf -like "*.apk") {
                continue
            }

            $destination = Join-Path $jobDir $leaf
            Invoke-WebRequest -Uri ([string]$artifact.url) -Headers $headers -OutFile $destination
        }
    }
    catch {
        Write-Host "  Artifact download warning for job $($job.name): $($_.Exception.Message)" -ForegroundColor Yellow
    }
}

Write-Host ""
if ($passed) {
    $apk = Get-ChildItem -Path $downloadRoot -Recurse -Filter "Flower-Standalone-Android.apk" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -eq $apk) {
        throw "Pipeline passed but the release APK artifact was not found."
    }

    Write-Host "PASS: build + Android emulator startup smoke passed." -ForegroundColor Green
    Write-Host "APK: $($apk.FullName)" -ForegroundColor Green
    exit 0
}

Write-Host "FAIL: build or emulator smoke failed." -ForegroundColor Red
Write-Host "The APK was intentionally NOT downloaded. Diagnostic artifacts were downloaded instead."
exit 1
