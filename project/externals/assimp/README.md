# Assimp for CG2

This directory contains the Assimp 5.4.3 runtime used by CG2.

- Built with Visual Studio 2026 (`v145`), x64.
- Shared-library build to keep the repository and executable small.
- Only the glTF/glb importer is enabled for the animation pipeline.
- `runtime/assimp-vc145-mt.dll` is copied to `$(TargetDir)` automatically.

The source archive is not vendored. To rebuild the binaries, run:

```powershell
.\tools\build_assimp_vs2026.ps1 -SourceDir C:\path\to\assimp-5.4.3
```

Assimp is distributed under the BSD 3-Clause license. See `LICENSE.txt`.
