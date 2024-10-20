#pragma once

#include "input_utils.h"
#include "smx/smx_wrapper.h"
#include "spiceapi/wrappers.h"
#include <vector>
#include <map>

// Our hardcoded constants for what color to use for simulating the color
// of the pads on the DDR Gold cabinets, so we can use this color for the
// center and corner panels of the stages.
#define GOLD_R 0xFF
#define GOLD_G 0xFF
#define GOLD_B 0x00

using namespace spiceapi;
using namespace std;

void perform_lights_tasks(Connection& con);
void handle_stage_lights_update();
void handle_cabinet_lights_updates();
void handle_arrow_panel_light(string& light_data, size_t pad, size_t panel_index);
void handle_corner_panel_light(string& light_data, size_t pad, size_t panel_index);
void fill_stage_panel_color(string& lights_data, uint8_t red, uint8_t green, uint8_t blue);
void add_color(string& lights_data, uint8_t red, uint8_t green, uint8_t blue);

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
static uint8_t pad_upper_left_leds[4][4] = {
	{ 1, 1, 1, 1 },
	{ 1, 0, 0, 0 },
	{ 1, 0, 0, 0 },
	{ 1, 0, 0, 0 }
};

// Pad lower-left
static uint8_t pad_lower_left_leds[4][4] = {
	{ 1, 0, 0, 0 },
	{ 1, 0, 0, 0 },
	{ 1, 0, 0, 0 },
	{ 1, 1, 1, 1 }
};

// Pad upper-right
static uint8_t pad_upper_right_leds[4][4] = {
	{ 1, 1, 1, 1 },
	{ 0, 0, 0, 1 },
	{ 0, 0, 0, 1 },
	{ 0, 0, 0, 1 }
};

// Pad lower-right
static uint8_t pad_lower_right_leds[4][4] = {
	{ 0, 0, 0, 1 },
	{ 0, 0, 0, 1 },
	{ 0, 0, 0, 1 },
	{ 1, 1, 1, 1 }
};

// Perform the various lights related tasks on a cadence of 30Hz
void perform_lights_tasks(Connection& con) {
    // Read all the light states from SpiceAPI, for both the regular lights and the tape LEDs
    light_states.clear();
    tape_led_states.clear();
    lights_read(con, light_states);
    ddr_tapeled_get(con, tape_led_states);

    // Output the stage lights first, since those go as a single update to a single API
    handle_stage_lights_update();

    // Next, output the cabinet lights, since those are all handled separately, by a different API
    handle_cabinet_lights_updates();
}

// Handles sending the lights updates to the stages. This sources data from both the "normal" LEDs, 
// as well as the tape LEDs, since we want the RGB strips and also the corner lights. The StepManiaX
// SDK accepts one large payload for the lights for all 18 panels at once (both players).
void handle_stage_lights_update() {
    if (light_states.empty() || tape_led_states.empty())
        return;

    string light_data;

    // Iterate through each panel in the order SMX expects the output to be in
    for (size_t pad = 0; pad < 2; pad++) {
        for (size_t panel = 0; panel < 9; panel++) {
            switch (panel) {
            case UP:
            case LEFT:
            case RIGHT:
            case DOWN:
                handle_arrow_panel_light(light_data, pad, panel);
                break;
            case UP_LEFT:
            case UP_RIGHT:
            case DOWN_LEFT:
            case DOWN_RIGHT:
                handle_corner_panel_light(light_data, pad, panel);
                break;
            case CENTER:
                // Just make the center panel statically gold
                fill_stage_panel_color(light_data, GOLD_R, GOLD_G, GOLD_B);
                break;
            default:
                break;
            }
        }
    }

    // Send the lights update
    SMXWrapper::getInstance().SMX_SetLights2(light_data.data(), light_data.size());
}

// Handles the lights updates for an arrow panel of a stage by appending the appropriate
// lights data to the given string
void handle_arrow_panel_light(string& light_data, size_t pad, size_t panel_index) {
    string device_name;
    string device_prefix = "p" + to_string(pad + 1) + "_foot_";

    // Figure out which device name we need to index on when pulling the LED data
    switch (panel_index) {
    case UP:
        device_name = device_prefix + "up";
        break;
    case LEFT:
        device_name = device_prefix + "left";
        break;
    case DOWN:
        device_name = device_prefix + "down";
        break;
    case RIGHT:
        device_name = device_prefix + "right";
        break;
    default:
        return;
    }

    // Pull the LED data for this device, and output it to the light string. All
    // arrow panel LED PCBs have 25 LEDs, which matches SMX exactly.
    vector<uint8_t> tapeled = tape_led_states[device_name];

    for (size_t led = 0; led < 25; led++) {
        uint8_t r = tapeled[(led * 3)];
        uint8_t g = tapeled[(led * 3) + 1];
        uint8_t b = tapeled[(led * 3) + 2];
        add_color(light_data, r, g, b);
    }
}

// Handles the lights update for a corner panel of a stage by appending the appropiate
// lights data to the given string based on the given flags (indicating which corner
// this is for), combined with the incoming lights data from SpiceAPI.
void handle_corner_panel_light(string& light_data, size_t pad, size_t panel_index) {
    string device_name;
    string device_prefix = "GOLD P" + to_string(pad + 1) + " ";
    uint8_t (*flags)[4] = nullptr;

    // Figure out which device name we need to index on when pulling the LED data, and
    // also which set of LED flags we should use when constructing the outputs
    switch (panel_index) {
    case UP_LEFT:
        device_name = device_prefix + "Stage Corner Up-Left";
        flags = pad_upper_left_leds;
        break;
    case UP_RIGHT:
        device_name = device_prefix + "Stage Corner Up-Right";
        flags = pad_upper_right_leds;
        break;
    case DOWN_LEFT:
        device_name = device_prefix + "Stage Corner Down-Left";
        flags = pad_lower_left_leds;
        break;
    case DOWN_RIGHT:
        device_name = device_prefix + "Stage Corner Down-Right";
        flags = pad_lower_right_leds;
        break;
    default:
        return;
    }

    // Read the value of the lights from the given device name
    uint8_t light_value = light_states[device_name] * 255.f;

    // Iterate through the flags and write data based on whether each LED should be lit or not. We are
    // only populating the data for the 4x4 grid of outer LEDs with controllable data. Anything that's
    // flagged as "off" will be statically gold to simulate a gold pad.
    for (size_t row = 0; row < 4; row++) {
        for (size_t col = 0; col < 4; col++) {
            if (flags[row][col] != 0) {
                add_color(light_data, light_value, light_value, light_value);
            } else {
                add_color(light_data, GOLD_R, GOLD_G, GOLD_B);
            }
        }
    }

    // Just make the inner 3x3 grid also gold, to match the rest of the pad
    for (size_t i = 0; i < 9; i++) {
        add_color(light_data, GOLD_R, GOLD_G, GOLD_B);
    }
}

// Outputs data to fill an entire stage LED panel with a single color
void fill_stage_panel_color(string& lights_data, uint8_t red, uint8_t green, uint8_t blue) {
    for (size_t i = 0; i < 25; i++) {
        add_color(lights_data, red, green, blue);
    }
}

// Handles all the updates for all the stage light devices
void handle_cabinet_lights_updates() {
    // TODO
}

// Adds an RGB color to the given string, so we can send the string to the SMX SDK
// as lights data
void add_color(string& lights_data, uint8_t red, uint8_t green, uint8_t blue) {
    lights_data.append(1, red);
    lights_data.append(1, green);
    lights_data.append(1, blue);
}
