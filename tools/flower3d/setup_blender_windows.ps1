param(
  [string]$Root = "$PSScriptRoot\..\..\.flower3d-tools"
)

$ErrorActionPreference = "Stop"
$Version = "4.5.14"
$Archive = "blender-$Version-windows-x64.zip"
$Url = "https://download.blender.org/release/Blender4.5/$Archive"
$Dest = Join-Path $Root "blender-$Version-windows-x64"
$Exe = Join-Path $Dest "blender.exe"

New-Item -ItemType Directory -Force -Path $Root | Out-Null

if (-not (Test-Path $Exe)) {
  $Zip = Join-Path $Root $Archive
  if (-not (Test-Path $Zip)) {
    Invoke-WebRequest -Uri $Url -OutFile $Zip
  }
  Expand-Archive -Path $Zip -DestinationPath $Root -Force
}

& $Exe --version
