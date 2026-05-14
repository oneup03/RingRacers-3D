<#
.SYNOPSIS
    Configure + build Dr. Robotnik's Ring Racers on Windows (clang-cl + vcpkg),
    then deploy ringracers.exe into the local testing directory.

.DESCRIPTION
    Sets up the Visual Studio toolchain environment (clang-cl needs the MSVC
    headers/libs and the Windows SDK's mt.exe on PATH), then drives CMake with
    the ninja-x64_windows_vcpkg-develop preset.

    Used both locally (VS Code task) and by the GitHub Actions workflow.

.PARAMETER Preset
    CMake configure/build preset name. Default: ninja-x64_windows_vcpkg-develop

.PARAMETER VcpkgRoot
    Path to a git-based vcpkg checkout. Defaults to $env:VCPKG_ROOT, then
    $env:VCPKG_INSTALLATION_ROOT (set on GitHub-hosted runners), then
    C:\Users\oneup\source\repos\vcpkg.

.PARAMETER TestingDir
    Where to copy the built exe. Default: <repo>\testing

.PARAMETER Configure
    Force a CMake re-configure even if the build directory already exists.

.PARAMETER Clean
    Delete the build directory before configuring.

.PARAMETER NoDeploy
    Skip copying the exe into the testing directory (used by CI).
#>
[CmdletBinding()]
param(
    [string] $Preset     = "ninja-x64_windows_vcpkg-develop",
    [string] $VcpkgRoot  = $(
        if     ($env:VCPKG_ROOT)              { $env:VCPKG_ROOT }
        elseif ($env:VCPKG_INSTALLATION_ROOT) { $env:VCPKG_INSTALLATION_ROOT }
        else                                  { "C:\Users\oneup\source\repos\vcpkg" }
    ),
    [string] $TestingDir,
    [switch] $Configure,
    [switch] $Clean,
    [switch] $NoDeploy
)

$ErrorActionPreference = "Stop"

$RepoRoot  = Split-Path -Parent $PSScriptRoot
$BuildDir  = Join-Path $RepoRoot "build\$Preset"
if (-not $TestingDir) { $TestingDir = Join-Path $RepoRoot "testing" }

function Write-Step($msg) { Write-Host "`n=== $msg ===" -ForegroundColor Cyan }

# --- Locate Visual Studio -------------------------------------------------
Write-Step "Locating Visual Studio toolchain"
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found - is Visual Studio installed?" }

$vsPath = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $vsPath) { throw "No Visual Studio install with the MSVC x64 toolset was found." }

$vcvars  = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
$llvmBin = Join-Path $vsPath "VC\Tools\Llvm\x64\bin"
if (-not (Test-Path $vcvars))  { throw "vcvars64.bat not found at $vcvars" }
if (-not (Test-Path $llvmBin)) { throw "clang-cl not found - install the 'C++ Clang tools for Windows' VS component." }
Write-Host "Visual Studio: $vsPath"

# --- Import the developer environment -------------------------------------
# Launch-VsDevShell.ps1 can be blocked by the execution policy, so import the
# environment from the batch file instead - that always works.
Write-Step "Importing VS x64 developer environment"
cmd /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { Set-Item "env:$($matches[1])" $matches[2] }
}
$env:PATH = "$llvmBin;$env:PATH"
$env:VCPKG_ROOT = $VcpkgRoot

foreach ($tool in 'clang-cl', 'cl', 'mt', 'rc', 'ninja', 'cmake') {
    $resolved = (Get-Command $tool -ErrorAction SilentlyContinue).Source
    if (-not $resolved) { throw "Required tool '$tool' not found on PATH after environment setup." }
    Write-Host ("  {0,-9} {1}" -f $tool, $resolved)
}
if (-not (Test-Path (Join-Path $VcpkgRoot ".git"))) {
    throw "VCPKG_ROOT '$VcpkgRoot' is not a git checkout. The manifest's builtin-baseline needs git-based vcpkg."
}

# --- Configure ------------------------------------------------------------
Push-Location $RepoRoot
try {
    if ($Clean -and (Test-Path $BuildDir)) {
        Write-Step "Cleaning $BuildDir"
        Remove-Item -Recurse -Force $BuildDir
    }

    if ($Configure -or $Clean -or -not (Test-Path (Join-Path $BuildDir "CMakeCache.txt"))) {
        Write-Step "Configuring ($Preset)"
        cmake --preset $Preset
        if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)" }
    } else {
        Write-Host "`nSkipping configure (build directory already configured; pass -Configure to force)."
    }

    Write-Step "Building ($Preset)"
    cmake --build --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)" }
}
finally {
    Pop-Location
}

# --- Locate the produced executable ---------------------------------------
$exe = Join-Path $BuildDir "bin\ringracers.exe"
if (-not (Test-Path $exe)) { throw "Build reported success but $exe is missing." }
Write-Host "`nBuilt: $exe ($([math]::Round((Get-Item $exe).Length / 1MB, 1)) MB)" -ForegroundColor Green

# --- Deploy to the testing directory --------------------------------------
if ($NoDeploy) {
    Write-Host "Skipping deploy (-NoDeploy)."
} else {
    Write-Step "Deploying to testing directory"
    if (-not (Test-Path $TestingDir)) { New-Item -ItemType Directory -Path $TestingDir | Out-Null }
    Copy-Item $exe $TestingDir -Force
    $pdb = Join-Path $BuildDir "bin\ringracers.pdb"
    if (Test-Path $pdb) { Copy-Item $pdb $TestingDir -Force }
    Write-Host "Copied ringracers.exe -> $TestingDir" -ForegroundColor Green
}

Write-Host "`nDone." -ForegroundColor Green
