#pragma once
#include <math.h>
#include "Calculation.h"

#pragma region イージングの種類
namespace cg2 {

float EaseInSine(float t);
float EaseInQuad(float t);
float EaseInCubic(float t);
float EaseInQuart(float t);
float EaseInQuint(float t);
float EaseInExpo(float t);
float EaseInCirc(float t);
float EaseInBack(float t);
float EaseOutSine(float t);
float EaseOutQuad(float t);
float EaseOutCubic(float t);
float EaseOutQuart(float t);
float EaseOutQuint(float t);
float EaseOutExpo(float t);
float EaseOutCirc(float t);
float EaseOutBack(float t);
float EaseInOutSine(float t);
float EaseInOutQuad(float t);
float EaseInOutCubic(float t);
float EaseInOutQuart(float t);
float EaseInOutQuint(float t);
float EaseInOutExpo(float t);
float EaseInOutCirc(float t);
float EaseInOutBack(float t);
float EaseInElastic(float t);
float EaseOutElastic(float t);
float EaseInOutElastic(float t);
float EaseInBounce(float t);
float EaseOutBounce(float t);
float EaseInOutBounce(float t);

float Lerp(float start, float end, float t);
Vector3 Lerp(const Vector3& start, const Vector3& end, float t);
Vector4 Lerp(const Vector4& start, const Vector4& end, float t);
#pragma endregion

} // namespace cg2
