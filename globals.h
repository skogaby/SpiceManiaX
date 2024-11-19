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
// Says whether the overlay is currently being shown or not for each player
extern bool is_overlay_visible[2];
// The card IDs to use for each player, if available
extern std::string card_ids[2];
// The opacity value to use for the overlay, between 0.0 and 1.0
extern float overlay_opacity;
