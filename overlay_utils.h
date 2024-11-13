#pragma once

#include "globals.h"

#include <d2d1.h>
#include <windows.h>
#include <winuser.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>

using namespace std;

void CALLBACK RedrawTimerCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
void CALLBACK WindowPosTimerCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
void InitializeTouchOverlay();
void CleanupTouchOverlay();
void DrawButtons();
void DrawButtonsToCache();

// Fullscreen width when DDR is running
const int kDdrScreenWidth = 1280;
// Fullscreen height when DDR is running
const int kDdrScreenHeight = 720;
// TODO: Remove these
const int kButtonWidth = 100;
const int kButtonHeight = 50;

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
