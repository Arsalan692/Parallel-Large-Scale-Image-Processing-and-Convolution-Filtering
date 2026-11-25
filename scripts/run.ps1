<#
.SYNOPSIS
    Runs a program from bin\ with the correct DLL path, optionally under MPI.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\run.ps1 test_common
    powershell -ExecutionPolicy Bypass -File scripts\run.ps1 sequential --count 100
    powershell -ExecutionPolicy Bypass -File scripts\run.ps1 -Procs 4 mpi_batch --count 1000 --threads 2
#>
param(
    [int]$Procs = 0,  # > 0: launch with mpiexec -n <Procs>
    [Parameter(Mandatory = $true, Position = 0)][string]$Program,
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$ProgramArgs = @()
)

$ErrorActionPreference = 'Stop'

$Root    = Split-Path $PSScriptRoot -Parent
$Mingw   = 'C:\msys64\mingw64'
$MpiExec = 'C:\Program Files\Microsoft MPI\Bin\mpiexec.exe'
$Exe     = Join-Path $Root "bin\$Program.exe"

# MSYS2 DLLs first (OpenCV, OpenMP, C++ runtime).
$env:PATH = "$Mingw\bin;$env:PATH"

if (-not (Test-Path $Exe)) { throw "$Exe not found - run scripts\build.ps1 first" }

if ($Procs -gt 0) {
    & $MpiExec -n $Procs $Exe @ProgramArgs
} else {
    & $Exe @ProgramArgs
}
exit $LASTEXITCODE
