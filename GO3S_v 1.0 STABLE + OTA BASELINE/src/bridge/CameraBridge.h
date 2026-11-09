#pragma once

#include <Arduino.h>

class FCInterface;


// ============================================================
// CAMERA BRIDGE
// ============================================================
//
// Translates FC RC channels into camera control events.
//
// The bridge owns:
//   - RC channel selection
//   - RC threshold interpretation
//   - transition detection
//   - event generation
//
// It does NOT:
//   - communicate with the camera
//   - send BLE commands
//   - decide whether a command is safe to execute
//
// AutoRun owns the high-level camera decision.
// ============================================================

class CameraBridge
{
public:

    void begin(
        FCInterface &fc
    );

    void update();


    // --------------------------------------------------------
    // CAMERA EVENTS
    // --------------------------------------------------------

    bool isShutterEvent();


    // --------------------------------------------------------
    // CONFIGURATION
    // --------------------------------------------------------

    void setShutterChannel(
        uint8_t channel
    );

    uint8_t getShutterChannel() const;


private:

    FCInterface *_fc = nullptr;


    // --------------------------------------------------------
    // SHUTTER RC CONFIGURATION
    // --------------------------------------------------------

    // Zero-based:
    // 9 = CH10

    uint8_t _shutterChannel = 9;


    static constexpr uint16_t RC_LOW_THRESHOLD = 1300;
    static constexpr uint16_t RC_HIGH_THRESHOLD = 1700;


    // --------------------------------------------------------
    // RC STATE
    // --------------------------------------------------------

    enum class RcState
    {
        UNKNOWN,
        RC_LOW,
        RC_MID,
        RC_HIGH
    };


    RcState _shutterState = RcState::UNKNOWN;


    bool _shutterEvent = false;


    // --------------------------------------------------------
    // INTERNAL
    // --------------------------------------------------------

    RcState decodeRcState(
        uint16_t value
    ) const;


    void reset();
};