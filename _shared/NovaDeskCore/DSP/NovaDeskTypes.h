#pragma once

namespace nova::desk
{
    // Two EQ/coloration personalities (build spec sections 5-8, 15, 63): E
    // and G must produce a REAL, measurable difference, not merely
    // cosmetic presets. Shared between NovaDeskEQBands (proportional-Q
    // shaping) and NovaDeskDrive (asymmetry/response-speed shaping) so
    // both stages respond to the same personality switch consistently.
    enum class EQPersonality { E, G };
}
