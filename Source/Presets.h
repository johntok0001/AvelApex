#pragma once

#include <utility>
#include <vector>

// Factory presets. Values are plain (denormalised) parameter values; parameters not listed get their default.
// Style index: 0 Transparent, 1 Punchy, 2 Dynamic, 3 Aggressive, 4 Allround, 5 Bus, 6 Safe, 7 Modern.
// Oversampling index: 0 Off, 1 2x, 2 4x, 3 8x, 4 16x. Dither index: 0 Off, 1 24, 2 20, 3 18, 4 16 bit.
struct FactoryPreset
{
    const char* name;
    const char* category;
    std::vector<std::pair<const char*, float>> values;
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> p {
        { "Default", "", {} },
        { "Transparent Master", "Mastering", { { "style", 0 }, { "gain", 4.0f }, { "ceiling", -1.0f }, { "lookahead", 3.0f }, { "attack", 25.0f }, { "release", 150.0f }, { "oversampling", 2 }, { "truePeak", 1 } } },
        { "Streaming Master", "Mastering", { { "style", 7 }, { "gain", 6.0f }, { "ceiling", -1.0f }, { "lookahead", 2.0f }, { "attack", 15.0f }, { "release", 100.0f }, { "oversampling", 2 }, { "truePeak", 1 } } },
        { "Loud & Modern", "Mastering", { { "style", 7 }, { "gain", 9.0f }, { "ceiling", -0.3f }, { "lookahead", 1.5f }, { "attack", 8.0f }, { "release", 60.0f }, { "oversampling", 3 }, { "truePeak", 1 } } },
        { "Aggressive EDM", "Mastering", { { "style", 3 }, { "gain", 10.0f }, { "ceiling", -0.3f }, { "lookahead", 1.0f }, { "attack", 5.0f }, { "release", 40.0f }, { "oversampling", 3 }, { "truePeak", 1 } } },
        { "Gentle Acoustic", "Mastering", { { "style", 2 }, { "gain", 3.0f }, { "ceiling", -1.0f }, { "lookahead", 4.0f }, { "attack", 40.0f }, { "release", 250.0f }, { "oversampling", 2 }, { "truePeak", 1 } } },
        { "Broadcast -1 dBTP", "Delivery", { { "style", 6 }, { "gain", 2.0f }, { "ceiling", -1.0f }, { "lookahead", 5.0f }, { "attack", 30.0f }, { "release", 300.0f }, { "oversampling", 2 }, { "truePeak", 1 }, { "dcFilter", 1 } } },
        { "CD 16-bit Export", "Delivery", { { "style", 4 }, { "gain", 4.0f }, { "ceiling", -0.3f }, { "lookahead", 2.0f }, { "release", 120.0f }, { "oversampling", 2 }, { "truePeak", 1 }, { "dither", 4 }, { "noiseShaping", 1 } } },
        { "Drum Bus Punch", "Mix", { { "style", 1 }, { "gain", 4.0f }, { "ceiling", -0.5f }, { "lookahead", 0.5f }, { "attack", 5.0f }, { "release", 50.0f }, { "linkTransients", 50.0f }, { "oversampling", 1 } } },
        { "Mix Bus Glue", "Mix", { { "style", 5 }, { "gain", 3.0f }, { "ceiling", -1.0f }, { "lookahead", 3.0f }, { "attack", 50.0f }, { "release", 300.0f }, { "oversampling", 1 } } },
        { "Vocal Peak Catcher", "Mix", { { "style", 0 }, { "gain", 2.0f }, { "ceiling", -3.0f }, { "lookahead", 2.0f }, { "attack", 10.0f }, { "release", 80.0f }, { "linkTransients", 100.0f } } },
        { "Safety Ceiling", "Utility", { { "style", 6 }, { "gain", 0.0f }, { "ceiling", -0.1f }, { "lookahead", 1.0f }, { "release", 100.0f }, { "truePeak", 1 } } },
    };
    return p;
}
