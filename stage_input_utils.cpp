#include "stage_input_utils.h"

const string StageInputUtils::input_names[2][4] = {
    { "P1 Panel Up", "P1 Panel Down", "P1 Panel Left", "P1 Panel Right" },
    { "P2 Panel Up", "P2 Panel Down", "P2 Panel Left", "P2 Panel Right" },
};

// This is the callback that's actually registered with the StepManiaX SDK. This is separate from the "real"
// callback with our internal logic, due to how the callback has to be registered and because we store the input
// as a member variable.
void StageInputUtils::SMXStateChangedCallback(int pad, SMXUpdateCallbackReason reason, void* pUser) {
    StageInputUtils* pSelf = (StageInputUtils*)pUser;
    pSelf->smx_on_state_changed(pad);
}

void StageInputUtils::smx_on_state_changed(int pad) {
    // Get the input state (for some reason the callback does not include it as a parameter...)
    pad_input_states[pad] = SMXWrapper::getInstance().SMX_GetInputState(pad);
}

// Main function for sending inputs to SpiceAPI
void StageInputUtils::perform_stage_input_tasks(Connection& con) {
    // Send a SpiceAPI update with all our button values
    vector<ButtonState> button_states;
    for (size_t player = 0; player < 2; player++) {
        for (size_t panel = 0; panel < 4; panel++) {
            ButtonState state;
            state.name = input_names[player][panel];
            state.value = (float) BIT(pad_input_states[player], panel_indices[panel]);
            button_states.push_back(state);
        }
    }

    buttons_write(con, button_states);
}
