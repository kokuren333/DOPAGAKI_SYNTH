$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$sdkPath = Join-Path $projectRoot 'external/vst3sdk'
$sdkRevision = '3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96'
if (-not (Test-Path -LiteralPath $sdkPath)) {
    git clone --recursive https://github.com/steinbergmedia/vst3sdk.git $sdkPath
    if ($LASTEXITCODE -ne 0) { throw 'SDK clone failed' }
    git -C $sdkPath checkout $sdkRevision
    if ($LASTEXITCODE -ne 0) { throw 'SDK checkout failed' }
    git -C $sdkPath submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) { throw 'SDK submodule setup failed' }
} else {
    $revision = git -C $sdkPath rev-parse HEAD
    if ($revision -ne $sdkRevision) { throw "Existing SDK differs from pinned revision. Review before changing: $revision" }
    git -C $sdkPath submodule status --recursive
    if ($LASTEXITCODE -ne 0) { throw 'SDK status failed' }
}
