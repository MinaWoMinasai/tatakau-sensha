# Level AI-ditor External

`Level AI-ditor External` is a small standalone editor for `resources/levels/level_test.json`.

It is meant to make the tool look and behave like an external level/balance editor, while the game remains the runtime preview.

## Features

- Open and save `level_test.json`
- Inspect `objects`, `spawnAreas`, and `bossPhases`
- Edit scalar values under `balance`
- Validate common level problems
- Generate `resources/levels/ai_balance_handoff.md` for AI-assisted tuning

## Run

From the project root:

```powershell
python tools\level_aiditor\level_aiditor.py
```

## Workflow

1. Place objects in Blender and export `level_test.json`
2. Open `level_test.json` in this external editor
3. Check validation warnings
4. Tune `balance`
5. Save JSON
6. Press `F10` in the game to hot reload
7. Generate AI handoff markdown when you want AI review or tuning suggestions

## Positioning

The game-side ImGui window is a runtime preview and quick tuning surface.
This external editor is the part that can be presented as a standalone tool.
