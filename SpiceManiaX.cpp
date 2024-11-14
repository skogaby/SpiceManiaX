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
#include <windowsx.h>
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

// Forward function declarations
void CreateOverlayWindow(HINSTANCE h_instance, int cmd_show);
void InitializeTimers();
void CleanupTimers();
void HandleWindowPress(int x, int y, bool pressed);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void SmxOnLog(const char* log);
void WaitForConnection();
void CALLBACK LightsTimerCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
void CALLBACK InputTimerCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
void CALLBACK ConnectivityCheckTimerCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);

// Timer interval for how often we update the overlay graphics (30fps)
const int kFramerateIntervalMs = 33;
// Timer interval for when to update the lights on the cabinet (30Hz)
const int kLightsOutputIntervalMs = 33;
// Timer interval for when to send the inputs from the stage and overlay (1000Hz)
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
// Media Timer ID for the lights timer
static UINT lights_timer_id;
// Media Timer ID for the stage inputs timer
static UINT stage_input_timer_id;
// Media Timer ID for checking Spice API connectivity
static UINT connection_check_timer_id;
// Media Timer ID for redrawing the overlay
static UINT redraw_timer_id;
// Media Timer ID for updating the window position
static UINT window_position_timer_id;

// Program entrypoint
int WINAPI WinMain(HINSTANCE h_instance, HINSTANCE, LPSTR, int cmd_show) {
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

// Creates the overlay window and initializes the Direct2D contexts
void CreateOverlayWindow(HINSTANCE hInstance, int nCmdShow) {
    // Setup the actual overlay window, now that SpiceAPI is connected
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"OverlayWindowClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    hwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_APPWINDOW,
        wc.lpszClassName,
        L"SpiceManiaX Overlay",
        WS_POPUP | WS_VISIBLE,
        0, 0, kWindowRenderWidth, kWindowRenderHeight,
        NULL, NULL, hInstance, NULL
    );

    // Set the window styling so it's transparent, borderless, and always on top
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_APPWINDOW | WS_EX_NOACTIVATE);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, kWindowRenderWidth, kWindowRenderHeight, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);

    // Disable visual feedback for touch inputs
    BOOL feedbackEnabled = FALSE;
    SystemParametersInfo(SPI_SETTOUCHPREDICTIONPARAMETERS, 0, &feedbackEnabled, SPIF_SENDCHANGE);
    SystemParametersInfo(SPI_SETCONTACTVISUALIZATION, 0, NULL, SPIF_SENDCHANGE);

    // Actually show the overlay window
    ShowWindow(hwnd, nCmdShow);

    // Enable touch input
    RegisterTouchWindow(hwnd, 0);

    // Initialize Direct2D and create the touch overlay elements
    InitializeTouchOverlay();
}

// Initialize all of our system timers for various IO tasks
void InitializeTimers() {
    // Set system media timer resolution to 1 ms, so we can have accurate timers for inputs and outputs
    timeBeginPeriod(1);

    // Start a 33ms timer, so we can output lights at 30Hz
    lights_timer_id = timeSetEvent(kLightsOutputIntervalMs, 1, LightsTimerCallback, 0, TIME_PERIODIC);
    // Start a 1ms timer, so we can send stage inputs at 1000Hz
    stage_input_timer_id = timeSetEvent(kInputsUpdateIntervalMs, 1, InputTimerCallback, 0, TIME_PERIODIC);
    // Start a 3 second timer which polls for SpiceAPI connectivity
    connection_check_timer_id = timeSetEvent(kConnectionCheckIntervalMs, 1, ConnectivityCheckTimerCallback, 0, TIME_PERIODIC);
    // Start a 33ms timer for triggering overlay redraws
    redraw_timer_id = timeSetEvent(kFramerateIntervalMs, 1, RedrawTimerCallback, 0, TIME_PERIODIC);
    // Start a 5 second timer to make sure the window is always repositioned on top
    window_position_timer_id = timeSetEvent(kSetWindowPosIntervalMs, 1, WindowPosTimerCallback, 0, TIME_PERIODIC);
} 

// Kills our timers and cleans up the timer resolution settings
void CleanupTimers() {
    // Kill the timers
    timeKillEvent(lights_timer_id);
    timeKillEvent(stage_input_timer_id);
    timeKillEvent(connection_check_timer_id);
    timeKillEvent(redraw_timer_id);
    timeKillEvent(window_position_timer_id);
    // Reset the timer resolution period
    timeEndPeriod(1);
}

// Handles a window press, either from a touchscreen or from a mouse, and presses the appropriate
// Overlay button
void HandleWindowPress(int x, int y, bool pressed) {
    // Check which buttons the touches are in bounds for
    D2D1_POINT_2F touchPoint = D2D1::Point2F(x, y);
    for (OverlayButton& button: touch_overlay_buttons) {
        if (IsTouchInside(button, touchPoint)) {
            touch_overlay_button_states[button.id_] = pressed;
            return;
        }
    }
}

// Handler for incoming Windows messages to the overlay window
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    switch (msg) {
    case WM_PAINT:
        // Message signalling to redraw the screen
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        DrawButtons();
        EndPaint(hwnd, &ps);
        break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP: {
        // Handle mouse input for remote debugging
        HandleWindowPress(
            GET_X_LPARAM(l_param),
            GET_Y_LPARAM(l_param),
            (msg == WM_LBUTTONDOWN)
        );

        break;
    }
    case WM_TOUCH: {
        // Message signalling an incoming touch event on the screen
        UINT num_inputs = LOWORD(w_param);
        TOUCHINPUT touches[10];

        if (GetTouchInputInfo((HTOUCHINPUT) l_param, num_inputs, touches, sizeof(TOUCHINPUT))) {
            for (UINT i = 0; i < num_inputs; i++) {
                // Convert from 0.01mm to pixels
                int x = touches[i].x / 100;
                int y = touches[i].y / 100;

                // Determine if this was a press or not
                if (touches[i].dwFlags & TOUCHEVENTF_DOWN) {
                    HandleWindowPress(x, y, true);
                } else if (touches[i].dwFlags & TOUCHEVENTF_UP) {
                    HandleWindowPress(x, y, false);
                }
            }

            CloseTouchInputHandle((HTOUCHINPUT) l_param);
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_KEYDOWN:
        // Exit the program if the user presses Escape
        if (w_param == VK_ESCAPE) {
            PostQuitMessage(0);
        }
        break;
    default:
        return DefWindowProc(hwnd, msg, w_param, l_param);
    }

    return 0;
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

// Callback for the 33ms timer we set for the lights, to always output at 30Hz
void CALLBACK LightsTimerCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR) {
    lights_util.PerformLightsTasks(con);
}

// Callback for the 1ms timer we set for the stage inputs, to always output at 1000Hz
void CALLBACK InputTimerCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR) {
    input_utils.PerformInputTasks(con);
}

// Callback for the timer which triggers a SpiceAPI connectivity check
void CALLBACK ConnectivityCheckTimerCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR) {
    if (!con.check()) {
        // If we lose the connection to SpiceAPI, exit the program
        PostQuitMessage(0);
    }
}
