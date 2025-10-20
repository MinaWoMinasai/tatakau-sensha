#include "Easing.h"

#pragma region イージングの種類

float easeInSine(float t) { return 1.0f - cosf(float(t * M_PI) / 2.0f); }

float easeInQuad(float t) { return t * t; }

float easeInCubic(float t) { return t * t * t; }

float easeInQuart(float t) { return t * t * t * t; }

float easeInQuint(float t) { return t * t * t * t * t; }

float easeInExpo(float t) {
	if (t == 0.0f) {
		return 0.0f;
	} else {
		return powf(2.0f, 10.0f * t - 10.0f);
	}
}

float easeInCirc(float t) { return 1.0f - sqrtf(1.0f - powf(t, 2.0f)); }

float easeInBack(float t) {
	const float c1 = 1.70158f;
	const float c3 = c1 + 1.0f;

	return c3 * t * t * t - c1 * t * t;
}

float easeOutSine(float t) { return sinf(float(t * M_PI) / 2.0f); }

float easeOutQuad(float t) { return 1.0f - (1.0f - t) * (1.0f - t); }

float easeOutCubic(float t) { return 1.0f - powf(1.0f - t, 3.0f); }

float easeOutQuart(float t) { return 1.0f - powf(1.0f - t, 4.0f); }

float easeOutQuint(float t) { return 1.0f - powf(1.0f - t, 5.0f); }

float easeOutExpo(float t) {
	if (t == 1.0f) {
		return 1.0f;
	} else {
		return 1.0f - powf(2.0f, -10.0f * t);
	}
}

float easeOutCirc(float t) { return sqrtf(1.0f - powf(t - 1.0f, 2.0f)); }

float easeOutBack(float t) {
	const float c1 = 1.70158f;
	const float c3 = c1 + 1.0f;

	return 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f);
}

float easeInOutSine(float t) { return -(cosf(float(M_PI) * t) - 1.0f) / 2.0f; }

float easeInOutQuad(float t) {
	if (t < 0.5f) {
		return 2.0f * t * t;
	} else {
		return 1.0f - ((-2.0f * t + 2.0f) * (-2.0f * t + 2.0f)) / 2.0f;
	}
}

float easeInOutCubic(float t) {
	if (t < 0.5f) {
		return 4.0f * t * t * t;
	} else {
		return 1.0f - ((-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f)) / 2.0f;
	}
}

float easeInOutQuart(float t) {
	if (t < 0.5f) {
		return 8.0f * t * t * t * t;
	} else {
		return 1.0f - ((-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f)) / 2.0f;
	}
}

float easeInOutQuint(float t) {
	if (t < 0.5f) {
		return 16.0f * t * t * t * t * t;
	} else {
		return 1.0f - ((-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f)) / 2.0f;
	}
}

float easeInOutExpo(float t) {
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

float easeInOutCirc(float t) {
	if (t < 0.5f) {
		return (1.0f - sqrtf(1.0f - powf(2.0f * t, 2.0f))) / 2.0f;
	} else {
		return (sqrtf(1.0f - powf(-2.0f * t + 2.0f, 2.0f)) + 1.0f) / 2.0f;
	}
}

float easeInOutBack(float t) {
	const float c1 = 1.70158f;
	const float c2 = c1 * 1.525f;

	if (t < 0.5f) {
		return (powf(2.0f * t, 2.0f) * ((c2 + 1.0f) * 2.0f * t - c2)) / 2.0f;
	} else {
		return (powf(2.0f * t - 2.0f, 2.0f) * ((c2 + 1.0f) * (t * 2.0f - 2.0f) + c2) + 2.0f) / 2.0f;
	}
}

float easeInElastic(float t) {
	const float c4 = (2.0f * float(M_PI)) / 3.0f;

	if (t == 0.0f) {
		return 0.0f;
	} else if (t == 1.0f) {
		return 1.0f;
	} else {
		return -powf(2.0f, 10.0f * t - 10.0f) * sinf((t * 10.0f - 10.75f) * c4);
	}
}

float easeOutElastic(float t) {
	const float c4 = (2.0f * float(M_PI)) / 3.0f;

	if (t == 0.0f) {
		return 0.0f;
	} else if (t == 1.0f) {
		return 1.0f;
	} else {
		return powf(2.0f, -10.0f * t) * sinf((t * 10.0f - 0.75f) * c4) + 1.0f;
	}
}

float easeInOutElastic(float t) {
	const float c5 = (2.0f * float(M_PI)) / 4.5f;

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

float easeInBounce(float t) { return 1.0f - easeOutBounce(1.0f - t); }

float easeOutBounce(float t) {
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

float easeInOutBounce(float t) {
	if (t < 0.5f) {
		return (1.0f - easeOutBounce(1.0f - 2.0f * t)) / 2.0f;
	} else {
		return (1.0f + easeOutBounce(2.0f * t - 1.0f)) / 2.0f;
	}
}

float Lerp(const float& start, const float& end, const float t) {

	// 線形補間
	return start + (end - start) * t;
}

Vector3 Lerp(const Vector3& start, const Vector3& end, float t) {

	// 線形補間
	Vector3 result = start + (end - start) * t;
	// 補間結果を返す
	return result;
}

#pragma endregion
