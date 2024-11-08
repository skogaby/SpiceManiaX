#pragma once

#include "smx/smx_wrapper.h"
#include "spiceapi/wrappers.h"
#include <string>

using namespace spiceapi;
using namespace std;

// Constants for the panel indices on each pad, these are defined by the StepManiaX SDK
// left-to-right, top-to-bottom
#define UP_LEFT 0
#define UP 1
#define UP_RIGHT 2
#define LEFT 3
#define CENTER 4
#define RIGHT 5
#define DOWN_LEFT 6
#define DOWN 7
#define DOWN_RIGHT 8

#define BIT(value, i) (((value) >> (i)) & 1)

extern Connection* con;
// Buffer to hold our input states
static uint16_t pad_input_states[2];
// The input names that SpiceAPI expects for each panel
static string input_names[2][4] = {
        { "P1 Panel Up", "P1 Panel Down", "P1 Panel Left", "P1 Panel Right" },
        { "P2 Panel Up", "P2 Panel Down", "P2 Panel Left", "P2 Panel Right" },
};
// The StepManiaX panel indices which correspond to the panel at the same index
// in `input_names` above.
static size_t panel_indices[4] = { 1, 7, 3, 5 };

// The callback that's invoked by the StepManiaX SDK whenver the state changes on either
// of the connected pads (this is not implemented for the cabinet IO, yet)
static void smx_on_state_changed(int pad, SMXUpdateCallbackReason reason, void* pUser) {
    // Get the input state (for some reason the callback does not include it as a parameter...)
    /* pad_input_states[pad] = SMXWrapper::getInstance().SMX_GetInputState(pad);

    // Send a SpiceAPI update with all our button values
    vector<ButtonState> button_states;
    for (size_t player = 0; player < 2; player++) {
        for (size_t panel = 0; panel < 4; panel++) {
            ButtonState state;
            state.name = input_names[player][panel];
            state.value = BIT(pad_input_states[player], panel_indices[panel]) == 1 ? 1.0f : 0.0f;
            button_states.push_back(state);
        }
    }

    buttons_write(*con, button_states); */
}
