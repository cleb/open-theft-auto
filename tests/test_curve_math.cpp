// Test curve arc math in isolation to verify correctness
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <cstdio>
#include <cassert>

// Minimal reproduction of curve math from TrafficManager.cpp

enum class CarDirection {
    None, North, South, East, West,
    NorthEast, NorthWest, SouthEast, SouthWest,
    SouthNorth, WestEast
};

struct CurveArc {
    glm::vec2 center;
    float radius = 0.0f;
    float startAngleRad = 0.0f;
    float endAngleRad = 0.0f;
    float totalAngleRad = 0.0f;
    bool valid = false;
};

static float headingOf(CarDirection d) {
    switch (d) {
        case CarDirection::East:  return 0.0f;
        case CarDirection::North: return 90.0f;
        case CarDirection::West:  return 180.0f;
        case CarDirection::South: return 270.0f;
        default: break;
    }
    return 0.0f;
}

static CarDirection oppositeDir(CarDirection d) {
    switch (d) {
        case CarDirection::East:  return CarDirection::West;
        case CarDirection::West:  return CarDirection::East;
        case CarDirection::North: return CarDirection::South;
        case CarDirection::South: return CarDirection::North;
        default: break;
    }
    return d;
}

static float shortestAngleDeltaDeg(float from, float to) {
    float diff = to - from;
    while (diff > 180.0f) diff -= 360.0f;
    while (diff < -180.0f) diff += 360.0f;
    return diff;
}

static bool curveCardinals(CarDirection tileDir, CarDirection& a, CarDirection& b) {
    switch (tileDir) {
        case CarDirection::NorthEast: a = CarDirection::North; b = CarDirection::East;  return true;
        case CarDirection::SouthEast: a = CarDirection::South; b = CarDirection::East;  return true;
        case CarDirection::NorthWest: a = CarDirection::North; b = CarDirection::West;  return true;
        case CarDirection::SouthWest: a = CarDirection::South; b = CarDirection::West;  return true;
        default: break;
    }
    return false;
}

static bool curveEntryExitFromHeading(CarDirection tileDir, float currentHeadingDeg,
                                      CarDirection& entry, CarDirection& exit) {
    CarDirection a, b;
    if (!curveCardinals(tileDir, a, b)) return false;

    float da = std::abs(shortestAngleDeltaDeg(currentHeadingDeg, headingOf(oppositeDir(a))));
    float db = std::abs(shortestAngleDeltaDeg(currentHeadingDeg, headingOf(oppositeDir(b))));

    if (da <= db) {
        entry = a;
        exit = b;
    } else {
        entry = b;
        exit = a;
    }
    return true;
}

static glm::vec2 dirToForward(CarDirection d) {
    switch (d) {
        case CarDirection::North: return glm::vec2(0.0f, 1.0f);
        case CarDirection::South: return glm::vec2(0.0f, -1.0f);
        case CarDirection::East:  return glm::vec2(1.0f, 0.0f);
        case CarDirection::West:  return glm::vec2(-1.0f, 0.0f);
        default: break;
    }
    return glm::vec2(0.0f, 1.0f);
}

static float angleOf(const glm::vec2& v) {
    return std::atan2(v.y, v.x);
}

static float normalizeAngleSigned(float a) {
    while (a > glm::pi<float>()) a -= glm::two_pi<float>();
    while (a < -glm::pi<float>()) a += glm::two_pi<float>();
    return a;
}

static CurveArc buildCurveArc(const glm::vec2& tileCenter, float tileSize, CarDirection tileDir,
                              float currentHeadingDeg) {
    CurveArc arc;
    CarDirection entry, exit;
    if (!curveEntryExitFromHeading(tileDir, currentHeadingDeg, entry, exit)) return arc;

    const float r = tileSize * 0.5f;
    glm::vec2 entryF = dirToForward(entry);
    glm::vec2 exitF = dirToForward(exit);

    glm::vec2 centerOffset(0.0f);
    switch (tileDir) {
        case CarDirection::NorthEast: centerOffset = glm::vec2(+r, +r); break;
        case CarDirection::NorthWest: centerOffset = glm::vec2(-r, +r); break;
        case CarDirection::SouthEast: centerOffset = glm::vec2(+r, -r); break;
        case CarDirection::SouthWest: centerOffset = glm::vec2(-r, -r); break;
        default: break;
    }
    arc.center = tileCenter + centerOffset;
    arc.radius = r;

    glm::vec2 startPoint = tileCenter + entryF * r;
    glm::vec2 endPoint = tileCenter + exitF * r;

    arc.startAngleRad = angleOf(startPoint - arc.center);
    arc.endAngleRad = angleOf(endPoint - arc.center);
    arc.totalAngleRad = normalizeAngleSigned(arc.endAngleRad - arc.startAngleRad);

    if (std::abs(arc.totalAngleRad) > glm::half_pi<float>() + 0.001f) {
        arc.totalAngleRad = (arc.totalAngleRad > 0.0f) ? (arc.totalAngleRad - glm::two_pi<float>())
                                                        : (arc.totalAngleRad + glm::two_pi<float>());
    }

    arc.valid = true;
    return arc;
}

const char* dirName(CarDirection d) {
    switch (d) {
        case CarDirection::North: return "North";
        case CarDirection::South: return "South";
        case CarDirection::East: return "East";
        case CarDirection::West: return "West";
        case CarDirection::NorthEast: return "NorthEast";
        case CarDirection::NorthWest: return "NorthWest";
        case CarDirection::SouthEast: return "SouthEast";
        case CarDirection::SouthWest: return "SouthWest";
        default: return "?";
    }
}

void testCurve(CarDirection tileDir, float incomingHeadingDeg, CarDirection expectedEntry, CarDirection expectedExit) {
    printf("\n=== Testing %s tile, vehicle heading %.0f° ===\n", dirName(tileDir), incomingHeadingDeg);
    
    CarDirection entry, exit;
    curveEntryExitFromHeading(tileDir, incomingHeadingDeg, entry, exit);
    
    printf("  Entry side: %s (expected %s) %s\n", dirName(entry), dirName(expectedEntry), 
           entry == expectedEntry ? "✓" : "✗");
    printf("  Exit side: %s (expected %s) %s\n", dirName(exit), dirName(expectedExit),
           exit == expectedExit ? "✓" : "✗");
    
    glm::vec2 tileCenter(0.0f, 0.0f);
    float tileSize = 3.0f;
    CurveArc arc = buildCurveArc(tileCenter, tileSize, tileDir, incomingHeadingDeg);
    
    printf("  Arc center: (%.2f, %.2f)\n", arc.center.x, arc.center.y);
    printf("  Arc start angle: %.1f°\n", glm::degrees(arc.startAngleRad));
    printf("  Arc end angle: %.1f°\n", glm::degrees(arc.endAngleRad));
    printf("  Arc total: %.1f° (%s)\n", glm::degrees(arc.totalAngleRad),
           arc.totalAngleRad > 0 ? "CCW" : "CW");
    
    // Verify start point is on entry edge
    glm::vec2 startPt = arc.center + glm::vec2(std::cos(arc.startAngleRad), std::sin(arc.startAngleRad)) * arc.radius;
    glm::vec2 expectedStart = tileCenter + dirToForward(entry) * (tileSize * 0.5f);
    float startErr = glm::length(startPt - expectedStart);
    printf("  Start point: (%.2f, %.2f), expected (%.2f, %.2f), error=%.4f %s\n",
           startPt.x, startPt.y, expectedStart.x, expectedStart.y, startErr, startErr < 0.01f ? "✓" : "✗");
    
    // Verify end point is on exit edge
    glm::vec2 endPt = arc.center + glm::vec2(std::cos(arc.startAngleRad + arc.totalAngleRad), 
                                              std::sin(arc.startAngleRad + arc.totalAngleRad)) * arc.radius;
    glm::vec2 expectedEnd = tileCenter + dirToForward(exit) * (tileSize * 0.5f);
    float endErr = glm::length(endPt - expectedEnd);
    printf("  End point: (%.2f, %.2f), expected (%.2f, %.2f), error=%.4f %s\n",
           endPt.x, endPt.y, expectedEnd.x, expectedEnd.y, endErr, endErr < 0.01f ? "✓" : "✗");
    
    // Check tangent at start (should match incoming heading direction)
    glm::vec2 radialAtStart(std::cos(arc.startAngleRad), std::sin(arc.startAngleRad));
    glm::vec2 tangentAtStart = (arc.totalAngleRad >= 0.0f) 
        ? glm::vec2(-radialAtStart.y, radialAtStart.x)  // CCW: rotate 90° CCW
        : glm::vec2(radialAtStart.y, -radialAtStart.x); // CW: rotate 90° CW
    float tangentHeading = glm::degrees(std::atan2(tangentAtStart.y, tangentAtStart.x));
    if (tangentHeading < 0) tangentHeading += 360.0f;
    printf("  Tangent at start: (%.2f, %.2f) = %.1f°, incoming was %.1f°\n",
           tangentAtStart.x, tangentAtStart.y, tangentHeading, incomingHeadingDeg);
    
    // The tangent should point in a direction consistent with the expected travel
    // i.e., if entering from South (heading North), tangent should be roughly North initially
    float tangentError = std::abs(shortestAngleDeltaDeg(tangentHeading, incomingHeadingDeg));
    printf("  Tangent vs incoming heading error: %.1f° %s\n", tangentError, tangentError < 45.0f ? "✓" : "✗");
    
    if (entry != expectedEntry || exit != expectedExit || startErr > 0.01f || endErr > 0.01f) {
        printf("  >>> TEST FAILED <<<\n");
    }
}

int main() {
    printf("Curve Math Test\n");
    printf("===============\n");
    
    // SouthEast connects South and East edges
    // Entry from South (heading North=90°) should exit East
    testCurve(CarDirection::SouthEast, 90.0f, CarDirection::South, CarDirection::East);
    // Entry from East (heading West=180°) should exit South
    testCurve(CarDirection::SouthEast, 180.0f, CarDirection::East, CarDirection::South);
    
    // NorthEast connects North and East edges
    // Entry from North (heading South=270°) should exit East
    testCurve(CarDirection::NorthEast, 270.0f, CarDirection::North, CarDirection::East);
    // Entry from East (heading West=180°) should exit North
    testCurve(CarDirection::NorthEast, 180.0f, CarDirection::East, CarDirection::North);
    
    // NorthWest connects North and West edges
    // Entry from North (heading South=270°) should exit West
    testCurve(CarDirection::NorthWest, 270.0f, CarDirection::North, CarDirection::West);
    // Entry from West (heading East=0°) should exit North
    testCurve(CarDirection::NorthWest, 0.0f, CarDirection::West, CarDirection::North);
    
    // SouthWest connects South and West edges
    // Entry from South (heading North=90°) should exit West
    testCurve(CarDirection::SouthWest, 90.0f, CarDirection::South, CarDirection::West);
    // Entry from West (heading East=0°) should exit South
    testCurve(CarDirection::SouthWest, 0.0f, CarDirection::West, CarDirection::South);
    
    printf("\nDone.\n");
    return 0;
}
