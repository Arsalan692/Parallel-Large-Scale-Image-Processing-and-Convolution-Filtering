<#
.SYNOPSIS
    Phase 6 - final benchmark: runs every configuration (sequential, OpenMP, MPI, hybrid)
    on the same images, repeated several times, and appends one CSV row per run.

.DESCRIPTION
    The repeat loop is OUTSIDE the configuration loop, so any slow drift of the machine
    (temperature, background activity) affects all configurations equally.
    Each run gets a unique label "rep<r>:<config-id>". Runs whose label is already in the
    CSV are skipped, so an interrupted benchmark can simply be started again (resume).

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\benchmark.ps1                 # 2 passes x 10,000 images
    powershell -ExecutionPolicy Bypass -File scripts\benchmark.ps1 -Repeats 3
    powershell -ExecutionPolicy Bypass -File scripts\benchmark.ps1 -Count 500 -Csv results\raw\quick.csv   # quick test
#>
param(
    [int]$Count = 10000,
    [int]$Repeats = 2,
    [string]$Csv = 'results\benchmarks.csv',
    [string]$Log = 'results\raw\benchmark_log.txt'
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot -Parent
Set-Location $Root
$Run = Join-Path $PSScriptRoot 'run.ps1'
New-Item -ItemType Directory -Force (Split-Path $Log -Parent), (Split-Path $Csv -Parent) | Out-Null

# ---- The experiment matrix ----------------------------------------------------------
# Procs = 0 means "run directly" (no mpiexec).
$Configs = @()
function Add-Config($Id, $Program, $Procs, $Threads, $ExtraArgs) {
    $script:Configs += [pscustomobject]@{ Id = $Id; Program = $Program; Procs = $Procs; Threads = $Threads; Extra = $ExtraArgs }
}
Add-Config 'seq'                  'sequential' 0 1 @()
foreach ($t in 1, 2, 4, 8) { Add-Config "omp-rows-$t"   'openmp' 0 $t @('--mode', 'rows', '--schedule', 'static') }
foreach ($t in 1, 2, 4, 8) { Add-Config "omp-images-$t" 'openmp' 0 $t @('--mode', 'images', '--schedule', 'static') }
Add-Config 'omp-rows-8-dynamic'   'openmp' 0 8 @('--mode', 'rows', '--schedule', 'dynamic')
Add-Config 'omp-images-8-dynamic' 'openmp' 0 8 @('--mode', 'images', '--schedule', 'dynamic')
foreach ($p in 1, 2, 4, 8) { Add-Config "batch-${p}x1"  'mpi_batch'  $p 1 @() }
foreach ($p in 1, 2, 4, 8) { Add-Config "strips-${p}x1" 'mpi_strips' $p 1 @() }
foreach ($c in @(1, 8), @(2, 4), @(4, 2), @(2, 2)) {
    Add-Config "batch-$($c[0])x$($c[1])"  'mpi_batch'  $c[0] $c[1] @()
    Add-Config "strips-$($c[0])x$($c[1])" 'mpi_strips' $c[0] $c[1] @()
}

# ---- Resume support: labels already present in the CSV are skipped -------------------
$done = @{}
if (Test-Path $Csv) { Import-Csv $Csv | ForEach-Object { $done[$_.label] = $true } }

$total = $Configs.Count * $Repeats
$i = 0
$start = Get-Date
"=== Benchmark started $start : $($Configs.Count) configurations x $Repeats repeats, $Count images ===" | Tee-Object -FilePath $Log -Append

for ($r = 1; $r -le $Repeats; $r++) {
    foreach ($c in $Configs) {
        $i++
        $label = "rep${r}:$($c.Id)"
        if ($done.ContainsKey($label)) { Write-Host "[$i/$total] $label already done - skipped"; continue }

        $progArgs = @('--count', $Count, '--threads', $c.Threads, '--csv', $Csv, '--label', $label) + $c.Extra
        if ($c.Procs -gt 0) { $runArgs = @('-Procs', $c.Procs, $c.Program) + $progArgs }
        else { $runArgs = @($c.Program) + $progArgs }

        # Programs print harmless warnings/progress on stderr; Windows PowerShell 5.1 would
        # treat redirected stderr as errors under 'Stop', so relax it for this call only.
        $ErrorActionPreference = 'Continue'
        $out = powershell -ExecutionPolicy Bypass -File $Run @runArgs 2>&1 |
               ForEach-Object { $_.ToString() } | Where-Object { $_ -notmatch 'libpng warning|images done' }
        $ErrorActionPreference = 'Stop'
        "--- $label ---" | Add-Content $Log
        $out | Add-Content $Log

        $summary = ($out | Where-Object { $_ -match 'TOTAL|checksum' } | ForEach-Object { $_.Trim() }) -join ' | '
        $elapsed = [int]((Get-Date) - $start).TotalMinutes
        Write-Host ("[{0}/{1}] {2,-28} {3}   ({4} min elapsed)" -f $i, $total, $label, $summary, $elapsed)
    }
}
"=== Benchmark finished $(Get-Date) ===" | Tee-Object -FilePath $Log -Append
