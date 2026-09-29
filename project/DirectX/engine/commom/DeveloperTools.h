#pragma once

// MSBuild: /p:CG2DeveloperTools=true|false. Release defaults to false.
// Keep a safe default for standalone tools that do not use the main project.
#ifndef CG2_DEVELOPER_TOOLS
#ifdef NDEBUG
#define CG2_DEVELOPER_TOOLS 0
#else
#define CG2_DEVELOPER_TOOLS 1
#endif
#endif

namespace cg2 {
inline constexpr bool kDeveloperTools = CG2_DEVELOPER_TOOLS != 0;
}
