param(
  [string]$Out = "$PSScriptRoot\..\..\build\flower3d\student01",
  [string]$BlenderRoot = "$PSScriptRoot\..\..\.flower3d-tools"
)

$ErrorActionPreference = "Stop"
$Version = "4.5.14"
$Blender = Join-Path $BlenderRoot "blender-$Version-windows-x64\blender.exe"

if (-not (Test-Path $Blender)) {
  & "$PSScriptRoot\setup_blender_windows.ps1" -Root $BlenderRoot
}

New-Item -ItemType Directory -Force -Path $Out | Out-Null

& $Blender --background --python "$PSScriptRoot\blender_student01_stage.py" -- --out $Out --save-blend

if ($LASTEXITCODE -ne 0) {
  throw "Blender student_01 stage failed with exit code $LASTEXITCODE"
}

Write-Host "Rendered student_01 technical stage to: $Out"
