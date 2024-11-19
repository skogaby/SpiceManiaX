#include "spiceapi/connection.h"
#include "spiceapi/wrappers.h"
#include "smx/smx_wrapper.h"
#include "lights_utils.h"
#include "input_utils.h"
#include "overlay_utils.h"
#include "math_utils.h"
#include "globals.h"

#include <d2d1.h>
#include <mmsystem.h>
#include <windows.h>
#include <winuser.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "d2d1")

using namespace spiceapi;
using namespace std;

// Commandline argument keys
const string kP1CardArg = "p1card";
const string kP2CardArg = "p2card";

// Forward function declarations
void ParseArgs();
void InitializeTimers();
void CleanupTimers();
void SmxOnLog(const char* log);
void WaitForConnection();
void CALLBACK ThirtyHzTimerCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
void CALLBACK InputTimerCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
void CALLBACK ConnectivityCheckTimerCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
void CALLBACK WindowPosTimerCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);

// Timer interval for how often we update the overlay graphics, update lights, and send
// pinpad inputs (30Hz)
const int k30HzTasksIntervalMs = 33;
// Timer interval for when to send the button inputs from the stage (1000Hz)
const int kInputsUpdateIntervalMs = 1;
// We reposition the window every 5 seconds, to make sure it always sits on top of DDR after it goes fullscreen
const int kSetWindowPosIntervalMs = 5000;
// We check for SpiceAPI connections during runtime every 3 seconds
const int kConnectionCheckIntervalMs = 3000;

// Our connection object for communication with SpiceAPI
Connection con("localhost", 1337, "spicemaniax");
// Util class for handling lights interactions (reading lights from SpiceAPI, outputting via SMX SDK)
LightsUtils lights_util;
// Util class for handling stage input ineractions (read stage inputs when the state changes, output via SpiceAPI)
InputUtils input_utils;
// Media Timer ID for the stage inputs timer
static UINT stage_input_timer_id;
// Media Timer ID for checking Spice API connectivity
static UINT connection_check_timer_id;
// Media Timer ID for redrawing the overlay
static UINT thirty_hz_timer_id;
// Media Timer ID for updating the window position
static UINT window_position_timer_id;

// Program entrypoint
int WINAPI WinMain(HINSTANCE h_instance, HINSTANCE, LPSTR, int cmd_show) {
    // Parse the CLI arguments
    ParseArgs();

    // Create a console window we can use for logging
    if (AllocConsole()) {
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        freopen_s(&fp, "CONIN$", "r", stdin);
    }

    // Set the logging callback before we init the SDK
    SMXWrapper& smx = SMXWrapper::getInstance();
    smx.SMX_SetLogCallback(SmxOnLog);

    // Initialize the SMX SDK, which handles device connections for us
    smx.SMX_Start(InputUtils::SMXStateChangedCallback, static_cast<void*>(&input_utils));

    if (!smx.loaded) {
        printf("Unable to load StepManiaX SDK, exiting");
        return 1;
    }

    printf("Loaded SMX.dll successfully, attempting to connect to SpiceAPI now\n");

    // Connect to SpiceAPI, retry until it's successful
    WaitForConnection();

    printf("Connected to SpiceAPI successfully\n");

    // Create the actual overlay window and initialize Direct2D drawing
    CreateOverlayWindow(h_instance, cmd_show);

    // Initialize the timers that drive our IO, draw calls, etc.
    InitializeTimers();

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Spice API is no longer connected, clean up and shut down
    printf("Lost connection to SpiceAPI, exiting\n");

    // Clean up the timers we created
    CleanupTimers();
    // Deregister the window for touch events
    UnregisterTouchWindow(hwnd);
    // Cleanup the touch overlay and release the Direct2D objects
    CleanupTouchOverlay();
    // Kill the SMX SDK
    smx.SMX_Stop();
    // Free the console window we allocated
    FreeConsole();

    return 0;
}

// Parses the command-line arguments
void ParseArgs() {
    map<string, string> args_map;

    // Use GetCommandLineW to get the full command-line as a wide string
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (argv == NULL) {
        return;
    }

    // Iterate the arguments (skip argument 0, the program name)
    for (int i = 1; i < argc; i++) {
        wstring arg = argv[i];

        // Check if the argument is a key
        if (arg[0] == L'-') {
            // Remove one or two dashes
            string key = (arg[1] == L'-') ? string(arg.begin() + 2, arg.end())
                : string(arg.begin() + 1, arg.end());

            // Look for the value (next argument)
            if (i + 1 < argc) {
                wstring value = argv[i + 1];

                // If the next argument is not a key, treat it as the value
                if (value[0] != L'-') {
                    args_map[key] = string(value.begin(), value.end());
                    // Skip the value in the next iteration
                    ++i; 
                } else {
                    // Handle case where value is missing
                    args_map[key] = ""; 
                }
            } else {
                // Handle case where value is missing
                args_map[key] = "";
            }
        }
    }

    // Free the memory allocated by CommandLineToArgvW
    LocalFree(argv);

    // Set the global variables
    if (args_map.count(kP1CardArg) > 0) {
        card_ids[0] = args_map[kP1CardArg];
    }

    if (args_map.count(kP2CardArg) > 0) {
        card_ids[1] = args_map[kP2CardArg];
    }
}

// Initialize all of our system timers for various IO tasks
void InitializeTimers() {
    // Set system media timer resolution to 1 ms, so we can have accurate timers for inputs and outputs
    timeBeginPeriod(1);

    // Start a 1ms timer, so we can send stage inputs at 1000Hz
    stage_input_timer_id = timeSetEvent(kInputsUpdateIntervalMs, 1, InputTimerCallback, 0, TIME_PERIODIC);
    // Start a 33ms timer for triggering overlay redraws, sending pinpad inputs, and updating the lights
    thirty_hz_timer_id = timeSetEvent(k30HzTasksIntervalMs, 1, ThirtyHzTimerCallback, 0, TIME_PERIODIC);
    // Start a 3 second timer which polls for SpiceAPI connectivity
    connection_check_timer_id = timeSetEvent(kConnectionCheckIntervalMs, 1, ConnectivityCheckTimerCallback, 0, TIME_PERIODIC);
    // Start a 5 second timer to make sure the window is always repositioned on top
    window_position_timer_id = timeSetEvent(kSetWindowPosIntervalMs, 1, WindowPosTimerCallback, 0, TIME_PERIODIC);
} 

// Kills our timers and cleans up the timer resolution settings
void CleanupTimers() {
    // Kill the timers
    timeKillEvent(stage_input_timer_id);
    timeKillEvent(thirty_hz_timer_id);
    timeKillEvent(connection_check_timer_id);
    timeKillEvent(window_position_timer_id);
    // Reset the timer resolution period
    timeEndPeriod(1);
}

// Logging callback for the StepManiaX SDK
void SmxOnLog(const char* log) {
    printf("[SMX] %s\n", log);
}

// Checks for the SpiceAPI connection, and waits for it to be available if it's not
void WaitForConnection() {
    if (!con.check()) {
        printf("Unable to connect to SpiceAPI, waiting until connection is successful\n");
        while (!con.check()) {
            printf(".");
        }
        printf("\n");
    }
}

// Callback for the 30Hz timer which updates the lights, sends pinpad updates, and redraws the overlay
void CALLBACK ThirtyHzTimerCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR) {
    lights_util.PerformLightsTasks(con);
    input_utils.PerformPinpadInputTasks(con);
    input_utils.PerformLoginInputTasks(con);
    InvalidateRect(hwnd, NULL, FALSE);
}

// Callback for the 1ms timer we set for the stage inputs, to always output at 1000Hz
void CALLBACK InputTimerCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR) {
    input_utils.PerformMainInputTasks(con);
}

// Callback for the timer which triggers a SpiceAPI connectivity check
void CALLBACK ConnectivityCheckTimerCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR) {
    if (!con.check()) {
        // If we lose the connection to SpiceAPI, exit the program
        PostQuitMessage(0);
    }
}

// Callback for the timer which triggers the window to reposition itself on top of everything else
void CALLBACK WindowPosTimerCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR) {
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, kWindowRenderWidth, kWindowRenderHeight, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
}
