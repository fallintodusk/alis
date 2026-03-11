@echo off
setlocal
set PROJ=<project-root>\Alis.uproject
set UE=<ue-path>
set OUT=<project-root>\Saved\Automation
set LOG=<project-root>\Saved\Logs\unit_tests.log

if not exist "<project-root>\Saved\Logs" mkdir "<project-root>\Saved\Logs"
if not exist "%OUT%" mkdir "%OUT%"

rem Unit test filters based on run_tests.sh --unit
set FILTERS=ProjectCore;ProjectBoot;ProjectLoading;ProjectSession;ProjectData;ProjectUI;ProjectMenuUI;ProjectContentPacks;ProjectGameFeatures;ProjectSave;ProjectCombat;ProjectDialogue;ProjectInventory

set EXEC_CMDS=
for %%F in (%FILTERS%) do (
    if defined EXEC_CMDS (
        set EXEC_CMDS=!EXEC_CMDS!;Automation RunTests %%F
    ) else (
        set EXEC_CMDS=Automation RunTests %%F
    )
)

set EXEC_CMDS=%EXEC_CMDS%;Quit

"%UE%\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%PROJ%" -ExecCmds="%EXEC_CMDS%" -unattended -nop4 -NoSound -NullRHI -testexit="Automation Test Queue Empty" -ReportOutputPath="%OUT%" > "%LOG%" 2>&1
exit /b %ERRORLEVEL%
