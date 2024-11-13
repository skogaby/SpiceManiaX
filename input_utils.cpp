#include "input_utils.h"

const string InputUtils::kInputNames[2][4] = {
    { "P1 Panel Up", "P1 Panel Down", "P1 Panel Left", "P1 Panel Right" },
    { "P2 Panel Up", "P2 Panel Down", "P2 Panel Left", "P2 Panel Right" },
};

// This is the callback that's actually registered with the StepManiaX SDK. This is separate from the "real"
// callback with our internal logic, due to how the callback has to be registered and because we store the input
// as a member variable.
void InputUtils::SMXStateChangedCallback(int pad, SMXUpdateCallbackReason reason, void* pUser) {
    InputUtils* pSelf = (InputUtils*) pUser;
    pSelf->SmxOnStateChanged(pad);
}

void InputUtils::SmxOnStateChanged(int pad) {
    // Get the input state (for some reason the callback does not include it as a parameter...)
    pad_input_states_[pad] = SMXWrapper::getInstance().SMX_GetInputState(pad);
}

// Main function for sending inputs to SpiceAPI
void InputUtils::PerformInputTasks(Connection& con) {
    // Send a SpiceAPI update with all our button values
    vector<ButtonState> button_states;

    // Get the stage input values
    for (size_t player = 0; player < 2; player++) {
        for (size_t panel = 0; panel < 4; panel++) {
            ButtonState state;
            state.name = kInputNames[player][panel];
            state.value = (float) BIT(pad_input_states_[player], kPanelIndices[panel]);
            button_states.push_back(state);
        }
    }

    // Get the touch overlay input values
    // TODO: Refactor this once the overlay is better
    static string names[2] = { "Service", "Test" };
    for (int button = 0; button < 2; button++) {
        ButtonState state;
        state.name = names[button];
        state.value = overlay_button_states[button] ? 1.0f : 0.0f;
        button_states.push_back(state);
    } 

    buttons_write(con, button_states);
}
