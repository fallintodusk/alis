@echo off
setlocal

rem Validate text files and paths against the ALIS character-set policy:
rem no foreign-script symbols/comments in content, and ASCII-only paths.
rem Content blocks are extensible data in the .py.
rem SOT: CLAUDE.md "ASCII-ONLY DOCUMENTATION".

set SCRIPT_DIR=%~dp0

where python >nul 2>&1
if %ERRORLEVEL% equ 0 (
    python "%SCRIPT_DIR%validate_text_format.py" %*
    exit /b %ERRORLEVEL%
)

rem Fall back to UE bundled Python (scripts/ue/check/governance -> scripts/config)
call "%SCRIPT_DIR%..\..\..\config\resolve_ue_path.bat" >nul 2>&1
if defined UE_PATH (
    "%UE_PATH%\Engine\Binaries\ThirdParty\Python3\Win64\python.exe" "%SCRIPT_DIR%validate_text_format.py" %*
    exit /b %ERRORLEVEL%
)

echo ERROR: Python not found (system or UE bundled)
exit /b 1
