#include "spiceapi/connection.h"
#include "spiceapi/wrappers.h"
#include "smx/smx_wrapper.h"
#include "lights_utils.h"
#include "stage_input_utils.h"
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")

#define DEBUG false

using namespace spiceapi;
using namespace std;

// Logging callback for the StepManiaX SDK
static void smx_on_log(const char* log) {
    printf("[SMX] %s\n", log);
}

// Checks for the SpiceAPI connection, and waits for it to be available if it's not
static void wait_for_connection(Connection& con) {
    if (!con.check()) {
        printf("Unable to connect to SpiceAPI, waiting until connection is successful\n");
        while (!con.check()) {
            printf(".");
        }
        printf("\n");
    }
}

int main() {
    // Util classes for abstracting away the hardware and software interactions with Spice2x and SMX
    LightsUtils lights_util;
    StageInputUtils stage_input_utils;

    // Set the logging callback before we init the SDK
    SMXWrapper& smx = SMXWrapper::getInstance();
    smx.SMX_SetLogCallback(smx_on_log);

    // Initialize the SMX SDK, which handles device connections for us
    smx.SMX_Start(StageInputUtils::SMXStateChangedCallback, static_cast<void*>(&stage_input_utils));

    if (!smx.loaded) {
        printf("Unable to load StepManiaX SDK, exiting");
        return 1;
    }

    printf("Loaded SMX.dll successfully, attempting to connect to SpiceAPI now\n");

    // Connect to SpiceAPI, retry until it's successful
    Connection con("localhost", 1337, "spicemaniax");
    wait_for_connection(con);

    printf("Connected to SpiceAPI successfully\n");

    // Control the number of times per second we poll for and output the lights
    size_t poll_rate = 20;
    size_t sleep_interval = 1000 / poll_rate;
    size_t loops = 0;
    bool is_connected = true;

    while (is_connected) {
        loops++;

        // Just send the lights roughly 20 times a second. This still looks fine visually, and is slightly 
        // less stressful on my dogshit mini PC that's inside the SMX cab right now.
        Sleep(sleep_interval);
        lights_util.perform_lights_tasks(con);

        if (loops == poll_rate) {
            // Once a second we'll explicitly check the SpiceAPI connection, so we can exit the program
            // if the connection is lost.
            if (!con.check()) {
                is_connected = false;
            }

            loops = 0;
        }
    }

    printf("Lost connection to SpiceAPI, exiting\n");
    smx.SMX_Stop();
    return 0;
}
