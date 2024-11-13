#include "lights_utils.h"

// Perform the various lights related tasks on a cadence of 30Hz
void LightsUtils::PerformLightsTasks(Connection& con) {
    // Read all the light states from SpiceAPI, for both the regular lights and the tape LEDs
    light_states_.clear();
    tape_led_states_.clear();

    if (POLL_LIGHTS) {
        lights_read(con, light_states_);
    }

    if (POLL_TAPE_LED) {
        ddr_tapeled_get(con, tape_led_states_);
    }

    if (OUTPUT_LIGHTS) {
        // Output the stage lights first, since those go as a single update to a single API
        HandleStageLightsUpdate();

        // Next, output the cabinet lights, since those are all handled separately, by a different API
        HandleMarqueeLightsUpdate();
        HandleVerticalStripLightsUpdate();
        HandleSpotlightLightsUpdate();
    }
}

// Handles sending the lights updates to the stages. This sources data from both the "normal" LEDs, 
// as well as the tape LEDs, since we want the RGB strips and also the corner lights. The StepManiaX
// SDK accepts one large payload for the lights for all 18 panels at once (both players).
void LightsUtils::HandleStageLightsUpdate() {
    if (light_states_.empty() || tape_led_states_.empty())
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
                HandleArrowPanelLight(light_data, pad, panel);
                break;
            case UP_LEFT:
            case UP_RIGHT:
            case DOWN_LEFT:
            case DOWN_RIGHT:
                HandleCornerPanelLight(light_data, pad, panel);
                break;
            case CENTER:
                // Just make the center panel a static color
                FillStagePanelColor(light_data, kPadRed, kPadGreen, kPadBlue);
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
void LightsUtils::HandleArrowPanelLight(string& light_data, size_t pad, size_t panel_index) {
    if (tape_led_states_.empty())
        return;

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
    vector<uint8_t> tapeled = tape_led_states_[device_name];

    for (size_t led = 0; led < kSmxArrowLedCount; led++) {
        uint8_t r = tapeled[(led * 3)];
        uint8_t g = tapeled[(led * 3) + 1];
        uint8_t b = tapeled[(led * 3) + 2];
        AddColor(light_data, r, g, b);
    }
}

// Handles the lights update for a corner panel of a stage by appending the appropiate
// lights data to the given string based on the given flags (indicating which corner
// this is for), combined with the incoming lights data from SpiceAPI.
void LightsUtils::HandleCornerPanelLight(string& light_data, size_t pad, size_t panel_index) {
    if (light_states_.empty())
        return;

    string device_name;
    string device_prefix = "GOLD P" + to_string(pad + 1) + " ";
    const uint8_t(*flags)[4] = nullptr;

    // Figure out which device name we need to index on when pulling the LED data, and
    // also which set of LED flags we should use when constructing the outputs
    switch (panel_index) {
    case UP_LEFT:
        device_name = device_prefix + "Stage Corner Up-Left";
        flags = kPadUpperLeftLeds;
        break;
    case UP_RIGHT:
        device_name = device_prefix + "Stage Corner Up-Right";
        flags = kPadUpperRightLeds;
        break;
    case DOWN_LEFT:
        device_name = device_prefix + "Stage Corner Down-Left";
        flags = kPadLowerLeftLeds;
        break;
    case DOWN_RIGHT:
        device_name = device_prefix + "Stage Corner Down-Right";
        flags = kPadLowerRightLeds;
        break;
    default:
        return;
    }

    // Read the value of the lights from the given device name
    uint8_t light_value = light_states_[device_name] * 255.f;

    // Iterate through the flags and write data based on whether each LED should be lit or not. We are
    // only populating the data for the 4x4 grid of outer LEDs with controllable data. Anything that's
    // flagged as "off" will be statically gold to simulate a gold pad.
    for (size_t row = 0; row < 4; row++) {
        for (size_t col = 0; col < 4; col++) {
            if (flags[row][col] != 0) {
                AddColor(light_data, light_value, light_value, light_value);
            }
            else {
                AddColor(light_data, kPadRed, kPadGreen, kPadBlue);
            }
        }
    }

    // Just make the inner 3x3 grid also gold, to match the rest of the pad
    for (size_t i = 0; i < 9; i++) {
        AddColor(light_data, kPadRed, kPadGreen, kPadBlue);
    }
}

// Outputs data to fill an entire stage LED panel with a single color
void LightsUtils::FillStagePanelColor(string& lights_data, uint8_t red, uint8_t green, uint8_t blue) {
    for (size_t i = 0; i < kSmxArrowLedCount; i++) {
        AddColor(lights_data, red, green, blue);
    }
}

// Handles the lights updates for the marquee
void LightsUtils::HandleMarqueeLightsUpdate() {
    if (tape_led_states_.empty())
        return;

    // Read the lights values for the top panel strip
    vector<uint8_t> tapeled = tape_led_states_["top_panel"];

    if (tapeled.empty()) {
        return;
    }

    // Map the 40 DDR LEDs onto the 12 SMX marquee LEDs. Since we're mapping more LEDs onto less LEDs,
    // we'll prefer lit LEDs over blank ones. This will at least make sure that things like single
    // pixel sweeps won't be missed due to integer mappings going poorly. If we have a conflict between
    // two lit LEDs, we'll just average them. This is the only strip we need to do this for, since the others
    // are mapping less LEDs onto more, so we just repeat some, rather than needing a conflict resolution strategy.
    uint8_t smx_led_out[kSmxMarqueeLogicalLedCount * 3] = { 0 };
    for (size_t ddr_i = 0; ddr_i < kDdrTopPanelLedCount; ddr_i++) {
        size_t smx_i = MapValue(ddr_i, 0, kDdrTopPanelLedCount, 12, 0);

        // See what we need to do based on if the current DDR LED is lit, and if we've already
        // lit the SMX LED at the mapped index
        uint8_t ddr_r = tapeled[(ddr_i * 3)];
        uint8_t ddr_g = tapeled[(ddr_i * 3) + 1];
        uint8_t ddr_b = tapeled[(ddr_i * 3) + 2];
        bool ddr_is_on = (ddr_r != 0 || ddr_g != 0 || ddr_b != 0);

        // If the current LED isn't on, then it's a no-op, just go to the next LED
        // since we don't overwrite or average lit LEDs with unlit ones
        if (!ddr_is_on)
            continue;

        uint8_t smx_r = smx_led_out[(smx_i * 3)];
        uint8_t smx_g = smx_led_out[(smx_i * 3) + 1];
        uint8_t smx_b = smx_led_out[(smx_i * 3) + 2];
        bool smx_is_on = (smx_r != 0 || smx_g != 0 || smx_b != 0);

        if (!smx_is_on) {
            // If the DDR LED is on and the SMX LED is off, just replace the SMX LED
            smx_led_out[(smx_i * 3)] = ddr_r;
            smx_led_out[(smx_i * 3) + 1] = ddr_g;
            smx_led_out[(smx_i * 3) + 2] = ddr_b;
        }
        else {
            // If both LEDs are on, then just average them
            smx_led_out[(smx_i * 3)] = Average(ddr_r, smx_r);
            smx_led_out[(smx_i * 3) + 1] = Average(ddr_g, smx_g);
            smx_led_out[(smx_i * 3) + 2] = Average(ddr_b, smx_b);
        }
    }

    // Send the lights update
    SMXWrapper::getInstance().SMX_SetDedicatedCabinetLights(
        SMXDedicatedCabinetLights::MARQUEE,
        reinterpret_cast<const char*>(&(smx_led_out[0])),
        kSmxMarqueeLogicalLedCount * 3
    );
}

// Handles the lights updates for the vertical strip lights
void LightsUtils::HandleVerticalStripLightsUpdate() {
    if (tape_led_states_.empty())
        return;

    // Read the lights values for the monitor strips
    vector<uint8_t> tapeled[2] = {
        tape_led_states_["monitor_left"],
        tape_led_states_["monitor_right"]
    };

    if (tapeled[0].empty() || tapeled[1].empty())
        return;

    static SMXDedicatedCabinetLights device_ids[2] = {
        LEFT_STRIP,
        RIGHT_STRIP
    };

    // Iterate over both strips, map the monitor LEDs to the vertical strips (26 -> 28 LEDs)
    for (size_t strip = 0; strip < 2; strip++) {
        string light_data;

        for (size_t smx_i = 0; smx_i < kSmxVerticalStripLedCount; smx_i++) {
            // Map the 26 gold cab LEDs to our 28 strip LEDs on SMX
            size_t ddr_i = MapValue(smx_i, 0, kSmxVerticalStripLedCount, kDdrVerticalStripLedCount, 0);
            uint8_t r = tapeled[strip][(ddr_i * 3)];
            uint8_t g = tapeled[strip][(ddr_i * 3) + 1];
            uint8_t b = tapeled[strip][(ddr_i * 3) + 2];
            AddColor(light_data, r, g, b);
        }

        // Send the lights update
        SMXWrapper::getInstance().SMX_SetDedicatedCabinetLights(
            device_ids[strip], light_data.data(), light_data.size()
        );
    }
}

// Handles the lights updates for the 3 spotlights
void LightsUtils::HandleSpotlightLightsUpdate() {
    // Read the lights values for the subwoofer corner lights
    uint8_t light1 = light_states_["GOLD P1 Woofer Corner"] * 255.f;
    uint8_t light2 = light_states_["GOLD P2 Woofer Corner"] * 255.f;
    vector<uint8_t> light_values = { light1, light2 };

    static SMXDedicatedCabinetLights device_ids[2] = {
        LEFT_SPOTLIGHTS,
        RIGHT_SPOTLIGHTS
    };

    // Iterate over each set of spotlights, turn them all white according to the brightness of the
    // woofer corner lights for each player
    for (size_t device = 0; device < 2; device++) {
        uint8_t light_value = light_values[device];
        string light_data;

        for (size_t i = 0; i < 8; i++) {
            AddColor(light_data, light_value, light_value, light_value);
        }

        // Send the lights update
        SMXWrapper::getInstance().SMX_SetDedicatedCabinetLights(
            device_ids[device], light_data.data(), light_data.size()
        );
    }
}

// Adds an RGB color to the given string, so we can send the string to the SMX SDK
// as lights data
void LightsUtils::AddColor(string& lights_data, uint8_t red, uint8_t green, uint8_t blue) {
    lights_data.append(1, red);
    lights_data.append(1, green);
    lights_data.append(1, blue);
}

// Map a value from one numberspace to another, while maintaining proportionality between the input
// numberspace and the output value with respect to the output numberspace.
int LightsUtils::MapValue(int x, int in_min, int in_max, int out_min, int out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Average two bytes together, for crude color averaging during LED interpolation
uint8_t LightsUtils::Average(uint8_t a, uint8_t b) {
    return (uint8_t)(((uint16_t)a + (uint16_t)b) / 2);
}
