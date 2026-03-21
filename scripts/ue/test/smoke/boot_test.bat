@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem ============================================================================
rem Standalone Boot Map Test (robust)
rem ============================================================================

rem Go to repo root (scripts\ue\test\smoke\ -> ..\..\..\..)
pushd "%~dp0\..\..\..\.." || (echo ERROR: cannot cd to repo root & exit /b 1)

rem Optional: UTF-8 for nice glyphs in console
chcp 65001 >nul

rem ---------- Config (SOT: resolve_ue_path.bat) ----------
call "scripts\config\resolve_ue_path.bat"
if errorlevel 1 (echo ERROR: UE_PATH not resolved & exit /b 1)

set "PROJECT_FILE=%CD%\Alis.uproject"
set "UE_EDITOR=%UE_PATH%\Engine\Binaries\Win64\UnrealEditor.exe"
set "BOOT_MAP=/MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent"
set "RUN_TIME_SECONDS=60"

for %%# in ("%CD%\Saved\Logs") do if not exist "%%~f#" mkdir "%%~f#" >nul 2>&1
set "RUN_ID=%RANDOM%%RANDOM%"
set "LOG_FILE=%CD%\Saved\Logs\boot_test_%RUN_ID%.log"
set "LOG_FILE_ARG=%LOG_FILE%"
for %%I in ("%LOG_FILE%") do set "LOG_FILE_ARG=%%~sI"
if not defined LOG_FILE_ARG set "LOG_FILE_ARG=%LOG_FILE%"
set "PID_FILE=%CD%\Saved\Logs\boot_test.pid"

echo ============================================================================
echo   Standalone Boot Map Test
echo ============================================================================
echo   UE Editor: %UE_EDITOR%
echo   Project :  %PROJECT_FILE%
echo   Boot Map:  %BOOT_MAP%
echo   Runtime :  %RUN_TIME_SECONDS%s
echo   Log     :  %LOG_FILE%
echo.

rem ---------- Sanity checks ----------
if not exist "%UE_EDITOR%" (echo ERROR: UnrealEditor.exe not found at: %UE_EDITOR% & exit /b 1)
if not exist "%PROJECT_FILE%" (echo ERROR: Project file not found at: %PROJECT_FILE% & exit /b 1)

rem ---------- Cleanup ----------
echo [1/5] Cleaning up...
set "GAME_PID="
if exist "%PID_FILE%" (
  set /p GAME_PID=<"%PID_FILE%"
  if defined GAME_PID taskkill /F /PID !GAME_PID! >nul 2>&1
  del "%PID_FILE%" >nul 2>&1
)
echo.

rem ---------- Launch ----------
echo [2/5] Launching Boot map...
echo   %UE_EDITOR% "%PROJECT_FILE%" %BOOT_MAP% -game -windowed -ResX=1280 -ResY=720 -log -abslog="%LOG_FILE%"
for /f "usebackq delims=" %%P in (`powershell -NoProfile -Command "$absLogArg = '-abslog=' + '%LOG_FILE_ARG%'; $argsList = @('%PROJECT_FILE%','%BOOT_MAP%','-game','-windowed','-ResX=1280','-ResY=720','-log',$absLogArg); $p = Start-Process -FilePath '%UE_EDITOR%' -ArgumentList $argsList -PassThru; $p.Id"`) do (
  set "GAME_PID=%%P"
)
if not defined GAME_PID (echo ERROR: failed to start UnrealEditor & exit /b 1)
> "%PID_FILE%" echo %GAME_PID%

rem ---------- Wait loop ----------
echo [3/5] Waiting for Boot flow to complete (%RUN_TIME_SECONDS%s)...
set /a ELAPSED=0
set /a CHECK_INTERVAL=2

:WAIT_LOOP
timeout /t %CHECK_INTERVAL% /nobreak >nul 2>&1
set /a ELAPSED+=%CHECK_INTERVAL%

if defined GAME_PID (
  tasklist /FI "PID eq !GAME_PID!" 2>nul | findstr /C:"!GAME_PID!" >nul 2>&1
  if !errorlevel! neq 0 (
    echo.
    echo   [X] Editor process exited early. Check log for crash details.
    goto KILL_GAME
  )
)

if exist "%LOG_FILE%" (
  findstr /C:"EXCEPTION_ACCESS_VIOLATION" /C:"Fatal error!" "%LOG_FILE%" >nul 2>&1
  if !errorlevel! equ 0 (
    echo.
    echo   [X] CRASH DETECTED! Check log for details.
    goto KILL_GAME
  )
)

echo   Running... (!ELAPSED!/%RUN_TIME_SECONDS%s)
if !ELAPSED! geq %RUN_TIME_SECONDS% goto KILL_GAME
goto WAIT_LOOP

:KILL_GAME
echo.
echo [4/5] Stopping game...
if defined GAME_PID taskkill /F /PID !GAME_PID! >nul 2>&1
if exist "%PID_FILE%" del "%PID_FILE%" >nul 2>&1
timeout /t 1 /nobreak >nul 2>&1
echo   [OK] Game stopped
echo.

rem ---------- Analyze logs ----------
echo [5/5] Analyzing logs...
if not exist "%LOG_FILE%" (echo   [X] ERROR: Log file not found! & popd & endlocal & exit /b 1)

set /a CHECK_FAILED=0

echo Checking orchestrator boot path...
findstr /C:"=== Orchestrator Boot Complete ===" "%LOG_FILE%" >nul 2>&1
if !errorlevel! equ 0 (
  echo   [OK] Orchestrator boot completed
  findstr /C:"=== Orchestrator Boot Complete ===" "%LOG_FILE%"
) else (
  echo   [X] Orchestrator boot completion marker missing
  set /a CHECK_FAILED+=1
)

echo.
echo Checking menu feature bootstrap...
findstr /C:"LogProjectMenuMain: BootInitialize()" "%LOG_FILE%" >nul 2>&1
if !errorlevel! equ 0 (
  echo   [OK] ProjectMenuMain BootInitialize executed
  findstr /C:"LogProjectMenuMain: BootInitialize()" "%LOG_FILE%"
) else (
  echo   [X] ProjectMenuMain BootInitialize marker missing
  set /a CHECK_FAILED+=1
)

echo.
echo Checking UI framework module startup...
findstr /C:"ProjectUI module started" "%LOG_FILE%" >nul 2>&1
if !errorlevel! equ 0 (
  echo   [OK] ProjectUI module started
  findstr /C:"ProjectUI module started" "%LOG_FILE%"
) else (
  echo   [X] ProjectUI module startup marker missing
  set /a CHECK_FAILED+=1
)

echo.
echo Checking boot map command line...
findstr /C:"Command Line: /MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent" "%LOG_FILE%" >nul 2>&1
if !errorlevel! equ 0 (
  echo   [OK] Boot map command line detected
  findstr /C:"Command Line: /MainMenuWorld/Maps/MainMenu_Persistent.MainMenu_Persistent" "%LOG_FILE%"
) else (
  echo   [X] Boot map command line marker missing
  set /a CHECK_FAILED+=1
)

echo.
echo ============================================================================
if !CHECK_FAILED! equ 0 (
  echo   TEST RESULT: SUCCESS [OK]
  echo   Boot smoke checks passed.
  echo   Full logs: %LOG_FILE%
  echo ============================================================================
  popd
  endlocal & exit /b 0
) else (
  echo   TEST RESULT: FAILED [X]
  echo   Required checks failed: !CHECK_FAILED!
  echo   Check logs: %LOG_FILE%
  echo   Common issues:
  echo     1. Orchestrator boot sequence did not complete
  echo     2. ProjectMenuMain BootInitialize was not executed
  echo     3. ProjectUI module did not start
  echo     4. Boot map command line is not the expected MainMenu map
  echo ============================================================================
  popd
  endlocal & exit /b 1
)
