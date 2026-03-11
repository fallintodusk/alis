@echo off
REM Build Orchestrator plugin for standalone distribution
REM This builds the plugin in Shipping configuration for Windows

setlocal enabledelayedexpansion

REM Configuration
set ENGINE_PATH=<ue-path>
set PROJECT_PATH=<project-root>
set PLUGIN_PATH=%PROJECT_PATH%\Plugins\Orchestrator
set OUTPUT_PATH=%PROJECT_PATH%\Build\Plugins\Orchestrator
set VERSION=1.0.0

echo ========================================
echo Building Orchestrator Plugin
echo ========================================
echo.
echo Engine: %ENGINE_PATH%
echo Plugin: %PLUGIN_PATH%
echo Output: %OUTPUT_PATH%
echo Version: %VERSION%
echo.

REM Check if plugin exists
if not exist "%PLUGIN_PATH%\Orchestrator.uplugin" (
    echo ERROR: Orchestrator.uplugin not found at %PLUGIN_PATH%
    exit /b 1
)

REM Create output directory
if not exist "%OUTPUT_PATH%" (
    mkdir "%OUTPUT_PATH%"
)

REM Build the plugin using RunUAT
echo.
echo Running RunUAT BuildPlugin...
echo.

"%ENGINE_PATH%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin ^
    -Plugin="%PLUGIN_PATH%\Orchestrator.uplugin" ^
    -Package="%OUTPUT_PATH%\%VERSION%" ^
    -TargetPlatforms=Win64 ^
    -Rocket

if errorlevel 1 (
    echo.
    echo ERROR: BuildPlugin failed
    exit /b 1
)

echo.
echo ========================================
echo Build Complete!
echo ========================================
echo.
echo Output: %OUTPUT_PATH%\%VERSION%
echo.
echo Files:
dir /b "%OUTPUT_PATH%\%VERSION%"
echo.

REM Optional: Create a versioned zip for distribution
if exist "%OUTPUT_PATH%\%VERSION%\Orchestrator.uplugin" (
    echo Creating distribution package...

    set ZIP_NAME=Orchestrator_%VERSION%_Win64.zip
    set ZIP_PATH=%OUTPUT_PATH%\!ZIP_NAME!

    REM Use PowerShell to create zip (available on Windows 10+)
    powershell -Command "Compress-Archive -Path '%OUTPUT_PATH%\%VERSION%\*' -DestinationPath '%ZIP_PATH%' -Force"

    if exist "!ZIP_PATH!" (
        echo.
        echo Distribution package created: !ZIP_PATH!

        REM Compute SHA256 hash for verification
        for /f "tokens=*" %%h in ('powershell -Command "Get-FileHash -Path '!ZIP_PATH!' -Algorithm SHA256 | Select-Object -ExpandProperty Hash"') do (
            echo SHA256: %%h
            echo %%h > "%OUTPUT_PATH%\!ZIP_NAME!.sha256"
        )
    )
)

echo.
echo ========================================
echo Next Steps:
echo ========================================
echo 1. Test the plugin by copying to %<local-app-data>%\Alis\Plugins\Orchestrator\%VERSION%
echo 2. Update manifest with code_hash (SHA256 from above)
echo 3. Deploy to CDN for OTA updates
echo.

endlocal
