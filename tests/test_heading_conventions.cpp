#include "Heading.hpp"

#include <cassert>
#include <cmath>

static bool approx(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}

int main() {
    // Forward vectors for cardinal headings.
    {
        auto f0 = Heading::forwardFromHeadingDeg(0.0f);
        assert(approx(f0.x, 1.0f) && approx(f0.y, 0.0f)); // East

        auto f90 = Heading::forwardFromHeadingDeg(90.0f);
        assert(approx(f90.x, 0.0f) && approx(f90.y, 1.0f)); // North

        auto f180 = Heading::forwardFromHeadingDeg(180.0f);
        assert(approx(f180.x, -1.0f) && approx(f180.y, 0.0f)); // West

        auto f270 = Heading::forwardFromHeadingDeg(270.0f);
        assert(approx(f270.x, 0.0f) && approx(f270.y, -1.0f)); // South
    }

    // Inverse mapping.
    {
        assert(approx(Heading::headingDegFromForward({1.0f, 0.0f}), 0.0f));
        assert(approx(Heading::headingDegFromForward({0.0f, 1.0f}), 90.0f));
        assert(approx(Heading::headingDegFromForward({-1.0f, 0.0f}), 180.0f));
        assert(approx(Heading::headingDegFromForward({0.0f, -1.0f}), 270.0f));
    }

    // Legacy conversion sanity:
    // legacy 0° (North) -> heading 90° (North)
    assert(approx(Heading::headingDegFromLegacyRotationDeg(0.0f), 90.0f));
    // legacy 90° (East) -> heading 0° (East)
    assert(approx(Heading::headingDegFromLegacyRotationDeg(90.0f), 0.0f));
    // round trip
    assert(approx(Heading::legacyRotationDegFromHeadingDeg(Heading::headingDegFromLegacyRotationDeg(123.0f)), Heading::wrapDegrees360(123.0f)));

    // shortestAngleDeltaDeg should pick the short way.
    assert(approx(Heading::shortestAngleDeltaDeg(350.0f, 10.0f), 20.0f));
    assert(approx(Heading::shortestAngleDeltaDeg(10.0f, 350.0f), -20.0f));

    return 0;
}
