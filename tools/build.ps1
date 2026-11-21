param([string]$Distro = 'NineTimeSDK')
$ErrorActionPreference = 'Stop'
$projectPath = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Push-Location -LiteralPath $projectPath
try {
    node tools/prepare-project.cjs
    if ($LASTEXITCODE -ne 0) { throw 'Project preparation failed.' }
    $windowsScript = (Join-Path $PSScriptRoot 'build.sh').Replace('\', '/')
    $linuxScript = & wsl -d $Distro -u root -- wslpath -a $windowsScript
    if ($LASTEXITCODE -ne 0 -or !$linuxScript) { throw 'The WSL build environment is unavailable.' }
    & wsl -d $Distro -u root -- bash $linuxScript.Trim()
    if ($LASTEXITCODE -ne 0) { throw 'Pebble build failed.' }
    New-Item -ItemType Directory -Force -Path (Join-Path $projectPath 'dist') | Out-Null
    $version = (Get-Content -LiteralPath 'pebble/package.json' -Raw | ConvertFrom-Json).version
    $pbwName = "Kiddo-$version.pbw"
    $pbwPath = Join-Path $projectPath (Join-Path 'dist' $pbwName)
    Copy-Item -LiteralPath (Join-Path $projectPath 'pebble/build/pebble.pbw') -Destination $pbwPath -Force
    $checksums = foreach ($file in Get-ChildItem -LiteralPath 'dist' -Filter 'Kiddo-*.pbw' -File) {
        $digest = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        "$digest  $($file.Name)"
    }
    Set-Content -LiteralPath (Join-Path $projectPath 'dist/SHA256SUMS.txt') -Encoding ascii -Value $checksums
    Write-Output "Built: $pbwPath"
} finally { Pop-Location }
