@echo off
REM Setup development environment for Build Service
REM Run this after installing Rust

echo ============================================
echo   Build Service - Development Setup
echo ============================================
echo.

REM Check if Rust is installed
cargo --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Rust is not installed or not in PATH
    echo Please run: scripts\install_rust_windows.bat
    pause
    exit /b 1
)

echo Rust version:
cargo --version
echo.

REM Check the SOURCE engine (BuildService packages with UE_SOURCE_PATH
REM from scripts/config/ue_path.conf - the repo SOT, not a toml key)
call "%~dp0..\..\..\scripts\configesolve_ue_path.bat"
if not defined UE_SOURCE_PATH (
    echo WARNING: UE_SOURCE_PATH not declared in scripts/config/ue_path.conf
    echo.
) else if not exist "%UE_SOURCE_PATH%\Engine\Build\BatchFiles\RunUAT.bat" (
    echo WARNING: source engine not found at %UE_SOURCE_PATH%
    echo.
)

echo Running cargo check to verify workspace...
cd /d "%~dp0.."
cargo check

if errorlevel 1 (
    echo ERROR: cargo check failed
    echo Please review error messages above
    pause
    exit /b 1
)

echo.
echo ============================================
echo   Development environment ready!
echo ============================================
echo.
echo Next steps:
echo 1. Review config/build_service.toml
echo 2. Run: cargo build
echo 3. Run: cargo test
echo.

pause
