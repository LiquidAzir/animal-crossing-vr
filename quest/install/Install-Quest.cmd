@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Quest.ps1" %*
set "install_result=%errorlevel%"
echo.
pause
exit /b %install_result%
