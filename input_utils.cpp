#include "input_utils.h"

const string InputUtils::kStageInputNames[2][4] = {
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

// Function for sending stage inputs and menu button inputs to SpiceAPI
void InputUtils::PerformMainInputTasks(Connection& con) {
    // Send a SpiceAPI update with all our button values
    vector<ButtonState> button_states;

    // Get the stage input values
    for (size_t player = 0; player < 2; player++) {
        for (size_t panel = 0; panel < 4; panel++) {
            ButtonState state;
            state.name = kStageInputNames[player][panel];
            state.value = (float) BIT(pad_input_states_[player], kPanelIndices[panel]);
            button_states.push_back(state);
        }
    }

    // Get the touch overlay input values
    for (OverlayButton& button : touch_overlay_buttons) {
        if (button.type_ == OverlayButtonType::MENU) {
            ButtonState state;
            state.name = button.input_name_;
            state.value = (float)touch_overlay_button_states[button.id_];
            button_states.push_back(state);
        }
    }

    // Send the regular button updates + stage updates
    buttons_write(con, button_states);
}

// Function for sending pinpad inputs to SpiceAPI
void InputUtils::PerformPinpadInputTasks(Connection& con) {
    string key_strs[2] = { "", "" };

    // Get the touch overlay input values
    for (OverlayButton& button : touch_overlay_buttons) {
        if (button.type_ == OverlayButtonType::PINPAD && touch_overlay_button_states[button.id_]) {
            char label;
            int player = 0;

            // Find the right key label to send to SpiceAPI
            if (button.label_ == "") {
                label = 'D';
            } else if (button.label_ == "00") {
                label = 'A';
            } else {
                label = button.label_.c_str()[0];
            }

             key_strs[button.player_] += label;
        }
    }

    // Handle the pinpad updates
    for (int player = 0; player < 2; player++) {
        vector<char> keys;
        for (auto& c : key_strs[player]) {
            keys.push_back(c);
        }

        keypads_set(con, player, keys);
    }
}
