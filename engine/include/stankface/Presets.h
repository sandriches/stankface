#pragma once

#include "stankface/Params.h"

namespace stankface {

/** Factory presets.

    Each preset lists only the parameters it changes; everything else takes the
    descriptor default. That keeps a preset readable as the handful of moves
    that make the sound, and means a parameter added to the engine later lands
    in every preset at its neutral value instead of at zero.

    Lives in the engine rather than the plugin so the tests and the offline
    renderer can play exactly what the plugin ships.
*/
inline constexpr int kNumPresets = 7;

/** Display name. Index 0 is "Init", every parameter at its default. */
const char* presetName(int index);

/** The value the preset sets for one parameter, in natural units. Out-of-range
    indices fall back to Init. */
float presetValue(int index, ParamId id);

} // namespace stankface
