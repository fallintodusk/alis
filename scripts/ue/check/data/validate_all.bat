@echo off
REM Cross-reference data validation - checks object, dialogue, and audio refs
python "%~dp0validate_all.py" %*
