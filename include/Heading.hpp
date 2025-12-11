#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>

// Standardized heading convention for the project:
// - Angles are in degrees
// - 0° points along +X (East)
// - 90° points along +Y (North)
// - Positive angles rotate counter-clockwise (CCW)
//
// This matches the common math convention and makes atan2(y, x) the natural inverse.

namespace Heading {

inline float wrapDegrees360(float deg) {
    float r = std::fmod(deg, 360.0f);
    if (r < 0.0f) r += 360.0f;
    return r;
}

inline float shortestAngleDeltaDeg(float fromDeg, float toDeg) {
    float a = wrapDegrees360(toDeg) - wrapDegrees360(fromDeg);
    while (a > 180.0f) a -= 360.0f;
    while (a < -180.0f) a += 360.0f;
    return a;
}

inline glm::vec2 forwardFromHeadingDeg(float headingDeg) {
    float r = glm::radians(headingDeg);
    return glm::vec2(std::cos(r), std::sin(r));
}

inline float headingDegFromForward(glm::vec2 forward) {
    float len = std::sqrt(forward.x * forward.x + forward.y * forward.y);
    if (len > 1e-6f) {
        forward /= len;
    }
    float deg = glm::degrees(std::atan2(forward.y, forward.x));
    return wrapDegrees360(deg);
}

// Legacy engine "rotation.z" convention currently used in movement code:
// - forward = (sin(rad), cos(rad))
// - 0° points +Y, 90° points +X
//
// Relationship to new heading:
//   heading = 90° - legacyRotation
//   legacyRotation = 90° - heading
inline float headingDegFromLegacyRotationDeg(float legacyRotationDeg) {
    return wrapDegrees360(90.0f - legacyRotationDeg);
}

inline float legacyRotationDegFromHeadingDeg(float headingDeg) {
    return wrapDegrees360(90.0f - headingDeg);
}

} // namespace Heading
