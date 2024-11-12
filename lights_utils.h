#pragma once

#include "input_utils.h"
#include "smx/smx_wrapper.h"
#include "spiceapi/wrappers.h"
#include <vector>
#include <map>

#define POLL_TAPE_LED true
#define POLL_LIGHTS true
#define OUTPUT_LIGHTS true

// Hardcode the color of the pads, for the center panel and the corner panels (besides
// the portion emulating the corner lights)
#define PAD_R 0xBB
#define PAD_G 0xBB
#define PAD_B 0x00

// LED counts for various SMX devices
#define SMX_ARROW_LED_COUNT 25
#define SMX_MARQUEE_LOGICAL_LED_COUNT 24
#define SMX_MARQUEE_PHYSICAL_LED_COUNT 12
#define SMX_VERTICAL_STRIP_LED_COUNT 28
#define SMX_SPOTLIGHT_LED_COUNT 8

// LED counts for various DDR devices
#define DDR_ARROW_LED_COUNT 25
#define DDR_TOP_PANEL_LED_COUNT 40
#define DDR_VERTICAL_STRIP_LED_COUNT 25

using namespace spiceapi;
using namespace std;

/*
    Utility class for handling lights output. It basically just exposes a single function to
    "do all the things", which calls a number of private functions for handling various lights
    devices and lights types.
*/
class LightsUtils {
public:
    void perform_lights_tasks(Connection& con);
private:
    void handle_stage_lights_update();
    void handle_marquee_lights_update();
    void handle_vertical_strip_lights_update();
    void handle_spotlight_lights_update();
    void handle_arrow_panel_light(string& light_data, size_t pad, size_t panel_index);
    void handle_corner_panel_light(string& light_data, size_t pad, size_t panel_index);
    void fill_stage_panel_color(string& lights_data, uint8_t red, uint8_t green, uint8_t blue);
    void add_color(string& lights_data, uint8_t red, uint8_t green, uint8_t blue);
    int map_value(int x, int in_min, int in_max, int out_min, int out_max);
    uint8_t average(uint8_t a, uint8_t b);

    // The storage for the incoming lights states from Spice API when we call lights::read
    map<string, float> light_states;
    // The storage for the incoming tape LED states from Spice API when we call ddr:tapeled_get
    map<string, vector<uint8_t>> tape_led_states;

    /*
        These are just static sets of flags for whether a particular LED is on or off in the outer 4x4
        grid of LEDs in an SMX panel, whenever a "pad corner light" is on. This is just an L-shaped
        on-off grid, basically. We'll interpolate this into the actual LED values to send, based on how
        bright the lights are during runtime. We won't need to touch the inner 3x3 grid of LEDs, since
        we're simulating an L-shape.
    */
    // Pad upper-left
    static constexpr uint8_t pad_upper_left_leds[4][4] = {
        { 1, 1, 1, 1 },
        { 1, 0, 0, 0 },
        { 1, 0, 0, 0 },
        { 1, 0, 0, 0 }
    };

    // Pad lower-left
    static constexpr uint8_t pad_lower_left_leds[4][4] = {
        { 1, 0, 0, 0 },
        { 1, 0, 0, 0 },
        { 1, 0, 0, 0 },
        { 1, 1, 1, 1 }
    };

    // Pad upper-right
    static constexpr uint8_t pad_upper_right_leds[4][4] = {
        { 1, 1, 1, 1 },
        { 0, 0, 0, 1 },
        { 0, 0, 0, 1 },
        { 0, 0, 0, 1 }
    };

    // Pad lower-right
    static constexpr uint8_t pad_lower_right_leds[4][4] = {
        { 0, 0, 0, 1 },
        { 0, 0, 0, 1 },
        { 0, 0, 0, 1 },
        { 1, 1, 1, 1 }
    };
};
