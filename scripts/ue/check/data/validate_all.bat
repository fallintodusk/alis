@echo off
REM Cross-reference data validation - checks object, dialogue, and audio refs
setlocal

set SCRIPT_DIR=%~dp0

call "%SCRIPT_DIR%..\..\..\config\resolve_ue_path.bat" >nul 2>&1
if defined UE_PATH (
    if exist "%UE_PATH%\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" (
        "%UE_PATH%\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" "%SCRIPT_DIR%validate_all.py" %*
        exit /b %ERRORLEVEL%
    )
)

python "%SCRIPT_DIR%validate_all.py" %*
exit /b %ERRORLEVEL%
