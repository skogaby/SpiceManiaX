#include "spiceapi/connection.h"
#include "spiceapi/wrappers.h"
#include "smx/smx_wrapper.h"
#include "lights_utils.h"
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")

#define DEBUG false

using namespace spiceapi;
using namespace std;

// SpiceAPI connection wrapper
Connection* con;

// Logging callback for the StepManiaX SDK
static void smx_on_log(const char* log) {
    printf("[SMX] %s\n", log);
}

// Checks for the SpiceAPI connection, and waits for it to be available if it's not
static void wait_for_connection(Connection* con) {
    if (!con->check()) {
        printf("Unable to connect to SpiceAPI, waiting until connection is successful\n");
        while (!con->check()) {
            printf(".");
        }
        printf("\n");
    }
}

int main() {
    // Start up the StepManiaX SDK, which should init devices automatically as they connect
    SMXWrapper::getInstance().SMX_SetLogCallback(smx_on_log);
    // Setup the input callback, which will invoke SpiceAPI's inputs modules from the callback
    SMXWrapper::getInstance().SMX_Start(smx_on_state_changed, nullptr);

    if (!SMXWrapper::getInstance().loaded) {
        printf("Unable to load StepManiaX SDK, exiting");
        return 1;
    }

    printf("Loaded SMX.dll successfully, attempting to connect to SpiceAPI now\n");

    // Connect to SpiceAPI, retry until it's successful
    // Connection con("localhost", 1337, "spicemaniax");
    con = new Connection("localhost", 1337, "spicemaniax");
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
        perform_lights_tasks(con);

        if (loops == poll_rate) {
            // Once a second we'll explicitly check the SpiceAPI connection, so we can exit the program
            // if the connection is lost.
            if (!con->check()) {
                is_connected = false;
            }

            loops = 0;
        }
    }

    printf("Lost connection to SpiceAPI, exiting\n");
    SMXWrapper::getInstance().SMX_Stop();
    return 0;
}
