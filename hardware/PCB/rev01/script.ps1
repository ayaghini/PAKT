# Close KiCad first
$p = "$env:APPDATA\kicad\9.0\fp-lib-table"
$bak = "$p.bak_fix_$(Get-Date -Format yyyyMMdd_HHmmss)"
Copy-Item $p $bak -Force

$txt = [System.IO.File]::ReadAllText($p)
$enc = New-Object System.Text.UTF8Encoding($false)   # UTF-8 WITHOUT BOM
[System.IO.File]::WriteAllText($p, $txt, $enc)

# quick check: first byte should be 28 (hex 1C? no, decimal 40?) Actually '(' is 40 decimal, 28 hex
$bytes = [System.IO.File]::ReadAllBytes($p)
"First bytes: " + (($bytes[0..3] | ForEach-Object { '{0:X2}' -f $_ }) -join ' ')
