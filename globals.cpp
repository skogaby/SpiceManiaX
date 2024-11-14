#include "globals.h"

// Reference to the main window, once it's created
HWND hwnd;
// Storage for the button objects we create for the touch overlay
std::vector<RECT> overlay_buttons;
// Storage for the new set of buttons, which include a lot more metadata for rendering
std::vector<OverlayButton> touch_overlay_buttons;
// Mapping of button IDs to button press states
std::map<int, bool> touch_overlay_button_states;
