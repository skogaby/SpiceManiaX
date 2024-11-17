#pragma once

#include "overlay_button.h"
#include "globals.h"

#include <d2d1.h>
#include <windows.h>
#include <windowsx.h>
#include <winuser.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>

using namespace std;

struct OverlayButton;
void CreateOverlayWindow(HINSTANCE h_instance, int cmd_show);
void InitializeTouchOverlay();
void SetupOverlayButtons();
void RenderTouchOverlay();
void CleanupTouchOverlay();
void HandleWindowPress(int x, int y, bool pressed);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void DrawSingleButton(OverlayButton& button, ID2D1RenderTarget* render_target, ID2D1SolidColorBrush* brush);
void DrawButtonsToCache();
bool IsTouchInside(OverlayButton& button, D2D1_POINT_2F& point);

// Fullscreen width when DDR is running
static const int kWindowRenderWidth = 1280;
// Fullscreen height when DDR is running
static const int kWindowRenderHeight = 720;
// Size of the buttons for menu navigation in the overlay
static const int kMenuNavButtonWidth = 50;
static const int kMenuNavButtonHeight = 50;
// Size of the buttons for the pinpad in the overlay
static const int kPinpadButtonWidth = 30;
static const int kPinpadButtonHeight = 30;

// Direct2D factory
extern ID2D1Factory* d2d_factory;
// Main render target for the overlay
extern ID2D1HwndRenderTarget* render_target;
// The offscreen, cached render target for the touch overlay. We use this for drawing
// the base state of the overlay a single time, then just render the cached target
// and draw the button presses on top of this, if necessary
extern ID2D1BitmapRenderTarget* cache_render_target;
// The Direct2D brush for drawing the base state of each button
extern ID2D1SolidColorBrush* brush_normal;
// The Direct2D brush for drawing the pressed state of each button
extern ID2D1SolidColorBrush* brush_pressed;
// A map of button IDs to their cached geometry objects, for faster redraws and the ability
// to use the geometry objects for bounds checking
extern map<int, ID2D1TransformedGeometry*> button_geometries;
