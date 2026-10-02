#pragma once
#include <math.h>
#include "Calculation.h"

#pragma region イージングの種類
namespace cg2 {

/// @brief 補間係数にEaseInSineのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseInSine(float t);
/// @brief 補間係数にEaseInQuadのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseInQuad(float t);
/// @brief 補間係数にEaseInCubicのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseInCubic(float t);
/// @brief 補間係数にEaseInQuartのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseInQuart(float t);
/// @brief 補間係数にEaseInQuintのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseInQuint(float t);
/// @brief 補間係数にEaseInExpoのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseInExpo(float t);
/// @brief 補間係数にEaseInCircのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseInCirc(float t);
/// @brief 補間係数にEaseInBackのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseInBack(float t);
/// @brief 補間係数にEaseOutSineのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseOutSine(float t);
/// @brief 補間係数にEaseOutQuadのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseOutQuad(float t);
/// @brief 補間係数にEaseOutCubicのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseOutCubic(float t);
/// @brief 補間係数にEaseOutQuartのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseOutQuart(float t);
/// @brief 補間係数にEaseOutQuintのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseOutQuint(float t);
/// @brief 補間係数にEaseOutExpoのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseOutExpo(float t);
/// @brief 補間係数にEaseOutCircのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseOutCirc(float t);
/// @brief 補間係数にEaseOutBackのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseOutBack(float t);
/// @brief 補間係数にEaseInOutSineのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseInOutSine(float t);
/// @brief 補間係数にEaseInOutQuadのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseInOutQuad(float t);
/// @brief 補間係数にEaseInOutCubicのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseInOutCubic(float t);
/// @brief 補間係数にEaseInOutQuartのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseInOutQuart(float t);
/// @brief 補間係数にEaseInOutQuintのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseInOutQuint(float t);
/// @brief 補間係数にEaseInOutExpoのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseInOutExpo(float t);
/// @brief 補間係数にEaseInOutCircのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseInOutCirc(float t);
/// @brief 補間係数にEaseInOutBackのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseInOutBack(float t);
/// @brief 補間係数にEaseInElasticのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseInElastic(float t);
/// @brief 補間係数にEaseOutElasticのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseOutElastic(float t);
/// @brief 補間係数にEaseInOutElasticのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseInOutElastic(float t);
/// @brief 補間係数にEaseInBounceのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseInBounce(float t);
/// @brief 補間係数にEaseOutBounceのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseOutBounce(float t);
/// @brief 補間係数にEaseInOutBounceのイージング曲線を適用する。動きの加減速を調整するために使う。
float EaseInOutBounce(float t);

/// @brief 始点と終点を係数tで線形補間した値を返す。tの範囲外の扱いは呼び出し先の実装に従う。
float Lerp(float start, float end, float t);
/// @brief 始点と終点を係数tで線形補間した値を返す。tの範囲外の扱いは呼び出し先の実装に従う。
Vector3 Lerp(const Vector3& start, const Vector3& end, float t);
/// @brief 始点と終点を係数tで線形補間した値を返す。tの範囲外の扱いは呼び出し先の実装に従う。
Vector4 Lerp(const Vector4& start, const Vector4& end, float t);
#pragma endregion

} // namespace cg2
