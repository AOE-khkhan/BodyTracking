#pragma once

#include <Kore/Math/Quaternion.h>

namespace Kore {

	namespace RotationUtility {
		void eulerToQuat(float roll, float pitch, float yaw, Kore::Quaternion* quat);
		void quatToEuler(const Kore::Quaternion* quat, float* roll, float* pitch, float* yaw);
		float getRadians(float degree);
	}
}