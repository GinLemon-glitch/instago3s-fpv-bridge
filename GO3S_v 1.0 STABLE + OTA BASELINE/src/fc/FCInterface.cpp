#include "FCInterface.h"

#include <string.h>


// =============================================================================
// BEGIN
// =============================================================================

void FCInterface::begin()
{
    _armed = false;

    _apiVersionReceived = false;

    _apiProtocol = 0;
    _apiMajor = 0;
    _apiMinor = 0;

    _boxArmKnown = false;
    _boxArmBit = 0;

    _flightModeFlags = 0;

    _rcCount = 0;
    _rcValid = false;

    _linkUp = false;
    _lastValidReply = 0;

    _lastPoll = 0;
    _lastBoxIdsRequest = 0;

    _goodReplies = 0;
    _crcErrors = 0;

    resetParser();

    memset(
        _rc,
        0,
        sizeof(_rc)
    );

    memset(
        _payload,
        0,
        sizeof(_payload)
    );


    Serial1.begin(
        FC_BAUD,
        SERIAL_8N1,
        FC_RX_PIN,
        FC_TX_PIN
    );


    Serial.println();

    Serial.println(
        F("[FC] MSP interface initialized")
    );


    Serial.printf(
        "[FC] UART1 @ %lu baud | RX=GPIO%d TX=GPIO%d\n",
        FC_BAUD,
        FC_RX_PIN,
        FC_TX_PIN
    );


    // Initial identification requests.

    sendMspV1(
        MSP_API_VERSION
    );

    sendBoxIdsRequest();


    const uint32_t now =
        millis();


    _lastPoll = now;

    _lastBoxIdsRequest = now;
}


// =============================================================================
// UPDATE
// =============================================================================

void FCInterface::update()
{
    processSerial();


    if (
        _parserState != ParserState::WAIT_HEADER_1 &&
        (millis() - _lastByteTime) > PARSER_BYTE_TIMEOUT
    )
    {
        resetParser();
    }


    const uint32_t now =
        millis();


    // -------------------------------------------------------------------------
    // FC link timeout
    // -------------------------------------------------------------------------

    if (_lastValidReply != 0)
    {
        const bool newLinkState =
            (now - _lastValidReply) <= FC_LINK_TIMEOUT;


        if (
            _linkUp &&
            !newLinkState
        )
        {
            Serial.println(
                F("[FC] MSP link LOST - forcing ARM=NO")
            );
        }


        // Report link recovery only once.

        if (
            !_linkUp &&
            newLinkState
        )
        {
            Serial.println(
                F("[FC] MSP link OK")
            );
        }


        _linkUp = newLinkState;


        // Critical safety rule:
        // no FC link = not armed + RC invalid.

        if (!_linkUp)
        {
            // (arm state conservato: link perso != disarmo)

            _rcValid = false;
        }
    }
    else
    {
        _linkUp = false;

        // (arm state conservato: link perso != disarmo)

        _rcValid = false;
    }


    // -------------------------------------------------------------------------
    // Regular STATUS + RC polling
    // -------------------------------------------------------------------------

    if (
        (now - _lastPoll) >=
        POLL_INTERVAL
    )
    {
        _lastPoll = now;


        sendStatusRequest();

        sendRcRequest();
    }


    // -------------------------------------------------------------------------
    // Periodic BOXIDS refresh
    // -------------------------------------------------------------------------

    if (
        (now - _lastBoxIdsRequest) >=
        (
            _boxArmKnown
                ? BOXIDS_INTERVAL
                : BOXIDS_RETRY_INTERVAL
        )
    )
    {
        _lastBoxIdsRequest = now;


        sendBoxIdsRequest();
    }
}


// =============================================================================
// STATE GETTERS
// =============================================================================

bool FCInterface::isArmed() const
{
    if (_simActive)
        return _armed;

    return _linkUp && _armed;
}


bool FCInterface::isArmKnown() const
{
    return _simActive || (_linkUp && _boxArmKnown);
}


bool FCInterface::isLinkUp() const
{
    return _linkUp;
}


bool FCInterface::getRcChannel(
    uint8_t channel,
    uint16_t &value
) const
{
    if (
        !_linkUp ||
        !_rcValid
    )
    {
        value = 0;

        return false;
    }


    if (channel >= _rcCount)
    {
        value = 0;

        return false;
    }


    value = _rc[channel];

    return true;
}


uint16_t FCInterface::getRcChannel(
    uint8_t channel
) const
{
    uint16_t value = 0;

    getRcChannel(
        channel,
        value
    );

    return value;
}


uint8_t FCInterface::getRcChannelCount() const
{
    return _rcCount;
}


bool FCInterface::isRcValid() const
{
    return _linkUp && _rcValid;
}


// =============================================================================
// OSD
// =============================================================================

bool FCInterface::setOsdMessage(
    uint8_t slot,
    const char *text
)
{
    if (!_linkUp)
    {
        return false;
    }


    if (slot >= CUSTOM_MESSAGE_COUNT)
    {
        return false;
    }


    if (text == nullptr)
    {
        return false;
    }


    // -------------------------------------------------------------------------
    // IMPORTANT:
    //
    // Custom OSD messages may contain Betaflight native glyph bytes above
    // 0x7F, for example:
    //
    //   0x92 = battery 4 bars
    //   0x93 = battery 3 bars
    //   0x94 = battery 2 bars
    //   0x95 = battery 1 bar
    //   0x96 = battery empty / critical
    //
    // Do NOT use signed char semantics for these bytes.
    // Copy the original bytes directly into an uint8_t buffer.
    // -------------------------------------------------------------------------

    uint8_t textBytes[
        CUSTOM_MESSAGE_MAX_LENGTH
    ];


    uint8_t textLength = 0;


    while (
        textLength < CUSTOM_MESSAGE_MAX_LENGTH &&
        text[textLength] != '\0'
    )
    {
        textBytes[textLength] =
            static_cast<uint8_t>(
                static_cast<unsigned char>(
                    text[textLength]
                )
            );


        textLength++;
    }


    // -------------------------------------------------------------------------
    // MSP2_SET_TEXT payload:
    //
    // byte 0 = Custom Message ID
    // byte 1 = text length
    // byte 2.. = raw text bytes
    // -------------------------------------------------------------------------

    uint8_t payload[
        2 + CUSTOM_MESSAGE_MAX_LENGTH
    ];


    payload[0] =
        MSP2TEXT_CUSTOM_MSG_0 + slot;


    payload[1] =
        textLength;


    if (textLength > 0)
    {
        memcpy(
            &payload[2],
            textBytes,
            textLength
        );
    }


    // -------------------------------------------------------------------------
    // Diagnostic for native battery glyphs.
    //
    // This confirms that 0x92 really leaves the ESP32 as byte 0x92.
    // -------------------------------------------------------------------------

    Serial.print(
        "[FC][OSD] slot="
    );

    Serial.print(
        static_cast<unsigned>(slot)
    );

    Serial.print(
        " len="
    );

    Serial.print(
        static_cast<unsigned>(textLength)
    );

    Serial.print(
        " bytes:"
    );


    for (
        uint8_t i = 0;
        i < textLength;
        i++
    )
    {
        Serial.printf(
            " %02X",
            textBytes[i]
        );
    }


    Serial.println();


    sendMspV2(
        MSP2_SET_TEXT,
        payload,
        static_cast<uint16_t>(
            2 + textLength
        )
    );


    return true;
}


bool FCInterface::setOsdMessage(
    uint8_t slot,
    const String &text
)
{
    return setOsdMessage(
        slot,
        text.c_str()
    );
}


// =============================================================================
// BENCH SIMULATION
// =============================================================================

void FCInterface::simulateArm()
{
    _simActive = true;
    _armed = true;


    Serial.println(
        F("[FC][SIM] ARM")
    );
}


void FCInterface::simulateDisarm()
{
    _simActive = true;
    _armed = false;


    Serial.println(
        F("[FC][SIM] DISARM")
    );
}


// =============================================================================
// SERIAL PROCESSING
// =============================================================================

void FCInterface::processSerial()
{
    while (
        Serial1.available() > 0
    )
    {
        const uint8_t byte =
            static_cast<uint8_t>(
                Serial1.read()
            );


        _lastByteTime = millis();

        processByte(
            byte
        );
    }
}


// =============================================================================
// MSP REQUESTS
// =============================================================================

void FCInterface::sendStatusRequest()
{
    sendMspV1(
        MSP_STATUS
    );
}


void FCInterface::sendRcRequest()
{
    sendMspV1(
        MSP_RC
    );
}


void FCInterface::sendBoxIdsRequest()
{
    sendMspV1(
        MSP_BOXIDS
    );
}


// =============================================================================
// MSP V1 TX
// =============================================================================

void FCInterface::sendMspV1(
    uint8_t command
)
{
    uint8_t frame[6];


    frame[0] = '$';
    frame[1] = 'M';
    frame[2] = '<';

    frame[3] = 0;

    frame[4] = command;

    frame[5] =
        static_cast<uint8_t>(
            0 ^ command
        );


    Serial1.write(
        frame,
        sizeof(frame)
    );


    Serial1.flush();
}


// =============================================================================
// MSP V2 TX
// =============================================================================

void FCInterface::sendMspV2(
    uint16_t function,
    const uint8_t *payload,
    uint16_t payloadSize
)
{
    if (
        payloadSize >
        MAX_MSP_PAYLOAD
    )
    {
        return;
    }


    uint8_t frame[
        9 + MAX_MSP_PAYLOAD
    ];


    uint16_t index = 0;


    frame[index++] = '$';
    frame[index++] = 'X';
    frame[index++] = '<';


    uint8_t crc = 0;


    frame[index] = 0;


    crc = crc8DvbS2(
        crc,
        frame[index]
    );


    index++;


    frame[index] =
        static_cast<uint8_t>(
            function & 0xFF
        );


    crc = crc8DvbS2(
        crc,
        frame[index]
    );


    index++;


    frame[index] =
        static_cast<uint8_t>(
            (function >> 8) & 0xFF
        );


    crc = crc8DvbS2(
        crc,
        frame[index]
    );


    index++;


    frame[index] =
        static_cast<uint8_t>(
            payloadSize & 0xFF
        );


    crc = crc8DvbS2(
        crc,
        frame[index]
    );


    index++;


    frame[index] =
        static_cast<uint8_t>(
            (payloadSize >> 8) & 0xFF
        );


    crc = crc8DvbS2(
        crc,
        frame[index]
    );


    index++;


    for (
        uint16_t i = 0;
        i < payloadSize;
        i++
    )
    {
        frame[index] =
            payload[i];


        crc = crc8DvbS2(
            crc,
            frame[index]
        );


        index++;
    }


    frame[index++] = crc;


    Serial1.write(
        frame,
        index
    );


    Serial1.flush();
}


// =============================================================================
// MSP V2 CRC8 DVB-S2
// =============================================================================

uint8_t FCInterface::crc8DvbS2(
    uint8_t crc,
    uint8_t value
)
{
    crc ^= value;


    for (
        uint8_t i = 0;
        i < 8;
        i++
    )
    {
        if (crc & 0x80)
        {
            crc =
                static_cast<uint8_t>(
                    (crc << 1) ^ 0xD5
                );
        }
        else
        {
            crc =
                static_cast<uint8_t>(
                    crc << 1
                );
        }
    }


    return crc;
}


// =============================================================================
// PARSER
// =============================================================================

void FCInterface::processByte(
    uint8_t byte
)
{
    switch (_parserState)
    {
        // ---------------------------------------------------------------------
        case ParserState::WAIT_HEADER_1:

            if (byte == '$')
            {
                _parserState =
                    ParserState::WAIT_HEADER_TYPE;
            }

            break;


        // ---------------------------------------------------------------------
        case ParserState::WAIT_HEADER_TYPE:

            if (byte == 'M')
            {
                _parserV2 = false;

                _parserState =
                    ParserState::WAIT_DIRECTION;
            }
            else if (byte == 'X')
            {
                _parserV2 = true;

                _parserState =
                    ParserState::WAIT_DIRECTION;
            }
            else if (byte == '$')
            {
                _parserState =
                    ParserState::WAIT_HEADER_TYPE;
            }
            else
            {
                resetParser();
            }

            break;


        // ---------------------------------------------------------------------
        case ParserState::WAIT_DIRECTION:

            _parserDirection = byte;


            if (byte != '>')
            {
                if (byte == '!')
                {
                    Serial.println(
                        F("[FC] MSP error response")
                    );
                }


                resetParser();

                break;
            }


            if (_parserV2)
            {
                _parserState =
                    ParserState::V2_WAIT_FLAGS;
            }
            else
            {
                _parserState =
                    ParserState::V1_WAIT_SIZE;
            }

            break;


        // =====================================================================
        // MSP V1
        // =====================================================================

        case ParserState::V1_WAIT_SIZE:

            _parserPayloadSize = byte;


            if (
                _parserPayloadSize >
                MAX_MSP_PAYLOAD
            )
            {
                resetParser();

                break;
            }


            _parserChecksum = byte;


            _parserState =
                ParserState::V1_WAIT_COMMAND;

            break;


        case ParserState::V1_WAIT_COMMAND:

            _parserFunction = byte;

            _parserChecksum ^= byte;

            _parserPayloadIndex = 0;


            if (
                _parserPayloadSize == 0
            )
            {
                _parserState =
                    ParserState::V1_WAIT_CHECKSUM;
            }
            else
            {
                _parserState =
                    ParserState::V1_WAIT_PAYLOAD;
            }

            break;


        case ParserState::V1_WAIT_PAYLOAD:

            _payload[
                _parserPayloadIndex++
            ] = byte;


            _parserChecksum ^= byte;


            if (
                _parserPayloadIndex >=
                _parserPayloadSize
            )
            {
                _parserState =
                    ParserState::V1_WAIT_CHECKSUM;
            }

            break;


        case ParserState::V1_WAIT_CHECKSUM:

            _parserReceivedChecksum =
                byte;


            if (
                _parserChecksum ==
                _parserReceivedChecksum
            )
            {
                handleMessage(
                    _parserFunction,
                    _payload,
                    _parserPayloadSize,
                    false
                );
            }
            else
            {
                _crcErrors++;
            }


            resetParser();

            break;


        // =====================================================================
        // MSP V2
        // =====================================================================

        case ParserState::V2_WAIT_FLAGS:

            _parserChecksum =
                crc8DvbS2(
                    0,
                    byte
                );


            _parserState =
                ParserState::V2_WAIT_FUNCTION_LOW;

            break;


        case ParserState::V2_WAIT_FUNCTION_LOW:

            _parserFunction = byte;


            _parserChecksum =
                crc8DvbS2(
                    _parserChecksum,
                    byte
                );


            _parserState =
                ParserState::V2_WAIT_FUNCTION_HIGH;

            break;


        case ParserState::V2_WAIT_FUNCTION_HIGH:

            _parserFunction |=
                static_cast<uint16_t>(
                    byte
                ) << 8;


            _parserChecksum =
                crc8DvbS2(
                    _parserChecksum,
                    byte
                );


            _parserState =
                ParserState::V2_WAIT_SIZE_LOW;

            break;


        case ParserState::V2_WAIT_SIZE_LOW:

            _parserPayloadSize = byte;


            _parserChecksum =
                crc8DvbS2(
                    _parserChecksum,
                    byte
                );


            _parserState =
                ParserState::V2_WAIT_SIZE_HIGH;

            break;


        case ParserState::V2_WAIT_SIZE_HIGH:

            _parserPayloadSize |=
                static_cast<uint16_t>(
                    byte
                ) << 8;


            _parserChecksum =
                crc8DvbS2(
                    _parserChecksum,
                    byte
                );


            if (
                _parserPayloadSize >
                MAX_MSP_PAYLOAD
            )
            {
                resetParser();

                break;
            }


            _parserPayloadIndex = 0;


            if (
                _parserPayloadSize == 0
            )
            {
                _parserState =
                    ParserState::V2_WAIT_CHECKSUM;
            }
            else
            {
                _parserState =
                    ParserState::V2_WAIT_PAYLOAD;
            }

            break;


        case ParserState::V2_WAIT_PAYLOAD:

            _payload[
                _parserPayloadIndex++
            ] = byte;


            _parserChecksum =
                crc8DvbS2(
                    _parserChecksum,
                    byte
                );


            if (
                _parserPayloadIndex >=
                _parserPayloadSize
            )
            {
                _parserState =
                    ParserState::V2_WAIT_CHECKSUM;
            }

            break;


        case ParserState::V2_WAIT_CHECKSUM:

            _parserReceivedChecksum =
                byte;


            if (
                _parserChecksum ==
                _parserReceivedChecksum
            )
            {
                handleMessage(
                    _parserFunction,
                    _payload,
                    _parserPayloadSize,
                    true
                );
            }
            else
            {
                _crcErrors++;
            }


            resetParser();

            break;
    }
}


// =============================================================================
// MESSAGE DISPATCH
// =============================================================================

void FCInterface::handleMessage(
    uint16_t function,
    const uint8_t *payload,
    uint16_t payloadSize,
    bool v2
)
{
    (void)v2;


    _lastValidReply =
        millis();


    _goodReplies++;


    switch (function)
    {
        case MSP_API_VERSION:

            handleApiVersion(
                payload,
                payloadSize
            );

            break;


        case MSP_STATUS:

            handleStatus(
                payload,
                payloadSize
            );

            break;


        case MSP_RC:

            handleRc(
                payload,
                payloadSize
            );

            break;


        case MSP_BOXIDS:

            handleBoxIds(
                payload,
                payloadSize
            );

            break;


        default:

            break;
    }
}


// =============================================================================
// API VERSION
// =============================================================================

void FCInterface::handleApiVersion(
    const uint8_t *payload,
    uint16_t payloadSize
)
{
    if (payloadSize < 3)
    {
        return;
    }


    _apiProtocol = payload[0];

    _apiMajor = payload[1];

    _apiMinor = payload[2];


    _apiVersionReceived = true;
}


// =============================================================================
// STATUS
// =============================================================================

void FCInterface::handleStatus(
    const uint8_t *payload,
    uint16_t payloadSize
)
{
    _simActive = false;


    if (payloadSize < 10)
    {
        return;
    }


    const uint32_t flightModeFlags =
        static_cast<uint32_t>(
            payload[6]
        ) |
        (
            static_cast<uint32_t>(
                payload[7]
            ) << 8
        ) |
        (
            static_cast<uint32_t>(
                payload[8]
            ) << 16
        ) |
        (
            static_cast<uint32_t>(
                payload[9]
            ) << 24
        );


    _flightModeFlags =
        flightModeFlags;


    if (_boxArmKnown)
    {
        const bool newArmed =
            (
                (
                    _flightModeFlags >>
                    _boxArmBit
                ) & 1U
            ) != 0;


        if (newArmed != _armed)
        {
            _armed = newArmed;


            Serial.printf(
                "[FC] %s\n",
                _armed
                    ? "ARM"
                    : "DISARM"
            );
        }
    }
}


// =============================================================================
// RC
// =============================================================================

void FCInterface::handleRc(
    const uint8_t *payload,
    uint16_t payloadSize
)
{
    const uint8_t channelCount =
        static_cast<uint8_t>(
            payloadSize / 2
        );


    _rcCount =
        min(
            channelCount,
            static_cast<uint8_t>(16)
        );


    for (
        uint8_t i = 0;
        i < _rcCount;
        i++
    )
    {
        _rc[i] =
            static_cast<uint16_t>(
                payload[i * 2] |
                (
                    static_cast<uint16_t>(
                        payload[i * 2 + 1]
                    ) << 8
                )
            );
    }


    _rcValid =
        (_rcCount > 0);
}


// =============================================================================
// BOXIDS
// =============================================================================

void FCInterface::handleBoxIds(
    const uint8_t *payload,
    uint16_t payloadSize
)
{
    // BOXARM permanent ID is 0.
    // The returned order corresponds to the bit position
    // in flightModeFlags.

    for (
        uint16_t i = 0;
        i < payloadSize && i < 32;
        i++
    )
    {
        if (payload[i] == 0)
        {
            if (
                !_boxArmKnown ||
                _boxArmBit != i
            )
            {
                Serial.printf(
                    "[FC] BOXARM bit %u\n",
                    static_cast<unsigned>(i)
                );
            }


            _boxArmKnown = true;


            _boxArmBit =
                static_cast<uint8_t>(i);


            return;
        }
    }


    if (
        !_boxArmKnown &&
        payloadSize > 0
    )
    {
        Serial.println(
            F("[FC] BOXARM not found - default bit 0")
        );


        _boxArmKnown = true;

        _boxArmBit = 0;
    }
}


// =============================================================================
// PARSER RESET
// =============================================================================

void FCInterface::resetParser()
{
    _parserState =
        ParserState::WAIT_HEADER_1;


    _parserV2 = false;

    _parserDirection = 0;


    _parserFunction = 0;

    _parserPayloadSize = 0;

    _parserPayloadIndex = 0;


    _parserChecksum = 0;

    _parserReceivedChecksum = 0;
}