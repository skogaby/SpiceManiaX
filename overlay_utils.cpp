#include "overlay_utils.h"

// Callback for the timer which triggers an overlay redraw
void CALLBACK RedrawTimerCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR) {
    InvalidateRect(hwnd, NULL, FALSE);
}

// Callback for the timer which triggers the window to reposition itself on top of everything else
void CALLBACK WindowPosTimerCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR) {
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, kDdrScreenWidth, kDdrScreenHeight, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
}

// Initializes the Direct2D contexts, render targets, etc. and sets up the cached button rendering
void InitializeTouchOverlay() {
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

    // Define button positions (TODO: overhaul this)
    overlay_buttons.push_back({ 100, 100, 100 + kButtonWidth, 100 + kButtonHeight });
    overlay_buttons.push_back({ 300, 100, 300 + kButtonWidth, 100 + kButtonHeight });

    // Initialize button states
    for (size_t i = 0; i < overlay_buttons.size(); ++i) {
        overlay_button_states[i] = false;
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
}

// Draw the static button backgrounds and default visual states to the cached render target
void DrawButtonsToCache() {
    // Draw static button backgrounds to the off-screen render target
    for (const auto& button : overlay_buttons) {
        D2D1_RECT_F rect = D2D1::RectF(
            static_cast<float>(button.left),
            static_cast<float>(button.top),
            static_cast<float>(button.right),
            static_cast<float>(button.bottom)
        );
        cache_render_target->FillRectangle(rect, brush_normal);
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
        render_target->DrawBitmap(bitmap, D2D1::RectF(0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)));
        bitmap->Release();
    }

    // Draw pressed state over buttons that are pressed
    for (size_t i = 0; i < overlay_buttons.size(); ++i) {
        if (overlay_button_states[i]) {
            D2D1_RECT_F rect = D2D1::RectF(
                static_cast<float>(overlay_buttons[i].left),
                static_cast<float>(overlay_buttons[i].top),
                static_cast<float>(overlay_buttons[i].right),
                static_cast<float>(overlay_buttons[i].bottom)
            );

            render_target->FillRectangle(rect, brush_pressed);
        }
    }

    render_target->EndDraw();
}

