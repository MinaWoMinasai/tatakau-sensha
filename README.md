[![DebugBuild](https://github.com/MinaWoMinasai/CG2/actions/workflows/DebugBuild.yml/badge.svg)](https://github.com/MinaWoMinasai/CG2/actions/workflows/DebugBuild.yml)
[![ReleaseBuild](https://github.com/MinaWoMinasai/CG2/actions/workflows/ReleaseBuild.yml/badge.svg)](https://github.com/MinaWoMinasai/CG2/actions/workflows/ReleaseBuild.yml)
[![DevelopmentBuild](https://github.com/MinaWoMinasai/CG2/actions/workflows/DevelopmentBuild.yml/badge.svg)](https://github.com/MinaWoMinasai/CG2/actions/workflows/DevelopmentBuild.yml)
[![CheckUnwantedFiles](https://github.com/MinaWoMinasai/CG2/actions/workflows/CheckUnwantedFiles.yml/badge.svg)](https://github.com/MinaWoMinasai/CG2/actions/workflows/CheckUnwantedFiles.yml)

## Local dependencies

Generated build outputs and binary libraries are intentionally not committed to
this repository. In particular, Assimp must exist only as local generated files.
After downloading the project ZIP from GitHub, run:

```powershell
.\project\tools\bootstrap_dependencies.ps1
```

Then open `project/CG2.sln` in Visual Studio 2022 and build the solution.

The bootstrap step creates files such as:

- `project/externals/assimp/lib/assimp-vc143-mt.lib`
- `project/externals/assimp/runtime/assimp-vc143-mt.dll`

Do not add generated `.lib` or `.dll` files to Git; the repository health check
expects them to stay untracked.
