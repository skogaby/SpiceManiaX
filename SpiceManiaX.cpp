#include "spiceapi/connection.h"
#include "spiceapi/wrappers.h"
#include "smx/smx.h"
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")

#define BIT(value, i) (((value) >> (i)) & 1)

using namespace spiceapi;

static SMXWrapper smx_sdk;
static FILETIME fileTime;
static uint16_t pad_input_states[2];

// Logging callback for the StepManiaX SDK
static void smxLogCallback(const char* log) {
    printf("[SMX] %s\n", log);
}

// The callback that's invoked by the StepManiaX SDK whenver the state changes on either
// of the connected pads (this is not implemented for the cabinet IO, yet)
static void smxStateChangedCallback(int pad, SMXUpdateCallbackReason reason, void* pUser) {
    uint16_t state = smx_sdk.SMX_GetInputState(pad);
    pad_input_states[pad] = state;
    // printf("[SMX] Device %i state changed: %04x\n", pad, state);

    /*for (size_t panel = 0; panel < 9; panel++) {
        printf("%i ", BIT(state, panel));
    }
    printf("\n");*/
}

// Get the current system time in milliseconds
static ULONGLONG getCurrentTimeMicros() {
    GetSystemTimeAsFileTime(&fileTime);
    return ((static_cast<ULONGLONG>(fileTime.dwHighDateTime) << 32) | fileTime.dwLowDateTime) / 1L;
}

int main() {
    // First, go ahead and start up the StepManiaX SDK, which should connect to devices
    // as they become available automatically
    smx_sdk.SMX_SetLogCallback(smxLogCallback);
    smx_sdk.SMX_Start(smxStateChangedCallback, nullptr);

    if (!smx_sdk.loaded) {
        printf("Unable to load StepManiaX SDK, exiting");
        return -1;
    }

    printf("Loaded SMX.dll successfully, attempting to connect to SpiceAPI now\n");

    // Connect to SpiceAPI, retry until it's successful
    Connection con("localhost", 1337, "spicemaniax");

    if (!con.check()) {
        printf("Unable to connect to SpiceAPI, waiting until connection is successful\n");
        while (!con.check()) {
            printf(".");
        }
    }

    printf("Connected to SpiceAPI successfully\n");
    std::vector<LightState> lights;
    std::vector<TapeLedLightState> tape_leds;
    ULONGLONG last_log_time = getCurrentTimeMicros();
    ULONGLONG last_input_update = getCurrentTimeMicros();
    ULONGLONG last_lights_update = getCurrentTimeMicros();
    int lights_updates = 0;
    int input_updates = 0;
    int loops = 0;

    do {
        ULONGLONG current_time = getCurrentTimeMicros();

        // Attempt to update the lights every 166 microseconds (~60Hz)
        if (current_time - last_lights_update >= 166000) {
            // First, read the regular light states. We mainly just need this for the subwoofer
            // corner lights and the stage corner lights, since the rest of the lights we care
            // about come from the tape LEDs
            lights.clear();
            lights_read(con, lights);
            
            /*for (const LightState& l : lights) {
                printf("%s: %f\n", l.name.c_str(), l.value);
            }*/

            // Next, read the tape LEDs
            tape_leds.clear();
            ddr_tapeled_get(con, tape_leds);

            /*for (const TapeLedLightState& l : tape_leds) {
                if (l.name == "p1_foot_up") {
                    printf("%s: [ ", l.name.c_str());
                    for (uint8_t val : l.values) {
                        printf("%i ", val);
                    }
                    printf("]\n");
                }
            }*/

            // Output the stage lights first, since those go as a single update

            // Next, output the cabinet lights, since those are all handled as their own discrete devices

            last_lights_update = current_time;
            lights_updates++;
        }

        // Attempt to update the inputs at 1000Hz. This number gets us pretty consistently
        // at 999-1001Hz, but Windows scheduling ain't perfect.
        if (current_time - last_input_update >= 8000) {
            // We should already have the pad states in memory due to the SMX callbacks
            // being called from the SMX thread. Therefore, just send them out via SpiceAPI

            last_input_update = current_time;
            input_updates++;
        }

        // Output the current input and output rates once a second
        if (current_time - last_log_time >= 10000000L) {
            printf("Loops: %i - Lights updates: %i - Input updates: %i\n", loops, lights_updates, input_updates);
            last_log_time = current_time;
            lights_updates = 0;
            input_updates = 0;
            loops = 0;
        }

        loops++;
    } while (!(lights.empty() && lights_updates > 0));

    printf("Lost connection to SpiceAPI, exiting\n");
    smx_sdk.SMX_Stop();
    return 0;
}
