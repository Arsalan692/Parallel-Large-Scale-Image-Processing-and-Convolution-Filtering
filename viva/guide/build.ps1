<#
.SYNOPSIS
    Prints viva\guide\guide.html to viva\Project_Explained_Group18.pdf with Microsoft Edge (headless).
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File viva\guide\build.ps1
#>
$ErrorActionPreference = 'Stop'
$Edge = 'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe'
$Html = Join-Path $PSScriptRoot 'guide.html'
$Pdf  = Join-Path (Split-Path $PSScriptRoot -Parent) 'Project_Explained_Group18.pdf'
$Url  = 'file:///' + $Html.Replace('\', '/')
$EdgeProfile = Join-Path $env:TEMP 'pdc-guide-edge-profile'
if (Test-Path $Pdf) { Remove-Item $Pdf }
$ErrorActionPreference = 'Continue'  # Edge prints harmless log lines on stderr
& $Edge --headless=new --disable-gpu --no-pdf-header-footer --run-all-compositor-stages-before-draw `
        "--user-data-dir=$EdgeProfile" "--print-to-pdf=$Pdf" $Url 2>&1 | Out-Null
$ErrorActionPreference = 'Stop'
for ($i = 0; $i -lt 40 -and -not (Test-Path $Pdf); $i++) { Start-Sleep -Milliseconds 250 }
if (-not (Test-Path $Pdf)) { throw "PDF was not created" }
Write-Host "wrote $Pdf"
