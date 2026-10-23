# Assimp for MyEngine

This directory contains the headers and a prebuilt copy of the Assimp 5.4.3
runtime used by MyEngine, so the solution builds right after cloning or
downloading the repository as a ZIP.

- Committed binaries: `lib/assimp-vc143-mt.lib` and `runtime/assimp-vc143-mt.dll`,
  built with Visual Studio 2022 (`v143`), x64. They work with both the `v143`
  and `v145` toolsets.
- Shared-library build with a static CRT (`USE_STATIC_CRT=ON`), to keep the
  repository and executable small.
- Only the glTF/glb importer is enabled for the animation pipeline
  (`ASSIMP_BUILD_ZLIB=ON`, no exporter, no Draco).
- `runtime/assimp-vc*-mt.dll` is copied to `$(TargetDir)` automatically when
  the game is built.

To rebuild Assimp locally (for example with Visual Studio 2026, which produces
`assimp-vc145-mt`), run from the repository root:

```powershell
.\project\build\bootstrap_dependencies.ps1 -Force
```

Locally rebuilt binaries other than the committed `vc143` pair stay untracked,
and a local `assimp-vc145-mt.lib` takes precedence when it exists. Run the script
from a short, ASCII-only path; CMake can fail under very long paths or when the
current directory contains non-ASCII characters.

Assimp is distributed under the BSD 3-Clause license. See `LICENSE.txt`.
