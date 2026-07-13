# Level AI-ditor External

`Level AI-ditor External` is a small standalone editor for `resources/levels/level_test.json`.

It is meant to make the tool look and behave like an external level/balance editor, while the game remains the runtime preview.

## Features

- Open and save `level_test.json`
- Inspect `objects`, `spawnAreas`, and `bossPhases`
- Preview the arena as a neon top-down map
- Edit scalar values under `balance`
- Validate common level problems
- Capture human design intent for broad AI consultation
- Generate `resources/levels/ai_design_brief.md` for stage, tank, enemy, and balance proposals
- Generate `resources/levels/tank_dictionary.json` as shared vocabulary for tank ideas
- Generate `resources/levels/ai_balance_handoff.md` for AI-assisted tuning

## Run

From the project root:

```powershell
python tools\level_aiditor\level_aiditor.py
```

## Workflow

1. Place objects in Blender and export `level_test.json`
2. Open `level_test.json` in this external editor
3. Check the Arena Preview and validation warnings
4. Use the AI Plan tab to write a broad design brief when you want Codex or ChatGPT to suggest stage, enemy, tank, or boss phase ideas
5. Tune `balance` manually after choosing the useful AI proposals
6. Save JSON
7. Press `F10` in the game to hot reload
8. Generate AI handoff markdown when you want focused balance tuning suggestions

## AI Collaboration Flow

The intended workflow is not full auto-generation.

1. The human designer chooses the high-level direction in the AI Plan tab.
2. The tool writes `ai_design_brief.md`.
3. Codex or ChatGPT proposes broad ideas and small JSON edits.
4. The human designer checks the Arena Preview and validation report.
5. Fine tuning happens in the Balance tab and in the running game.

## Positioning

The game-side ImGui window is a runtime preview and quick tuning surface.
This external editor is the part that can be presented as a standalone tool.
