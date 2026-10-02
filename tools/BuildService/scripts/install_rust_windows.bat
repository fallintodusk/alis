@echo off
REM Install Rust toolchain on Windows
REM This script downloads and installs rustup (Rust toolchain installer)

echo ============================================
echo   Build Service - Rust Installation
echo ============================================
echo.

echo This script will install Rust via rustup.
echo.
echo Prerequisites:
echo - Windows 10 or later
echo - Visual Studio Build Tools (C++ compiler)
echo.

pause

echo.
echo Downloading rustup-init.exe...
curl --proto '=https' --tlsv1.2 -sSf https://win.rustup.rs/x86_64 -o %TEMP%\rustup-init.exe

if errorlevel 1 (
    echo ERROR: Failed to download rustup-init.exe
    echo Please download manually from: https://rustup.rs/
    pause
    exit /b 1
)

echo.
echo Running rustup-init.exe...
%TEMP%\rustup-init.exe -y --default-toolchain stable

if errorlevel 1 (
    echo ERROR: Rust installation failed
    pause
    exit /b 1
)

echo.
echo Rust installed successfully!
echo.
echo Please close this terminal and open a new one to use cargo.
echo Then run: cargo --version
echo.

pause
