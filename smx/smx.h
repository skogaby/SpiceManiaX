#pragma once

#include <thread>
#include <Windows.h>
#include <setupapi.h>
#include <string>
#include <stdio.h>

enum SMXUpdateCallbackReason {
    SMXUpdateCallback_Updated,
    SMXUpdateCallback_FactoryResetCommandComplete
};

enum SMXDedicatedCabinetLights {
    MARQUEE = 0,
    LEFT_STRIP = 1,
    LEFT_SPOTLIGHTS = 2,
    RIGHT_STRIP = 3,
    RIGHT_SPOTLIGHTS = 4
};

typedef void SMXUpdateCallback(int pad, SMXUpdateCallbackReason reason, void *pUser);
typedef void SMXLogCallback(const char* log);

/**
 * Wrapper for the StepManiaX SDK DLL, so we can use its functionality without having to import the source
 * of the SDK wholesale.
 */
class SMXWrapper {
public:
    static SMXWrapper& getInstance() {
        static SMXWrapper instance;
        return instance;
    }
    ~SMXWrapper();
    void SMX_SetLogCallback(SMXLogCallback callback);
    uint16_t SMX_GetInputState(int pad);
    void SMX_Start(SMXUpdateCallback UpdateCallback, void *pUser);
    void SMX_SetLights2(const char *lightData, int lightDataSize);
    void SMX_SetDedicatedCabinetLights(SMXDedicatedCabinetLights lightDevice, const char* lightData, int lightDataSize);

    bool loaded = false;
private:
    SMXWrapper();
};
