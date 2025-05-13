param(
    [switch] $Check,
    [string] $ClangFormatPath = "clang-format"
)

$ErrorActionPreference = "Stop"

$Root = $PSScriptRoot
$ClangFormat = (Get-Command $ClangFormatPath -ErrorAction Stop).Source
$Sources = @(
    Get-ChildItem -Path "$Root\src" -Recurse -File -Include *.c, *.h |
        Sort-Object FullName
)

if ($Sources.Count -eq 0) {
    return
}

if ($Check) {
    & $ClangFormat --dry-run --Werror @($Sources.FullName)
} else {
    & $ClangFormat -i @($Sources.FullName)
}

exit $LASTEXITCODE
