@echo off
setlocal
title Eternal Launcher - Preparar atualizacao
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\PrepareRelease.ps1" %*
set "release_result=%errorlevel%"
echo.
if not "%release_result%"=="0" echo A preparacao nao foi concluida. Confira a mensagem acima.
pause
exit /b %release_result%
