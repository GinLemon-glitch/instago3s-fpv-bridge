#pragma once

#include <Arduino.h>

#include "../camera/CameraBLE.h"
#include "../autorun/AutoRun.h"


// ============================================================
// LED STATUS
// ============================================================
//
// ESP32-C3 SuperMini
//
// Blue onboard LED:
//   GPIO8
//   LOW  = ON
//   HIGH = OFF
//
// Tutta la gestione è NON BLOCCANTE.
// Nessun delay() viene utilizzato.
//
// ============================================================

class LedStatus
{
public:

    void begin();

    void update(
        const CameraBLE &camera,
        const AutoRun &autorun
    );


private:

    // --------------------------------------------------------
    // HARDWARE
    // --------------------------------------------------------

    static constexpr uint8_t LED_PIN = 8;


    // --------------------------------------------------------
    // TIMING
    // --------------------------------------------------------

    unsigned long _patternStartedAt = 0;
    unsigned long _lastBootUpdate = 0;


    // --------------------------------------------------------
    // PATTERN STATE
    // --------------------------------------------------------

    const char *_lastPattern = nullptr;

    bool _bootFinished = false;

    bool _ledState = false;


    // --------------------------------------------------------
    // INTERNAL HELPERS
    // --------------------------------------------------------

    void setLed(
        bool on
    );

    void resetPattern(
        const char *pattern
    );

    void updateBoot();

    void updateManualPowerOff();

    void updateRecording();

    void updateWake();

    void updateGattOffline();

    void updateReady();

    void updateReconnect();

    void updateError();
};