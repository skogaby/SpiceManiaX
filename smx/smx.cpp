#include "smx.h"

using namespace std;

static HINSTANCE LIBSMX_INSTANCE = nullptr;
static string LIBSMX_NAME = "SMX.dll";

// SMX_Start API
typedef void (*SMX_Start_t)(SMXUpdateCallback UpdateCallback, void *pUser);
static SMX_Start_t pSMX_Start = nullptr;

// SMX_Stop API
typedef void (*SMX_Stop_t)();
static SMX_Stop_t pSMX_Stop = nullptr;

// SMX_SetLogCallback API
typedef void (*SMX_SetLogCallback_t)(SMXLogCallback callback);
static SMX_SetLogCallback_t pSMX_SetLogCallback = nullptr;

// SMX_GetInputState API
typedef uint16_t (*SMX_GetInputState_t)(int pad);
static SMX_GetInputState_t pSMX_GetInputState = nullptr;

// SMX_SetLights2 API
typedef void (*SMX_SetLights2_t)(const char *lightData, int lightDataSize);
static SMX_SetLights2_t pSMX_SetLights2 = nullptr;

// SMX_SetDedicatedCabinetLights API
typedef void (*SMX_SetDedicatedCabinetLights_t)(
    SMXDedicatedCabinetLights lightDevice, const char* lightData, int lightDataSize
);
static SMX_SetDedicatedCabinetLights_t pSMX_SetDedicatedCabinetLights = nullptr;

SMXWrapper::SMXWrapper() {
    // See if we even have the SMX.dll available.
    LIBSMX_INSTANCE = LoadLibraryA(LIBSMX_NAME.c_str());
    
    if (LIBSMX_INSTANCE != nullptr) {
        // Load functions
        pSMX_Start = (SMX_Start_t) GetProcAddress(LIBSMX_INSTANCE, "SMX_Start");
        pSMX_Stop = (SMX_Stop_t) GetProcAddress(LIBSMX_INSTANCE, "SMX_Stop");
        pSMX_SetLogCallback = (SMX_SetLogCallback_t) GetProcAddress(LIBSMX_INSTANCE, "SMX_SetLogCallback");
        pSMX_GetInputState = (SMX_GetInputState_t) GetProcAddress(LIBSMX_INSTANCE, "SMX_GetInputState");
        pSMX_SetLights2 = (SMX_SetLights2_t) GetProcAddress(LIBSMX_INSTANCE, "SMX_SetLights2");
        pSMX_SetDedicatedCabinetLights =
            (SMX_SetDedicatedCabinetLights_t) GetProcAddress(LIBSMX_INSTANCE, "SMX_SetDedicatedCabinetLights");

        // Make sure they actually loaded
        if (pSMX_Start == nullptr ||
            pSMX_Stop == nullptr || 
            pSMX_SetLogCallback == nullptr ||
            pSMX_GetInputState == nullptr ||
            pSMX_SetLights2 == nullptr ||
            pSMX_SetDedicatedCabinetLights == nullptr
        ) {
            printf("Unable to load external StepManiaX functions from SMX.dll\n");
        } else {
            loaded = true;
        }
    }
}

SMXWrapper::~SMXWrapper() {
    if (LIBSMX_INSTANCE != nullptr) {
        FreeLibrary(LIBSMX_INSTANCE);
    }
}

void SMXWrapper::SMX_Start(SMXUpdateCallback UpdateCallback, void* pUser) {
    if (pSMX_Start != nullptr) {
        pSMX_Start(UpdateCallback, pUser);
    }
}

void SMXWrapper::SMX_Stop() {
    if (pSMX_Stop != nullptr) {
        pSMX_Stop();
    }
}

void SMXWrapper::SMX_SetLogCallback(SMXLogCallback callback) {
    if (pSMX_SetLogCallback != nullptr) {
        pSMX_SetLogCallback(callback);
    }
}

uint16_t SMXWrapper::SMX_GetInputState(int pad) {
    if (pSMX_GetInputState != nullptr) {
        return pSMX_GetInputState(pad);
    }
}

void SMXWrapper::SMX_SetLights2(const char *lightData, int lightDataSize) {
    if (pSMX_SetLights2 != nullptr) {
        pSMX_SetLights2(lightData, lightDataSize);
    }
}

void SMXWrapper::SMX_SetDedicatedCabinetLights(SMXDedicatedCabinetLights lightDevice, const char *lightData, int lightDataSize) {
    if (pSMX_SetDedicatedCabinetLights != nullptr) {
        pSMX_SetDedicatedCabinetLights(lightDevice, lightData, lightDataSize);
    }
}
