@echo off
rem Rebuild project\*.vcxproj.filters from the folders of the files (see project\build\sync_vcxproj_filters.ps1).
setlocal
cd /d "%~dp0project"
powershell -NoProfile -ExecutionPolicy Bypass -File "build\sync_vcxproj_filters.ps1" %*
set RESULT=%ERRORLEVEL%
if "%~1"=="" pause
exit /b %RESULT%
