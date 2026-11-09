#include "CameraBridge.h"

#include "../fc/FCInterface.h"


void CameraBridge::begin(
    FCInterface &fc
)
{
    _fc = &fc;

    _shutterChannel = 9;

    reset();

    Serial.println(
        "[BRIDGE] Camera Bridge initialized"
    );

    Serial.printf(
        "[BRIDGE] RC shutter = CH%u\n",
        static_cast<unsigned>(
            _shutterChannel + 1
        )
    );
}


void CameraBridge::update()
{
    if (!_fc)
        return;


    /*
     * FC link or RC data unavailable.
     *
     * Reset the RC state machine so that when
     * the signal comes back the current switch
     * position becomes the new baseline.
     */
    if (
        !_fc->isLinkUp() ||
        !_fc->isRcValid()
    )
    {
        if (
            _shutterState != RcState::UNKNOWN ||
            _shutterEvent
        )
        {
            reset();
        }

        return;
    }


    uint16_t value = 0;

    if (
        !_fc->getRcChannel(
            _shutterChannel,
            value
        )
    )
    {
        reset();
        return;
    }


    const RcState newState =
        decodeRcState(
            value
        );


    /*
     * Ignore invalid RC values.
     */
    if (
        newState == RcState::UNKNOWN
    )
    {
        return;
    }


    /*
     * First valid sample establishes the
     * initial switch position.
     *
     * IMPORTANT:
     * No event is generated here.
     */
    if (
        _shutterState == RcState::UNKNOWN
    )
    {
        _shutterState = newState;

        Serial.printf(
            "[BRIDGE] CH%u initial = %s\n",
            static_cast<unsigned>(
                _shutterChannel + 1
            ),
            newState == RcState::RC_LOW
                ? "LOW"
                : newState == RcState::RC_MID
                    ? "MID"
                    : "HIGH"
        );

        return;
    }


    /*
     * SHUTTER EVENT
     *
     * Only LOW -> HIGH generates an event.
     *
     * MID is deliberately treated as an
     * intermediate state and does not trigger
     * anything.
     */
    if (
        _shutterState == RcState::RC_LOW &&
        newState == RcState::RC_HIGH
    )
    {
        _shutterEvent = true;

        Serial.println(
            "[BRIDGE] SHUTTER EVENT"
        );
    }


    /*
     * Store the latest valid state.
     */
    _shutterState = newState;
}


bool CameraBridge::isShutterEvent()
{
    if (!_shutterEvent)
        return false;


    /*
     * Event is a one-shot latch.
     */
    _shutterEvent = false;

    return true;
}


void CameraBridge::setShutterChannel(
    uint8_t channel
)
{
    if (channel >= 16)
        return;


    _shutterChannel = channel;

    /*
     * Changing channel invalidates the
     * previous switch state.
     */
    reset();


    Serial.printf(
        "[BRIDGE] RC shutter channel = CH%u\n",
        static_cast<unsigned>(
            _shutterChannel + 1
        )
    );
}


uint8_t CameraBridge::getShutterChannel() const
{
    return _shutterChannel;
}


CameraBridge::RcState
CameraBridge::decodeRcState(
    uint16_t value
) const
{
    /*
     * Zero means that no valid RC value
     * is currently available.
     */
    if (value == 0)
    {
        return RcState::UNKNOWN;
    }


    if (value <= RC_LOW_THRESHOLD)
    {
        return RcState::RC_LOW;
    }


    if (value >= RC_HIGH_THRESHOLD)
    {
        return RcState::RC_HIGH;
    }


    return RcState::RC_MID;
}


void CameraBridge::reset()
{
    _shutterState = RcState::UNKNOWN;

    _shutterEvent = false;
}