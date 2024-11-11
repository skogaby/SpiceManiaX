#include "spiceapi/connection.h"
#include "spiceapi/wrappers.h"
#include "smx/smx_wrapper.h"
#include "lights_utils.h"
#include "stage_input_utils.h"
#include <iostream>
#include <mmsystem.h>
#include <windows.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "winmm.lib")

using namespace spiceapi;
using namespace std;

void smx_on_log(const char* log);
void wait_for_connection();
void CALLBACK lights_timer_callback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
void CALLBACK stage_input_timer_callback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);

// Our connection object for communication with SpiceAPI
Connection con("localhost", 1337, "spicemaniax");
// Util class for handling lights interactions (reading lights from SpiceAPI, outputting via SMX SDK)
LightsUtils lights_util;
// Util class for handling stage input ineractions (read stage inputs when the state changes, output via SpiceAPI)
StageInputUtils stage_input_utils;

// Flag for whether SpiceAPI is connected or not
bool is_connected = false;
// TimerID for the lights timer
UINT lights_timer_id;
// TimerID for the stage inputs timer
UINT stage_input_timer_id;

int main() {
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
    wait_for_connection();

    printf("Connected to SpiceAPI successfully\n");
    is_connected = true;

    // Set system timer resolution to 1 ms, so we can have accurate timers for inputs and outputs
    timeBeginPeriod(1);

    // Start a 33ms timer, so we can output lights at 30Hz
    lights_timer_id = timeSetEvent(33, 1, lights_timer_callback, 0, TIME_PERIODIC);
    // Start a 1ms timer, so we can send stage inputs at 1000Hz
    stage_input_timer_id = timeSetEvent(1, 1, stage_input_timer_callback, 0, TIME_PERIODIC);

    while (is_connected) {
        // Just check the connection every 5 seconds. Inputs happen on a separate callback
        // thread managed by the SMX SDK, and outputs are invoked on a separate thread managed
        // by the timer API. The main thread should just check the connection and exit the program
        // once it's unavailable.
        Sleep(5000);
        is_connected = con.check();
    }

    // If we lose connection, reset the timer resolution, stop SMX SDK, and exit
    printf("Lost connection to SpiceAPI, exiting\n");

    timeKillEvent(lights_timer_id);
    timeKillEvent(stage_input_timer_id);
    timeEndPeriod(1);
    smx.SMX_Stop();

    return 0;
}


// Logging callback for the StepManiaX SDK
void smx_on_log(const char* log) {
    printf("[SMX] %s\n", log);
}

// Checks for the SpiceAPI connection, and waits for it to be available if it's not
void wait_for_connection() {
    if (!con.check()) {
        printf("Unable to connect to SpiceAPI, waiting until connection is successful\n");
        while (!con.check()) {
            printf(".");
        }
        printf("\n");
    }
}

// Callback for the 33ms timer we set for the lights, to always output at 30Hz
void CALLBACK lights_timer_callback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR) {
    lights_util.perform_lights_tasks(con);
}

// Callback for the 1ms timer we set for the stage inputs, to always output at 1000Hz
void CALLBACK stage_input_timer_callback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR) {
    stage_input_utils.perform_stage_input_tasks(con);
}
