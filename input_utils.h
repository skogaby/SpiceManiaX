#pragma once

#include "smx/smx_wrapper.h"
#include "spiceapi/wrappers.h"
#include <array>
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

extern map<int, bool> overlay_button_states;

class InputUtils {
public:
    static void SMXStateChangedCallback(int pad, SMXUpdateCallbackReason reason, void* pUser);
    void perform_input_tasks(Connection& con);

private:
    void smx_on_state_changed(int pad);

    // Buffer to hold our input states
    array<uint16_t, 2> pad_input_states;

    // The input names that SpiceAPI expects for each panel
    static const string input_names[2][4];
    // The StepManiaX panel indices which correspond to the panel at the same index
    // in `input_names` above.
    static constexpr size_t panel_indices[4] = { 1, 7, 3, 5 };
};
