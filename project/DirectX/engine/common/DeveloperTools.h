#pragma once

// MSBuild: /p:MyEngineDeveloperTools=true|false。Releaseの既定はfalse。
// メインプロジェクトを使わない単体ツール向けに、安全な既定値を残しておく。
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
