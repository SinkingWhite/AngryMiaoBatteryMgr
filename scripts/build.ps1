$ErrorActionPreference = 'Stop'
& "$PSScriptRoot/build-native.ps1"
& "$PSScriptRoot/package-source.ps1"
Write-Output 'Ready: artifacts/native/Miao.exe'
