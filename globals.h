#pragma once

#define WIN32_LEAN_AND_MEAN

#include "overlay_button.h"
#include <windows.h>
#include <map>
#include <vector>

// Reference to the main window, once it's created
extern HWND hwnd;
// Storage for the new set of buttons, which include a lot more metadata for rendering
extern std::vector<OverlayButton> touch_overlay_buttons;
// Mapping of button IDs to button press states
extern std::map<int, bool> touch_overlay_button_states;
