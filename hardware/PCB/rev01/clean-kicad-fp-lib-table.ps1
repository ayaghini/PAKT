# clean-kicad-fp-lib-table.ps1
# One-click cleanup for KiCad global footprint library table with backup.

$ErrorActionPreference = "Stop"

# Find global fp-lib-table (prefer KiCad 9.0, fallback to highest version)
$kicadRoot = Join-Path $env:APPDATA "kicad"
if (-not (Test-Path $kicadRoot)) { throw "KiCad config folder not found: $kicadRoot" }

$preferred = Join-Path $kicadRoot "9.0\fp-lib-table"
if (Test-Path $preferred) {
    $tablePath = $preferred
} else {
    $candidate = Get-ChildItem $kicadRoot -Directory |
        Sort-Object Name -Descending |
        ForEach-Object { Join-Path $_.FullName "fp-lib-table" } |
        Where-Object { Test-Path $_ } |
        Select-Object -First 1
    if (-not $candidate) { throw "No global fp-lib-table found under $kicadRoot" }
    $tablePath = $candidate
}

Write-Host "Using: $tablePath"

# Backup
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$backupPath = "$tablePath.bak_$timestamp"
Copy-Item $tablePath $backupPath -Force
Write-Host "Backup: $backupPath"

$lines = Get-Content $tablePath
$newLines = New-Object System.Collections.Generic.List[string]
$removed = New-Object System.Collections.Generic.List[string]

foreach ($line in $lines) {
    # Keep non-lib lines
    if ($line -notmatch '^\s*\(lib\s') {
        $newLines.Add($line)
        continue
    }

    # Extract name + uri if present
    $name = ""
    $uri  = ""
    if ($line -match '\(name\s+"([^"]+)"\)') { $name = $matches[1] }
    if ($line -match '\(uri\s+"([^"]*)"\)')  { $uri  = $matches[1] }

    $remove = $false

    # Remove known stale project-specific OneDrive libs
    if ($uri -match 'OneDrive/Documents/uConsole_HAM_HAT' -or
        $uri -match 'OneDrive/Documents/PDRM/PDRM/PCB/symbols') {
        $remove = $true
    }

    # Remove absolute-path libs that do not exist (keep env-var libs like ${KICAD9_FOOTPRINT_DIR})
    if (-not $remove -and $uri -and $uri -notmatch '\$\{[^}]+\}') {
        # normalize slashes
        $testUri = $uri -replace '/', '\'
        if (($testUri -match '^[A-Za-z]:\\' -or $testUri -like '\\*') -and -not (Test-Path $testUri)) {
            $remove = $true
        }
    }

    if ($remove) {
        $removed.Add(("{0} -> {1}" -f $name, $uri))
    } else {
        $newLines.Add($line)
    }
}

Set-Content -Path $tablePath -Value $newLines -Encoding UTF8

Write-Host ""
Write-Host "Cleanup complete."
Write-Host "Removed entries: $($removed.Count)"
if ($removed.Count -gt 0) {
    $removed | ForEach-Object { Write-Host " - $_" }
}
Write-Host ""
Write-Host "If needed, restore backup:"
Write-Host "Copy-Item `"$backupPath`" `"$tablePath`" -Force"
