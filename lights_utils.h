#pragma once

#include "input_utils.h"
#include "smx/smx_wrapper.h"
#include "spiceapi/wrappers.h"
#include <vector>
#include <map>

#define POLL_TAPE_LED true
#define POLL_LIGHTS true
#define OUTPUT_LIGHTS true

using namespace spiceapi;
using namespace std;

// Hardcode the color of the pads, for the center panel and the corner panels (besides
// the portion emulating the corner lights)
const uint8_t kPadRed = 0xBB;
const uint8_t kPadGreen = 0xBB;
const uint8_t kPadBlue = 0x00;

// LED counts for various SMX devices
const size_t kSmxArrowLedCount = 25;
const size_t kSmxMarqueeLogicalLedCount = 24;
const size_t kSmxMarqueePhysicalLedCount = 12;
const size_t kSmxVerticalStripLedCount = 28;
const size_t kSmxSpotlightLedCount = 8;

// LED counts for various DDR devices
const size_t kDdrArrowLedCount = 25;
const size_t kDdrTopPanelLedCount = 40;
const size_t kDdrVerticalStripLedCount = 25;

/*
    Utility class for handling lights output. It basically just exposes a single function to
    "do all the things", which calls a number of private functions for handling various lights
    devices and lights types.
*/
class LightsUtils {
public:
    void PerformLightsTasks(Connection& con);
private:
    void HandleStageLightsUpdate();
    void HandleMarqueeLightsUpdate();
    void HandleVerticalStripLightsUpdate();
    void HandleSpotlightLightsUpdate();
    void HandleArrowPanelLight(string& light_data, size_t pad, size_t panel_index);
    void HandleCornerPanelLight(string& light_data, size_t pad, size_t panel_index);
    void FillStagePanelColor(string& lights_data, uint8_t red, uint8_t green, uint8_t blue);
    static inline void AddColor(string& lights_data, uint8_t red, uint8_t green, uint8_t blue);
    static inline int MapValue(int x, int in_min, int in_max, int out_min, int out_max);
    static inline uint8_t Average(uint8_t a, uint8_t b);

    // The storage for the incoming lights states from Spice API when we call lights::read
    map<string, float> light_states_;
    // The storage for the incoming tape LED states from Spice API when we call ddr:tapeled_get
    map<string, vector<uint8_t>> tape_led_states_;

    /*
        These are just static sets of flags for whether a particular LED is on or off in the outer 4x4
        grid of LEDs in an SMX panel, whenever a "pad corner light" is on. This is just an L-shaped
        on-off grid, basically. We'll interpolate this into the actual LED values to send, based on how
        bright the lights are during runtime. We won't need to touch the inner 3x3 grid of LEDs, since
        we're simulating an L-shape.
    */
    // Pad upper-left
    static constexpr uint8_t kPadUpperLeftLeds[4][4] = {
        { 1, 1, 1, 1 },
        { 1, 0, 0, 0 },
        { 1, 0, 0, 0 },
        { 1, 0, 0, 0 }
    };

    // Pad lower-left
    static constexpr uint8_t kPadLowerLeftLeds[4][4] = {
        { 1, 0, 0, 0 },
        { 1, 0, 0, 0 },
        { 1, 0, 0, 0 },
        { 1, 1, 1, 1 }
    };

    // Pad upper-right
    static constexpr uint8_t kPadUpperRightLeds[4][4] = {
        { 1, 1, 1, 1 },
        { 0, 0, 0, 1 },
        { 0, 0, 0, 1 },
        { 0, 0, 0, 1 }
    };

    // Pad lower-right
    static constexpr uint8_t kPadLowerRightLeds[4][4] = {
        { 0, 0, 0, 1 },
        { 0, 0, 0, 1 },
        { 0, 0, 0, 1 },
        { 1, 1, 1, 1 }
    };
};
