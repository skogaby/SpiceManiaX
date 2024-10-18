#include "spiceapi/connection.h"
#include "spiceapi/wrappers.h"
#include "smx/smx.h"
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")

using namespace spiceapi;

// Logging callback for the StepManiaX SDK
static void smxLogCallback(const char* log) {
    printf("[stepmaniax-sdk] -> %s\n", log);
}

// The callback that's invoked by the StepManiaX SDK whenver the state changes on either
// of the connected pads (this is not implemented for the cabinet IO, yet)
static void smxStateChangedCallback(int pad, SMXUpdateCallbackReason reason, void* pUser) {
    printf("Device %i state changed: %04x\n", pad, SMXWrapper::getInstance().SMX_GetInputState(pad));
}

int main() {
    // First, go ahead and start up the StepManiaX SDK, which should connect to devices
    // as they become available automatically
    SMXWrapper::getInstance().SMX_SetLogCallback(smxLogCallback);
    SMXWrapper::getInstance().SMX_Start(smxStateChangedCallback, nullptr);

    if (!SMXWrapper::getInstance().loaded) {
        printf("Unable to load StepManiaX SDK, exiting");
        return -1;
    }

    printf("Loaded SMX.dll successfully, attempting to connect to SpiceAPI now\n");

    // TODO: Remove hardcoded credentials, port, etc.
    // Connect to SpiceAPI
    Connection con("localhost", 1337, "lol");

    if (!con.check()) {
        printf("Unable to connect to SpiceAPI, exiting\n");
        SMXWrapper::getInstance().SMX_Stop();

        return -1;
    }

    printf("Connected to SpiceAPI successfully\n");

    while (1) {
        printf("Getting lights\n");
        std::vector<LightState> lights;
        lights_read(con, lights);

        for (const LightState& l : lights) {
            printf("%s: %f\n", l.name.c_str(), l.value);
        }

        Sleep(100);
    }

    SMXWrapper::getInstance().SMX_Stop();
    return 0;
}
