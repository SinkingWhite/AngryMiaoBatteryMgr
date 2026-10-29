$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
Add-Type -AssemblyName System.IO.Compression
New-Item -ItemType Directory -Force "$repoRoot/artifacts/native" | Out-Null
$sourceFiles = @('README.md', 'NOTICE.md', 'LICENSE', '.gitignore', '.gitattributes', '.editorconfig', '.clang-format')
$sourceFiles += Get-ChildItem -LiteralPath "$repoRoot/native", "$repoRoot/scripts", "$repoRoot/docs", "$repoRoot/LICENSES", "$repoRoot/.github" -File -Recurse |
    ForEach-Object { $_.FullName.Substring($repoRoot.Length + 1) }
$stream = [IO.File]::Open((Join-Path $repoRoot 'artifacts/AngryMiaoBattery-source.zip'), [IO.FileMode]::Create)
$zip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($relative in $sourceFiles) {
        $entry = $zip.CreateEntry($relative.Replace('\','/'))
        $target = $entry.Open()
        $inputFile = [IO.File]::OpenRead((Join-Path $repoRoot $relative))
        try { $inputFile.CopyTo($target) } finally { $inputFile.Dispose(); $target.Dispose() }
    }
} finally { $zip.Dispose(); $stream.Dispose() }
Copy-Item "$repoRoot/LICENSE", "$repoRoot/NOTICE.md" -Destination "$repoRoot/artifacts/native"
New-Item -ItemType Directory -Force "$repoRoot/artifacts/native/LICENSES" | Out-Null
Get-ChildItem -LiteralPath "$repoRoot/LICENSES" -File | Copy-Item -Destination "$repoRoot/artifacts/native/LICENSES" -Force
