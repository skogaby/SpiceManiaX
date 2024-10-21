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

static uint16_t pad_input_states[2];

void perform_input_tasks(Connection& con);

// Main function for sending inputs to SpiceAPI, called once every millisecond.
void perform_input_tasks(Connection& con) {
    static string input_names[2][4] = {
        { "P1 Panel Up", "P1 Panel Down", "P1 Panel Left", "P1 Panel Right" },
        { "P2 Panel Up", "P2 Panel Down", "P2 Panel Left", "P2 Panel Right" },
    };
    static size_t panel_indices[4] = { 1, 7, 3, 5 };

    // Just send a SpiceAPI update with all our button values
    vector<ButtonState> button_states;
    for (size_t player = 0; player < 2; player++) {
        for (size_t panel = 0; panel < 4; panel++) {
            ButtonState state;
            state.name = input_names[player][panel];
            state.value = BIT(pad_input_states[player], panel_indices[panel]) == 1 ? 1.0f : 0.0f;
            button_states.push_back(state);
        }
    }

    buttons_write(con, button_states);
}

// The callback that's invoked by the StepManiaX SDK whenver the state changes on either
// of the connected pads (this is not implemented for the cabinet IO, yet)
static void smx_on_state_changed(int pad, SMXUpdateCallbackReason reason, void* pUser) {
    uint16_t state = SMXWrapper::getInstance().SMX_GetInputState(pad);
    pad_input_states[pad] = state;
}
