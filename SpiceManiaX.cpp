#include "spiceapi/connection.h"
#include "spiceapi/wrappers.h"
#include "smx/smx_wrapper.h"
#include "lights_utils.h"
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")

#define DEBUG false

using namespace spiceapi;
using namespace std;

static FILETIME fileTime;
extern uint16_t pad_input_states[2];
extern map<string, float> light_states;
extern map<string, vector<uint8_t>> tape_led_states;

// Logging callback for the StepManiaX SDK
static void smx_on_log(const char* log) {
    printf("[SMX] %s\n", log);
}

// Get the current system time in microseconds
static ULONGLONG get_current_time_micros() {
    GetSystemTimeAsFileTime(&fileTime);
    return ((static_cast<ULONGLONG>(fileTime.dwHighDateTime) << 32) | fileTime.dwLowDateTime) / 1000L;
}

int main() {
    // Set the thread priority to the highest, so we maintain timing
    HANDLE hThread = GetCurrentThread();

    if (SetThreadPriority(hThread, THREAD_PRIORITY_HIGHEST) == 0) {
        std::cerr << "Failed to set thread priority: " << GetLastError() << std::endl;
        return 1;
    }

    // Start up the StepManiaX SDK, which should init devices automatically as they connect
    SMXWrapper::getInstance().SMX_SetLogCallback(smx_on_log);
    SMXWrapper::getInstance().SMX_Start(smx_on_state_changed, nullptr);

    if (!SMXWrapper::getInstance().loaded) {
        printf("Unable to load StepManiaX SDK, exiting");
        return 1;
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

    // Make sure we're only outputting 
    ULONGLONG last_log_time = get_current_time_micros();
    ULONGLONG last_input_update = get_current_time_micros();
    ULONGLONG last_lights_update = get_current_time_micros();
    int lights_updates = 0;
    int input_updates = 0;
    int max_input_updates = 0;
    int min_input_updates = 100000000;

    do {
        ULONGLONG current_time = get_current_time_micros();

        // Update lights ever 322 microseconds, which works out to 30Hz with Windows timing resolutions
        if (current_time - last_lights_update >= 322) {
            perform_lights_tasks(con);

            // Bookeeping
            last_lights_update = current_time;
            lights_updates++;
        }

        // Attempt to update the inputs at 1000Hz. This number gets us pretty consistently
        // at 995-10005Hz, but Windows scheduling ain't perfect. The resolution of the system
        // time isn't actually high enough that this happens every 5 microseconds, but if we go
        // higher than "5", we start to see instances of only hitting like 970Hz of input rate.
        // This sometimes goes slightly over 1000Hz, but that's not inherently a bad thing since
        // we're just sending Spice API calls here, and the updates from the SMX pads are happening
        // asynchronously anyway.
        if (current_time - last_input_update >= 5) {
            perform_input_tasks(con);

            // Bookkeeping
            last_input_update = current_time;
            input_updates++;
        }

        // Output the current input and output rates once every 2 seconds
        static int out_interval = 2;
        if (DEBUG && (current_time - last_log_time >= 10000L * out_interval)) {
            if (input_updates > max_input_updates) {
                max_input_updates = input_updates;
            }

            if (input_updates < min_input_updates) {
                min_input_updates = input_updates;
            }

            printf("Max: %i - Min: %i - Lights: %i - Input: %i\n",
                max_input_updates / out_interval,
                min_input_updates / out_interval,
                lights_updates / out_interval,
                input_updates / out_interval
            );

            last_log_time = current_time;
            lights_updates = 0;
            input_updates = 0;
        }
    } while (!(light_states.empty() && lights_updates > 0));

    printf("Lost connection to SpiceAPI, exiting\n");
    SMXWrapper::getInstance().SMX_Stop();
    return 0;
}
