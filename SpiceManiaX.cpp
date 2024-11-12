#include "spiceapi/connection.h"
#include "spiceapi/wrappers.h"
#include "smx/smx_wrapper.h"
#include "lights_utils.h"
#include "input_utils.h"
#include "math_utils.h"

#include <d2d1.h>
#include <mmsystem.h>
#include <windows.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "d2d1")

using namespace spiceapi;
using namespace std;

// We output the overlay every 33ms (30fps)
const int FRAME_RATE_INTERVAL_MS = 33;
// We reposition the window every 3 seconds, to make sure it comes back
// into view if DDR starts after the overlay is spawned
const int SET_WINDOW_POS_INTERVAL_MS = 5000;
// We check for SpiceAPI connections during runtime every 3 seconds
const int CONNECTION_CHECK_INTERVAL_MS = 1000;
// Fullscreen width when DDR is running
const int DDR_SCREEN_W = 1280;
// Fullscreen height when DDR is running
const int DDR_SCREEN_H = 720;
// TODO: Remove these
const int BUTTON_WIDTH = 100;
const int BUTTON_HEIGHT = 50;

// Forward function declarations
LRESULT CALLBACK wnd_proc(HWND, UINT, WPARAM, LPARAM);
void smx_on_log(const char* log);
void wait_for_connection();
void CALLBACK lights_timer_callback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
void CALLBACK stage_input_timer_callback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
void initialize_direct2d(HWND hwnd);
void cleanup_direct2d();
void draw_buttons();
void draw_buttons_to_cache();

// Our connection object for communication with SpiceAPI
Connection con("localhost", 1337, "spicemaniax");
// Util class for handling lights interactions (reading lights from SpiceAPI, outputting via SMX SDK)
LightsUtils lights_util;
// Util class for handling stage input ineractions (read stage inputs when the state changes, output via SpiceAPI)
InputUtils stage_input_utils;
// Flag for whether SpiceAPI is connected or not
bool is_connected = false;
// Media Timer ID for the lights timer
UINT lights_timer_id;
// Media Timer ID for the stage inputs timer
UINT stage_input_timer_id;

// Global variables for Direct2D setup and contexts
HINSTANCE h_inst;
vector<RECT> buttons; // Array of button rectangles
map<int, bool> overlay_button_states; // Track button press states
ID2D1Factory* d2d_factory = nullptr;
ID2D1HwndRenderTarget* render_target = nullptr;
ID2D1SolidColorBrush* brush_normal = nullptr;
ID2D1SolidColorBrush* brush_pressed = nullptr;
ID2D1BitmapRenderTarget* cache_render_target = nullptr; // Cached render target

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // Create a console window we can use for logging
    if (AllocConsole()) {
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        freopen_s(&fp, "CONIN$", "r", stdin);
    }

    // Set the logging callback before we init the SDK
    SMXWrapper& smx = SMXWrapper::getInstance();
    smx.SMX_SetLogCallback(smx_on_log);

    // Initialize the SMX SDK, which handles device connections for us
    smx.SMX_Start(InputUtils::SMXStateChangedCallback, static_cast<void*>(&stage_input_utils));

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

    // Setup the actual overlay window, now thw SpiceAPI is connected
    h_inst = hInstance;
    WNDCLASS wc = {};
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"OverlayWindowClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_APPWINDOW,
        wc.lpszClassName,
        L"SpiceManiaX Overlay",
        WS_POPUP | WS_VISIBLE,
        0, 0, DDR_SCREEN_W, DDR_SCREEN_H,
        NULL, NULL, hInstance, NULL
    );

    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    SetWindowLongPtr(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_APPWINDOW | WS_EX_NOACTIVATE);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, DDR_SCREEN_W, DDR_SCREEN_H, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);

    ShowWindow(hwnd, nCmdShow);

    // Define button positions
    buttons.push_back({ 100, 100, 100 + BUTTON_WIDTH, 100 + BUTTON_HEIGHT });
    buttons.push_back({ 300, 100, 300 + BUTTON_WIDTH, 100 + BUTTON_HEIGHT });

    // Initialize button states
    for (size_t i = 0; i < buttons.size(); ++i) {
        overlay_button_states[i] = false; // False means "not pressed"
    }

    // Enable touch input
    RegisterTouchWindow(hwnd, 0);

    // Initialize Direct2D
    initialize_direct2d(hwnd);

    // Set a timer to trigger redraws at 30fps
    SetTimer(hwnd, 1, FRAME_RATE_INTERVAL_MS, NULL);
    // Set a timer every 5 seconds to make sure the window is always repositioned on top
    SetTimer(hwnd, 2, SET_WINDOW_POS_INTERVAL_MS, NULL);
    // Set a timer every 3 seconds to check for SpiceAPI connectivity
    SetTimer(hwnd, 3, CONNECTION_CHECK_INTERVAL_MS, NULL);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0) && is_connected) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Spice API is no longer connected, clean up and shut down
    printf("Lost connection to SpiceAPI, exiting\n");

    // Kill the timers (we have 2 media timers for managing IO, and 3 window timers for managing overlay redraws/positioning, and connectivity checks)
    timeKillEvent(lights_timer_id);
    timeKillEvent(stage_input_timer_id);
    KillTimer(hwnd, 1);
    KillTimer(hwnd, 2);
    KillTimer(hwnd, 3);
    // Reset the timer resolution period
    timeEndPeriod(1);
    // Cleanup the Direct2D contexts
    cleanup_direct2d();
    // Deregister the window for touch events
    UnregisterTouchWindow(hwnd);
    // Free the console window we allocated
    FreeConsole();
    // Kill the SMX SDK
    smx.SMX_Stop();

    return 0;
}

// Handler for incoming Windows messages to the overlay window
LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    switch (msg) {
    case WM_TIMER:
        // One of our Windows timers is being invoked, determine which one and act
        if (w_param == 1) {
            // Redraw logic for 33ms timer
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (w_param == 2) {
            // Periodically set the window to the topmost position, to handle the game starting
            // and taking over the screen
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, DDR_SCREEN_W, DDR_SCREEN_H, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
        } else if (w_param == 3) {
            // Periodically check the SpiceAPI connection, so we can auto exit when it's lost
            is_connected = con.check();
        }
        break;
    case WM_PAINT:
        // Message signalling to redraw the screen
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        draw_buttons();
        EndPaint(hwnd, &ps);
        break;
    case WM_TOUCH: {
        // Message signalling an incoming touch event on the screen
        UINT num_inputs = LOWORD(w_param);
        TOUCHINPUT touches[10];

        if (GetTouchInputInfo((HTOUCHINPUT)l_param, num_inputs, touches, sizeof(TOUCHINPUT))) {
            for (UINT i = 0; i < num_inputs; i++) {
                // Convert from 0.01mm to pixels
                int x = touches[i].x / 100;
                int y = touches[i].y / 100;

                // If the current screen width and height aren't 720p and don't match our render target resolution,
                // then we need to map the touch inputs from the screen coordinates to our render coordinates so
                // the touches still register correctly
                int screen_w = GetSystemMetrics(SM_CXSCREEN);
                int screen_h = GetSystemMetrics(SM_CYSCREEN);

                if (screen_w != DDR_SCREEN_W || screen_h != DDR_SCREEN_H) {
                    x = map_value(x, 0, screen_w, 0, DDR_SCREEN_W);
                    y = map_value(y, 0, screen_h, 0, DDR_SCREEN_H);
                }

                // Check which buttons the touches are in bounds for
                for (size_t j = 0; j < buttons.size(); ++j) {
                    if (PtInRect(&buttons[j], POINT{ x, y })) {
                        if ((touches[i].dwFlags & TOUCHEVENTF_DOWN) && !overlay_button_states[j]) {
                            overlay_button_states[j] = true;
                        } else if ((touches[i].dwFlags & TOUCHEVENTF_UP) && overlay_button_states[j]) {
                            overlay_button_states[j] = false;
                        }
                    }
                }
            }

            CloseTouchInputHandle((HTOUCHINPUT)l_param);
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_KEYDOWN:
        // Exit the overlay if the user presses Escape
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
    stage_input_utils.perform_input_tasks(con);
}

void initialize_direct2d(HWND hwnd) {
    // Initialize the Direct2D Factory
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_factory);

    // Create the main Direct2D render target for the window
    RECT rc;
    GetClientRect(hwnd, &rc);
    d2d_factory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)),
        &render_target
    );

    // Create an off-screen render target for caching static button backgrounds
    render_target->CreateCompatibleRenderTarget(&cache_render_target);

    // Create brushes for button states
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::CornflowerBlue), &brush_normal);
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), &brush_pressed);

    // Draw static content (buttons in normal state) to the off-screen render target
    cache_render_target->BeginDraw();
    draw_buttons_to_cache();
    cache_render_target->EndDraw();
}

void cleanup_direct2d() {
    if (brush_normal) brush_normal->Release();
    if (brush_pressed) brush_pressed->Release();
    if (cache_render_target) cache_render_target->Release();
    if (render_target) render_target->Release();
    if (d2d_factory) d2d_factory->Release();
}

void draw_buttons_to_cache() {
    // Draw static button backgrounds to the off-screen render target
    for (const auto& button : buttons) {
        D2D1_RECT_F rect = D2D1::RectF(
            static_cast<float>(button.left),
            static_cast<float>(button.top),
            static_cast<float>(button.right),
            static_cast<float>(button.bottom)
        );
        cache_render_target->FillRectangle(rect, brush_normal);
    }
}

void draw_buttons() {
    if (!render_target) return;

    render_target->BeginDraw();
    render_target->Clear(D2D1::ColorF(0, 0.0f)); // Clear with transparent background

    // Draw cached button backgrounds
    ID2D1Bitmap* pBitmap = nullptr;
    cache_render_target->GetBitmap(&pBitmap);
    if (pBitmap != nullptr) {
        render_target->DrawBitmap(pBitmap, D2D1::RectF(0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)));
        pBitmap->Release();
    }

    // Draw pressed state over buttons that are pressed
    for (size_t i = 0; i < buttons.size(); ++i) {
        if (overlay_button_states[i]) {
            D2D1_RECT_F rect = D2D1::RectF(
                static_cast<float>(buttons[i].left),
                static_cast<float>(buttons[i].top),
                static_cast<float>(buttons[i].right),
                static_cast<float>(buttons[i].bottom)
            );
            render_target->FillRectangle(rect, brush_pressed);
        }
    }

    render_target->EndDraw();
}
