<#
.SYNOPSIS
    Zugfish - Modern Chess.com-Style Game Reviewer & Analysis Suite
    Universal Installation Script for Windows (PowerShell)

.DESCRIPTION
    Builds and installs Zugfish (zugfish-qt GUI and/or zugfish-cli) natively on Windows.
    Automatically detects Visual Studio 2022/2019, MinGW GCC, Qt 6, and Stockfish.

.PARAMETER InstallDir
    Target directory to install Zugfish binaries (default: %LOCALAPPDATA%\Programs\Zugfish).

.PARAMETER QtPath
    Path to your Qt 6 installation (e.g. C:\Qt\6.8.0\msvc2022_64).
    If omitted, the script auto-detects Qt 6 in standard locations.

.PARAMETER NoDeps
    Skip dependency checking and winget prompts.

.PARAMETER CliOnly
    Build and install only the zugfish-cli executable (does not require Qt 6).

.PARAMETER GuiOnly
    Build and install only the zugfish-qt GUI executable.

.PARAMETER Generator
    Explicit CMake generator (e.g. "Visual Studio 17 2022", "Ninja").

.EXAMPLE
    # Recommended command in PowerShell:
    powershell -ExecutionPolicy Bypass -File .\install.ps1

    # Specify custom Qt directory:
    .\install.ps1 -QtPath "C:\Qt\6.8.0\msvc2022_64"

    # Build CLI only (no Qt required):
    .\install.ps1 -CliOnly
#>

[CmdletBinding()]
param (
    [string]$InstallDir = "$env:LOCALAPPDATA\Programs\Zugfish",
    [string]$QtPath = "",
    [switch]$NoDeps,
    [switch]$CliOnly,
    [switch]$GuiOnly,
    [string]$Generator = ""
)

$ErrorActionPreference = "Continue"

function Write-Info    { Write-Host "[zugfish] $args" -ForegroundColor Cyan }
function Write-Success { Write-Host "[zugfish] $args" -ForegroundColor Green }
function Write-Warn    { Write-Host "[zugfish:warn] $args" -ForegroundColor Yellow }
function Write-Err     { Write-Host "[zugfish:error] $args" -ForegroundColor Red }

function Refresh-Path {
    $machinePath = [System.Environment]::GetEnvironmentVariable("Path", "Machine")
    $userPath = [System.Environment]::GetEnvironmentVariable("Path", "User")
    $wingetLinks = "$env:LOCALAPPDATA\Microsoft\WinGet\Links"
    $env:Path = "$machinePath;$userPath"
    if (Test-Path $wingetLinks -and $env:Path -notlike "*$wingetLinks*") {
        $env:Path = "$env:Path;$wingetLinks"
    }
}

Write-Host ""
Write-Host "======================================================" -ForegroundColor Cyan
Write-Host "            Zugfish Windows Installer                 " -ForegroundColor Cyan
Write-Host "======================================================" -ForegroundColor Cyan
Write-Info "Target Installation Directory: $InstallDir"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

# Remove broken/stale CMakeCache if it was previously configured with NMake
if (Test-Path "build\CMakeCache.txt") {
    $cacheContent = Get-Content "build\CMakeCache.txt" -Raw -ErrorAction SilentlyContinue
    if ($cacheContent -match "NMake Makefiles" -or $cacheContent -match "CMAKE_CXX_COMPILER-NOTFOUND") {
        Write-Info "Cleaning stale/invalid CMake build cache..."
        Remove-Item -Recurse -Force "build" -ErrorAction SilentlyContinue
    }
}

# -----------------------------------------------------------------------------
# 1. Dependency Checks: CMake & Stockfish
# -----------------------------------------------------------------------------
if (-not $NoDeps) {
    Write-Info "Checking build dependencies..."

    # Check CMake
    $hasCmake = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $hasCmake) {
        Write-Warn "CMake not found on PATH."
        if (Get-Command winget -ErrorAction SilentlyContinue) {
            Write-Info "Attempting to install CMake via winget..."
            try {
                winget install --id Kitware.CMake -e --accept-source-agreements --accept-package-agreements
                Refresh-Path
            } catch {
                Write-Warn "winget install CMake failed: $_"
            }
        }
        if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
            Write-Err "CMake is required. Download it from: https://cmake.org/download/"
            exit 1
        }
    }
    
    Refresh-Path
    $cmakeVer = (& cmake --version 2>&1 | Select-Object -First 1)
    Write-Success "Found CMake: $cmakeVer"

    # Check Stockfish
    $hasSf = (Get-Command stockfish -ErrorAction SilentlyContinue) -or (Get-Command stockfish.exe -ErrorAction SilentlyContinue)
    if (-not $hasSf) {
        Write-Warn "Stockfish engine not found on PATH."
        if (Get-Command winget -ErrorAction SilentlyContinue) {
            Write-Info "Attempting to install Stockfish via winget..."
            try {
                winget install --id Stockfish.Stockfish -e --accept-source-agreements --accept-package-agreements
                Refresh-Path
            } catch {
                Write-Warn "winget install Stockfish failed: $_"
            }
        }
        Refresh-Path
        $hasSf = (Get-Command stockfish -ErrorAction SilentlyContinue) -or (Get-Command stockfish.exe -ErrorAction SilentlyContinue)
        if (-not $hasSf) {
            Write-Warn "Stockfish is not in PATH. Zugfish can still build, but engine analysis will require configuring Stockfish in Settings."
        } else {
            Write-Success "Found Stockfish engine: $(Get-Command stockfish -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source)"
        }
    } else {
        Write-Success "Found Stockfish engine on PATH."
    }
}

# -----------------------------------------------------------------------------
# 2. C++ Compiler & CMake Generator Detection
# -----------------------------------------------------------------------------
Write-Info "Detecting C++ compiler and CMake generator..."

$hasCompiler = $false
$detectedGenerator = ""
$generatorArch = ""

# 2a. Check Visual Studio via vswhere.exe
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vsInstall) {
        $vsVer = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion
        $hasCompiler = $true
        if ($vsVer -like "17.*") {
            $detectedGenerator = "Visual Studio 17 2022"
            $generatorArch = "x64"
        } elseif ($vsVer -like "16.*") {
            $detectedGenerator = "Visual Studio 16 2019"
            $generatorArch = "x64"
        } else {
            $detectedGenerator = "Visual Studio 17 2022"
            $generatorArch = "x64"
        }
        Write-Success "Detected Visual Studio C++ toolchain ($detectedGenerator) at: $vsInstall"
    }
}

# 2b. Check if running inside VS Developer Prompt (cl.exe in PATH)
if (-not $hasCompiler -and (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $hasCompiler = $true
    if (Get-Command ninja -ErrorAction SilentlyContinue) {
        $detectedGenerator = "Ninja"
    } else {
        $detectedGenerator = "NMake Makefiles"
    }
    Write-Success "Found MSVC cl.exe in PATH (using $detectedGenerator)."
}

# 2c. Check if MinGW GCC is in PATH
if (-not $hasCompiler -and (Get-Command g++ -ErrorAction SilentlyContinue)) {
    $hasCompiler = $true
    if (Get-Command ninja -ErrorAction SilentlyContinue) {
        $detectedGenerator = "Ninja"
    } else {
        $detectedGenerator = "MinGW Makefiles"
    }
    Write-Success "Found MinGW g++ in PATH (using $detectedGenerator)."
}

# 2d. If no C++ compiler found, prompt to install via winget
if (-not $hasCompiler) {
    Write-Err "No C++ compiler (Visual Studio or MinGW GCC) found on your machine!"
    Write-Warn "Zugfish requires a C++20 compiler to build."
    Write-Host ""
    Write-Info "Options to install a C++ compiler:"
    Write-Host "  1) Visual Studio 2022 C++ Build Tools (Recommended for Qt / MSVC)" -ForegroundColor Yellow
    Write-Host "     winget install Microsoft.VisualStudio.2022.BuildTools --override `"--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended`""
    Write-Host "  2) WinLibs MinGW GCC 14 (Lightweight, single install)" -ForegroundColor Yellow
    Write-Host "     winget install --id=BrechtSanders.WinLibs.POSIX.UCRT -e"
    Write-Host ""

    if (Get-Command winget -ErrorAction SilentlyContinue) {
        $choice = Read-Host "Would you like to install Visual Studio 2022 C++ Build Tools automatically now? (Y/n)"
        if ($choice -ne "n" -and $choice -ne "N") {
            Write-Info "Installing Visual Studio 2022 C++ Build Tools via winget (please wait)..."
            winget install Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
            Refresh-Path
            Write-Success "C++ compiler installation completed. Please re-run .\install.ps1"
            exit 0
        }
    }
    
    Write-Err "Please install a C++ compiler and re-run .\install.ps1"
    exit 1
}

# -----------------------------------------------------------------------------
# 3. Qt 6 Detection (for GUI builds)
# -----------------------------------------------------------------------------
$BuildGui = (-not $CliOnly)
$BuildCli = (-not $GuiOnly)

if ($BuildGui) {
    if ($QtPath -eq "") {
        # Check standard environment variables
        if ($env:Qt6_DIR -and (Test-Path $env:Qt6_DIR)) {
            $QtPath = $env:Qt6_DIR
        } elseif ($env:QTDIR -and (Test-Path $env:QTDIR)) {
            $QtPath = $env:QTDIR
        } elseif ($env:CMAKE_PREFIX_PATH -and (Test-Path $env:CMAKE_PREFIX_PATH)) {
            $QtPath = $env:CMAKE_PREFIX_PATH
        } else {
            # Search common Qt installation directories
            $SearchRoots = @(
                "C:\Qt",
                "D:\Qt",
                "$env:SystemDrive\Qt",
                "$env:ProgramFiles\Qt",
                "$env:LOCALAPPDATA\Qt"
            )
            foreach ($root in $SearchRoots) {
                if (Test-Path $root) {
                    $qtVersions = Get-ChildItem -Path $root -Directory -Filter "6.*" -ErrorAction SilentlyContinue |
                                  Sort-Object Name -Descending
                    foreach ($ver in $qtVersions) {
                        $targets = Get-ChildItem -Path $ver.FullName -Directory -ErrorAction SilentlyContinue |
                                   Where-Object { $_.Name -match "msvc2022_64|msvc2019_64|mingw_64|msvc" }
                        if ($targets) {
                            $QtPath = $targets[0].FullName
                            break
                        }
                    }
                }
                if ($QtPath -ne "") { break }
            }
        }
    }

    if ($QtPath -ne "") {
        Write-Success "Detected Qt 6 at: $QtPath"
    } else {
        Write-Warn "Qt 6 was not detected in standard locations."
        Write-Info "Options:"
        Write-Info "  1) Build only the command-line reviewer (no Qt required):"
        Write-Info "     .\install.ps1 -CliOnly"
        Write-Info "  2) Specify your Qt 6 folder:"
        Write-Info "     .\install.ps1 -QtPath `"C:\Qt\6.8.0\msvc2022_64`""
        Write-Info "  3) Install Qt 6 via the Qt Online Installer from https://www.qt.io/download"
        Write-Host ""
        
        $buildCliChoice = Read-Host "Would you like to build only the CLI reviewer (zugfish-cli) for now without Qt? (Y/n)"
        if ($buildCliChoice -ne "n" -and $buildCliChoice -ne "N") {
            $BuildGui = $false
            $BuildCli = $true
            Write-Info "Proceeding with CLI-only build..."
        } else {
            Write-Err "Please install Qt 6 or specify -QtPath, then re-run .\install.ps1"
            exit 1
        }
    }
}

# -----------------------------------------------------------------------------
# 4. Configure CMake
# -----------------------------------------------------------------------------
Write-Info "Configuring CMake project..."

$BuildGuiVal = if ($BuildGui) { "ON" } else { "OFF" }
$BuildCliVal = if ($BuildCli) { "ON" } else { "OFF" }

$CmakeArgs = @(
    "-B", "build",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DBUILD_GUI=$BuildGuiVal",
    "-DBUILD_CLI=$BuildCliVal"
)

if ($Generator -ne "") {
    $CmakeArgs += @("-G", $Generator)
} elseif ($detectedGenerator -ne "") {
    $CmakeArgs += @("-G", $detectedGenerator)
    if ($generatorArch -ne "") {
        $CmakeArgs += @("-A", $generatorArch)
    }
}

if ($QtPath -ne "" -and $BuildGui) {
    $CmakeArgs += @("-DCMAKE_PREFIX_PATH=$QtPath")
}

# Check for vcpkg toolchain
if ($env:VCPKG_ROOT -and (Test-Path "$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake")) {
    $CmakeArgs += @("-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake")
    Write-Info "Using vcpkg toolchain from: $env:VCPKG_ROOT"
} elseif (Test-Path "C:\vcpkg\scripts\buildsystems\vcpkg.cmake") {
    $CmakeArgs += @("-DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake")
    Write-Info "Using vcpkg toolchain from: C:\vcpkg"
}

Write-Info "Running: cmake $($CmakeArgs -join ' ')"
& cmake @CmakeArgs
if ($LASTEXITCODE -ne 0) {
    Write-Err "CMake configuration failed."
    exit $LASTEXITCODE
}

# -----------------------------------------------------------------------------
# 5. Compile Project
# -----------------------------------------------------------------------------
Write-Info "Compiling Zugfish in Release mode..."
& cmake --build build --config Release --parallel
if ($LASTEXITCODE -ne 0) {
    Write-Err "Compilation failed."
    exit $LASTEXITCODE
}

# -----------------------------------------------------------------------------
# 6. Install Binaries & Bundle Runtime
# -----------------------------------------------------------------------------
Write-Info "Installing binaries to $InstallDir..."
if (-not (Test-Path $InstallDir)) {
    New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
}

# Locate compiled executables (handles both single-config and multi-config generator layouts)
$CliBinary = $null
$GuiBinary = $null

$PossibleCli = @(
    "build\zugfish-cli.exe",
    "build\Release\zugfish-cli.exe",
    "build\cli\Release\zugfish-cli.exe",
    "build\cli\zugfish-cli.exe"
)
foreach ($p in $PossibleCli) {
    if (Test-Path $p) { $CliBinary = $p; break }
}

$PossibleGui = @(
    "build\gui\zugfish-qt.exe",
    "build\gui\Release\zugfish-qt.exe",
    "build\Release\zugfish-qt.exe",
    "build\zugfish-qt.exe"
)
foreach ($p in $PossibleGui) {
    if (Test-Path $p) { $GuiBinary = $p; break }
}

if ($BuildGui -and $GuiBinary) {
    Copy-Item $GuiBinary -Destination "$InstallDir\zugfish-qt.exe" -Force
    Copy-Item $GuiBinary -Destination "$InstallDir\zugfish.exe" -Force
    Write-Success "Installed GUI: $InstallDir\zugfish-qt.exe"

    # Search for windeployqt to bundle Qt DLLs & QML imports
    $windeployqt = $null
    if ($QtPath -ne "" -and (Test-Path "$QtPath\bin\windeployqt.exe")) {
        $windeployqt = "$QtPath\bin\windeployqt.exe"
    } elseif (Get-Command windeployqt -ErrorAction SilentlyContinue) {
        $windeployqt = "windeployqt"
    }

    if ($windeployqt) {
        Write-Info "Running windeployqt to bundle runtime libraries..."
        & $windeployqt --qmldir gui\qml "$InstallDir\zugfish-qt.exe" 2>&1 | Out-Null
        Write-Success "Bundled Qt runtime libraries into $InstallDir"
    }

    # Create Desktop and Start Menu Shortcuts
    try {
        $WshShell = New-Object -ComObject WScript.Shell
        
        # Desktop Shortcut
        $DesktopPath = [Environment]::GetFolderPath("Desktop")
        $Shortcut = $WshShell.CreateShortcut("$DesktopPath\Zugfish.lnk")
        $Shortcut.TargetPath = "$InstallDir\zugfish-qt.exe"
        $Shortcut.WorkingDirectory = "$InstallDir"
        $Shortcut.Description = "Zugfish Chess Reviewer"
        $Shortcut.Save()
        Write-Success "Created Desktop Shortcut: $DesktopPath\Zugfish.lnk"

        # Start Menu Shortcut
        $ProgramsPath = [Environment]::GetFolderPath("Programs")
        $StartShortcut = $WshShell.CreateShortcut("$ProgramsPath\Zugfish.lnk")
        $StartShortcut.TargetPath = "$InstallDir\zugfish-qt.exe"
        $StartShortcut.WorkingDirectory = "$InstallDir"
        $StartShortcut.Description = "Zugfish Chess Reviewer"
        $StartShortcut.Save()
        Write-Success "Created Start Menu Shortcut"
    } catch {
        Write-Warn "Could not create shortcuts: $_"
    }
}

if ($BuildCli -and $CliBinary) {
    Copy-Item $CliBinary -Destination "$InstallDir\zugfish-cli.exe" -Force
    Write-Success "Installed CLI: $InstallDir\zugfish-cli.exe"
}

# -----------------------------------------------------------------------------
# 7. User PATH Update
# -----------------------------------------------------------------------------
$UserPath = [System.Environment]::GetEnvironmentVariable("Path", "User")
if ($UserPath -notlike "*$InstallDir*") {
    Write-Info "Adding $InstallDir to User PATH..."
    [System.Environment]::SetEnvironmentVariable("Path", "$UserPath;$InstallDir", "User")
    Write-Success "Added $InstallDir to User PATH (restart terminal to take effect)."
}

Write-Host ""
Write-Host "======================================================" -ForegroundColor Green
Write-Host "     Zugfish has been successfully installed!         " -ForegroundColor Green
Write-Host "======================================================" -ForegroundColor Green
Write-Host ""
Write-Host "You can now run:"
if ($BuildGui) {
    Write-Host "  • GUI:  zugfish (or launch from Desktop/Start Menu)" -ForegroundColor Cyan
}
if ($BuildCli) {
    Write-Host "  • CLI:  zugfish-cli --pgn path\to\game.pgn" -ForegroundColor Cyan
}
Write-Host ""
