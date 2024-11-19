#pragma once

#include "overlay_button.h"
#include "globals.h"

#include <d2d1.h>
#include <dwrite.h>
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
void DrawSingleButton(OverlayButton& button, ID2D1RenderTarget* render_target, bool is_pressed);
void DrawButtonsToCaches();
bool IsTouchInside(OverlayButton& button, D2D1_POINT_2F& point);

// Size of the window when DDR is running in fullscreen, we'll match our render size
static constexpr int kWindowRenderWidth = 1280;
static constexpr int kWindowRenderHeight = 720;
// Size of the buttons for menu navigation in the overlay
static constexpr int kMenuNavButtonWidth = 50;
static constexpr int kMenuNavButtonHeight = 50;
// Size of the buttons for the pinpad in the overlay
static constexpr int kPinpadButtonWidth = 30;
static constexpr int kPinpadButtonHeight = 30;
// Size of the buttons for carding in and hiding the overlay
static constexpr int kUtilityButtonWidth = 120;
static constexpr int kUtilityButtonHeight = 30;


// Direct2D factory for brushes and render targets
extern ID2D1Factory* d2d_factory;
// Direct2D factory for creating text objects
extern IDWriteFactory* d2d_write_factory;
// The text formatter object for Direct2D text rendering
extern IDWriteTextFormat* d2d_text_format;
// Main render target for the overlay
extern ID2D1HwndRenderTarget* render_target;
// Cached render targets for our overlay in various states of visibility
extern ID2D1BitmapRenderTarget* cached_render_targets[2][2];
// The Direct2D brushes for drawing our overlay buttons in various states
extern ID2D1SolidColorBrush* brush_menu_fill;
extern ID2D1SolidColorBrush* brush_pinpad_fill;
extern ID2D1SolidColorBrush* brush_button_outline_normal;
extern ID2D1SolidColorBrush* brush_button_outline_pressed;
// A map of button IDs to their cached geometry objects, for faster redraws and the ability
// to use the geometry objects for bounds checking
extern map<int, ID2D1TransformedGeometry*> button_geometries;
