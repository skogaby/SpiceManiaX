#pragma once

#include "globals.h"
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

// Macro for finding the `i`th bit in an integer, used for reading panel values from the SMX SDK stage states
#define BIT(value, i) (((value) >> (i)) & 1)

class InputUtils {
public:
    static void SMXStateChangedCallback(int pad, SMXUpdateCallbackReason reason, void* pUser);
    void PerformStageInputTasks(Connection& con);
    void PerformPinpadInputTasks(Connection& con);

private:
    void SmxOnStateChanged(int pad);

    // Buffer to hold our input states
    array<uint16_t, 2> pad_input_states_;

    // The input names that SpiceAPI expects for each panel
    static const string kStageInputNames[2][4];
    // The StepManiaX panel indices which correspond to the panel at the same index
    // in `input_names` above.
    static constexpr size_t kPanelIndices[4] = { 1, 7, 3, 5 };
};
