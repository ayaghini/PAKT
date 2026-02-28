param(
  [string]$Manifest = "doc/hardware/components/sources_manifest.csv"
)

$rows = Import-Csv -Path $Manifest
foreach ($row in $rows) {
  $componentDir = Join-Path "doc/hardware/components" $row.component
  if (!(Test-Path $componentDir)) { New-Item -ItemType Directory -Force -Path $componentDir | Out-Null }

  if ($row.url -match "\.pdf($|\?)") {
    $safeTitle = ($row.title -replace '[^a-zA-Z0-9\- _]', '') -replace '\s+', '_'
    $outPath = Join-Path $componentDir ("{0}_{1}.pdf" -f $row.doc_type, $safeTitle)
    try {
      Invoke-WebRequest -UseBasicParsing -Uri $row.url -OutFile $outPath -TimeoutSec 120
      Write-Host "Downloaded: $outPath"
    } catch {
      Write-Warning "Failed: $($row.url)"
    }
  }
}
