<#
.SYNOPSIS
    Opens the generated report in Microsoft Word, fills in the table of contents (page
    numbers), saves it, and exports a PDF copy for checking.

.EXAMPLE
    cd report\scripts; node build_report.js; powershell -ExecutionPolicy Bypass -File finalize_report.ps1
#>
param([string]$Docx = (Join-Path (Split-Path $PSScriptRoot -Parent) 'PDC_Project_Report_Group18.docx'))

$ErrorActionPreference = 'Stop'
$Docx = (Resolve-Path $Docx).Path
$Pdf = [IO.Path]::ChangeExtension($Docx, '.pdf')

$word = New-Object -ComObject Word.Application
$word.Visible = $false
$word.DisplayAlerts = 0
try {
    $doc = $word.Documents.Open($Docx)
    $doc.Repaginate()
    foreach ($toc in $doc.TablesOfContents) { $toc.Update() }
    $doc.Fields.Update() | Out-Null
    $doc.Save()
    $doc.ExportAsFixedFormat($Pdf, 17)  # 17 = wdExportFormatPDF
    Write-Host "Updated $Docx; PDF: $Pdf"
} finally {
    # Word sometimes drops the COM connection after a PDF export; the files are already written.
    try { $doc.Close([ref]$false) } catch {}
    try { $word.Quit() } catch {}
    try { [void][Runtime.InteropServices.Marshal]::ReleaseComObject($word) } catch {}
}
