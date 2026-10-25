@echo off
setlocal
title Eternal Launcher - Prepare update
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\PrepareRelease.ps1" %*
set "release_result=%errorlevel%"
echo.
if not "%release_result%"=="0" echo Preparation did not finish. Check the message above.
pause
exit /b %release_result%
