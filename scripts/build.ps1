<#
.SYNOPSIS
    Builds all programs of the project with MSYS2 g++ (one command).

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\build.ps1               # build everything
    powershell -ExecutionPolicy Bypass -File scripts\build.ps1 sequential    # build one program
    powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Clean        # delete build outputs first
#>
param(
    [string[]]$Targets = @(),
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

$Root  = Split-Path $PSScriptRoot -Parent
$Mingw = 'C:\msys64\mingw64'
$Cxx   = Join-Path $Mingw 'bin\g++.exe'
$ObjDir = Join-Path $Root 'build\obj'
$BinDir = Join-Path $Root 'bin'

# MSYS2 DLLs must come first in PATH (Git for Windows ships older, incompatible copies).
$env:PATH = "$Mingw\bin;$env:PATH"

if (-not (Test-Path $Cxx)) { throw "g++ not found at $Cxx (is MSYS2 installed?)" }

# ---- Compiler settings ------------------------------------------------------
$CxxFlags = @('-std=c++20', '-O3', '-march=native', '-Wall', '-Wextra', '-fopenmp',
              "-I$Root\src", "-I$Mingw\include\opencv5")
$OpenCvLibs = @('-lopencv_imgcodecs', '-lopencv_imgproc', '-lopencv_core')
$MpiLibs    = @('-lmsmpi')

# ---- Programs: name -> main source file, needs MPI? -------------------------
$AllTargets = [ordered]@{
    'test_common'     = @{ Main = 'tests\test_common.cpp';     Mpi = $false }
    'validate_opencv' = @{ Main = 'tests\validate_opencv.cpp'; Mpi = $false }
    'sequential'  = @{ Main = 'src\sequential\main.cpp';    Mpi = $false }
    'openmp'      = @{ Main = 'src\openmp\main.cpp';        Mpi = $false }
    'mpi_batch'   = @{ Main = 'src\mpi_batch\main.cpp';     Mpi = $true  }
    'mpi_strips'  = @{ Main = 'src\mpi_strips\main.cpp';    Mpi = $true  }
}

function Invoke-Gxx([string[]]$GxxArgs) {
    & $Cxx @GxxArgs
    if ($LASTEXITCODE -ne 0) { throw "g++ failed (exit code $LASTEXITCODE)" }
}

if ($Clean) {
    foreach ($d in $ObjDir, $BinDir) { if (Test-Path $d) { Remove-Item -Recurse -Force $d } }
    Write-Host 'Cleaned build\ and bin\'
}
New-Item -ItemType Directory -Force $ObjDir, $BinDir | Out-Null

if ($Targets.Count -eq 0) { $Targets = @($AllTargets.Keys) }
foreach ($t in $Targets) {
    if (-not $AllTargets.Contains($t)) { throw "Unknown target '$t'. Known: $($AllTargets.Keys -join ', ')" }
}

# ---- 1. Compile the shared code once ----------------------------------------
Write-Host '== Compiling shared code (src\common)'
$Objects = @()
foreach ($src in Get-ChildItem (Join-Path $Root 'src\common\*.cpp')) {
    $obj = Join-Path $ObjDir ($src.BaseName + '.o')
    Write-Host "   $($src.Name)"
    Invoke-Gxx ($CxxFlags + @('-c', $src.FullName, '-o', $obj))
    $Objects += $obj
}

# ---- 2. Compile + link each program -----------------------------------------
foreach ($t in $Targets) {
    $info = $AllTargets[$t]
    $main = Join-Path $Root $info.Main
    if (-not (Test-Path $main)) { Write-Host "-- $t : skipped ($($info.Main) not written yet)"; continue }

    $exe  = Join-Path $BinDir "$t.exe"
    $libs = $OpenCvLibs
    if ($info.Mpi) { $libs = $libs + $MpiLibs }
    Write-Host "== Building $t"
    Invoke-Gxx ($CxxFlags + @($main) + $Objects + @('-o', $exe) + $libs)
}

Write-Host "`nBuild finished. Programs are in $BinDir"
