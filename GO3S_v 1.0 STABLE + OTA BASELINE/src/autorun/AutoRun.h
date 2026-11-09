#pragma once

#include <Arduino.h>

#include "../camera/CameraBLE.h"
#include "../fc/FCInterface.h"
#include "../bridge/CameraBridge.h"


class AutoRun
{
public:

    void begin(
        CameraBLE &camera,
        FCInterface &fc,
        CameraBridge &bridge
    );

    void update();


    // --------------------------------------------------------
    // MANUAL CAMERA POWER CONTROL
    // --------------------------------------------------------

    void manualPowerToggle();

    void manualWake();

    bool isManualPowerOff() const;


    // --------------------------------------------------------
    // WEB UI
    // --------------------------------------------------------

    const char *getStateName() const;


private:

    enum State
    {
        BOOT,
        WAKE,
        WAIT_CONNECTION,
        CAMERA_AWAKE_GATT_OFFLINE,
        CONNECTION_SETTLE,
        READY,
        CAMERA_OFFLINE,
        RECONNECT_WAIT
    };


    State _state = BOOT;


    CameraBLE *_camera = nullptr;

    FCInterface *_fc = nullptr;

    CameraBridge *_bridge = nullptr;


    bool _lastArmed = false;


    // --------------------------------------------------------
    // MANUAL POWER STATE
    // --------------------------------------------------------

    bool _manualPowerOff = false;


    unsigned long _stateStartedAt = 0;

    unsigned long _lastReconnectAttempt = 0;

    unsigned long _lastWaitMessage = 0;


    // ========================================================
    // START VERIFICATION
    // ========================================================

    bool _startVerificationPending = false;

    unsigned long _startVerificationStartedAt = 0;


    static constexpr unsigned long START_VERIFY_TIMEOUT =
        5000UL;


    static constexpr unsigned long CE81_RECENT_TIMEOUT =
        15000UL;


    static constexpr unsigned long GATT_CONNECT_TIMEOUT =
        12000UL;


    static constexpr unsigned long GATT_WAIT_WHILE_CE81_ACTIVE =
        120000UL;


    static constexpr unsigned long CONNECTION_SETTLE_TIME =
        500UL;


    static constexpr unsigned long RECONNECT_DELAY =
        2000UL;


    // Debounce disarmo / decisione start con stato camera sconosciuto
static constexpr unsigned long DISARM_CONFIRM_TIME = 2000UL;
static constexpr unsigned long UNKNOWN_STATE_WAIT = 3000UL;

bool _disarmPending = false;
unsigned long _disarmPendingSince = 0;

bool _decisionPending = false;
unsigned long _decisionStartedAt = 0;
const char *_decisionReason = "";

unsigned long _startBaselinePacket = 0;

void decideStartRecording(const char *reason);
void serviceDecision();

void enterState(
        State newState
    );


    void handleArmState();

    void handleDisarmState();

    void handleShutterEvent();

    void synchronizeCameraWithAircraft();

    void startRecordingWithVerification(
        const char *reason
    );

    void verifyStartCommand();


    bool cameraReady() const;

    bool cameraCommunicating() const;
};