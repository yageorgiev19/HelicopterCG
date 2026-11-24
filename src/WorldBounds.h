// WorldBounds.h - размери на полето за летене (общи за физиката и за стените).
#pragma once

namespace WorldBounds {
    constexpr float kHalfSize    = 260.0f;   // полето е от -260 до +260 m по X и по Z
    constexpr float kMaxAltitude = 60.0f;    // максимална височина [m]
}
