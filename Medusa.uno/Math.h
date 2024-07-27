#pragma once

#include "SDK/matrix3x4.h"
#include <DirectXMath.h>
#include <iostream>
#include <random>

#define RAD2DEG(x) DirectX::XMConvertToDegrees(x)
#define DEG2RAD(x) DirectX::XMConvertToRadians(x)

namespace Math {

	void normalize_angles(Vector& angles);
	extern float NormilizeYaw(float value);
	int RandomInt(int min, int max);
	void angle_vectors(const Vector& angles, Vector& forward);
	void angle_vectors(const Vector& angles, Vector* forward, Vector* right, Vector* up);
	Vector calculate_angle(const Vector& src, const Vector& dst);




}