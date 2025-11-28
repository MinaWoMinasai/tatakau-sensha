#pragma once
#define M_PI 3.141592653589793
#include <math.h>
#include "Calculation.h"

#pragma region イージングの種類
float easeInSine(float t);
float easeInQuad(float t);
float easeInCubic(float t);
float easeInQuart(float t);
float easeInQuint(float t);
float easeInExpo(float t);
float easeInCirc(float t);
float easeInBack(float t);
float easeOutSine(float t);
float easeOutQuad(float t);
float easeOutCubic(float t);
float easeOutQuart(float t);
float easeOutQuint(float t);
float easeOutExpo(float t);
float easeOutCirc(float t);
float easeOutBack(float t);
float easeInOutSine(float t);
float easeInOutQuad(float t);
float easeInOutCubic(float t);
float easeInOutQuart(float t);
float easeInOutQuint(float t);
float easeInOutExpo(float t);
float easeInOutCirc(float t);
float easeInOutBack(float t);
float easeInElastic(float t);
float easeOutElastic(float t);
float easeInOutElastic(float t);
float easeInBounce(float t);
float easeOutBounce(float t);
float easeInOutBounce(float t);

float Lerp(const float& start, const float& end, const float t);
Vector3 Lerp(const Vector3& start, const Vector3& end, float t);
Vector4 Lerp(const Vector4& start, const Vector4& end, float t);
#pragma endregion