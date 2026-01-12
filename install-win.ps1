# Build and install JamWide plugin on Windows

$ErrorActionPreference = "Stop"

# Change to script directory
Set-Location $PSScriptRoot

# Increment build number
$buildFile = "src\build_number.h"
if (Test-Path $buildFile) {
    $content = Get-Content $buildFile
    $currentLine = $content | Select-String "JAMWIDE_BUILD_NUMBER"
    if ($currentLine) {
        $current = [int]($currentLine -replace '.*JAMWIDE_BUILD_NUMBER\s+', '')
        $new = $current + 1
        "#pragma once" | Out-File $buildFile -Encoding ASCII
        "#define JAMWIDE_BUILD_NUMBER $new" | Out-File $buildFile -Append -Encoding ASCII
        Write-Host "Build number: r$new"
    }
}

# Build
Write-Host "Building plugins..."
$MSBUILD_PATH = "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"
& $MSBUILD_PATH "build\jamwide.sln" /p:Configuration=Release /v:minimal
if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

# Install locations (user)
$clapDir = "$env:LOCALAPPDATA\Programs\Common\CLAP"
$vst3Dir = "$env:LOCALAPPDATA\Programs\Common\VST3"

# Create directories
New-Item -ItemType Directory -Force -Path $clapDir | Out-Null
New-Item -ItemType Directory -Force -Path $vst3Dir | Out-Null

# Install CLAP
$clapSource = "build\CLAP\Release\JamWide.clap"
$clapTarget = Join-Path $clapDir "JamWide.clap"
if (Test-Path $clapTarget) {
    Remove-Item $clapTarget -Recurse -Force
}
Copy-Item -Path $clapSource -Destination $clapDir -Recurse -Force
Write-Host "Installed JamWide.clap to $clapDir"

# Install VST3
$vst3Target = Join-Path $vst3Dir "JamWide.vst3"
if (Test-Path $vst3Target) {
    Remove-Item $vst3Target -Recurse -Force
}
Copy-Item -Path "build\Release\JamWide.vst3" -Destination $vst3Dir -Recurse -Force
Write-Host "Installed JamWide.vst3 to $vst3Dir"

Write-Host ""
if (Test-Path $buildFile) {
    $content = Get-Content $buildFile
    $currentLine = $content | Select-String "JAMWIDE_BUILD_NUMBER"
    if ($currentLine) {
        $buildNum = [int]($currentLine -replace '.*JAMWIDE_BUILD_NUMBER\s+', '')
        Write-Host "JamWide r$buildNum installed (CLAP, VST3)"
    }
} else {
    Write-Host "JamWide installed (CLAP, VST3)"
}
