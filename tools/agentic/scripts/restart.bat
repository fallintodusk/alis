@echo off
REM Restart LangGraph dev in tools/agentic
REM Assumes python and langgraph are installed

echo Stopping any running dev server on port 5001...
for /f "tokens=2" %%i in ('netstat -ano ^| find "5001" ^| find "LISTENING"') do (
    taskkill /PID %%i /F >nul 2>&1
)

cd /d %~dp0\..
echo Starting LangGraph dev with .env loaded...
python scripts/run.py
pause
