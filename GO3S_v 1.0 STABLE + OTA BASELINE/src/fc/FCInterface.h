#pragma once

#include <Arduino.h>


class FCInterface
{
public:

    void begin();
    void update();


    // -------------------------------------------------------------------------
    // Flight controller state
    // -------------------------------------------------------------------------

    bool isArmed() const;
    bool isLinkUp() const;

    // true solo se link MSP attivo e bit ARM noto (o simulazione)
    bool isArmKnown() const;


    // -------------------------------------------------------------------------
    // RC channels
    //
    // channel is zero-based:
    //
    // 0 = CH1
    // 1 = CH2
    // ...
    // 9 = CH10
    // -------------------------------------------------------------------------

    bool getRcChannel(
        uint8_t channel,
        uint16_t &value
    ) const;

    uint16_t getRcChannel(
        uint8_t channel
    ) const;


    uint8_t getRcChannelCount() const;

    bool isRcValid() const;


    // -------------------------------------------------------------------------
    // OSD Custom Messages
    //
    // Betaflight Custom Message 0..3
    // Maximum 16 characters.
    // -------------------------------------------------------------------------

    bool setOsdMessage(
        uint8_t slot,
        const char *text
    );

    bool setOsdMessage(
        uint8_t slot,
        const String &text
    );


    // -------------------------------------------------------------------------
    // Bench simulation
    // -------------------------------------------------------------------------

    void simulateArm();
    void simulateDisarm();


private:

    // -------------------------------------------------------------------------
    // UART
    // -------------------------------------------------------------------------

    static constexpr int FC_RX_PIN = 20;
    static constexpr int FC_TX_PIN = 21;

    static constexpr uint32_t FC_BAUD = 115200;


    // -------------------------------------------------------------------------
    // MSP v1 commands
    // -------------------------------------------------------------------------

    static constexpr uint8_t MSP_API_VERSION = 1;
    static constexpr uint8_t MSP_STATUS      = 101;
    static constexpr uint8_t MSP_RC          = 105;
    static constexpr uint8_t MSP_BOXIDS      = 119;


    // -------------------------------------------------------------------------
    // MSP v2 commands
    // -------------------------------------------------------------------------

    static constexpr uint16_t MSP2_SET_TEXT = 0x3007;

    static constexpr uint8_t MSP2TEXT_CUSTOM_MSG_0 = 7;

    static constexpr uint8_t CUSTOM_MESSAGE_COUNT = 4;
    static constexpr uint8_t CUSTOM_MESSAGE_MAX_LENGTH = 16;


    // -------------------------------------------------------------------------
    // Timing
    // -------------------------------------------------------------------------

    static constexpr uint32_t POLL_INTERVAL = 100;
    static constexpr uint32_t BOXIDS_INTERVAL = 10000;
    static constexpr uint32_t FC_LINK_TIMEOUT = 500;


    // -------------------------------------------------------------------------
    // MSP framing
    // -------------------------------------------------------------------------

    static constexpr uint8_t MSP_HEADER_1 = '$';
    static constexpr uint8_t MSP_HEADER_V1 = 'M';
    static constexpr uint8_t MSP_HEADER_V2 = 'X';

    static constexpr uint8_t MSP_REQUEST = '<';
    static constexpr uint8_t MSP_RESPONSE = '>';
    static constexpr uint8_t MSP_ERROR = '!';

    static constexpr uint16_t MAX_MSP_PAYLOAD = 255;


    // -------------------------------------------------------------------------
    // Parser
    // -------------------------------------------------------------------------

    enum class ParserState
    {
        WAIT_HEADER_1,
        WAIT_HEADER_TYPE,
        WAIT_DIRECTION,

        V1_WAIT_SIZE,
        V1_WAIT_COMMAND,
        V1_WAIT_PAYLOAD,
        V1_WAIT_CHECKSUM,

        V2_WAIT_FLAGS,
        V2_WAIT_FUNCTION_LOW,
        V2_WAIT_FUNCTION_HIGH,
        V2_WAIT_SIZE_LOW,
        V2_WAIT_SIZE_HIGH,
        V2_WAIT_PAYLOAD,
        V2_WAIT_CHECKSUM
    };


    // -------------------------------------------------------------------------
    // MSP processing
    // -------------------------------------------------------------------------

    void processSerial();

    void processByte(
        uint8_t byte
    );


    void sendMspV1(
        uint8_t command
    );


    void sendMspV2(
        uint16_t function,
        const uint8_t *payload,
        uint16_t payloadSize
    );


    void sendStatusRequest();

    void sendRcRequest();

    void sendBoxIdsRequest();


    void handleMessage(
        uint16_t function,
        const uint8_t *payload,
        uint16_t payloadSize,
        bool v2
    );


    void handleApiVersion(
        const uint8_t *payload,
        uint16_t payloadSize
    );


    void handleStatus(
        const uint8_t *payload,
        uint16_t payloadSize
    );


    void handleRc(
        const uint8_t *payload,
        uint16_t payloadSize
    );


    void handleBoxIds(
        const uint8_t *payload,
        uint16_t payloadSize
    );


    void resetParser();


    // -------------------------------------------------------------------------
    // MSP v2 CRC
    // -------------------------------------------------------------------------

    static uint8_t crc8DvbS2(
        uint8_t crc,
        uint8_t value
    );


    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    static constexpr uint32_t BOXIDS_RETRY_INTERVAL = 1000;
static constexpr uint32_t PARSER_BYTE_TIMEOUT = 50;

bool _simActive = false;
uint32_t _lastByteTime = 0;

bool _armed = false;

    bool _apiVersionReceived = false;

    uint8_t _apiProtocol = 0;
    uint8_t _apiMajor = 0;
    uint8_t _apiMinor = 0;


    bool _boxArmKnown = false;
    uint8_t _boxArmBit = 0;


    uint32_t _flightModeFlags = 0;


    uint16_t _rc[16] = {};

    uint8_t _rcCount = 0;

    bool _rcValid = false;


    bool _linkUp = false;

    uint32_t _lastValidReply = 0;


    uint32_t _lastPoll = 0;

    uint32_t _lastBoxIdsRequest = 0;


    uint32_t _goodReplies = 0;

    uint32_t _crcErrors = 0;


    // -------------------------------------------------------------------------
    // Parser state
    // -------------------------------------------------------------------------

    ParserState _parserState =
        ParserState::WAIT_HEADER_1;


    bool _parserV2 = false;

    uint8_t _parserDirection = 0;


    uint16_t _parserFunction = 0;

    uint16_t _parserPayloadSize = 0;

    uint16_t _parserPayloadIndex = 0;


    uint8_t _parserChecksum = 0;

    uint8_t _parserReceivedChecksum = 0;


    uint8_t _payload[MAX_MSP_PAYLOAD];
};