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

// Says whether the given touch point is inside the given button area
bool IsTouchInside(OverlayButton& button, D2D1_POINT_2F& point) {
    static D2D1_MATRIX_3X2_F identity = D2D1::Matrix3x2F::Identity();

    // The cached geometries for the buttons have a built-in method to determine if a point is contained inside
    if (button_geometries.count(button.id_) > 0) {
        BOOL contains = FALSE;
        button_geometries[button.id_]->FillContainsPoint(point, identity, &contains);
        return contains;
    }
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

    // Define the P1 menu navigation buttons. All the coordinates are defined pretty jankily, but it is what it is. Everything
    // is anchored from the position of the P1 Menu Up button.
    int p1_up_cx = 100;
    int p1_up_cy = 575;
    touch_overlay_buttons.push_back({ 0, "P1 Menu Up", "",
        p1_up_cx, p1_up_cy, kMenuNavButtonWidth, kMenuNavButtonHeight, true });
    touch_overlay_buttons.push_back({ 1, "P1 Menu Down", "",
        p1_up_cx, static_cast<int>(p1_up_cy + (kMenuNavButtonHeight * 1.75)), kMenuNavButtonWidth, kMenuNavButtonHeight, true});
    touch_overlay_buttons.push_back({ 2, "P1 Menu Left", "",
        p1_up_cx - kMenuNavButtonWidth + 5, static_cast<int>(p1_up_cy + (kMenuNavButtonHeight * (1.75 / 2))), kMenuNavButtonWidth, kMenuNavButtonHeight, true});
    touch_overlay_buttons.push_back({ 3, "P1 Menu Right", "",
        p1_up_cx + kMenuNavButtonWidth - 5, static_cast<int>(p1_up_cy + (kMenuNavButtonHeight * (1.75 / 2))), kMenuNavButtonWidth, kMenuNavButtonHeight, true });
    touch_overlay_buttons.push_back({ 4, "P1 Start", "",
        p1_up_cx + (kMenuNavButtonWidth * 3), p1_up_cy + kMenuNavButtonHeight, kMenuNavButtonWidth, kMenuNavButtonHeight, false });
    
    // Define the P2 menu navigation buttons. Everything is anchored from the position of the P2 Menu Up button.
    int p2_up_cx = 1080;
    int p2_up_cy = 575;
    touch_overlay_buttons.push_back({ 5, "P2 Menu Up", "",
        p2_up_cx, p2_up_cy, kMenuNavButtonWidth, kMenuNavButtonHeight, true });
    touch_overlay_buttons.push_back({ 6, "P2 Menu Down", "",
        p2_up_cx, static_cast<int>(p2_up_cy + (kMenuNavButtonHeight * 1.75)), kMenuNavButtonWidth, kMenuNavButtonHeight, true });
    touch_overlay_buttons.push_back({ 7, "P2 Menu Left", "",
        p2_up_cx - kMenuNavButtonWidth + 5, static_cast<int>(p2_up_cy + (kMenuNavButtonHeight * (1.75 / 2))), kMenuNavButtonWidth, kMenuNavButtonHeight, true });
    touch_overlay_buttons.push_back({ 8, "P2 Menu Right", "",
        p2_up_cx + kMenuNavButtonWidth - 5, static_cast<int>(p2_up_cy + (kMenuNavButtonHeight * (1.75 / 2))), kMenuNavButtonWidth, kMenuNavButtonHeight, true });
    touch_overlay_buttons.push_back({ 9, "P2 Start", "",
        p2_up_cx + (kMenuNavButtonWidth * 3), p2_up_cy + kMenuNavButtonHeight, kMenuNavButtonWidth, kMenuNavButtonHeight, false });

    // Initialize button states
    for (OverlayButton& button: touch_overlay_buttons) {
        touch_overlay_button_states[button.id_] = false;
    }

    // Draw static content (buttons in normal state) to the off-screen render target
    cache_render_target->BeginDraw();
    DrawButtonsToCache();
    cache_render_target->EndDraw();
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

// Draws the current state of the buttons on top of the cached, default state
void DrawButtons() {
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

