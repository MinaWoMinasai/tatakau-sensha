@echo off
setlocal
cd /d "%~dp0project"
if not exist "..\generated\neon_windmill\iphone_atlas.png" (
    echo iPhone emoji atlas is missing. Use the author-local project/tools/prepare_windmill_emoji.py.
    pause
    exit /b 1
)
if exist "..\generated\outputs\Release\MyEngine.exe" (
    "..\generated\outputs\Release\MyEngine.exe" --project resources/projects/neon_windmill.project.json
) else (
    "..\generated\outputs\Development\MyEngine.exe" --project resources/projects/neon_windmill.project.json
)
