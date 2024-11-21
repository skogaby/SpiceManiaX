#include "overlay_utils.h"

// Direct2D factory
static ID2D1Factory* d2d_factory = nullptr;
// Direct2D factory for creating text objects
static IDWriteFactory* d2d_write_factory = nullptr;
// The text formatter object for Direct2D text rendering
static IDWriteTextFormat* d2d_text_format = nullptr;
// Main render target for the overlay
static ID2D1HwndRenderTarget* render_target = nullptr;
// Cached render targets for our overlay in various states of visibility
static ID2D1BitmapRenderTarget* cached_render_targets[2][2] =
    { { nullptr, nullptr }, { nullptr, nullptr } };
// The Direct2D brushes for drawing our overlay buttons in various states
static ID2D1SolidColorBrush* brush_menu_fill = nullptr;
static ID2D1SolidColorBrush* brush_pinpad_fill = nullptr;
static ID2D1SolidColorBrush* brush_button_outline_normal = nullptr;
static ID2D1SolidColorBrush* brush_button_outline_pressed = nullptr;
// A map of button IDs to their cached geometry objects, for faster redraws and the ability
// to use the geometry objects for bounds checking
static map<int, ID2D1TransformedGeometry*> button_geometries;

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
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        wc.lpszClassName,
        L"SpiceManiaX Overlay",
        WS_POPUP | WS_VISIBLE,
        0, 0, kWindowRenderWidth, kWindowRenderHeight,
        NULL, NULL, hInstance, NULL
    );

    // Set the window styling so it's transparent, borderless, and always on top
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 255 * overlay_opacity, LWA_COLORKEY | LWA_ALPHA);
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

    // Create off-screen render targets for caching static button backgrounds
    render_target->CreateCompatibleRenderTarget(&cached_render_targets[0][0]);
    render_target->CreateCompatibleRenderTarget(&cached_render_targets[0][1]);
    render_target->CreateCompatibleRenderTarget(&cached_render_targets[1][0]);
    render_target->CreateCompatibleRenderTarget(&cached_render_targets[1][1]);

    // Create brushes for button states
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::WhiteSmoke), &brush_menu_fill);
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::SlateGray), &brush_pinpad_fill);
    render_target->CreateSolidColorBrush(D2D1::ColorF(0.01, 0.01, 0.01), &brush_button_outline_normal);
    render_target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), &brush_button_outline_pressed);

    // Create the objects needed for text rendering
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&d2d_write_factory));
    d2d_write_factory->CreateTextFormat(
        L"Arial",
        NULL,
        DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        14.0f,
        L"en-us",
        &d2d_text_format
    );

    // Center align text horizontally and vertically
    d2d_text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    d2d_text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    // Create all the buttons for the overlay
    SetupOverlayButtons();

    // Initialize button states
    for (OverlayButton& button: touch_overlay_buttons) {
        touch_overlay_button_states[button.id_] = false;
    }

    // Draw static content (buttons in normal state) to the off-screen render targets
    DrawButtonsToCaches();
}

// Sets up the overlay button objects for both players
void SetupOverlayButtons() {
    // Build the overlay elements for each player
    for (int player = 0; player < 2; player++) {
        string player_str = "P" + to_string(player + 1) + " ";
        int button_index = (player * 100);

        // Define the menu navigation buttons. All the coordinates are defined pretty jankily, but it is what it is. Everything
        // is anchored from the position of the Menu Up button for each player.
        static constexpr int menu_up_cxs[2] = { 100, 1072 };
        static constexpr int menu_up_cy = 575;
        int menu_up_down_cx = menu_up_cxs[player];
        int menu_left_right_start_cy = static_cast<int>(menu_up_cy + (kMenuNavButtonHeight * (1.75 / 2)));

        touch_overlay_buttons.push_back({ button_index++, player_str + "Menu Up", L"",
            menu_up_down_cx,
            menu_up_cy,
            kMenuNavButtonWidth, kMenuNavButtonHeight, true, OverlayButtonType::MENU, player });
        touch_overlay_buttons.push_back({ button_index++, player_str + "Menu Down", L"",
            menu_up_down_cx,
            static_cast<int>(menu_up_cy + (kMenuNavButtonHeight * 1.75)),
            kMenuNavButtonWidth, kMenuNavButtonHeight, true, OverlayButtonType::MENU, player });
        touch_overlay_buttons.push_back({ button_index++, player_str + "Menu Left", L"",
            menu_up_down_cx - kMenuNavButtonWidth + 5,
            menu_left_right_start_cy,
            kMenuNavButtonWidth, kMenuNavButtonHeight, true, OverlayButtonType::MENU, player });
        touch_overlay_buttons.push_back({ button_index++, player_str + "Menu Right", L"",
            menu_up_down_cx + kMenuNavButtonWidth - 5,
            menu_left_right_start_cy,
            kMenuNavButtonWidth, kMenuNavButtonHeight, true, OverlayButtonType::MENU, player });
        touch_overlay_buttons.push_back({ button_index++, player_str + "Start", L"",
            menu_up_down_cx + static_cast<int>(kMenuNavButtonWidth * 3.25),
            menu_left_right_start_cy,
            kMenuNavButtonWidth, kMenuNavButtonHeight, false, OverlayButtonType::MENU, player });

        // Define the pinpad buttons for each player. Similar to the menu buttons, we'll anchor these based on the coordinates
        // for the top-left key on the pinpad (Key 7)
        static string pinpad_key_input_names[4][3] = {
            { "Keypad 7", "Keypad 8", "Keypad 9" },
            { "Keypad 4", "Keypad 5", "Keypad 6" },
            { "Keypad 1", "Keypad 2", "Keypad 3" },
            { "Keypad 0", "Keypad 00", "Keypad Decimal" }
        };
        static wstring pinpad_key_labels[4][3] = {
            { L"7", L"8", L"9" },
            { L"4", L"5", L"6" },
            { L"1", L"2", L"3" },
            { L"0", L"00", L"" }
        };
        // 20 pixels from either edge of the screen to the edge of the overlay elements
        static constexpr int pinpad_edge_to_screen = 20;
        // 10 pixels between the keys on the pinpad
        static constexpr int pinpad_spacing = 10;
        // The width of a whole pinpad, edge-to-edge, including spacing between keys
        static constexpr int pinpad_width = (kPinpadButtonWidth * 3) + (2 * pinpad_spacing);
        // X-coords of the top-left key of each pinpad
        static constexpr int first_pinpad_key_cxs[2] = {
            pinpad_edge_to_screen + (kPinpadButtonWidth / 2), 
            kWindowRenderWidth - pinpad_width - pinpad_edge_to_screen + (kPinpadButtonWidth / 2)
        };
        // Y-coords of the top-left key of each pinpad
        static constexpr int first_pinpad_key_cy =
            (2 * pinpad_edge_to_screen) + (kUtilityButtonHeight) + (kPinpadButtonHeight / 2);
        // Get the coords for the first key, then lay everything else
        // out based on the first key
        int first_key_cx = first_pinpad_key_cxs[player];

        for (int row = 0; row < 4; row++) {
            for (int col = 0; col < 3; col++) {
                touch_overlay_buttons.push_back({
                    button_index++,
                    player_str + pinpad_key_input_names[row][col],
                    pinpad_key_labels[row][col],
                    first_key_cx + ((kPinpadButtonWidth + pinpad_spacing) * col),
                    first_pinpad_key_cy + ((kPinpadButtonHeight + pinpad_spacing) * row),
                    kPinpadButtonWidth,
                    kPinpadButtonHeight,
                    false,
                    OverlayButtonType::PINPAD,
                    player
                });
            }
        }

        // Setup the toggle overlay button
        static const int edge_to_vis_center = pinpad_edge_to_screen + (kUtilityButtonWidth / 2);
        static constexpr int overlay_vis_cxs[2] = {
            edge_to_vis_center,
            kWindowRenderWidth - edge_to_vis_center
        };
        static constexpr int overlay_vis_cy = pinpad_edge_to_screen + (kUtilityButtonHeight / 2);

        touch_overlay_buttons.push_back({ button_index++, "", L"Toggle Overlay",
            overlay_vis_cxs[player],
            overlay_vis_cy,
            kUtilityButtonWidth, kUtilityButtonHeight, false, OverlayButtonType::VISIBILITY, player });

        // Setup the card-in button if a card ID is configured for this player
        static const int edge_to_card_center = pinpad_edge_to_screen + (kUtilityButtonWidth * 1.5) + pinpad_spacing;
        static constexpr int card_in_cxs[2] = {
            edge_to_card_center,
            kWindowRenderWidth - edge_to_card_center
        };
        static constexpr int card_in_cy = pinpad_edge_to_screen + (kUtilityButtonHeight / 2);

        if (card_ids[player] != "") {
            touch_overlay_buttons.push_back({ button_index++, "", L"Insert Card",
                card_in_cxs[player],
                card_in_cy,
                kUtilityButtonWidth, kUtilityButtonHeight, false, OverlayButtonType::CARD_IN, player });
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
    cached_render_targets[is_overlay_visible[0]][is_overlay_visible[1]]->GetBitmap(&bitmap);

    if (bitmap != nullptr) {
        render_target->DrawBitmap(bitmap, D2D1::RectF(0, 0, kWindowRenderWidth, kWindowRenderHeight));
        bitmap->Release();
    }

    // Draw pressed state over buttons that are pressed
    for (OverlayButton& button : touch_overlay_buttons) {
        if (touch_overlay_button_states[button.id_]) {
            DrawSingleButton(button, render_target, true);
        }
    }

    render_target->EndDraw();
}

// Dispose of the cached D2D objects and free up memory from brushes, render targets, etc.
void CleanupTouchOverlay() {
    if (brush_menu_fill != nullptr) brush_menu_fill->Release();
    if (brush_pinpad_fill != nullptr) brush_pinpad_fill->Release();
    if (brush_button_outline_normal != nullptr) brush_button_outline_normal->Release();
    if (brush_button_outline_pressed != nullptr) brush_button_outline_pressed->Release();
    if (cached_render_targets[0][0] != nullptr) cached_render_targets[0][0]->Release();
    if (cached_render_targets[0][1] != nullptr) cached_render_targets[0][1]->Release();
    if (cached_render_targets[1][0] != nullptr) cached_render_targets[1][0]->Release();
    if (cached_render_targets[1][1] != nullptr) cached_render_targets[1][1]->Release();
    if (render_target != nullptr) render_target->Release();
    if (d2d_factory != nullptr) d2d_factory->Release();
    if (d2d_write_factory != nullptr) d2d_write_factory->Release();
    if (d2d_text_format != nullptr) d2d_text_format->Release();

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
                } else if (touches[i].dwFlags & TOUCHEVENTF_UP) {
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
    case WM_ACTIVATE:
        if (w_param != WA_INACTIVE) {
            return 0;
        }
        break;
    default:
        return DefWindowProc(hwnd, msg, w_param, l_param);
    }

    return 0;
}

// Draws a single button on the touch overlay, using the given render target and the given brush,
// so this can be re-used for the cached base layer as well as the top layer render target.
void DrawSingleButton(OverlayButton& button, ID2D1RenderTarget* render_target, bool is_pressed) {
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

    // Pull the cached geometry for this button and draw it in its given state
    render_target->FillGeometry(
        button_geometries[button.id_],
        button.type_ == OverlayButtonType::MENU ? brush_menu_fill : brush_pinpad_fill
    );
    render_target->DrawGeometry(
        button_geometries[button.id_],
        is_pressed ? brush_button_outline_pressed : brush_button_outline_normal,
        2.0f
    );

    // Draw the button label, if it has one
    if (button.label_ != L"") {
        int half_width = button.width_ / 2;
        int half_height = button.height_ / 2;
        float left = button.center_x_ - half_width;
        float right = button.center_x_ + half_width;
        float top = button.center_y_ - half_height;
        float bottom = button.center_y_ + half_height;

        render_target->DrawText(
            button.label_.c_str(),
            static_cast<UINT32>(button.label_.length()),
            d2d_text_format,
            D2D1::RectF(left, top, right, bottom),
            brush_menu_fill
        );
    }
}

// Draw the static button backgrounds and default visual states to the cached render target
void DrawButtonsToCaches() {
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++)
            cached_render_targets[i][j]->BeginDraw();

    // Draw static button backgrounds to the off-screen render target
    for (OverlayButton& button : touch_overlay_buttons) {
        // Draw each player's buttons to both of their own render targets, and the toggle overlay
        // buttons should be drawn to both player's render targets regardless
        if (button.player_ == 0 || button.type_ == OverlayButtonType::VISIBILITY) {
            DrawSingleButton(button, cached_render_targets[1][0], false);
            DrawSingleButton(button, cached_render_targets[1][1], false);
        }
        
        if (button.player_ == 1 || button.type_ == OverlayButtonType::VISIBILITY) {
            DrawSingleButton(button, cached_render_targets[0][1], false);
            DrawSingleButton(button, cached_render_targets[1][1], false);
        }
    }

    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++)
            cached_render_targets[i][j]->EndDraw();
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
