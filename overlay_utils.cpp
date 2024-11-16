#include "overlay_utils.h"

// Direct2D factory
static ID2D1Factory* d2d_factory = nullptr;
// Main render target for the overlay
static ID2D1HwndRenderTarget* render_target = nullptr;
// The offscreen, cached render target for the touch overlay. We use this for drawing
// the base state of the overlay a single time, then just render the cached target
// and draw the button presses on top of this, if necessary
static ID2D1BitmapRenderTarget* cache_render_target = nullptr;
// The Direct2D brush for drawing the base state of each button
static ID2D1SolidColorBrush* brush_normal = nullptr;
// The Direct2D brush for drawing the pressed state of each button
static ID2D1SolidColorBrush* brush_pressed = nullptr;
// A map of button IDs to their cached geometry objects, for faster redraws and the ability
// to use the geometry objects for bounds checking
static map<int, ID2D1TransformedGeometry*> button_geometries;

// Callback for the timer which triggers an overlay redraw
void CALLBACK RedrawTimerCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR) {
    InvalidateRect(hwnd, NULL, FALSE);
}

// Callback for the timer which triggers the window to reposition itself on top of everything else
void CALLBACK WindowPosTimerCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR) {
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, kWindowRenderWidth, kWindowRenderHeight, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
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

// Initializes the Direct2D contexts, render targets, etc. and sets up the cached button rendering
void InitializeTouchOverlay() {
    // Initialize the Direct2D Factory
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_factory);

    // Create the main Direct2D render target for the window
    RECT rc { 0, 0, kWindowRenderWidth, kWindowRenderHeight };
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

    // Create all the buttons for the overlay
    SetupOverlayButtons();

    // Initialize button states
    for (OverlayButton& button: touch_overlay_buttons) {
        touch_overlay_button_states[button.id_] = false;
    }

    // Draw static content (buttons in normal state) to the off-screen render target
    cache_render_target->BeginDraw();
    DrawButtonsToCache();
    cache_render_target->EndDraw();
}

// Sets up the overlay button objects for both players
void SetupOverlayButtons() {
    for (int player = 0; player < 2; player++) {
        string player_str = "P" + to_string(player + 1) + " ";
        int button_index = (player * 100);

        // Define the menu navigation buttons. All the coordinates are defined pretty jankily, but it is what it is. Everything
        // is anchored from the position of the Menu Up button for each player.
        static int menu_up_cxs[2] = { 100, 1080 };
        static int menu_up_cys[2] = { 575, 575 };
        int menu_up_cx = menu_up_cxs[player];
        int menu_up_cy = menu_up_cys[player];

        touch_overlay_buttons.push_back({ button_index++, player_str + "Menu Up", "",
            menu_up_cx,
            menu_up_cy,
            kMenuNavButtonWidth, kMenuNavButtonHeight, true, false });
        touch_overlay_buttons.push_back({ button_index++, player_str + "Menu Down", "",
            menu_up_cx,
            static_cast<int>(menu_up_cy + (kMenuNavButtonHeight * 1.75)),
            kMenuNavButtonWidth, kMenuNavButtonHeight, true, false });
        touch_overlay_buttons.push_back({ button_index++, player_str + "Menu Left", "",
            menu_up_cx - kMenuNavButtonWidth + 5,
            static_cast<int>(menu_up_cy + (kMenuNavButtonHeight * (1.75 / 2))),
            kMenuNavButtonWidth, kMenuNavButtonHeight, true, false });
        touch_overlay_buttons.push_back({ button_index++, player_str + "Menu Right", "",
            menu_up_cx + kMenuNavButtonWidth - 5,
            static_cast<int>(menu_up_cy + (kMenuNavButtonHeight * (1.75 / 2))),
            kMenuNavButtonWidth, kMenuNavButtonHeight, true, false });
        touch_overlay_buttons.push_back({ button_index++, player_str + "Start", "",
            menu_up_cx + (kMenuNavButtonWidth * 3),
            menu_up_cy + kMenuNavButtonHeight - 5,
            kMenuNavButtonWidth, kMenuNavButtonHeight, false, false });

        // Define the pinpad buttons for each player. Similar to the menu buttons, we'll anchor these based on the coordinates
        // for the top-left key on the pinpad (Key 7)
        static string pinpad_key_input_names[4][3] = {
            { "Keypad 7", "Keypad 8", "Keypad 9" },
            { "Keypad 4", "Keypad 5", "Keypad 6" },
            { "Keypad 1", "Keypad 2", "Keypad 3" },
            { "Keypad 0", "Keypad 00", "Keypad Decimal" }
        };
        static string pinpad_key_labels[4][3] = {
            { "7", "8", "9" },
            { "4", "5", "6" },
            { "1", "2", "3" },
            { "0", "00", "" }
        };
        static int first_key_cxs[2] = { 75, 1065 };
        static int first_key_cys[2] = { 100, 100 };
        int first_key_cx = first_key_cxs[player];
        int first_key_cy = first_key_cys[player];

        for (int row = 0; row < 4; row++) {
            for (int col = 0; col < 3; col++) {
                touch_overlay_buttons.push_back({
                    button_index++,
                    player_str + pinpad_key_input_names[row][col],
                    pinpad_key_labels[row][col],
                    first_key_cx + ((kPinpadButtonWidth + 10) * col),
                    first_key_cy + ((kPinpadButtonHeight + 10) * row),
                    kPinpadButtonWidth,
                    kPinpadButtonHeight,
                    false,
                    true
                });
            }
        }
    }
}

// Draws the current state of the buttons on top of their cached, unpressed states
void RenderTouchOverlay() {
    if (render_target == nullptr)
        return;

    // Clear with transparent background
    render_target->BeginDraw();
    render_target->Clear(D2D1::ColorF(0, 0.0f));

    // Draw cached button backgrounds
    ID2D1Bitmap* bitmap = nullptr;
    cache_render_target->GetBitmap(&bitmap);

    if (bitmap != nullptr) {
        render_target->DrawBitmap(bitmap, D2D1::RectF(0, 0, kWindowRenderWidth, kWindowRenderHeight));
        bitmap->Release();
    }

    // Draw pressed state over buttons that are pressed
    for (OverlayButton& button : touch_overlay_buttons) {
        if (touch_overlay_button_states[button.id_]) {
            DrawSingleButton(button, render_target, brush_pressed);
        }
    }

    render_target->EndDraw();
}

// Dispose of the cached D2D objects and free up memory from brushes, render targets, etc.
void CleanupTouchOverlay() {
    if (brush_normal != nullptr) brush_normal->Release();
    if (brush_pressed != nullptr) brush_pressed->Release();
    if (cache_render_target != nullptr) cache_render_target->Release();
    if (render_target != nullptr) render_target->Release();
    if (d2d_factory != nullptr) d2d_factory->Release();

    // Release our cached button geometries
    for (OverlayButton& button: touch_overlay_buttons) {
        if (button_geometries.count(button.id_) > 0) {
            button_geometries[button.id_]->Release();
        }
    }
}

// Handles a window press, either from a touchscreen or from a mouse, and presses the appropriate
// Overlay button
void HandleWindowPress(int x, int y, bool pressed) {
    // Check which buttons the touches are in bounds for
    D2D1_POINT_2F touchPoint = D2D1::Point2F(x, y);
    for (OverlayButton& button : touch_overlay_buttons) {
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
        RenderTouchOverlay();
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

        if (GetTouchInputInfo((HTOUCHINPUT)l_param, num_inputs, touches, sizeof(TOUCHINPUT))) {
            for (UINT i = 0; i < num_inputs; i++) {
                // Convert from 0.01mm to pixels
                int x = touches[i].x / 100;
                int y = touches[i].y / 100;

                // Determine if this was a press or not
                if (touches[i].dwFlags & TOUCHEVENTF_DOWN) {
                    HandleWindowPress(x, y, true);
                }
                else if (touches[i].dwFlags & TOUCHEVENTF_UP) {
                    HandleWindowPress(x, y, false);
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

// Draws a single button on the touch overlay, using the given render target and the given brush,
// so this can be re-used for the cached base layer as well as the top layer render target.
void DrawSingleButton(OverlayButton& button, ID2D1RenderTarget* render_target, ID2D1SolidColorBrush* brush) {
    if (button_geometries.count(button.id_) == 0) {
        // Create a rectangle geometry if we don't have one cached for this button
        int half_width = (button.width_ / 2);
        int half_height = (button.height_ / 2);
        int left = button.center_x_ - half_width;
        int right = button.center_x_ + half_width;
        int top = button.center_y_ - half_height;
        int bottom = button.center_y_ + half_height;
        ID2D1RectangleGeometry* rectangle_geometry = nullptr;
        D2D1_RECT_F rect = D2D1::RectF(left, top, right, bottom);
        d2d_factory->CreateRectangleGeometry(rect, &rectangle_geometry);

        if (rectangle_geometry == nullptr)
            return;

        // Create a rotation transform
        D2D1_POINT_2F center = D2D1::Point2F(button.center_x_, button.center_y_);
        ID2D1TransformedGeometry* transformed_geometry = nullptr;
        d2d_factory->CreateTransformedGeometry(
            rectangle_geometry,
            D2D1::Matrix3x2F::Rotation(button.rotated_ ? 45.0f : 0.0f, center),
            &transformed_geometry
        );

        rectangle_geometry->Release();

        // Cache the transformed geometry
        button_geometries[button.id_] = transformed_geometry;
    }

    // Pull the cached geometry for this button and fill it
    render_target->FillGeometry(button_geometries[button.id_], brush);
}

// Draw the static button backgrounds and default visual states to the cached render target
void DrawButtonsToCache() {
    // Draw static button backgrounds to the off-screen render target
    for (OverlayButton& button : touch_overlay_buttons) {
        DrawSingleButton(button, cache_render_target, brush_normal);
    }
}

// Says whether the given touch point is inside the given button's drawn area
bool IsTouchInside(OverlayButton& button, D2D1_POINT_2F& point) {
    static D2D1_MATRIX_3X2_F identity = D2D1::Matrix3x2F::Identity();

    // The cached geometries for the buttons have a built-in method to determine if a point is contained inside
    if (button_geometries.count(button.id_) > 0) {
        BOOL contains = FALSE;
        button_geometries[button.id_]->FillContainsPoint(point, identity, &contains);
        return contains;
    }
}
