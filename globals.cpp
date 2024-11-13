#include "globals.h"

// Says whether SpiceAPI is connected
bool is_connected = false;
// Reference to the main , once it's created
HWND hwnd;
// Storage for the button objects we create for the touch overlay
std::vector<RECT> overlay_buttons;
// Mapping of button IDs to button press states
std::map<int, bool> overlay_button_states;
