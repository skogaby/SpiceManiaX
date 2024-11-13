#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <map>
#include <vector>

// Says whether SpiceAPI is connected
extern bool is_connected;
// Reference to the main window, once it's created
extern HWND hwnd;
// Storage for the button objects we create for the touch overlay
extern std::vector<RECT> overlay_buttons;
// Mapping of button IDs to button press states
extern std::map<int, bool> overlay_button_states;