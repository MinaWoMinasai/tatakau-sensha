# Blender Level AI-ditor Scripts

This folder contains small Blender Text Editor scripts for `Level AI-ditor`.

## Export Blender To JSON

Script:

```text
tools/blender/export_level_test.py
```

Use this when you placed objects in Blender and want to create `level_test.json`.
By default, the script saves to:

```text
resources/levels/level_test.json
```

If that JSON already exists, the script replaces only `objects` and keeps existing `balance`, `spawnAreas`, and `bossPhases`.

Name examples:

```text
PlayerSpawn_001
BossSpawn_001
Enemy_Basic_001
SpawnArea_Shooter_001
Obstacle_Wall_001
Obstacle_DamageBlock_001
Item_Heal_001
```

## Import JSON To Blender

Script:

```text
tools/blender/import_level_test.py
```

Use this when you already have `resources/levels/level_test.json` and want to visualize it in Blender.

The script creates a collection named:

```text
Level AI-ditor Import
```

It imports:

- `objects`
- root `spawnAreas`
- `bossPhases[].objects`

## Import Visual Rules

- `PlayerSpawn`: green sphere Empty
- `BossSpawn`: red sphere Empty
- `SpawnArea`: blue wire cube
- `Wall`: green cube
- `DamageBlock`: red cube
- `Enemy`: simple mesh based on prefab
- `Item`: small sphere
- boss phase objects are grouped by phase collection

## If The JSON File Is Not Found

Open `tools/blender/import_level_test.py` and set:

```python
LEVEL_JSON_PATH = r"C:\path\to\resources\levels\level_test.json"
```

Then run the script again from Blender's Text Editor.

## If Export Goes To The Wrong Place

Open `tools/blender/export_level_test.py` and set:

```python
LEVEL_JSON_PATH = r"C:\path\to\resources\levels\level_test.json"
```

Then run the script again.
