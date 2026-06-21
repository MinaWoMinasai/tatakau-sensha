Package: assimp:x64-windows@5.4.3

**Host Environment**

- Host: x64-windows
- Compiler: MSVC 19.44.35227.0
-    vcpkg-tool version: 2025-04-01-9c604254140797833b6f76908435c9fcbf09920e
    vcpkg-readonly: true
    vcpkg-scripts version: 4f8fe05871555c1798dbcb1957d0d595e94f7b57

**To Reproduce**

`vcpkg install `

**Failure logs**

```
Downloading https://github.com/assimp/assimp/archive/v5.4.3.tar.gz -> assimp-assimp-v5.4.3.tar.gz
Successfully downloaded assimp-assimp-v5.4.3.tar.gz
-- Extracting source C:/Users/k024g/AppData/Local/vcpkg/downloads/assimp-assimp-v5.4.3.tar.gz
-- Applying patch build_fixes.patch
-- Using source at C:/tmp/cg2-vcpkg-buildtrees-assimp543/assimp/src/v5.4.3-ee50b368a2.clean
-- Found external ninja('1.13.2').
-- Configuring x64-windows
CMake Error at scripts/cmake/vcpkg_execute_required_process.cmake:127 (message):
    Command failed: "C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe" -v
    Working Directory: C:/tmp/cg2-vcpkg-buildtrees-assimp543/assimp/x64-windows-rel/vcpkg-parallel-configure
    Error code: 1
    See logs for more information:
      C:\tmp\cg2-vcpkg-buildtrees-assimp543\assimp\config-x64-windows-dbg-CMakeConfigureLog.yaml.log
      C:\tmp\cg2-vcpkg-buildtrees-assimp543\assimp\config-x64-windows-rel-CMakeConfigureLog.yaml.log
      C:\tmp\cg2-vcpkg-buildtrees-assimp543\assimp\config-x64-windows-out.log

Call Stack (most recent call first):
  C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/vcpkg_installed/x64-windows/share/vcpkg-cmake/vcpkg_cmake_configure.cmake:269 (vcpkg_execute_required_process)
  C:/Users/k024g/AppData/Local/vcpkg/registries/git-trees/3356e9c8083aae3cfcd24dd3269b45e2cae3173f/portfile.cmake:30 (vcpkg_cmake_configure)
  scripts/ports.cmake:203 (include)



```

<details><summary>C:\tmp\cg2-vcpkg-buildtrees-assimp543\assimp\config-x64-windows-out.log</summary>

```
[1/2] "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/bin/cmake.exe" -E chdir "../../x64-windows-dbg" "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/bin/cmake.exe" "C:/tmp/cg2-vcpkg-buildtrees-assimp543/assimp/src/v5.4.3-ee50b368a2.clean" "-G" "Ninja" "-DCMAKE_BUILD_TYPE=Debug" "-DCMAKE_INSTALL_PREFIX=C:/tmp/cg2-vcpkg-packages-assimp543/assimp_x64-windows/debug" "-DFETCHCONTENT_FULLY_DISCONNECTED=ON" "-DASSIMP_BUILD_ZLIB=OFF" "-DASSIMP_BUILD_ASSIMP_TOOLS=OFF" "-DASSIMP_BUILD_TESTS=OFF" "-DASSIMP_WARNINGS_AS_ERRORS=OFF" "-DASSIMP_IGNORE_GIT_HASH=ON" "-DASSIMP_INSTALL_PDB=OFF" "-DCMAKE_MAKE_PROGRAM=C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe" "-DBUILD_SHARED_LIBS=ON" "-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/toolchains/windows.cmake" "-DVCPKG_TARGET_TRIPLET=x64-windows" "-DVCPKG_SET_CHARSET_FLAG=ON" "-DVCPKG_PLATFORM_TOOLSET=v143" "-DCMAKE_EXPORT_NO_PACKAGE_REGISTRY=ON" "-DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY=ON" "-DCMAKE_FIND_PACKAGE_NO_SYSTEM_PACKAGE_REGISTRY=ON" "-DCMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP=TRUE" "-DCMAKE_VERBOSE_MAKEFILE=ON" "-DVCPKG_APPLOCAL_DEPS=OFF" "-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake" "-DCMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION=ON" "-DVCPKG_CXX_FLAGS= -D_CRT_SECURE_NO_WARNINGS" "-DVCPKG_CXX_FLAGS_RELEASE=" "-DVCPKG_CXX_FLAGS_DEBUG=" "-DVCPKG_C_FLAGS= -D_CRT_SECURE_NO_WARNINGS" "-DVCPKG_C_FLAGS_RELEASE=" "-DVCPKG_C_FLAGS_DEBUG=" "-DVCPKG_CRT_LINKAGE=dynamic" "-DVCPKG_LINKER_FLAGS=" "-DVCPKG_LINKER_FLAGS_RELEASE=" "-DVCPKG_LINKER_FLAGS_DEBUG=" "-DVCPKG_TARGET_ARCHITECTURE=x64" "-DCMAKE_INSTALL_LIBDIR:STRING=lib" "-DCMAKE_INSTALL_BINDIR:STRING=bin" "-D_VCPKG_ROOT_DIR=C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg" "-D_VCPKG_INSTALLED_DIR=C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/vcpkg_installed" "-DVCPKG_MANIFEST_INSTALL=OFF"
FAILED: [code=1] ../../x64-windows-dbg/CMakeCache.txt 
"C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/bin/cmake.exe" -E chdir "../../x64-windows-dbg" "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/bin/cmake.exe" "C:/tmp/cg2-vcpkg-buildtrees-assimp543/assimp/src/v5.4.3-ee50b368a2.clean" "-G" "Ninja" "-DCMAKE_BUILD_TYPE=Debug" "-DCMAKE_INSTALL_PREFIX=C:/tmp/cg2-vcpkg-packages-assimp543/assimp_x64-windows/debug" "-DFETCHCONTENT_FULLY_DISCONNECTED=ON" "-DASSIMP_BUILD_ZLIB=OFF" "-DASSIMP_BUILD_ASSIMP_TOOLS=OFF" "-DASSIMP_BUILD_TESTS=OFF" "-DASSIMP_WARNINGS_AS_ERRORS=OFF" "-DASSIMP_IGNORE_GIT_HASH=ON" "-DASSIMP_INSTALL_PDB=OFF" "-DCMAKE_MAKE_PROGRAM=C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe" "-DBUILD_SHARED_LIBS=ON" "-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/toolchains/windows.cmake" "-DVCPKG_TARGET_TRIPLET=x64-windows" "-DVCPKG_SET_CHARSET_FLAG=ON" "-DVCPKG_PLATFORM_TOOLSET=v143" "-DCMAKE_EXPORT_NO_PACKAGE_REGISTRY=ON" "-DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY=ON" "-DCMAKE_FIND_PACKAGE_NO_SYSTEM_PACKAGE_REGISTRY=ON" "-DCMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP=TRUE" "-DCMAKE_VERBOSE_MAKEFILE=ON" "-DVCPKG_APPLOCAL_DEPS=OFF" "-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake" "-DCMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION=ON" "-DVCPKG_CXX_FLAGS= -D_CRT_SECURE_NO_WARNINGS" "-DVCPKG_CXX_FLAGS_RELEASE=" "-DVCPKG_CXX_FLAGS_DEBUG=" "-DVCPKG_C_FLAGS= -D_CRT_SECURE_NO_WARNINGS" "-DVCPKG_C_FLAGS_RELEASE=" "-DVCPKG_C_FLAGS_DEBUG=" "-DVCPKG_CRT_LINKAGE=dynamic" "-DVCPKG_LINKER_FLAGS=" "-DVCPKG_LINKER_FLAGS_RELEASE=" "-DVCPKG_LINKER_FLAGS_DEBUG=" "-DVCPKG_TARGET_ARCHITECTURE=x64" "-DCMAKE_INSTALL_LIBDIR:STRING=lib" "-DCMAKE_INSTALL_BINDIR:STRING=bin" "-D_VCPKG_ROOT_DIR=C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg" "-D_VCPKG_INSTALLED_DIR=C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/vcpkg_installed" "-DVCPKG_MANIFEST_INSTALL=OFF"
-- The C compiler identification is MSVC 19.44.35227.0
-- The CXX compiler identification is MSVC 19.44.35227.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.44.35207/bin/HostX64/x64/cl.exe - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.44.35207/bin/HostX64/x64/cl.exe - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Shared libraries enabled
Exit code 0xc0000409

[2/2] "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/bin/cmake.exe" -E chdir ".." "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/bin/cmake.exe" "C:/tmp/cg2-vcpkg-buildtrees-assimp543/assimp/src/v5.4.3-ee50b368a2.clean" "-G" "Ninja" "-DCMAKE_BUILD_TYPE=Release" "-DCMAKE_INSTALL_PREFIX=C:/tmp/cg2-vcpkg-packages-assimp543/assimp_x64-windows" "-DFETCHCONTENT_FULLY_DISCONNECTED=ON" "-DASSIMP_BUILD_ZLIB=OFF" "-DASSIMP_BUILD_ASSIMP_TOOLS=OFF" "-DASSIMP_BUILD_TESTS=OFF" "-DASSIMP_WARNINGS_AS_ERRORS=OFF" "-DASSIMP_IGNORE_GIT_HASH=ON" "-DASSIMP_INSTALL_PDB=OFF" "-DCMAKE_MAKE_PROGRAM=C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe" "-DBUILD_SHARED_LIBS=ON" "-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/toolchains/windows.cmake" "-DVCPKG_TARGET_TRIPLET=x64-windows" "-DVCPKG_SET_CHARSET_FLAG=ON" "-DVCPKG_PLATFORM_TOOLSET=v143" "-DCMAKE_EXPORT_NO_PACKAGE_REGISTRY=ON" "-DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY=ON" "-DCMAKE_FIND_PACKAGE_NO_SYSTEM_PACKAGE_REGISTRY=ON" "-DCMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP=TRUE" "-DCMAKE_VERBOSE_MAKEFILE=ON" "-DVCPKG_APPLOCAL_DEPS=OFF" "-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake" "-DCMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION=ON" "-DVCPKG_CXX_FLAGS= -D_CRT_SECURE_NO_WARNINGS" "-DVCPKG_CXX_FLAGS_RELEASE=" "-DVCPKG_CXX_FLAGS_DEBUG=" "-DVCPKG_C_FLAGS= -D_CRT_SECURE_NO_WARNINGS" "-DVCPKG_C_FLAGS_RELEASE=" "-DVCPKG_C_FLAGS_DEBUG=" "-DVCPKG_CRT_LINKAGE=dynamic" "-DVCPKG_LINKER_FLAGS=" "-DVCPKG_LINKER_FLAGS_RELEASE=" "-DVCPKG_LINKER_FLAGS_DEBUG=" "-DVCPKG_TARGET_ARCHITECTURE=x64" "-DCMAKE_INSTALL_LIBDIR:STRING=lib" "-DCMAKE_INSTALL_BINDIR:STRING=bin" "-D_VCPKG_ROOT_DIR=C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg" "-D_VCPKG_INSTALLED_DIR=C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/vcpkg_installed" "-DVCPKG_MANIFEST_INSTALL=OFF"
FAILED: [code=1] ../CMakeCache.txt 
"C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/bin/cmake.exe" -E chdir ".." "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/bin/cmake.exe" "C:/tmp/cg2-vcpkg-buildtrees-assimp543/assimp/src/v5.4.3-ee50b368a2.clean" "-G" "Ninja" "-DCMAKE_BUILD_TYPE=Release" "-DCMAKE_INSTALL_PREFIX=C:/tmp/cg2-vcpkg-packages-assimp543/assimp_x64-windows" "-DFETCHCONTENT_FULLY_DISCONNECTED=ON" "-DASSIMP_BUILD_ZLIB=OFF" "-DASSIMP_BUILD_ASSIMP_TOOLS=OFF" "-DASSIMP_BUILD_TESTS=OFF" "-DASSIMP_WARNINGS_AS_ERRORS=OFF" "-DASSIMP_IGNORE_GIT_HASH=ON" "-DASSIMP_INSTALL_PDB=OFF" "-DCMAKE_MAKE_PROGRAM=C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe" "-DBUILD_SHARED_LIBS=ON" "-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/toolchains/windows.cmake" "-DVCPKG_TARGET_TRIPLET=x64-windows" "-DVCPKG_SET_CHARSET_FLAG=ON" "-DVCPKG_PLATFORM_TOOLSET=v143" "-DCMAKE_EXPORT_NO_PACKAGE_REGISTRY=ON" "-DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY=ON" "-DCMAKE_FIND_PACKAGE_NO_SYSTEM_PACKAGE_REGISTRY=ON" "-DCMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP=TRUE" "-DCMAKE_VERBOSE_MAKEFILE=ON" "-DVCPKG_APPLOCAL_DEPS=OFF" "-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake" "-DCMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION=ON" "-DVCPKG_CXX_FLAGS= -D_CRT_SECURE_NO_WARNINGS" "-DVCPKG_CXX_FLAGS_RELEASE=" "-DVCPKG_CXX_FLAGS_DEBUG=" "-DVCPKG_C_FLAGS= -D_CRT_SECURE_NO_WARNINGS" "-DVCPKG_C_FLAGS_RELEASE=" "-DVCPKG_C_FLAGS_DEBUG=" "-DVCPKG_CRT_LINKAGE=dynamic" "-DVCPKG_LINKER_FLAGS=" "-DVCPKG_LINKER_FLAGS_RELEASE=" "-DVCPKG_LINKER_FLAGS_DEBUG=" "-DVCPKG_TARGET_ARCHITECTURE=x64" "-DCMAKE_INSTALL_LIBDIR:STRING=lib" "-DCMAKE_INSTALL_BINDIR:STRING=bin" "-D_VCPKG_ROOT_DIR=C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg" "-D_VCPKG_INSTALLED_DIR=C:/Users/k024g/OneDrive/デスクトップ/自作エンジン2/project/vcpkg_installed" "-DVCPKG_MANIFEST_INSTALL=OFF"
-- The C compiler identification is MSVC 19.44.35227.0
-- The CXX compiler identification is MSVC 19.44.35227.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.44.35207/bin/HostX64/x64/cl.exe - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.44.35207/bin/HostX64/x64/cl.exe - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Shared libraries enabled
-- MSVC PDB generation disabled. Release binary will not be debuggable.
Exit code 0xc0000409

ninja: build stopped: subcommand failed.
```
</details>

<details><summary>C:\tmp\cg2-vcpkg-buildtrees-assimp543\assimp\config-x64-windows-dbg-CMakeConfigureLog.yaml.log</summary>

```

---
events:
  -
    kind: "message-v1"
    backtrace:
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineSystem.cmake:205 (message)"
      - "CMakeLists.txt:74 (PROJECT)"
    message: |
      The system is: Windows - 10.0.26200 - AMD64
  -
    kind: "message-v1"
    backtrace:
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerId.cmake:17 (message)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerId.cmake:64 (__determine_compiler_id_test)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCCompiler.cmake:123 (CMAKE_DETERMINE_COMPILER_ID)"
      - "CMakeLists.txt:74 (PROJECT)"
    message: |
      Compiling the C compiler identification source file "CMakeCCompilerId.c" succeeded.
      Compiler: C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.44.35207/bin/HostX64/x64/cl.exe 
      Build flags: /nologo;/DWIN32;/D_WINDOWS;/utf-8;/MP;-D_CRT_SECURE_NO_WARNINGS
      Id flags:  
      
      The output was:
      0
      CMakeCCompilerId.c
      
      
      Compilation of the C compiler identification source "CMakeCCompilerId.c" produced "CMakeCCompilerId.exe"
      
      Compilation of the C compiler identification source "CMakeCCompilerId.c" produced "CMakeCCompilerId.obj"
      
      The C compiler identification is MSVC, found in:
        C:/tmp/cg2-vcpkg-buildtrees-assimp543/assimp/x64-windows-dbg/CMakeFiles/3.30.1/CompilerIdC/CMakeCCompilerId.exe
      
  -
    kind: "message-v1"
    backtrace:
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerId.cmake:1243 (message)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerId.cmake:250 (CMAKE_DETERMINE_MSVC_SHOWINCLUDES_PREFIX)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCCompiler.cmake:123 (CMAKE_DETERMINE_COMPILER_ID)"
      - "CMakeLists.txt:74 (PROJECT)"
    message: |
      Detecting C compiler /showIncludes prefix:
        main.c
        メモ: インクルード ファイル:  C:\\tmp\\cg2-vcpkg-buildtrees-assimp543\\assimp\\x64-windows-dbg\\CMakeFiles\\ShowIncludes\\foo.h
        
      Found prefix "メモ: インクルード ファイル:  "
  -
    kind: "message-v1"
    backtrace:
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerId.cmake:17 (message)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerId.cmake:64 (__determine_compiler_id_test)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCXXCompiler.cmake:126 (CMAKE_DETERMINE_COMPILER_ID)"
      - "CMakeLists.txt:74 (PROJECT)"
    message: |
      Compiling the CXX compiler identification source file "CMakeCXXCompilerId.cpp" succeeded.
      Compiler: C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.44.35207/bin/HostX64/x64/cl.exe 
      Build flags: /nologo;/DWIN32;/D_WINDOWS;/utf-8;/GR;/EHsc;/MP;-D_CRT_SECURE_NO_WARNINGS
      Id flags:  
      
      The output was:
      0
      CMakeCXXCompilerId.cpp
      
      
      Compilation of the CXX compiler identification source "CMakeCXXCompilerId.cpp" produced "CMakeCXXCompilerId.exe"
      
      Compilation of the CXX compiler identification source "CMakeCXXCompilerId.cpp" produced "CMakeCXXCompilerId.obj"
      
      The CXX compiler identification is MSVC, found in:
        C:/tmp/cg2-vcpkg-buildtrees-assimp543/assimp/x64-windows-dbg/CMakeFiles/3.30.1/CompilerIdCXX/CMakeCXXCompilerId.exe
      
  -
    kind: "message-v1"
    backtrace:
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerId.cmake:1243 (message)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerId.cmake:250 (CMAKE_DETERMINE_MSVC_SHOWINCLUDES_PREFIX)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCXXCompiler.cmake:126 (CMAKE_DETERMINE_COMPILER_ID)"
      - "CMakeLists.txt:74 (PROJECT)"
    message: |
      Detecting CXX compiler /showIncludes prefix:
        main.c
        メモ: インクルード ファイル:  C:\\tmp\\cg2-vcpkg-buildtrees-assimp543\\assimp\\x64-windows-dbg\\CMakeFiles\\ShowIncludes\\foo.h
        
      Found prefix "メモ: インクルード ファイル:  "
  -
    kind: "try_compile-v1"
    backtrace:
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerABI.cmake:74 (try_compile)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeTestCCompiler.cmake:26 (CMAKE_DETERMINE_COMPILER_ABI)"
      - "CMakeLists.txt:74 (PROJECT)"
    checks:
      - "Detecting C compiler ABI info"
    directories:
      source: "C:/tmp/cg2-vcpkg-buildtrees-assimp543/assimp/x64-windows-dbg/CMakeFiles/CMakeScratch/TryCompile-awu53q"
      binary: "C:/tmp/cg2-vcpkg-buildtrees-assimp543/assimp/x64-windows-dbg/CMakeFiles/CMakeScratch/TryCompile-awu53q"
    cmakeVariables:
      CMAKE_C_FLAGS: " /nologo /DWIN32 /D_WINDOWS /utf-8 /MP  -D_CRT_SECURE_NO_WARNINGS"
      CMAKE_C_FLAGS_DEBUG: "/MDd /Z7 /Ob0 /Od /RTC1 "
      CMAKE_EXE_LINKER_FLAGS: "/machine:x64"
      CMAKE_MSVC_DEBUG_INFORMATION_FORMAT: ""
      CMAKE_MSVC_RUNTIME_LIBRARY: "MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<STREQUAL:dynamic,dynamic>:DLL>"
      VCPKG_CHAINLOAD_TOOLCHAIN_FILE: "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/toolchains/windows.cmake"
      VCPKG_CRT_LINKAGE: "dynamic"
      VCPKG_CXX_FLAGS: " -D_CRT_SECURE_NO_WARNINGS"
      VCPKG_CXX_FLAGS_DEBUG: ""
      VCPKG_CXX_FLAGS_RELEASE: ""
      VCPKG_C_FLAGS: " -D_CRT_SECURE_NO_WARNINGS"
      VCPKG_C_FLAGS_DEBUG: ""
      VCPKG_C_FLAGS_RELEASE: ""
      VCPKG_INSTALLED_DIR: "C:/Users/k024g/OneDrive/\u30c7\u30b9\u30af\u30c8\u30c3\u30d7/\u81ea\u4f5c\u30a8\u30f3\u30b8\u30f32/project/vcpkg_installed"
      VCPKG_LINKER_FLAGS: ""
      VCPKG_LINKER_FLAGS_DEBUG: ""
      VCPKG_LINKER_FLAGS_RELEASE: ""
      VCPKG_PLATFORM_TOOLSET: "v143"
      VCPKG_PREFER_SYSTEM_LIBS: "OFF"
      VCPKG_SET_CHARSET_FLAG: "ON"
      VCPKG_TARGET_ARCHITECTURE: "x64"
      VCPKG_TARGET_TRIPLET: "x64-windows"
      Z_VCPKG_ROOT_DIR: "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg"
    buildResult:
      variable: "CMAKE_C_ABI_COMPILED"
      cached: true
      stdout: |
        Change Dir: 'C:/tmp/cg2-vcpkg-buildtrees-assimp543/assimp/x64-windows-dbg/CMakeFiles/CMakeScratch/TryCompile-awu53q'
        
        Run Build Command(s): "C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe" -v cmTC_a0b30
        [1/2] C:\\PROGRA~1\\MICROS~2\\18\\COMMUN~1\\VC\\Tools\\MSVC\\1444~1.352\\bin\\HostX64\\x64\\cl.exe  /nologo   /nologo /DWIN32 /D_WINDOWS /utf-8 /MP  -D_CRT_SECURE_NO_WARNINGS  /MDd /Z7 /Ob0 /Od /RTC1  -MDd /showIncludes /FoCMakeFiles\\cmTC_a0b30.dir\\CMakeCCompilerABI.c.obj /FdCMakeFiles\\cmTC_a0b30.dir\\ /FS -c C:\\Users\\k024g\\AppData\\Local\\vcpkg\\downloads\\tools\\cmake-3.30.1-windows\\cmake-3.30.1-windows-i386\\share\\cmake-3.30\\Modules\\CMakeCCompilerABI.c
        [2/2] C:\\WINDOWS\\system32\\cmd.exe /C "cd . && C:\\Users\\k024g\\AppData\\Local\\vcpkg\\downloads\\tools\\cmake-3.30.1-windows\\cmake-3.30.1-windows-i386\\bin\\cmake.exe -E vs_link_exe --intdir=CMakeFiles\\cmTC_a0b30.dir --rc=C:\\PROGRA~2\\WI3CF2~1\\10\\bin\\100261~1.0\\x64\\rc.exe --mt=C:\\PROGRA~2\\WI3CF2~1\\10\\bin\\100261~1.0\\x64\\mt.exe --manifests  -- C:\\PROGRA~1\\MICROS~2\\18\\COMMUN~1\\VC\\Tools\\MSVC\\1444~1.352\\bin\\HostX64\\x64\\link.exe /nologo CMakeFiles\\cmTC_a0b30.dir\\CMakeCCompilerABI.c.obj  /out:cmTC_a0b30.exe /implib:cmTC_a0b30.lib /pdb:cmTC_a0b30.pdb /version:0.0 /machine:x64  /nologo    /debug /INCREMENTAL /subsystem:console  kernel32.lib user32.lib gdi32.lib winspool.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib && cd ."
        
      exitCode: 0
  -
    kind: "message-v1"
    backtrace:
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerABI.cmake:218 (message)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeTestCCompiler.cmake:26 (CMAKE_DETERMINE_COMPILER_ABI)"
      - "CMakeLists.txt:74 (PROJECT)"
    message: |
      Parsed C implicit link information:
        link line regex: [^( *|.*[/\\])(ld[0-9]*(\\.[a-z]+)?|link\\.exe|lld-link(\\.exe)?|CMAKE_LINK_STARTFILE-NOTFOUND|([^/\\]+-)?ld|collect2)[^/\\]*( |$)]
        linker tool regex: [^[ 	]*(->|")?[ 	]*(([^"]*[/\\])?(ld[0-9]*(\\.[a-z]+)?|link\\.exe|lld-link(\\.exe)?))("|,| |$)]
        linker tool for 'C': C:/PROGRA~1/MICROS~2/18/COMMUN~1/VC/Tools/MSVC/1444~1.352/bin/HostX64/x64/link.exe
        implicit libs: []
        implicit objs: []
        implicit dirs: []
        implicit fwks: []
      
      
  -
    kind: "message-v1"
    backtrace:
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/Internal/CMakeDetermineLinkerId.cmake:40 (message)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerABI.cmake:255 (cmake_determine_linker_id)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeTestCCompiler.cmake:26 (CMAKE_DETERMINE_COMPILER_ABI)"
      - "CMakeLists.txt:74 (PROJECT)"
    message: |
      Running the C compiler's linker: "C:/PROGRA~1/MICROS~2/18/COMMUN~1/VC/Tools/MSVC/1444~1.352/bin/HostX64/x64/link.exe" "-v"
      Microsoft (R) Incremental Linker Version 14.44.35227.0
      Copyright (C) Microsoft Corporation.  All rights reserved.
  -
    kind: "try_compile-v1"
    backtrace:
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerABI.cmake:74 (try_compile)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeTestCXXCompiler.cmake:26 (CMAKE_DETERMINE_COMPILER_ABI)"
      - "CMakeLists.txt:74 (PROJECT)"
    checks:
      - "Detecting CXX compiler ABI info"
    directories:
      source: "C:/tmp/cg2-vcpkg-buildtrees-assimp543/assimp/x64-windows-dbg/CMakeFiles/CMakeScratch/TryCompile-hgp63i"
      binary: "C:/tmp/cg2-vcpkg-buildtrees-assimp543/assimp/x64-windows-dbg/CMakeFiles/CMakeScratch/TryCompile-hgp63i"
    cmakeVariables:
      CMAKE_CXX_FLAGS: " /nologo /DWIN32 /D_WINDOWS /utf-8 /GR /EHsc /MP  -D_CRT_SECURE_NO_WARNINGS"
      CMAKE_CXX_FLAGS_DEBUG: "/MDd /Z7 /Ob0 /Od /RTC1 "
      CMAKE_CXX_SCAN_FOR_MODULES: "OFF"
      CMAKE_EXE_LINKER_FLAGS: "/machine:x64"
      CMAKE_MSVC_DEBUG_INFORMATION_FORMAT: ""
      CMAKE_MSVC_RUNTIME_LIBRARY: "MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<STREQUAL:dynamic,dynamic>:DLL>"
      VCPKG_CHAINLOAD_TOOLCHAIN_FILE: "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/toolchains/windows.cmake"
      VCPKG_CRT_LINKAGE: "dynamic"
      VCPKG_CXX_FLAGS: " -D_CRT_SECURE_NO_WARNINGS"
      VCPKG_CXX_FLAGS_DEBUG: ""
      VCPKG_CXX_FLAGS_RELEASE: ""
      VCPKG_C_FLAGS: " -D_CRT_SECURE_NO_WARNINGS"
      VCPKG_C_FLAGS_DEBUG: ""
      VCPKG_C_FLAGS_RELEASE: ""
      VCPKG_INSTALLED_DIR: "C:/Users/k024g/OneDrive/\u30c7\u30b9\u30af\u30c8\u30c3\u30d7/\u81ea\u4f5c\u30a8\u30f3\u30b8\u30f32/project/vcpkg_installed"
      VCPKG_LINKER_FLAGS: ""
      VCPKG_LINKER_FLAGS_DEBUG: ""
      VCPKG_LINKER_FLAGS_RELEASE: ""
      VCPKG_PLATFORM_TOOLSET: "v143"
      VCPKG_PREFER_SYSTEM_LIBS: "OFF"
      VCPKG_SET_CHARSET_FLAG: "ON"
      VCPKG_TARGET_ARCHITECTURE: "x64"
      VCPKG_TARGET_TRIPLET: "x64-windows"
      Z_VCPKG_ROOT_DIR: "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg"
    buildResult:
      variable: "CMAKE_CXX_ABI_COMPILED"
      cached: true
      stdout: |
        Change Dir: 'C:/tmp/cg2-vcpkg-buildtrees-assimp543/assimp/x64-windows-dbg/CMakeFiles/CMakeScratch/TryCompile-hgp63i'
        
        Run Build Command(s): "C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe" -v cmTC_3e154
        [1/2] C:\\PROGRA~1\\MICROS~2\\18\\COMMUN~1\\VC\\Tools\\MSVC\\1444~1.352\\bin\\HostX64\\x64\\cl.exe  /nologo /TP   /nologo /DWIN32 /D_WINDOWS /utf-8 /GR /EHsc /MP  -D_CRT_SECURE_NO_WARNINGS  /MDd /Z7 /Ob0 /Od /RTC1  -MDd /showIncludes /FoCMakeFiles\\cmTC_3e154.dir\\CMakeCXXCompilerABI.cpp.obj /FdCMakeFiles\\cmTC_3e154.dir\\ /FS -c C:\\Users\\k024g\\AppData\\Local\\vcpkg\\downloads\\tools\\cmake-3.30.1-windows\\cmake-3.30.1-windows-i386\\share\\cmake-3.30\\Modules\\CMakeCXXCompilerABI.cpp
        [2/2] C:\\WINDOWS\\system32\\cmd.exe /C "cd . && C:\\Users\\k024g\\AppData\\Local\\vcpkg\\downloads\\tools\\cmake-3.30.1-windows\\cmake-3.30.1-windows-i386\\bin\\cmake.exe -E vs_link_exe --intdir=CMakeFiles\\cmTC_3e154.dir --rc=C:\\PROGRA~2\\WI3CF2~1\\10\\bin\\100261~1.0\\x64\\rc.exe --mt=C:\\PROGRA~2\\WI3CF2~1\\10\\bin\\100261~1.0\\x64\\mt.exe --manifests  -- C:\\PROGRA~1\\MICROS~2\\18\\COMMUN~1\\VC\\Tools\\MSVC\\1444~1.352\\bin\\HostX64\\x64\\link.exe /nologo CMakeFiles\\cmTC_3e154.dir\\CMakeCXXCompilerABI.cpp.obj  /out:cmTC_3e154.exe /implib:cmTC_3e154.lib /pdb:cmTC_3e154.pdb /version:0.0 /machine:x64  /nologo    /debug /INCREMENTAL /subsystem:console  kernel32.lib user32.lib gdi32.lib winspool.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib && cd ."
        
      exitCode: 0
  -
    kind: "message-v1"
    backtrace:
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerABI.cmake:218 (message)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeTestCXXCompiler.cmake:26 (CMAKE_DETERMINE_COMPILER_ABI)"
      - "CMakeLists.txt:74 (PROJECT)"
    message: |
      Parsed CXX implicit link information:
        link line regex: [^( *|.*[/\\])(ld[0-9]*(\\.[a-z]+)?|link\\.exe|lld-link(\\.exe)?|CMAKE_LINK_STARTFILE-NOTFOUND|([^/\\]+-)?ld|collect2)[^/\\]*( |$)]
        linker tool regex: [^[ 	]*(->|")?[ 	]*(([^"]*[/\\])?(ld[0-9]*(\\.[a-z]+)?|link\\.exe|lld-link(\\.exe)?))("|,| |$)]
        linker tool for 'CXX': C:/PROGRA~1/MICROS~2/18/COMMUN~1/VC/Tools/MSVC/1444~1.352/bin/HostX64/x64/link.exe
        implicit libs: []
        implicit objs: []
        implicit dirs: []
        implicit fwks: []
      
      
  -
    kind: "message-v1"
    backtrace:
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/Internal/CMakeDetermineLinkerId.cmake:40 (message)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerABI.cmake:255 (cmake_determine_linker_id)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeTestCXXCompiler.cmake:26 (CMAKE_DETERMINE_COMPILER_ABI)"
      - "CMakeLists.txt:74 (PROJECT)"
    message: |
      Running the CXX compiler's linker: "C:/PROGRA~1/MICROS~2/18/COMMUN~1/VC/Tools/MSVC/1444~1.352/bin/HostX64/x64/link.exe" "-v"
      Microsoft (R) Incremental Linker Version 14.44.35227.0
      Copyright (C) Microsoft Corporation.  All rights reserved.
```
</details>

<details><summary>C:\tmp\cg2-vcpkg-buildtrees-assimp543\assimp\config-x64-windows-rel-CMakeConfigureLog.yaml.log</summary>

```

---
events:
  -
    kind: "message-v1"
    backtrace:
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineSystem.cmake:205 (message)"
      - "CMakeLists.txt:74 (PROJECT)"
    message: |
      The system is: Windows - 10.0.26200 - AMD64
  -
    kind: "message-v1"
    backtrace:
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerId.cmake:17 (message)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerId.cmake:64 (__determine_compiler_id_test)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCCompiler.cmake:123 (CMAKE_DETERMINE_COMPILER_ID)"
      - "CMakeLists.txt:74 (PROJECT)"
    message: |
      Compiling the C compiler identification source file "CMakeCCompilerId.c" succeeded.
      Compiler: C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.44.35207/bin/HostX64/x64/cl.exe 
      Build flags: /nologo;/DWIN32;/D_WINDOWS;/utf-8;/MP;-D_CRT_SECURE_NO_WARNINGS
      Id flags:  
      
      The output was:
      0
      CMakeCCompilerId.c
      
      
      Compilation of the C compiler identification source "CMakeCCompilerId.c" produced "CMakeCCompilerId.exe"
      
      Compilation of the C compiler identification source "CMakeCCompilerId.c" produced "CMakeCCompilerId.obj"
      
      The C compiler identification is MSVC, found in:
        C:/tmp/cg2-vcpkg-buildtrees-assimp543/assimp/x64-windows-rel/CMakeFiles/3.30.1/CompilerIdC/CMakeCCompilerId.exe
      
  -
    kind: "message-v1"
    backtrace:
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerId.cmake:1243 (message)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerId.cmake:250 (CMAKE_DETERMINE_MSVC_SHOWINCLUDES_PREFIX)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCCompiler.cmake:123 (CMAKE_DETERMINE_COMPILER_ID)"
      - "CMakeLists.txt:74 (PROJECT)"
    message: |
      Detecting C compiler /showIncludes prefix:
        main.c
        メモ: インクルード ファイル:  C:\\tmp\\cg2-vcpkg-buildtrees-assimp543\\assimp\\x64-windows-rel\\CMakeFiles\\ShowIncludes\\foo.h
        
      Found prefix "メモ: インクルード ファイル:  "
  -
    kind: "message-v1"
    backtrace:
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerId.cmake:17 (message)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerId.cmake:64 (__determine_compiler_id_test)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCXXCompiler.cmake:126 (CMAKE_DETERMINE_COMPILER_ID)"
      - "CMakeLists.txt:74 (PROJECT)"
    message: |
      Compiling the CXX compiler identification source file "CMakeCXXCompilerId.cpp" succeeded.
      Compiler: C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.44.35207/bin/HostX64/x64/cl.exe 
      Build flags: /nologo;/DWIN32;/D_WINDOWS;/utf-8;/GR;/EHsc;/MP;-D_CRT_SECURE_NO_WARNINGS
      Id flags:  
      
      The output was:
      0
      CMakeCXXCompilerId.cpp
      
      
      Compilation of the CXX compiler identification source "CMakeCXXCompilerId.cpp" produced "CMakeCXXCompilerId.exe"
      
      Compilation of the CXX compiler identification source "CMakeCXXCompilerId.cpp" produced "CMakeCXXCompilerId.obj"
      
      The CXX compiler identification is MSVC, found in:
        C:/tmp/cg2-vcpkg-buildtrees-assimp543/assimp/x64-windows-rel/CMakeFiles/3.30.1/CompilerIdCXX/CMakeCXXCompilerId.exe
      
  -
    kind: "message-v1"
    backtrace:
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerId.cmake:1243 (message)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerId.cmake:250 (CMAKE_DETERMINE_MSVC_SHOWINCLUDES_PREFIX)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCXXCompiler.cmake:126 (CMAKE_DETERMINE_COMPILER_ID)"
      - "CMakeLists.txt:74 (PROJECT)"
    message: |
      Detecting CXX compiler /showIncludes prefix:
        main.c
        メモ: インクルード ファイル:  C:\\tmp\\cg2-vcpkg-buildtrees-assimp543\\assimp\\x64-windows-rel\\CMakeFiles\\ShowIncludes\\foo.h
        
      Found prefix "メモ: インクルード ファイル:  "
  -
    kind: "try_compile-v1"
    backtrace:
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerABI.cmake:74 (try_compile)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeTestCCompiler.cmake:26 (CMAKE_DETERMINE_COMPILER_ABI)"
      - "CMakeLists.txt:74 (PROJECT)"
    checks:
      - "Detecting C compiler ABI info"
    directories:
      source: "C:/tmp/cg2-vcpkg-buildtrees-assimp543/assimp/x64-windows-rel/CMakeFiles/CMakeScratch/TryCompile-1atowu"
      binary: "C:/tmp/cg2-vcpkg-buildtrees-assimp543/assimp/x64-windows-rel/CMakeFiles/CMakeScratch/TryCompile-1atowu"
    cmakeVariables:
      CMAKE_C_FLAGS: " /nologo /DWIN32 /D_WINDOWS /utf-8 /MP  -D_CRT_SECURE_NO_WARNINGS"
      CMAKE_C_FLAGS_DEBUG: "/MDd /Z7 /Ob0 /Od /RTC1 "
      CMAKE_EXE_LINKER_FLAGS: "/machine:x64"
      CMAKE_MSVC_DEBUG_INFORMATION_FORMAT: ""
      CMAKE_MSVC_RUNTIME_LIBRARY: "MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<STREQUAL:dynamic,dynamic>:DLL>"
      VCPKG_CHAINLOAD_TOOLCHAIN_FILE: "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/toolchains/windows.cmake"
      VCPKG_CRT_LINKAGE: "dynamic"
      VCPKG_CXX_FLAGS: " -D_CRT_SECURE_NO_WARNINGS"
      VCPKG_CXX_FLAGS_DEBUG: ""
      VCPKG_CXX_FLAGS_RELEASE: ""
      VCPKG_C_FLAGS: " -D_CRT_SECURE_NO_WARNINGS"
      VCPKG_C_FLAGS_DEBUG: ""
      VCPKG_C_FLAGS_RELEASE: ""
      VCPKG_INSTALLED_DIR: "C:/Users/k024g/OneDrive/\u30c7\u30b9\u30af\u30c8\u30c3\u30d7/\u81ea\u4f5c\u30a8\u30f3\u30b8\u30f32/project/vcpkg_installed"
      VCPKG_LINKER_FLAGS: ""
      VCPKG_LINKER_FLAGS_DEBUG: ""
      VCPKG_LINKER_FLAGS_RELEASE: ""
      VCPKG_PLATFORM_TOOLSET: "v143"
      VCPKG_PREFER_SYSTEM_LIBS: "OFF"
      VCPKG_SET_CHARSET_FLAG: "ON"
      VCPKG_TARGET_ARCHITECTURE: "x64"
      VCPKG_TARGET_TRIPLET: "x64-windows"
      Z_VCPKG_ROOT_DIR: "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg"
    buildResult:
      variable: "CMAKE_C_ABI_COMPILED"
      cached: true
      stdout: |
        Change Dir: 'C:/tmp/cg2-vcpkg-buildtrees-assimp543/assimp/x64-windows-rel/CMakeFiles/CMakeScratch/TryCompile-1atowu'
        
        Run Build Command(s): "C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe" -v cmTC_749e7
        [1/2] C:\\PROGRA~1\\MICROS~2\\18\\COMMUN~1\\VC\\Tools\\MSVC\\1444~1.352\\bin\\HostX64\\x64\\cl.exe  /nologo   /nologo /DWIN32 /D_WINDOWS /utf-8 /MP  -D_CRT_SECURE_NO_WARNINGS  /MDd /Z7 /Ob0 /Od /RTC1  -MDd /showIncludes /FoCMakeFiles\\cmTC_749e7.dir\\CMakeCCompilerABI.c.obj /FdCMakeFiles\\cmTC_749e7.dir\\ /FS -c C:\\Users\\k024g\\AppData\\Local\\vcpkg\\downloads\\tools\\cmake-3.30.1-windows\\cmake-3.30.1-windows-i386\\share\\cmake-3.30\\Modules\\CMakeCCompilerABI.c
        [2/2] C:\\WINDOWS\\system32\\cmd.exe /C "cd . && C:\\Users\\k024g\\AppData\\Local\\vcpkg\\downloads\\tools\\cmake-3.30.1-windows\\cmake-3.30.1-windows-i386\\bin\\cmake.exe -E vs_link_exe --intdir=CMakeFiles\\cmTC_749e7.dir --rc=C:\\PROGRA~2\\WI3CF2~1\\10\\bin\\100261~1.0\\x64\\rc.exe --mt=C:\\PROGRA~2\\WI3CF2~1\\10\\bin\\100261~1.0\\x64\\mt.exe --manifests  -- C:\\PROGRA~1\\MICROS~2\\18\\COMMUN~1\\VC\\Tools\\MSVC\\1444~1.352\\bin\\HostX64\\x64\\link.exe /nologo CMakeFiles\\cmTC_749e7.dir\\CMakeCCompilerABI.c.obj  /out:cmTC_749e7.exe /implib:cmTC_749e7.lib /pdb:cmTC_749e7.pdb /version:0.0 /machine:x64  /nologo    /debug /INCREMENTAL /subsystem:console  kernel32.lib user32.lib gdi32.lib winspool.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib && cd ."
        
      exitCode: 0
  -
    kind: "message-v1"
    backtrace:
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerABI.cmake:218 (message)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeTestCCompiler.cmake:26 (CMAKE_DETERMINE_COMPILER_ABI)"
      - "CMakeLists.txt:74 (PROJECT)"
    message: |
      Parsed C implicit link information:
        link line regex: [^( *|.*[/\\])(ld[0-9]*(\\.[a-z]+)?|link\\.exe|lld-link(\\.exe)?|CMAKE_LINK_STARTFILE-NOTFOUND|([^/\\]+-)?ld|collect2)[^/\\]*( |$)]
        linker tool regex: [^[ 	]*(->|")?[ 	]*(([^"]*[/\\])?(ld[0-9]*(\\.[a-z]+)?|link\\.exe|lld-link(\\.exe)?))("|,| |$)]
        linker tool for 'C': C:/PROGRA~1/MICROS~2/18/COMMUN~1/VC/Tools/MSVC/1444~1.352/bin/HostX64/x64/link.exe
        implicit libs: []
        implicit objs: []
        implicit dirs: []
        implicit fwks: []
      
      
  -
    kind: "message-v1"
    backtrace:
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/Internal/CMakeDetermineLinkerId.cmake:40 (message)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerABI.cmake:255 (cmake_determine_linker_id)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeTestCCompiler.cmake:26 (CMAKE_DETERMINE_COMPILER_ABI)"
      - "CMakeLists.txt:74 (PROJECT)"
    message: |
      Running the C compiler's linker: "C:/PROGRA~1/MICROS~2/18/COMMUN~1/VC/Tools/MSVC/1444~1.352/bin/HostX64/x64/link.exe" "-v"
      Microsoft (R) Incremental Linker Version 14.44.35227.0
      Copyright (C) Microsoft Corporation.  All rights reserved.
  -
    kind: "try_compile-v1"
    backtrace:
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerABI.cmake:74 (try_compile)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeTestCXXCompiler.cmake:26 (CMAKE_DETERMINE_COMPILER_ABI)"
      - "CMakeLists.txt:74 (PROJECT)"
    checks:
      - "Detecting CXX compiler ABI info"
    directories:
      source: "C:/tmp/cg2-vcpkg-buildtrees-assimp543/assimp/x64-windows-rel/CMakeFiles/CMakeScratch/TryCompile-lhw6e5"
      binary: "C:/tmp/cg2-vcpkg-buildtrees-assimp543/assimp/x64-windows-rel/CMakeFiles/CMakeScratch/TryCompile-lhw6e5"
    cmakeVariables:
      CMAKE_CXX_FLAGS: " /nologo /DWIN32 /D_WINDOWS /utf-8 /GR /EHsc /MP  -D_CRT_SECURE_NO_WARNINGS"
      CMAKE_CXX_FLAGS_DEBUG: "/MDd /Z7 /Ob0 /Od /RTC1 "
      CMAKE_CXX_SCAN_FOR_MODULES: "OFF"
      CMAKE_EXE_LINKER_FLAGS: "/machine:x64"
      CMAKE_MSVC_DEBUG_INFORMATION_FORMAT: ""
      CMAKE_MSVC_RUNTIME_LIBRARY: "MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<STREQUAL:dynamic,dynamic>:DLL>"
      VCPKG_CHAINLOAD_TOOLCHAIN_FILE: "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/toolchains/windows.cmake"
      VCPKG_CRT_LINKAGE: "dynamic"
      VCPKG_CXX_FLAGS: " -D_CRT_SECURE_NO_WARNINGS"
      VCPKG_CXX_FLAGS_DEBUG: ""
      VCPKG_CXX_FLAGS_RELEASE: ""
      VCPKG_C_FLAGS: " -D_CRT_SECURE_NO_WARNINGS"
      VCPKG_C_FLAGS_DEBUG: ""
      VCPKG_C_FLAGS_RELEASE: ""
      VCPKG_INSTALLED_DIR: "C:/Users/k024g/OneDrive/\u30c7\u30b9\u30af\u30c8\u30c3\u30d7/\u81ea\u4f5c\u30a8\u30f3\u30b8\u30f32/project/vcpkg_installed"
      VCPKG_LINKER_FLAGS: ""
      VCPKG_LINKER_FLAGS_DEBUG: ""
      VCPKG_LINKER_FLAGS_RELEASE: ""
      VCPKG_PLATFORM_TOOLSET: "v143"
      VCPKG_PREFER_SYSTEM_LIBS: "OFF"
      VCPKG_SET_CHARSET_FLAG: "ON"
      VCPKG_TARGET_ARCHITECTURE: "x64"
      VCPKG_TARGET_TRIPLET: "x64-windows"
      Z_VCPKG_ROOT_DIR: "C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg"
    buildResult:
      variable: "CMAKE_CXX_ABI_COMPILED"
      cached: true
      stdout: |
        Change Dir: 'C:/tmp/cg2-vcpkg-buildtrees-assimp543/assimp/x64-windows-rel/CMakeFiles/CMakeScratch/TryCompile-lhw6e5'
        
        Run Build Command(s): "C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe" -v cmTC_fbdc2
        [1/2] C:\\PROGRA~1\\MICROS~2\\18\\COMMUN~1\\VC\\Tools\\MSVC\\1444~1.352\\bin\\HostX64\\x64\\cl.exe  /nologo /TP   /nologo /DWIN32 /D_WINDOWS /utf-8 /GR /EHsc /MP  -D_CRT_SECURE_NO_WARNINGS  /MDd /Z7 /Ob0 /Od /RTC1  -MDd /showIncludes /FoCMakeFiles\\cmTC_fbdc2.dir\\CMakeCXXCompilerABI.cpp.obj /FdCMakeFiles\\cmTC_fbdc2.dir\\ /FS -c C:\\Users\\k024g\\AppData\\Local\\vcpkg\\downloads\\tools\\cmake-3.30.1-windows\\cmake-3.30.1-windows-i386\\share\\cmake-3.30\\Modules\\CMakeCXXCompilerABI.cpp
        [2/2] C:\\WINDOWS\\system32\\cmd.exe /C "cd . && C:\\Users\\k024g\\AppData\\Local\\vcpkg\\downloads\\tools\\cmake-3.30.1-windows\\cmake-3.30.1-windows-i386\\bin\\cmake.exe -E vs_link_exe --intdir=CMakeFiles\\cmTC_fbdc2.dir --rc=C:\\PROGRA~2\\WI3CF2~1\\10\\bin\\100261~1.0\\x64\\rc.exe --mt=C:\\PROGRA~2\\WI3CF2~1\\10\\bin\\100261~1.0\\x64\\mt.exe --manifests  -- C:\\PROGRA~1\\MICROS~2\\18\\COMMUN~1\\VC\\Tools\\MSVC\\1444~1.352\\bin\\HostX64\\x64\\link.exe /nologo CMakeFiles\\cmTC_fbdc2.dir\\CMakeCXXCompilerABI.cpp.obj  /out:cmTC_fbdc2.exe /implib:cmTC_fbdc2.lib /pdb:cmTC_fbdc2.pdb /version:0.0 /machine:x64  /nologo    /debug /INCREMENTAL /subsystem:console  kernel32.lib user32.lib gdi32.lib winspool.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib && cd ."
        
      exitCode: 0
  -
    kind: "message-v1"
    backtrace:
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerABI.cmake:218 (message)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeTestCXXCompiler.cmake:26 (CMAKE_DETERMINE_COMPILER_ABI)"
      - "CMakeLists.txt:74 (PROJECT)"
    message: |
      Parsed CXX implicit link information:
        link line regex: [^( *|.*[/\\])(ld[0-9]*(\\.[a-z]+)?|link\\.exe|lld-link(\\.exe)?|CMAKE_LINK_STARTFILE-NOTFOUND|([^/\\]+-)?ld|collect2)[^/\\]*( |$)]
        linker tool regex: [^[ 	]*(->|")?[ 	]*(([^"]*[/\\])?(ld[0-9]*(\\.[a-z]+)?|link\\.exe|lld-link(\\.exe)?))("|,| |$)]
        linker tool for 'CXX': C:/PROGRA~1/MICROS~2/18/COMMUN~1/VC/Tools/MSVC/1444~1.352/bin/HostX64/x64/link.exe
        implicit libs: []
        implicit objs: []
        implicit dirs: []
        implicit fwks: []
      
      
  -
    kind: "message-v1"
    backtrace:
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/Internal/CMakeDetermineLinkerId.cmake:40 (message)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeDetermineCompilerABI.cmake:255 (cmake_determine_linker_id)"
      - "C:/Users/k024g/AppData/Local/vcpkg/downloads/tools/cmake-3.30.1-windows/cmake-3.30.1-windows-i386/share/cmake-3.30/Modules/CMakeTestCXXCompiler.cmake:26 (CMAKE_DETERMINE_COMPILER_ABI)"
      - "CMakeLists.txt:74 (PROJECT)"
    message: |
      Running the CXX compiler's linker: "C:/PROGRA~1/MICROS~2/18/COMMUN~1/VC/Tools/MSVC/1444~1.352/bin/HostX64/x64/link.exe" "-v"
      Microsoft (R) Incremental Linker Version 14.44.35227.0
      Copyright (C) Microsoft Corporation.  All rights reserved.
```
</details>

**Additional context**

<details><summary>vcpkg.json</summary>

```
{
  "name": "cg2-engine",
  "version-string": "1.0.0",
  "dependencies": [
    "assimp"
  ],
  "overrides": [
    {
      "name": "assimp",
      "version": "5.4.3"
    }
  ],
  "builtin-baseline": "a0b1c8d3a477c1cb4813d8e127a56961707ca42b"
}

```
</details>
