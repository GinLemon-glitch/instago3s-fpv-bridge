#include "AutoRun.h"


// ============================================================
// BEGIN
// ============================================================

void AutoRun::begin(
    CameraBLE &camera,
    FCInterface &fc,
    CameraBridge &bridge
)
{
    _camera = &camera;
    _fc = &fc;
    _bridge = &bridge;

    _state = BOOT;

    _lastArmed = false;

    _manualPowerOff = false;

    _stateStartedAt = millis();
    _lastReconnectAttempt = 0;
    _lastWaitMessage = 0;

    _startVerificationPending = false;
    _startVerificationStartedAt = 0;


    Serial.println();
    Serial.println("[AUTORUN] ========================================");
    Serial.println("[AUTORUN] AutoRun initialized");
    Serial.println("[AUTORUN] ========================================");
    Serial.println();

    Serial.println(
        "[AUTORUN] Automatic startup sequence enabled"
    );


    Serial.println(
        "[AUTORUN] Starting automatic camera wake..."
    );

    Serial.println();


    _camera->wake();


    Serial.println();

    Serial.println(
        "[AUTORUN] Wake advertising sequence finished"
    );

    Serial.println(
        "[AUTORUN] Waiting for GATT + CE82 command channel..."
    );

    Serial.println();


    enterState(
        WAIT_CONNECTION
    );
}


// ============================================================
// UPDATE
// ============================================================

void AutoRun::update()
{
    if (!_camera || !_fc || !_bridge)
        return;


    // ========================================================
    // MANUAL POWER-OFF LOCK
    // ========================================================

    if (_manualPowerOff)
    {
        if (_bridge->isShutterEvent())
        {
            Serial.println(
                "[AUTORUN] RC SHUTTER ignored: manual camera power-off lock active"
            );
        }


        return;
    }


    const unsigned long now =
        millis();


    // ========================================================
    // START VERIFICATION
    // ========================================================

    if (
        _startVerificationPending &&
        _state == READY
    )
    {
        verifyStartCommand();
    }


    switch (_state)
    {
        // ====================================================
        // BOOT
        // ====================================================

        case BOOT:
        {
            if (now - _stateStartedAt < 500)
                return;


            Serial.println();

            Serial.println(
                "[AUTORUN] BOOT"
            );

            Serial.println(
                "[AUTORUN] Starting automatic camera wake..."
            );

            Serial.println();


            enterState(
                WAKE
            );

            break;
        }


        // ====================================================
        // WAKE
        // ====================================================

        case WAKE:
        {
            Serial.println(
                "[AUTORUN] WAKE"
            );

            Serial.println(
                "[AUTORUN] Sending verified GO 3S wake beacon..."
            );

            Serial.println();


            _camera->wake();


            Serial.println();

            Serial.println(
                "[AUTORUN] Wake advertising sequence finished"
            );

            Serial.println(
                "[AUTORUN] Waiting for command channel..."
            );

            Serial.println();


            enterState(
                WAIT_CONNECTION
            );

            break;
        }


        // ====================================================
        // WAIT CONNECTION
        // ====================================================

        case WAIT_CONNECTION:
        {
            if (_camera->isCommandChannelReady())
            {
                Serial.println();

                Serial.println(
                    "========================================"
                );

                Serial.println(
                    "[AUTORUN] GATT + CE82 COMMAND CHANNEL READY"
                );

                Serial.println(
                    "[AUTORUN] Waiting for BLE stack to settle..."
                );

                Serial.println(
                    "========================================"
                );

                Serial.println();


                enterState(
                    CONNECTION_SETTLE
                );


                break;
            }


            const bool ce81Active =
                cameraCommunicating();


            if (ce81Active)
            {
                Serial.println();

                Serial.println(
                    "[AUTORUN] CAMERA_AWAKE_GATT_OFFLINE"
                );

                Serial.println(
                    "[AUTORUN] CE81 telemetry is active"
                );


                if (_camera->isConnected())
                {
                    Serial.println(
                        "[AUTORUN] GATT connected but CE82 command channel is not subscribed"
                    );
                }
                else
                {
                    Serial.println(
                        "[AUTORUN] GATT connection is not present"
                    );
                }


                Serial.println(
                    "[AUTORUN] Camera state remains readable"
                );

                Serial.println(
                    "[AUTORUN] Camera is NOT commandable yet"
                );

                Serial.println();


                enterState(
                    CAMERA_AWAKE_GATT_OFFLINE
                );


                break;
            }


            if (
                now - _lastWaitMessage >=
                2000
            )
            {
                _lastWaitMessage = now;


                Serial.println(
                    "[AUTORUN] No recent CE81 telemetry"
                );

                Serial.println(
                    "[AUTORUN] Waiting for GATT + CE82 command channel..."
                );
            }


            if (
                now - _stateStartedAt >=
                GATT_CONNECT_TIMEOUT
            )
            {
                Serial.println();

                Serial.println(
                    "[AUTORUN] GATT/telemetry connection timeout"
                );

                Serial.println(
                    "[AUTORUN] Camera may be offline"
                );

                Serial.println(
                    "[AUTORUN] Preparing automatic reconnect..."
                );

                Serial.println();


                enterState(
                    RECONNECT_WAIT
                );
            }

            break;
        }


        // ====================================================
        // CAMERA AWAKE / GATT OFFLINE
        // ====================================================

        case CAMERA_AWAKE_GATT_OFFLINE:
        {
            const bool ce81Active =
                cameraCommunicating();


            if (_camera->isCommandChannelReady())
            {
                Serial.println();

                Serial.println(
                    "========================================"
                );

                Serial.println(
                    "[AUTORUN] COMMAND CHANNEL READY"
                );

                Serial.println(
                    "[AUTORUN] CE81 telemetry + GATT + CE82 subscription confirmed"
                );

                Serial.println(
                    "========================================"
                );

                Serial.println();


                enterState(
                    CONNECTION_SETTLE
                );


                break;
            }


            if (ce81Active)
            {
                if (
                    now - _lastWaitMessage >=
                    1000
                )
                {
                    _lastWaitMessage = now;


                    Serial.println(
                        "[AUTORUN] Camera awake - CE81 active"
                    );


                    if (_camera->isConnected())
                    {
                        Serial.println(
                            "[AUTORUN] GATT connected"
                        );

                        Serial.println(
                            "[AUTORUN] CE82 command channel NOT subscribed"
                        );
                    }
                    else
                    {
                        Serial.println(
                            "[AUTORUN] GATT offline"
                        );

                        Serial.println(
                            "[AUTORUN] Waiting for GATT connection..."
                        );
                    }


                    Serial.println(
                        "[AUTORUN] Recording telemetry remains available"
                    );

                    Serial.println(
                        "[AUTORUN] Commands remain blocked until CE82 is subscribed"
                    );
                }


                if (
                    now - _stateStartedAt >=
                    GATT_WAIT_WHILE_CE81_ACTIVE
                )
                {
                    Serial.println();

                    Serial.println(
                        "[AUTORUN] Extended command-channel wait expired"
                    );

                    Serial.println(
                        "[AUTORUN] CE81 is active but command channel is still unavailable"
                    );

                    Serial.println(
                        "[AUTORUN] Preparing automatic reconnect..."
                    );

                    Serial.println();


                    enterState(
                        RECONNECT_WAIT
                    );
                }


                break;
            }


            if (
                now - _lastWaitMessage >=
                2000
            )
            {
                _lastWaitMessage = now;


                Serial.println(
                    "[AUTORUN] CE81 telemetry timeout"
                );

                Serial.println(
                    "[AUTORUN] Camera may no longer be awake"
                );
            }


            if (
                now - _stateStartedAt >=
                GATT_CONNECT_TIMEOUT
            )
            {
                Serial.println();

                Serial.println(
                    "[AUTORUN] Camera awake/command wait ended without recent CE81"
                );

                Serial.println(
                    "[AUTORUN] Preparing automatic reconnect..."
                );

                Serial.println();


                enterState(
                    RECONNECT_WAIT
                );
            }


            break;
        }


        // ====================================================
        // CONNECTION SETTLE
        // ====================================================

        case CONNECTION_SETTLE:
        {
            if (!_camera->isCommandChannelReady())
            {
                if (cameraCommunicating())
                {
                    Serial.println(
                        "[AUTORUN] GATT present but CE82 command channel is not ready"
                    );


                    enterState(
                        CAMERA_AWAKE_GATT_OFFLINE
                    );
                }
                else
                {
                    Serial.println(
                        "[AUTORUN] Command channel disappeared during settle"
                    );


                    enterState(
                        CAMERA_OFFLINE
                    );
                }


                break;
            }


            if (
                now - _stateStartedAt <
                CONNECTION_SETTLE_TIME
            )
            {
                break;
            }


            Serial.println();

            Serial.println(
                "========================================"
            );

            Serial.println(
                "[AUTORUN] CAMERA READY"
            );

            Serial.println(
                "[AUTORUN] GATT connected"
            );

            Serial.println(
                "[AUTORUN] CE82 command channel subscribed"
            );


            if (cameraCommunicating())
            {
                Serial.println(
                    "[AUTORUN] CE81 telemetry active"
                );
            }
            else
            {
                Serial.println(
                    "[AUTORUN] Waiting for CE81 telemetry"
                );
            }


            Serial.println(
                "========================================"
            );

            Serial.println();


            enterState(
                READY
            );


            synchronizeCameraWithAircraft();


            break;
        }


        // ====================================================
        // READY
        // ====================================================

        case READY:
        {
            if (!_camera->isCommandChannelReady())
            {
                Serial.println();

                Serial.println(
                    "========================================"
                );

                Serial.println(
                    "[AUTORUN] COMMAND CHANNEL LOST"
                );

                Serial.println(
                    "[AUTORUN] Camera telemetry and commandability are now separated"
                );

                Serial.println(
                    "========================================"
                );

                Serial.println();


                if (cameraCommunicating())
                {
                    enterState(
                        CAMERA_AWAKE_GATT_OFFLINE
                    );
                }
                else
                {
                    enterState(
                        CAMERA_OFFLINE
                    );
                }


                break;
            }


            handleShutterEvent();


            handleArmState();


            serviceDecision();


            break;
        }


        // ====================================================
        // CAMERA OFFLINE
        // ====================================================

        case CAMERA_OFFLINE:
        {
            if (_camera->isCommandChannelReady())
            {
                Serial.println();

                Serial.println(
                    "[AUTORUN] GATT + CE82 command channel restored"
                );

                Serial.println();


                enterState(
                    CONNECTION_SETTLE
                );


                break;
            }


            if (cameraCommunicating())
            {
                Serial.println(
                    "[AUTORUN] Camera telemetry restored"
                );

                Serial.println(
                    "[AUTORUN] Moving to CAMERA_AWAKE_GATT_OFFLINE"
                );


                enterState(
                    CAMERA_AWAKE_GATT_OFFLINE
                );


                break;
            }


            if (
                now - _stateStartedAt <
                RECONNECT_DELAY
            )
            {
                break;
            }


            Serial.println(
                "[AUTORUN] Camera offline"
            );

            Serial.println(
                "[AUTORUN] Preparing automatic reconnect..."
            );

            Serial.println();


            enterState(
                RECONNECT_WAIT
            );


            break;
        }


        // ====================================================
        // RECONNECT WAIT
        // ====================================================

        case RECONNECT_WAIT:
        {
            if (_camera->isCommandChannelReady())
            {
                Serial.println();

                Serial.println(
                    "========================================"
                );

                Serial.println(
                    "[AUTORUN] CAMERA RECONNECTED"
                );

                Serial.println(
                    "[AUTORUN] GATT + CE82 command channel confirmed"
                );

                Serial.println(
                    "========================================"
                );

                Serial.println();


                enterState(
                    CONNECTION_SETTLE
                );


                break;
            }


            if (cameraCommunicating())
            {
                if (
                    now - _lastWaitMessage >=
                    1000
                )
                {
                    _lastWaitMessage = now;


                    Serial.println(
                        "[AUTORUN] Camera awake - CE81 active"
                    );


                    if (_camera->isConnected())
                    {
                        Serial.println(
                            "[AUTORUN] GATT connected"
                        );

                        Serial.println(
                            "[AUTORUN] Waiting for CE82 subscription..."
                        );
                    }
                    else
                    {
                        Serial.println(
                            "[AUTORUN] GATT offline"
                        );

                        Serial.println(
                            "[AUTORUN] Waiting for command channel..."
                        );
                    }
                }


                break;
            }


            if (
                now - _stateStartedAt <
                RECONNECT_DELAY
            )
            {
                break;
            }


            if (
                now - _lastReconnectAttempt <
                RECONNECT_DELAY
            )
            {
                break;
            }


            _lastReconnectAttempt = now;


            Serial.println();

            Serial.println(
                "========================================"
            );

            Serial.println(
                "[AUTORUN] AUTOMATIC RECONNECT"
            );

            Serial.println(
                "[AUTORUN] Sending Wake-on..."
            );

            Serial.println(
                "========================================"
            );

            Serial.println();


            _camera->wake();


            Serial.println();

            Serial.println(
                "[AUTORUN] Wake advertising sequence finished"
            );

            Serial.println(
                "[AUTORUN] Waiting for telemetry + GATT + CE82..."
            );

            Serial.println();


            enterState(
                WAIT_CONNECTION
            );


            break;
        }
    }
}


// ============================================================
// MANUAL POWER TOGGLE
// ============================================================

void AutoRun::manualPowerToggle()
{
    if (!_camera)
        return;


    if (_manualPowerOff)
    {
        Serial.println();

        Serial.println(
            "========================================"
        );

        Serial.println(
            "[AUTORUN] MANUAL POWER/WAKE"
        );

        Serial.println(
            "[AUTORUN] Camera is manually OFF"
        );

        Serial.println(
            "[AUTORUN] Sending Wake-on..."
        );

        Serial.println(
            "========================================"
        );

        Serial.println();


        _manualPowerOff = false;


        _camera->wake();


        Serial.println();

        Serial.println(
            "[AUTORUN] Manual Wake completed"
        );

        Serial.println(
            "[AUTORUN] Automatic camera management restored"
        );

        Serial.println();


        enterState(
            WAIT_CONNECTION
        );


        return;
    }


    const bool cameraAwake =
        _camera->isCommandChannelReady() ||
        cameraCommunicating();


    if (cameraAwake)
    {
        Serial.println();

        Serial.println(
            "========================================"
        );

        Serial.println(
            "[AUTORUN] MANUAL POWER/WAKE"
        );

        Serial.println(
            "[AUTORUN] Camera appears ON"
        );

        Serial.println(
            "[AUTORUN] Sending POWER OFF..."
        );

        Serial.println(
            "========================================"
        );

        Serial.println();


        _startVerificationPending = false;


        _lastArmed =
            _fc
                ? _fc->isArmed()
                : false;


        _camera->powerOff();


        _manualPowerOff = true;


        Serial.println();

        Serial.println(
            "[AUTORUN] Camera POWER OFF command sent"
        );

        Serial.println(
            "[AUTORUN] AutoRun automatic Wake/reconnect is LOCKED"
        );

        Serial.println(
            "[AUTORUN] Press POWER/WAKE again to wake the camera"
        );

        Serial.println();


        return;
    }


    Serial.println();

    Serial.println(
        "========================================"
    );

    Serial.println(
        "[AUTORUN] MANUAL POWER/WAKE"
    );

    Serial.println(
        "[AUTORUN] Camera appears OFFLINE/OFF"
    );

    Serial.println(
        "[AUTORUN] Sending Wake-on..."
    );

    Serial.println(
        "========================================"
    );

    Serial.println();


    _manualPowerOff = false;


    _camera->wake();


    Serial.println();

    Serial.println(
        "[AUTORUN] Manual Wake completed"
    );

    Serial.println(
        "[AUTORUN] Automatic camera management restored"
    );

    Serial.println();


    enterState(
        WAIT_CONNECTION
    );
}


// ============================================================
// MANUAL WAKE
// ============================================================

void AutoRun::manualWake()
{
    if (!_camera)
        return;


    Serial.println();

    Serial.println(
        "========================================"
    );

    Serial.println(
        "[AUTORUN] MANUAL WAKE"
    );

    Serial.println(
        "[AUTORUN] Clearing manual power-off lock"
    );

    Serial.println(
        "[AUTORUN] Sending Wake-on..."
    );

    Serial.println(
        "========================================"
    );

    Serial.println();


    _manualPowerOff = false;


    _camera->wake();


    Serial.println();

    Serial.println(
        "[AUTORUN] Manual Wake completed"
    );

    Serial.println(
        "[AUTORUN] Automatic camera management restored"
    );

    Serial.println();


    enterState(
        WAIT_CONNECTION
    );
}


// ============================================================
// MANUAL POWER-OFF STATE
// ============================================================

bool AutoRun::isManualPowerOff() const
{
    return _manualPowerOff;
}


// ============================================================
// CAMERA COMMUNICATION
// ============================================================

bool AutoRun::cameraCommunicating() const
{
    if (!_camera)
        return false;


    return _camera->hasRecentPacket(
        CE81_RECENT_TIMEOUT
    );
}


// ============================================================
// CAMERA READY
// ============================================================

bool AutoRun::cameraReady() const
{
    if (!_camera)
        return false;


    return _camera->isCommandChannelReady();
}


// ============================================================
// ENTER STATE
// ============================================================

void AutoRun::enterState(
    State newState
)
{
    _state = newState;

    _stateStartedAt = millis();

    _lastWaitMessage = 0;
}


// ============================================================
// RC SHUTTER EVENT
// ============================================================

void AutoRun::handleShutterEvent()
{
    if (!_camera || !_fc || !_bridge)
        return;


    if (!_bridge->isShutterEvent())
        return;


    Serial.println();

    Serial.println(
        "========================================"
    );

    Serial.println(
        "[AUTORUN] RC SHUTTER EVENT"
    );

    Serial.println(
        "[AUTORUN] Camera command channel READY"
    );

    Serial.println(
        "[AUTORUN] Sending CAMERA SHUTTER"
    );

    Serial.println(
        "========================================"
    );

    Serial.println();


    _startVerificationPending = false;


    _camera->shutter();


    Serial.println(
        "[AUTORUN] RC SHUTTER command sent."
    );

    Serial.println();
}


// ============================================================
// ARM / DISARM STATE
// ============================================================

void AutoRun::handleArmState()
{
    if (!_camera || !_fc)
        return;

    // Link FC perso / bit ARM ignoto: NON e' un disarmo, non fare nulla.
    if (!_fc->isArmKnown())
    {
        _disarmPending = false;
        return;
    }

    const bool armed = _fc->isArmed();
    const unsigned long now = millis();

    if (armed)
    {
        _disarmPending = false;

        if (!_lastArmed)
        {
            _lastArmed = true;
            Serial.println("[AUTORUN] AIRCRAFT -> ARMED");
            decideStartRecording("ARM event");
        }
        return;
    }

    if (_lastArmed)
    {
        if (!_disarmPending)
        {
            _disarmPending = true;
            _disarmPendingSince = now;
            return;
        }

        if (now - _disarmPendingSince >= DISARM_CONFIRM_TIME)
        {
            _disarmPending = false;
            _lastArmed = false;
            _decisionPending = false;
            handleDisarmState();
        }
    }
}

void AutoRun::decideStartRecording(const char *reason)
{
    if (!_camera)
        return;

    if (!_camera->isCommandChannelReady())
    {
        Serial.println("[AUTORUN] Cannot START: CE82 command channel not ready.");
        return;
    }

    if (_camera->isRecordingKnown())
    {
        if (_camera->isRecording())
        {
            Serial.println("[AUTORUN] Camera already recording - no command.");
            return;
        }

        startRecordingWithVerification(reason);
        return;
    }

    // Stato ignoto: aspetta CE81 prima di inviare uno shutter (toggle!)
    Serial.println("[AUTORUN] Camera state UNKNOWN - waiting for CE81...");
    _decisionPending = true;
    _decisionStartedAt = millis();
    _decisionReason = reason;
}

void AutoRun::serviceDecision()
{
    if (!_decisionPending)
        return;

    if (!_camera || !_fc || !_fc->isArmKnown() || !_fc->isArmed())
    {
        _decisionPending = false;
        return;
    }

    if (_camera->isRecordingKnown())
    {
        _decisionPending = false;

        if (_camera->isRecording())
            Serial.println("[AUTORUN] CE81 received: already recording - no command.");
        else
            startRecordingWithVerification(_decisionReason);
        return;
    }

    if (millis() - _decisionStartedAt >= UNKNOWN_STATE_WAIT)
    {
        _decisionPending = false;
        Serial.println("[AUTORUN] Camera state still UNKNOWN - sending START once.");
        startRecordingWithVerification("state UNKNOWN after wait");
    }
}

// ============================================================
// DISARM
// ============================================================

void AutoRun::handleDisarmState()
{
    Serial.println();

    Serial.println(
        "========================================"
    );

    Serial.println(
        "[AUTORUN] AIRCRAFT -> DISARMED"
    );

    Serial.println(
        "[AUTORUN] New camera decision pending."
    );

    Serial.println(
        "========================================"
    );

    Serial.println();


    _startVerificationPending = false;


    if (_camera->isRecordingKnown())
    {
        if (_camera->isRecording())
        {
            Serial.println(
                "[AUTORUN] Camera:   RECORDING"
            );

            Serial.println(
                "[AUTORUN] Camera still recording after DISARM."
            );

            Serial.println(
                "[AUTORUN] Sending STOP recording..."
            );


            _camera->shutter();


            Serial.println(
                "[AUTORUN] STOP command sent."
            );

            Serial.println(
                "[AUTORUN] Decision completed."
            );
        }
        else
        {
            Serial.println(
                "[AUTORUN] Camera:   STOPPED"
            );

            Serial.println(
                "[AUTORUN] Camera already stopped."
            );

            Serial.println(
                "[AUTORUN] No command required."
            );
        }


        return;
    }


    Serial.println(
        "[AUTORUN] Camera:   UNKNOWN"
    );

    Serial.println(
        "[AUTORUN] Camera recording state UNKNOWN."
    );

    Serial.println(
        "[AUTORUN] No STOP command sent."
    );

    Serial.println(
        "[AUTORUN] Decision completed."
    );
}


// ============================================================
// START RECORDING + VERIFICATION
// ============================================================

void AutoRun::startRecordingWithVerification(
    const char *reason
)
{
    if (!_camera)
        return;


    if (!_camera->isCommandChannelReady())
    {
        Serial.println(
            "[AUTORUN] Cannot START: CE82 command channel not ready."
        );

        return;
    }


    Serial.println();

    Serial.println(
        "[AUTORUN] ----------------------------------------"
    );

    Serial.println(
        "[AUTORUN] START recording"
    );

    Serial.printf(
        "[AUTORUN] Reason: %s\n",
        reason
    );

    Serial.println(
        "[AUTORUN] Sending SHUTTER..."
    );

    Serial.println(
        "[AUTORUN] ----------------------------------------"
    );


    _startVerificationPending = true;

    _startVerificationStartedAt =
        millis();

_startBaselinePacket = _camera->getLastPacketTime();


    _camera->shutter();


    Serial.println(
        "[AUTORUN] START command sent."
    );

    Serial.println(
        "[AUTORUN] Waiting for CE81 recording confirmation..."
    );
}


// ============================================================
// VERIFY START
// ============================================================

void AutoRun::verifyStartCommand()
{
    if (!_startVerificationPending)
        return;

    const unsigned long now = millis();
    const bool timedOut = (now - _startVerificationStartedAt) >= START_VERIFY_TIMEOUT;

    // Considera solo pacchetti CE81 arrivati DOPO l'invio dello shutter.
    if (_camera->getLastPacketTime() <= _startBaselinePacket)
    {
        if (timedOut)
        {
            Serial.println("[AUTORUN] START verification timeout: no new CE81.");
            _startVerificationPending = false;
        }
        return;
    }

    if (_camera->isRecordingKnown() && _camera->isRecording())
    {
        Serial.printf("[AUTORUN] START CONFIRMED by CE81. Timer: %s\n",
                      _camera->getRecordingTimer().c_str());
        _startVerificationPending = false;
        return;
    }

    const String timer = _camera->getRecordingTimer();
    if (timer.length() > 0 && timer != "00:00:00")
    {
        Serial.printf("[AUTORUN] START CONFIRMED by timer: %s\n", timer.c_str());
        _startVerificationPending = false;
        return;
    }

    if (timedOut)
    {
        Serial.println("[AUTORUN] WARNING: new CE81 received but camera NOT recording.");
        _startVerificationPending = false;
    }
}

// ============================================================
// SYNCHRONIZE CAMERA WITH AIRCRAFT
// ============================================================

void AutoRun::synchronizeCameraWithAircraft()
{
    if (!_camera || !_fc)
        return;

    if (!_camera->isCommandChannelReady())
    {
        Serial.println("[AUTORUN] Synchronization aborted: CE82 not ready");
        return;
    }

    if (!_fc->isArmKnown())
    {
        Serial.println("[AUTORUN] Synchronization: aircraft state unknown, skipped");
        return;
    }

    const bool armed = _fc->isArmed();

    Serial.printf("[AUTORUN] Synchronizing: aircraft %s\n",
                  armed ? "ARMED" : "DISARMED");

    // Allinea _lastArmed: evita un secondo evento ARM e un secondo shutter.
    _lastArmed = armed;
    _disarmPending = false;

    if (armed)
        decideStartRecording("startup synchronization");
}

// ============================================================
// STATE NAME
// ============================================================

const char *AutoRun::getStateName() const
{
    switch (_state)
    {
        case BOOT:
            return "BOOT";

        case WAKE:
            return "WAKE";

        case WAIT_CONNECTION:
            return "WAIT_CONNECTION";

        case CAMERA_AWAKE_GATT_OFFLINE:
            return "CAMERA_AWAKE_GATT_OFFLINE";

        case CONNECTION_SETTLE:
            return "CONNECTION_SETTLE";

        case READY:
            return "READY";

        case CAMERA_OFFLINE:
            return "CAMERA_OFFLINE";

        case RECONNECT_WAIT:
            return "RECONNECT_WAIT";

        default:
            return "UNKNOWN";
    }
}