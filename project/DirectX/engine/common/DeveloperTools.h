#pragma once

// MSBuild: /p:MyEngineDeveloperTools=true|false. Release defaults to false.
// Keep a safe default for standalone tools that do not use the main project.
#ifndef MYENGINE_DEVELOPER_TOOLS
#ifdef NDEBUG
#define MYENGINE_DEVELOPER_TOOLS 0
#else
#define MYENGINE_DEVELOPER_TOOLS 1
#endif
#endif

namespace myEngine {
inline constexpr bool kDeveloperTools = MYENGINE_DEVELOPER_TOOLS != 0;
}
