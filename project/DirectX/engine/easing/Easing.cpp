#include "Easing.h"

#pragma region イージングの種類

namespace cg2 {
namespace { constexpr double kEasingPi = 3.141592653589793; }

float EaseInSine(float t) { return 1.0f - cosf(float(t * kEasingPi) / 2.0f); }

float EaseInQuad(float t) { return t * t; }

float EaseInCubic(float t) { return t * t * t; }

float EaseInQuart(float t) { return t * t * t * t; }

float EaseInQuint(float t) { return t * t * t * t * t; }

float EaseInExpo(float t) {
	if (t == 0.0f) {
		return 0.0f;
	} else {
		return powf(2.0f, 10.0f * t - 10.0f);
	}
}

float EaseInCirc(float t) { return 1.0f - sqrtf(1.0f - powf(t, 2.0f)); }

float EaseInBack(float t) {
	const float c1 = 1.70158f;
	const float c3 = c1 + 1.0f;

	return c3 * t * t * t - c1 * t * t;
}

float EaseOutSine(float t) { return sinf(float(t * kEasingPi) / 2.0f); }

float EaseOutQuad(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }

float EaseOutCubic(float t) { return 1.0f - powf(1.0f - t, 3.0f); }

float EaseOutQuart(float t) { return 1.0f - powf(1.0f - t, 4.0f); }

float EaseOutQuint(float t) { return 1.0f - powf(1.0f - t, 5.0f); }

float EaseOutExpo(float t) {
	if (t == 1.0f) {
		return 1.0f;
	} else {
		return 1.0f - powf(2.0f, -10.0f * t);
	}
}

float EaseOutCirc(float t) { return sqrtf(1.0f - powf(t - 1.0f, 2.0f)); }

float EaseOutBack(float t) {
	const float c1 = 1.70158f;
	const float c3 = c1 + 1.0f;

	return 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f);
}

float EaseInOutSine(float t) { return -(cosf(float(kEasingPi) * t) - 1.0f) / 2.0f; }

float EaseInOutQuad(float t) {
	if (t < 0.5f) {
		return 2.0f * t * t;
	} else {
		return 1.0f - ((-2.0f * t + 2.0f) * (-2.0f * t + 2.0f)) / 2.0f;
	}
}

float EaseInOutCubic(float t) {
	if (t < 0.5f) {
		return 4.0f * t * t * t;
	} else {
		return 1.0f - ((-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f)) / 2.0f;
	}
}

float EaseInOutQuart(float t) {
	if (t < 0.5f) {
		return 8.0f * t * t * t * t;
	} else {
		return 1.0f - ((-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f)) / 2.0f;
	}
}

float EaseInOutQuint(float t) {
	if (t < 0.5f) {
		return 16.0f * t * t * t * t * t;
	} else {
		return 1.0f - ((-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f)) / 2.0f;
	}
}

float EaseInOutExpo(float t) {
	if (t == 0.0f) {
		return 0.0f;
	} else if (t == 1.0f) {
		return 1.0f;
	} else if (t < 0.5f) {
		return (powf(2.0f, 20.0f * t - 10.0f)) / 2.0f;
	} else {
		return (2.0f - powf(2.0f, -20.0f * t + 10.0f)) / 2.0f;
	}
}

float EaseInOutCirc(float t) {
	if (t < 0.5f) {
		return (1.0f - sqrtf(1.0f - powf(2.0f * t, 2.0f))) / 2.0f;
	} else {
		return (sqrtf(1.0f - powf(-2.0f * t + 2.0f, 2.0f)) + 1.0f) / 2.0f;
	}
}

float EaseInOutBack(float t) {
	const float c1 = 1.70158f;
	const float c2 = c1 * 1.525f;

	if (t < 0.5f) {
		return (powf(2.0f * t, 2.0f) * ((c2 + 1.0f) * 2.0f * t - c2)) / 2.0f;
	} else {
		return (powf(2.0f * t - 2.0f, 2.0f) * ((c2 + 1.0f) * (t * 2.0f - 2.0f) + c2) + 2.0f) / 2.0f;
	}
}

float EaseInElastic(float t) {
	const float c4 = (2.0f * float(kEasingPi)) / 3.0f;

	if (t == 0.0f) {
		return 0.0f;
	} else if (t == 1.0f) {
		return 1.0f;
	} else {
		return -powf(2.0f, 10.0f * t - 10.0f) * sinf((t * 10.0f - 10.75f) * c4);
	}
}

float EaseOutElastic(float t) {
	const float c4 = (2.0f * float(kEasingPi)) / 3.0f;

	if (t == 0.0f) {
		return 0.0f;
	} else if (t == 1.0f) {
		return 1.0f;
	} else {
		return powf(2.0f, -10.0f * t) * sinf((t * 10.0f - 0.75f) * c4) + 1.0f;
	}
}

float EaseInOutElastic(float t) {
	const float c5 = (2.0f * float(kEasingPi)) / 4.5f;

	if (t == 0.0f) {
		return 0.0f;
	} else if (t == 1.0f) {
		return 1.0f;
	} else if (t < 0.5f) {
		return -(powf(2.0f, 20.0f * t - 10.0f) * sinf((20.0f * t - 11.125f) * c5)) / 2.0f;
	} else {
		return (powf(2.0f, -20.0f * t + 10.0f) * sinf((20.0f * t - 11.125f) * c5)) / 2.0f + 1.0f;
	}
}

float EaseInBounce(float t) { return 1.0f - EaseOutBounce(1.0f - t); }

float EaseOutBounce(float t) {
	if (t < (1.0f / 2.75f)) {
		return 7.5625f * t * t;
	} else if (t < (2.0f / 2.75f)) {
		t -= (1.5f / 2.75f);
		return 7.5625f * t * t + 0.75f;
	} else if (t < (2.5f / 2.75f)) {
		t -= (2.25f / 2.75f);
		return 7.5625f * t * t + 0.9375f;
	} else {
		t -= (2.625f / 2.75f);
		return 7.5625f * t * t + 0.984375f;
	}
}

float EaseInOutBounce(float t) {
	if (t < 0.5f) {
		return (1.0f - EaseOutBounce(1.0f - 2.0f * t)) / 2.0f;
	} else {
		return (1.0f + EaseOutBounce(2.0f * t - 1.0f)) / 2.0f;
	}
}

float Lerp(float start, float end, float t) {

	// 線形補間
	return start + (end - start) * t;
}

Vector3 Lerp(const Vector3& start, const Vector3& end, float t) {

	// 線形補間
	Vector3 result = start + (end - start) * t;
	// 補間結果を返す
	return result;
}

Vector4 Lerp(const Vector4& start, const Vector4& end, float t)
{
	// 線形補間
	Vector4 result = start + (end - start) * t;
	// 補間結果を返す
	return result;
}

#pragma endregion

} // namespace cg2
