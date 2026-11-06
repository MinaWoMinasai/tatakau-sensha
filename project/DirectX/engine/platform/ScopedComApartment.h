#pragma once
#include <objbase.h>

namespace myEngine {

/// @brief 今のスレッドの COM をマルチスレッドアパートメントとして初期化し、破棄のときに片付ける。
/// WIC・Media Foundation・XAudio2 を使うスレッドの先頭で作る。
/// 別の方式で初期化済みのスレッドでは失敗するが、そのまま続ける（COM を使う側がそれぞれ失敗を返す）。
class ScopedComApartment {
public:
    ScopedComApartment() : result_(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}

    ~ScopedComApartment()
    {
        // CoInitializeEx が成功した（S_FALSE を含む）ときだけ、対になる CoUninitialize を呼ぶ。
        if (SUCCEEDED(result_)) {
            CoUninitialize();
        }
    }

    ScopedComApartment(const ScopedComApartment&) = delete;
    ScopedComApartment& operator=(const ScopedComApartment&) = delete;

private:
    HRESULT result_;
};

} // namespace myEngine
