@echo off
setlocal
cd /d "%~dp0project"
if not exist "..\generated\neon_windmill\iphone_atlas.png" (
    echo iPhone emoji atlas is missing. See docs/neon-windmill.md.
    pause
    exit /b 1
)
if exist "..\generated\outputs\Release\CG2.exe" (
    "..\generated\outputs\Release\CG2.exe" --project resources/projects/neon_windmill.project.json
) else (
    "..\generated\outputs\Development\CG2.exe" --project resources/projects/neon_windmill.project.json
)
