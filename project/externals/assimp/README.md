# Assimp for CG2

This directory contains the headers and build instructions for the Assimp 5.4.3
runtime used by CG2. Generated libraries are kept locally and are not committed
to the repository.

- Built with Visual Studio 2026 (`v145`), x64.
- Shared-library build to keep the repository and executable small.
- Only the glTF/glb importer is enabled for the animation pipeline.
- The rebuild script creates `lib/assimp-vc145-mt.lib` and
  `runtime/assimp-vc145-mt.dll` locally.
- `runtime/assimp-vc145-mt.dll` is copied to `$(TargetDir)` automatically when
  CG2 is built.

The source archive and generated binaries are not vendored. Obtain the official
Assimp 5.4.3 source, then rebuild the local dependencies with:

```powershell
.\tools\build_assimp_vs2026.ps1 -SourceDir C:\path\to\assimp-5.4.3
```

Assimp is distributed under the BSD 3-Clause license. See `LICENSE.txt`.
