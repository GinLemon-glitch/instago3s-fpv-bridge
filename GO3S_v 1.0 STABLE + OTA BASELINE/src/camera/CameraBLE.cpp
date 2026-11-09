#include "CameraBLE.h"

#include <ctype.h>
#include <string.h>


// ============================================================
// GLOBAL STATE
// ============================================================

static CameraBLE *g_cameraBLE = nullptr;

static uint16_t g_connHandle =
    BLE_HS_CONN_HANDLE_NONE;

static uint32_t g_sessionNumber = 0;

static ble_addr_t g_peerAddress;
static bool g_peerAddressValid = false;

static unsigned long g_lastConnectEvent = 0;
static unsigned long g_lastDisconnectEvent = 0;

// ============================================================
// DEBUG / WAKE STATE
// ============================================================

static bool g_debugEnabled = false;
static bool g_wakeActive = false;

// When the user explicitly powers the camera OFF, do not restart
// normal GATT advertising after the camera disconnects. Otherwise the
// GO 3S can immediately reconnect to the ESP32 and wake itself again.
// This lock is cleared only by an explicit wake() request.
static bool g_manualPowerOffAdvertisingLock = false;


// ============================================================
// CE82 SUBSCRIPTION STATE
// ============================================================

static bool g_ce82NotifyEnabled = false;
static uint16_t g_ce82SubscriptionValue = 0;

static unsigned long g_lastCE82SubscribeEvent = 0;


// ============================================================
// RECONNECT DIAGNOSTIC WINDOW
// ============================================================
//
// After every native GATT connection we monitor the first
// 10 seconds in detail.
//
// This does NOT modify BLE behavior.
// It only reports what happens.
//

static bool g_reconnectDiagnosticActive = false;

static unsigned long g_reconnectDiagnosticStarted = 0;

static unsigned long g_lastReconnectDiagnosticPrint = 0;

static const unsigned long RECONNECT_DIAGNOSTIC_WINDOW =
    10000UL;

static const unsigned long RECONNECT_DIAGNOSTIC_INTERVAL =
    250UL;


// ============================================================
// TX SEQUENCE
// ============================================================

static uint8_t g_txSequence = 0;

// ============================================================
// LOCAL RECORDING TIMER
// ============================================================
// The GO 3S capture packet tells us recording started, but the
// timer telemetry is not guaranteed to arrive on every cycle.
// Keep the recording start time locally so the OSD can always
// show HH:MM:SS while recording.
static unsigned long g_recordingStartedMillis = 0;
static bool g_localRecordingTimerActive = false;

// ============================================================
// CE82 NATIVE HANDLE
// ============================================================
//
// Stored when CE82 is created.
// Used by the native GAP SUBSCRIBE event handler to determine
// whether a subscription event belongs to CE82.
//
// ============================================================

static uint16_t g_ce82Handle = 0;

// ============================================================
// CONSTANTS
// ============================================================

static const char *CE80_UUID =
    "CE80";

static const char *CE81_UUID =
    "CE81";

static const char *CE82_UUID =
    "CE82";

static const char *CE83_UUID =
    "CE83";

static const char *D0FF_UUID =
    "0000D0FF-3C17-D293-8E48-14FE2E4DA212";


// ============================================================
// BLE IDENTITY ADDRESS
// ============================================================
//
// The GPS Remote architecture uses a stable identity address.
// For our ESP32-C3 we deliberately use a STATIC RANDOM address
// derived deterministically from this board's eFuse MAC.
//
// This is NOT an RPA and therefore does not rotate between
// advertisements / reboots. The derivation is only a convenient
// way to make the address stable for this physical board without
// hard-coding one address that could collide if the firmware is
// copied to another board.
//
// NimBLE expects the random address in little-endian host order.
// The most-significant address byte must have bits 7:6 = 11 for
// a STATIC RANDOM address.
//
// IMPORTANT: this does not enable BLE bonding/security. It only
// establishes the stable BLE identity address.
//
// ============================================================

static bool g_staticRandomAddressConfigured = false;


static bool configureStaticRandomAddress()
{
    const uint64_t mac =
        ESP.getEfuseMac();

    uint8_t rndAddr[6];

    rndAddr[0] =
        (uint8_t)(mac & 0xFF);

    rndAddr[1] =
        (uint8_t)((mac >> 8) & 0xFF);

    rndAddr[2] =
        (uint8_t)((mac >> 16) & 0xFF);

    rndAddr[3] =
        (uint8_t)((mac >> 24) & 0xFF);

    rndAddr[4] =
        (uint8_t)((mac >> 32) & 0xFF);

    rndAddr[5] =
        (uint8_t)(((mac >> 40) & 0x3F) | 0xC0);

    const bool addrOk =
        BLEDevice::setOwnAddr(
            rndAddr
        );

    const bool typeOk =
        BLEDevice::setOwnAddrType(
            BLE_OWN_ADDR_RANDOM
        );

    g_staticRandomAddressConfigured =
        typeOk && addrOk;

    Serial.println();
    Serial.println(
        "[BLE ID] Static-random identity configuration"
    );

    Serial.printf(
        "[BLE ID] setOwnAddrType(RANDOM): %s\n",
        typeOk ? "OK" : "FAILED"
    );

    Serial.printf(
        "[BLE ID] setOwnAddr():          %s\n",
        addrOk ? "OK" : "FAILED"
    );

    if (g_staticRandomAddressConfigured)
    {
        BLEAddress address =
            BLEDevice::getAddress();

        Serial.printf(
            "[BLE ID] Active address: %s\n",
            address.toString().c_str()
        );

        Serial.printf(
            "[BLE ID] Active type: %u (%s)\n",
            address.getType(),
            address.getType() == BLE_ADDR_RANDOM
                ? "RANDOM"
                : "PUBLIC"
        );
    }

    Serial.println();

    return g_staticRandomAddressConfigured;
}


// ============================================================
// HELPERS
// ============================================================

static void printBLEAddress(
    const ble_addr_t &addr
)
{
    Serial.printf(
        "%02X:%02X:%02X:%02X:%02X:%02X",
        addr.val[5],
        addr.val[4],
        addr.val[3],
        addr.val[2],
        addr.val[1],
        addr.val[0]
    );
}


static void printHexBytes(
    const uint8_t *data,
    size_t length
)
{
    for (size_t i = 0; i < length; i++)
    {
        Serial.printf(
            "%02X ",
            data[i]
        );
    }
}


static void printBLEIdentityDiagnostics()
{
    Serial.println();
    Serial.println("[BLE ID] ================================================");
    Serial.println("[BLE ID] ESP32 BLE identity / bonding diagnostics");

    // Arduino-ESP32 3.3.11 exposes the address as BLEAddress
    // (not NimBLEAddress). BLEAddress also exposes the address type.
    BLEAddress ownAddress = BLEDevice::getAddress();

    Serial.printf(
        "[BLE ID] ESP address      : %s\n",
        ownAddress.toString().c_str()
    );

    Serial.printf(
        "[BLE ID] ESP addr type   : %u (%s)\n",
        ownAddress.getType(),
        ownAddress.getType() == BLE_ADDR_PUBLIC
            ? "PUBLIC"
            : "RANDOM"
    );

    // IMPORTANT:
    // BLEDevice in the Arduino-ESP32 3.3.11 API does not expose
    // getNumBonds()/getBondedAddress() in the BLEDevice wrapper used
    // by this project. We therefore do NOT invent a bond count here.
    // The authoritative per-connection security state is printed below
    // from ble_gap_conn_desc::sec_state:
    // encrypted / authenticated / bonded.
    Serial.println(
        "[BLE ID] Stored bond count: not exposed by this BLEDevice API"
    );

    Serial.println(
        "[BLE ID] Security bonding  : NOT ENABLED BY THIS FIRMWARE"
    );

    Serial.println(
        "[BLE ID] NOTE: diagnostics only; no bonding/security settings changed"
    );

    Serial.println(
        "[BLE ID] =================================================="
    );
}


// ============================================================
// CE81 CAMERA RX CALLBACK
// ============================================================
// CE81 is the real CAMERA -> REMOTE telemetry channel.
// No forensic/sniffer layer is used in the production firmware.

static String bytesToHexString(const String &value)
{
    String hex;
    hex.reserve(value.length() * 2);

    static const char *digits = "0123456789ABCDEF";

    for (unsigned int i = 0; i < value.length(); i++)
    {
        uint8_t b = (uint8_t)value[i];
        hex += digits[(b >> 4) & 0x0F];
        hex += digits[b & 0x0F];
    }

    return hex;
}


class CameraRxCallbacks :
    public BLECharacteristicCallbacks
{
public:
    void onWrite(
        BLECharacteristic *characteristic
    ) override
    {
        if (!g_cameraBLE || !characteristic)
            return;

        String rawValue = characteristic->getValue();

        if (rawValue.length() == 0)
            return;

        String value = bytesToHexString(rawValue);

        Serial.printf(
            "[CE81 RX] rawLen=%u normalizedLen=%u\n",
            rawValue.length(),
            value.length()
        );

        g_cameraBLE->handleCameraPacket(value);
    }
};


static CameraRxCallbacks g_cameraRxCallbacks;


// ============================================================
// CE82 TX CALLBACKS
// ============================================================

class CameraTxCallbacks :
    public BLECharacteristicCallbacks
{
public:

    void onSubscribe(
        BLECharacteristic *characteristic,
        ble_gap_conn_desc *desc,
        uint16_t subValue
    ) override
    {
        if (!g_cameraBLE)
            return;

        uint16_t handle =
            BLE_HS_CONN_HANDLE_NONE;

        if (desc)
        {
            handle =
                desc->conn_handle;
        }

        Serial.printf(
            "[BLE] CE82 subscription: 0x%04X (%s)\n",
            subValue,
            (subValue & 0x0001) ? "notify ON" : "notify OFF"
        );

        if (characteristic)
        {
            uint16_t characteristicHandle =
                characteristic->getHandle();

            if (characteristicHandle != 0 &&
                characteristicHandle != 0xFFFF)
            {
                g_ce82Handle = characteristicHandle;
            }
        }

        g_cameraBLE->handleCE82Subscribe(
            handle,
            subValue
        );
    }


    void onStatus(
        BLECharacteristic *characteristic,
        Status status,
        uint32_t code
    ) override
    {
        if (!g_cameraBLE)
            return;

        g_cameraBLE->handleCE82Status(
            status,
            code
        );
    }
};


// ============================================================
// SERVER CALLBACKS
// ============================================================

// IMPORTANT:
// Keep a SINGLE source of truth for the GATT connection.
// We intentionally use only the detailed NimBLE Arduino callback
// (the overload carrying ble_gap_conn_desc).
// The previous version implemented both overloads, which caused
// the GO 3S connection callback to run twice and reset CE82 state.

class MyServerCallbacks :
    public BLEServerCallbacks
{
public:

    void onConnect(
        BLEServer *server,
        ble_gap_conn_desc *desc
    ) override
    {
        if (!g_cameraBLE || !desc)
            return;

        if (
            g_cameraBLE->_deviceConnected &&
            g_connHandle == desc->conn_handle
        )
        {
            return;
        }

        g_sessionNumber++;
        g_txSequence = 0;
        g_ce82NotifyEnabled = false;
        g_ce82SubscriptionValue = 0;
        g_lastCE82SubscribeEvent = 0;

        g_cameraBLE->_deviceConnected = true;
        g_cameraBLE->_cameraSeen = true;
        g_connHandle = desc->conn_handle;
        g_peerAddress = desc->peer_id_addr;
        g_peerAddressValid = true;
        g_lastConnectEvent = millis();
        g_reconnectDiagnosticActive = false;

        Serial.printf(
            "[BLE] Connected (session %lu, handle %u)\n",
            (unsigned long)g_sessionNumber,
            g_connHandle
        );
    }

    void onDisconnect(
        BLEServer *server,
        ble_gap_conn_desc *desc
    ) override
    {
        if (!g_cameraBLE)
            return;

        g_cameraBLE->_deviceConnected = false;
        g_connHandle = BLE_HS_CONN_HANDLE_NONE;
        g_peerAddressValid = false;
        g_ce82NotifyEnabled = false;
        g_ce82SubscriptionValue = 0;
        g_lastCE82SubscribeEvent = 0;
        g_lastDisconnectEvent = millis();
        g_reconnectDiagnosticActive = false;

        Serial.println("[BLE] Camera disconnected");

        if (g_manualPowerOffAdvertisingLock)
        {
            // IMPORTANT: after an explicit POWER OFF, do NOT restart
            // normal GATT advertising. The GO 3S may reconnect to that
            // advertisement and immediately wake back up.
            BLEDevice::stopAdvertising();

            Serial.println(
                "[BLE] Normal advertising SUPPRESSED (manual power-off lock)"
            );
        }
        else
        {
            BLEAdvertising *advertising = BLEDevice::getAdvertising();
            advertising->addServiceUUID(CE80_UUID);
            advertising->addServiceUUID(D0FF_UUID);
            advertising->setScanResponse(true);
            advertising->setMinPreferred(0x06);
            advertising->setMinPreferred(0x12);
            BLEDevice::startAdvertising();

            Serial.println(
                "[BLE] Normal advertising restarted"
            );
        }
    }
};


// ============================================================
// NATIVE GAP EVENT HANDLER
// ============================================================
//
// The native GAP API is used for the special wake-on advertisement.
// Because ble_gap_adv_start() is given this callback, the CONNECT event
// for that advertising procedure is delivered here. We therefore sync
// the CameraBLE GATT state from this callback. The normal Arduino
// MyServerCallbacks path remains active for ordinary advertising and is
// also protected against duplicate CONNECT events.
//

int wakeGapEventHandler(
    struct ble_gap_event *event,
    void *arg
)
{
    CameraBLE *camera =
        static_cast<CameraBLE *>(arg);

    if (!camera || !event)
        return 0;

    Serial.printf(
        "[GAP TRACE] event type=%d millis=%lu\n",
        event->type,
        millis()
    );

    switch (event->type)
    {
        case BLE_GAP_EVENT_CONNECT:
        {
            Serial.println(
                "[GAP EVENT] CONNECT"
            );

            Serial.printf(
                "[GAP EVENT] status = %d\n",
                event->connect.status
            );

            if (event->connect.status == 0)
            {
                const uint16_t handle =
                    event->connect.conn_handle;

                Serial.printf(
                    "[GAP EVENT] handle = %u\n",
                    handle
                );

                Serial.println(
                    "[GAP EVENT] Native GAP CONNECT belongs to the wake advertisement"
                );

                Serial.println(
                    "[GAP EVENT] Synchronizing CameraBLE GATT state now"
                );

                camera->handleNativeGapConnection(
                    true,
                    handle,
                    nullptr
                );
            }
            else
            {
                Serial.println(
                    "[GAP EVENT] CONNECT FAILED"
                );
            }

            break;
        }

        case BLE_GAP_EVENT_DISCONNECT:
        {
            Serial.println(
                "[GAP EVENT] DISCONNECT"
            );

            Serial.printf(
                "[GAP EVENT] handle = %u\n",
                event->disconnect.conn.conn_handle
            );

            Serial.printf(
                "[GAP EVENT] reason = %d\n",
                event->disconnect.reason
            );

            Serial.println(
                "[GAP EVENT] Synchronizing CameraBLE GATT disconnect state"
            );

            camera->handleNativeGapConnection(
                false,
                event->disconnect.conn.conn_handle,
                &event->disconnect.conn
            );

            break;
        }

        case BLE_GAP_EVENT_MTU:
        {
            Serial.println(
                "[GAP EVENT] MTU"
            );

            Serial.printf(
                "[GAP EVENT] handle = %u\n",
                event->mtu.conn_handle
            );

            Serial.printf(
                "[GAP EVENT] mtu = %u\n",
                event->mtu.value
            );

            break;
        }

        default:
            break;
    }

    return 0;
}


// ============================================================
// BEGIN
// ============================================================

void CameraBLE::begin()
{
    g_cameraBLE = this;

    _server = nullptr;
    _txCharacteristic = nullptr;

    _deviceConnected = false;
    _oldDeviceConnected = false;

    _cameraSeen = false;
    _lastPacketTime = 0;

    _status.recording = false;
    _status.recordingKnown = false;

    _status.recordingTimer = "";
    _status.remainingTime = "";
    _status.remainingRuntime = "";
    _status.remainingPhotos = "";

    _status.modeString = "";
    _status.resolution = "";
    _status.setting = "";
    _status.mode = "";
    _status.shootingMode = "";
    _status.modeCode = "";
    _status.lensMode = "";
    _status.photoTimer = "";
    _status.photoCaptureActive = false;

    _status.pureShotCountdown = false;
    _status.pureShotValue = "";

    _status.batteryKnown = false;
    _status.batteryState = 0;

    _status.batteryDescription = "";
    _status.batteryBars = 0;

    g_connHandle =
        BLE_HS_CONN_HANDLE_NONE;

    g_sessionNumber = 0;

    g_peerAddressValid = false;

    g_lastConnectEvent = 0;
    g_lastDisconnectEvent = 0;

    g_ce82NotifyEnabled = false;
    g_ce82SubscriptionValue = 0;

    g_lastCE82SubscribeEvent = 0;

    g_reconnectDiagnosticActive = false;
    g_reconnectDiagnosticStarted = 0;
    g_lastReconnectDiagnosticPrint = 0;

    g_txSequence = 0;

    g_ce82Handle = 0;

    g_manualPowerOffAdvertisingLock = false;

    setupBLE();
}


// ============================================================
// BLE SETUP
// ============================================================

void CameraBLE::setupBLE()
{
    Serial.println(
        "[BLE] Initializing BLE..."
    );

    BLEDevice::init(
        "Insta360 GPS Remote"
    );

    _server =
        BLEDevice::createServer();

    _server->setCallbacks(
        new MyServerCallbacks()
    );


    // --------------------------------------------------------
    // PRIMARY SERVICE CE80
    // --------------------------------------------------------

    BLEService *ce80 =
        _server->createService(
            CE80_UUID
        );


    // CE81: CAMERA -> REMOTE
    BLECharacteristic *ce81 =
        ce80->createCharacteristic(
            CE81_UUID,
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_WRITE_NR
        );

    ce81->setCallbacks(
        &g_cameraRxCallbacks
    );


    // CE82: REMOTE -> CAMERA
    //
    // BOTH NOTIFY + INDICATE.
    //
    // NimBLE automatically creates CCCD.
    //

    _txCharacteristic =
        ce80->createCharacteristic(
            CE82_UUID,
            BLECharacteristic::PROPERTY_NOTIFY
        );

    _txCharacteristic->setCallbacks(
        new CameraTxCallbacks()
    );

    uint8_t ce82Value[] =
    {
        0x00
    };

    _txCharacteristic->setValue(
        ce82Value,
        sizeof(ce82Value)
    );


    // CE83: READ
    BLECharacteristic *ce83 =
        ce80->createCharacteristic(
            CE83_UUID,
            BLECharacteristic::PROPERTY_READ
        );

    uint8_t ce83Value[] =
    {
        0x01,
        0x02
    };

    ce83->setValue(
        ce83Value,
        sizeof(ce83Value)
    );

    ce80->start();

    // CE82 handle is valid only after the service has been started.
    // Before ce80->start(), Arduino BLECharacteristic::getHandle()
    // returns 0xFFFF even though NimBLE later assigns the real ATT
    // value handle (0x0012 in this GATT database).
    g_ce82Handle =
        _txCharacteristic->getHandle();

    Serial.printf(
        "[BLE] CE82 native value handle = 0x%04X\n",
        g_ce82Handle
    );


    // --------------------------------------------------------
    // SECONDARY D0FF SERVICE
    // --------------------------------------------------------

    BLEService *d0ff =
        _server->createService(
            D0FF_UUID
        );


    BLECharacteristic *ffd1 =
        d0ff->createCharacteristic(
            "FFD1",
            BLECharacteristic::PROPERTY_WRITE
        );


    BLECharacteristic *ffd2 =
        d0ff->createCharacteristic(
            "FFD2",
            BLECharacteristic::PROPERTY_WRITE
        );


    BLECharacteristic *ffd3 =
        d0ff->createCharacteristic(
            "FFD3",
            BLECharacteristic::PROPERTY_READ
        );

    uint8_t ffd3Value[] =
    {
        0x01,
        0x90,
        0x1E,
        0x30
    };

    ffd3->setValue(
        ffd3Value,
        sizeof(ffd3Value)
    );


    BLECharacteristic *ffd4 =
        d0ff->createCharacteristic(
            "FFD4",
            BLECharacteristic::PROPERTY_READ
        );

    uint8_t ffd4Value[] =
    {
        0x01,
        0x20,
        0x00,
        0x18
    };

    ffd4->setValue(
        ffd4Value,
        sizeof(ffd4Value)
    );


    BLECharacteristic *ffd5 =
        d0ff->createCharacteristic(
            "FFD5",
            BLECharacteristic::PROPERTY_NOTIFY
        );


    BLECharacteristic *ffd8 =
        d0ff->createCharacteristic(
            "FFD8",
            BLECharacteristic::PROPERTY_NOTIFY
        );


    BLECharacteristic *fff1 =
        d0ff->createCharacteristic(
            "FFF1",
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE
        );


    BLECharacteristic *fff2 =
        d0ff->createCharacteristic(
            "FFF2",
            BLECharacteristic::PROPERTY_NOTIFY
        );


    BLECharacteristic *ffe0 =
        d0ff->createCharacteristic(
            "FFE0",
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE
        );


    d0ff->start();


    // --------------------------------------------------------
    // NORMAL ADVERTISING
    // --------------------------------------------------------

    BLEAdvertising *advertising =
        BLEDevice::getAdvertising();

    advertising->addServiceUUID(
        CE80_UUID
    );

    advertising->addServiceUUID(
        D0FF_UUID
    );

    advertising->setScanResponse(
        true
    );

    advertising->setMinPreferred(
        0x06
    );

    advertising->setMinPreferred(
        0x12
    );

    BLEDevice::startAdvertising();

    Serial.println(
        "[BLE] Normal advertising started"
    );
}


// ============================================================
// UPDATE
// ============================================================

void CameraBLE::update()
{
    // --------------------------------------------------------
    // CONNECT RISING EDGE
    // --------------------------------------------------------

    if (
        _deviceConnected &&
        !_oldDeviceConnected
    )
    {
        Serial.println();
        Serial.println(
            "[BLE] ===== CONNECT EDGE ====="
        );

        Serial.println(
            "[BLE] Camera GATT connection established"
        );

        Serial.printf(
            "[BLE] Session: %lu\n",
            (unsigned long)g_sessionNumber
        );

        Serial.printf(
            "[BLE] Handle: %u\n",
            g_connHandle
        );

        Serial.println(
            "[BLE] Connection state: SYNCHRONIZED BY NIMBLE GATT CALLBACK"
        );

        Serial.printf(
            "[BLE] CE82 notify subscription: %s\n",
            g_ce82NotifyEnabled
                ? "ENABLED"
                : "NOT YET ENABLED"
        );

        if (g_peerAddressValid)
        {
            Serial.print(
                "[BLE] Peer: "
            );

            printBLEAddress(
                g_peerAddress
            );

            Serial.println();
        }

        printConnectionDiagnostics(
            "CONNECT EDGE"
        );

        _oldDeviceConnected = true;
    }


    // --------------------------------------------------------
    // RECONNECT DIAGNOSTIC WINDOW
    // --------------------------------------------------------

    if (g_reconnectDiagnosticActive)
    {
        unsigned long elapsed =
            millis() -
            g_reconnectDiagnosticStarted;

        if (
            elapsed <=
            RECONNECT_DIAGNOSTIC_WINDOW
        )
        {
            if (
                millis() -
                g_lastReconnectDiagnosticPrint >=
                RECONNECT_DIAGNOSTIC_INTERVAL
            )
            {
                g_lastReconnectDiagnosticPrint =
                    millis();

                Serial.printf(
                    "[RECONNECT TRACE] +%lu ms "
                    "session=%lu "
                    "handle=%u "
                    "GATT=%s "
                    "CE82=0x%04X "
                    "notify=%s "
                    "lastSub=%lums ago "
                    "CE81=%lums ago\n",

                    elapsed,

                    (unsigned long)g_sessionNumber,

                    g_connHandle,

                    _deviceConnected
                        ? "YES"
                        : "NO",

                    g_ce82SubscriptionValue,

                    g_ce82NotifyEnabled
                        ? "YES"
                        : "NO",

                    g_lastCE82SubscribeEvent == 0
                        ? 0UL
                        : millis() -
                          g_lastCE82SubscribeEvent,

                    _lastPacketTime == 0
                        ? 0UL
                        : millis() -
                          _lastPacketTime
                );
            }
        }
        else
        {
            Serial.println();
            Serial.println(
                "[RECONNECT TRACE] ===== 10 SECOND WINDOW ENDED ====="
            );

            Serial.printf(
                "[RECONNECT TRACE] Final CE82 subscription = 0x%04X\n",
                g_ce82SubscriptionValue
            );

            Serial.printf(
                "[RECONNECT TRACE] Final CE82 notify = %s\n",
                g_ce82NotifyEnabled
                    ? "YES"
                    : "NO"
            );

            Serial.printf(
                "[RECONNECT TRACE] CE82 subscribe event = %s\n",
                g_lastCE82SubscribeEvent != 0
                    ? "SEEN"
                    : "NOT SEEN"
            );

            Serial.println(
                "[RECONNECT TRACE] ==========================================="
            );

            g_reconnectDiagnosticActive = false;
        }
    }


    // --------------------------------------------------------
    // CONNECTED BUT NO TELEMETRY
    // --------------------------------------------------------
    //
    // Diagnostic ONLY.
    //
    // This does NOT declare the camera offline and does NOT
    // trigger reconnect.
    //

    if (_deviceConnected)
    {
        if (
            _lastPacketTime > 0 &&
            !hasRecentPacket(15000UL)
        )
        {
            static unsigned long lastTimeoutLog = 0;

            if (
                millis() -
                lastTimeoutLog >
                5000UL
            )
            {
                lastTimeoutLog =
                    millis();

                Serial.println(
                    "[TELEMETRY] CE81 TIMEOUT"
                );

                Serial.println(
                    "[WARNING] GATT connected but no recent CE81 data"
                );

            }
        }
    }


    // --------------------------------------------------------
    // DISCONNECT FALLING EDGE
    // --------------------------------------------------------

    if (
        !_deviceConnected &&
        _oldDeviceConnected
    )
    {
        Serial.println();
        Serial.println(
            "[BLE] ===== DISCONNECT EDGE ====="
        );

        Serial.println(
            "[BLE] Camera GATT connection lost"
        );

        Serial.printf(
            "[BLE] Session: %lu\n",
            (unsigned long)g_sessionNumber
        );

        _oldDeviceConnected = false;

        delay(500);

        if (!_deviceConnected)
        {
            if (g_manualPowerOffAdvertisingLock)
            {
                BLEDevice::stopAdvertising();

                Serial.println(
                    "[BLE] Normal advertising SUPPRESSED (manual power-off lock)"
                );
            }
            else
            {
                Serial.println(
                    "[BLE] Restarting normal advertising..."
                );

                BLEAdvertising *advertising =
                    BLEDevice::getAdvertising();

                advertising->addServiceUUID(
                    CE80_UUID
                );

                advertising->addServiceUUID(
                    D0FF_UUID
                );

                advertising->setScanResponse(
                    true
                );

                advertising->setMinPreferred(
                    0x06
                );

                advertising->setMinPreferred(
                    0x12
                );

                BLEDevice::startAdvertising();

                Serial.println(
                    "[BLE] Normal advertising restarted"
                );
            }
        }
    }
}


// ============================================================
// NATIVE GAP -> GATT STATE SYNCHRONIZATION
// ============================================================
//
// The wake advertisement is started directly with ble_gap_adv_start()
// and wakeGapEventHandler as its GAP callback. NimBLE therefore delivers
// the CONNECT event for that advertising procedure to wakeGapEventHandler,
// rather than necessarily routing it through NimBLEServer::handleGapEvent()
// and MyServerCallbacks::onConnect().
//
// The wire traffic proves that the connection is real: CE81 writes arrive
// immediately after the native CONNECT event. We therefore use the native
// CONNECT event as the authoritative connection-state transition for the
// wake advertising path.
//
// If NimBLEServer later also delivers onConnect() for the same handle,
// MyServerCallbacks detects the duplicate and leaves this session intact.
// ============================================================

void CameraBLE::handleNativeGapConnection(
    bool connected,
    uint16_t handle,
    const struct ble_gap_conn_desc *desc
)
{
    if (connected)
    {
        if (handle == BLE_HS_CONN_HANDLE_NONE)
        {
            Serial.println(
                "[GAP SYNC] CONNECT received with invalid handle"
            );
            return;
        }

        // If the normal Arduino callback already established exactly this
        // connection, do not create another session.
        if (
            _deviceConnected &&
            g_connHandle == handle
        )
        {
            Serial.printf(
                "[GAP SYNC] CONNECT duplicate ignored; handle=%u\n",
                handle
            );
            return;
        }

        ble_gap_conn_desc localDesc{};
        const ble_gap_conn_desc *connectionDesc = desc;

        if (!connectionDesc)
        {
            if (
                ble_gap_conn_find(
                    handle,
                    &localDesc
                ) == 0
            )
            {
                connectionDesc = &localDesc;
            }
        }

        Serial.println();
        Serial.println(
            ">>> GO 3S NATIVE GAP CONNECT SYNC <<<"
        );

        g_sessionNumber++;
        g_txSequence = 0;

        g_ce82NotifyEnabled = false;
        g_ce82SubscriptionValue = 0;
        g_lastCE82SubscribeEvent = 0;

        _deviceConnected = true;
        _cameraSeen = true;

        g_connHandle = handle;

        if (connectionDesc)
        {
            g_peerAddress =
                connectionDesc->peer_id_addr;

            g_peerAddressValid = true;

            g_lastConnectEvent =
                millis();

            Serial.printf(
                "[GAP SYNC] Peer connection details recovered"
            );

            Serial.println();

            Serial.printf(
                "[GAP SYNC] Peer ID type=%u (%s)\n",
                connectionDesc->peer_id_addr.type,
                connectionDesc->peer_id_addr.type == BLE_ADDR_PUBLIC ? "PUBLIC" : "RANDOM"
            );

            Serial.print("[GAP SYNC] Peer ID: ");
            printBLEAddress(connectionDesc->peer_id_addr);
            Serial.println();

            Serial.printf(
                "[GAP SYNC] Peer OTA type=%u (%s)\n",
                connectionDesc->peer_ota_addr.type,
                connectionDesc->peer_ota_addr.type == BLE_ADDR_PUBLIC ? "PUBLIC" : "RANDOM"
            );

            Serial.print("[GAP SYNC] Peer OTA: ");
            printBLEAddress(connectionDesc->peer_ota_addr);
            Serial.println();

            Serial.printf(
                "[GAP SYNC] Security: encrypted=%u authenticated=%u bonded=%u\n",
                connectionDesc->sec_state.encrypted,
                connectionDesc->sec_state.authenticated,
                connectionDesc->sec_state.bonded
            );

            Serial.printf(
                "[GAP SYNC] Interval: %u\n",
                connectionDesc->conn_itvl
            );

            Serial.printf(
                "[GAP SYNC] Latency: %u\n",
                connectionDesc->conn_latency
            );

            Serial.printf(
                "[GAP SYNC] Supervision timeout: %u\n",
                connectionDesc->supervision_timeout
            );
        }
        else
        {
            g_peerAddressValid = false;
            g_lastConnectEvent = millis();

            Serial.println(
                "[GAP SYNC] Connection descriptor not available"
            );
        }

        Serial.printf(
            "[GAP SYNC] Session: %lu\n",
            (unsigned long)g_sessionNumber
        );

        Serial.printf(
            "[GAP SYNC] GATT handle: %u\n",
            g_connHandle
        );

        Serial.println(
            "[GAP SYNC] _deviceConnected = TRUE"
        );

        Serial.println(
            "[GAP SYNC] Waiting for GO 3S CE82 subscription..."
        );

        g_reconnectDiagnosticActive = false;
        g_reconnectDiagnosticStarted = millis();
        g_lastReconnectDiagnosticPrint = 0;


        return;
    }

    // DISCONNECT
    if (
        !_deviceConnected &&
        g_connHandle == BLE_HS_CONN_HANDLE_NONE
    )
    {
        Serial.printf(
            "[GAP SYNC] DISCONNECT duplicate ignored; handle=%u\n",
            handle
        );
        return;
    }

    if (
        g_connHandle != BLE_HS_CONN_HANDLE_NONE &&
        handle != BLE_HS_CONN_HANDLE_NONE &&
        g_connHandle != handle
    )
    {
        Serial.printf(
            "[GAP SYNC] DISCONNECT for non-current handle ignored; current=%u event=%u\n",
            g_connHandle,
            handle
        );
        return;
    }

    Serial.println();
    Serial.println(
        ">>> GO 3S NATIVE GAP DISCONNECT SYNC <<<"
    );

    _deviceConnected = false;

    g_connHandle =
        BLE_HS_CONN_HANDLE_NONE;

    g_peerAddressValid = false;

    g_ce82NotifyEnabled = false;
    g_ce82SubscriptionValue = 0;
    g_lastCE82SubscribeEvent = 0;

    g_lastDisconnectEvent =
        millis();

    g_reconnectDiagnosticActive = false;

    Serial.println(
        "[GAP SYNC] _deviceConnected = FALSE"
    );

    Serial.println(
        "[GAP SYNC] CE82 subscription state cleared"
    );
}


// ============================================================
// CE82 SUBSCRIPTION HANDLER
// ============================================================

void CameraBLE::handleCE82Subscribe(
    uint16_t connHandle,
    uint16_t subValue
)
{
    // The Arduino CE82 callback gives us the real connection handle
    // and the real CE82 characteristic handle. These are the only
    // handles we need to retain for diagnostics; command readiness
    // itself is based on the Arduino GATT connection + subscription.
    if (connHandle != BLE_HS_CONN_HANDLE_NONE)
    {
        g_connHandle = connHandle;
    }

    g_ce82SubscriptionValue =
        subValue;

    g_ce82NotifyEnabled =
        (subValue & 0x0001) != 0;

    g_lastCE82SubscribeEvent =
        millis();


    Serial.printf(
        "[BLE] CE82 subscription: 0x%04X (%s)\n",
        subValue,
        g_ce82NotifyEnabled ? "notify ON" : "notify OFF"
    );
}


// ============================================================
// CE82 NOTIFY STATUS
// ============================================================

void CameraBLE::handleCE82Status(
    BLECharacteristicCallbacks::Status status,
    uint32_t code
)
{
    if (status != BLECharacteristicCallbacks::SUCCESS_NOTIFY &&
        status != BLECharacteristicCallbacks::SUCCESS_INDICATE)
    {
        Serial.printf(
            "[BLE] CE82 notify error: status=%d code=%lu\n",
            (int)status,
            (unsigned long)code
        );
    }
}


// ============================================================
// GETTERS
// ============================================================

bool CameraBLE::isWakeActive() const
{
    return g_wakeActive;
}

void CameraBLE::toggleDebug()
{
    g_debugEnabled = !g_debugEnabled;

    Serial.printf(
        "[DEBUG] Full diagnostics %s\n",
        g_debugEnabled ? "ON" : "OFF"
    );
}

bool CameraBLE::isDebugEnabled() const
{
    return g_debugEnabled;
}

bool CameraBLE::isConnected() const
{
    return _deviceConnected;
}


bool CameraBLE::isCommandChannelReady() const
{
    return
        _deviceConnected &&
        _txCharacteristic != nullptr &&
        g_ce82NotifyEnabled;
}


bool CameraBLE::isReady() const
{
    return
        _deviceConnected &&
        hasRecentPacket(15000UL);
}


bool CameraBLE::isRecordingKnown() const
{
    return _status.recordingKnown;
}


bool CameraBLE::isRecording() const
{
    return _status.recording;
}


String CameraBLE::getRecordingTimer() const
{
    if (!_status.recording || !g_localRecordingTimerActive)
        return _status.recordingTimer;

    unsigned long elapsed =
        millis() - g_recordingStartedMillis;

    unsigned long totalSeconds = elapsed / 1000UL;
    unsigned long hours = totalSeconds / 3600UL;
    unsigned long minutes = (totalSeconds % 3600UL) / 60UL;
    unsigned long seconds = totalSeconds % 60UL;

    char buffer[16];
    snprintf(
        buffer,
        sizeof(buffer),
        "%02lu:%02lu:%02lu",
        hours,
        minutes,
        seconds
    );

    return String(buffer);
}


String CameraBLE::getRemainingTime() const
{
    return _status.remainingTime;
}


String CameraBLE::getRemainingRuntime() const
{
    return _status.remainingRuntime;
}


String CameraBLE::getRemainingPhotos() const
{
    return _status.remainingPhotos;
}


uint8_t CameraBLE::getBatteryBars() const
{
    return _status.batteryBars;
}


bool CameraBLE::isBatteryKnown() const
{
    return _status.batteryKnown;
}


String CameraBLE::getBatteryDescription() const
{
    return _status.batteryDescription;
}


String CameraBLE::getModeString() const
{
    return _status.modeString;
}


String CameraBLE::getResolution() const
{
    return _status.resolution;
}


String CameraBLE::getSetting() const
{
    return _status.setting;
}


String CameraBLE::getModeName() const
{
    return _status.shootingMode;
}


String CameraBLE::getShootingMode() const
{
    return _status.shootingMode;
}


String CameraBLE::getLensMode() const
{
    return _status.lensMode;
}


String CameraBLE::getPhotoTimer() const
{
    return _status.photoTimer;
}


bool CameraBLE::isPhotoCaptureActive() const
{
    return _status.photoCaptureActive;
}


// Legacy compatibility only. PureShot is NOT exposed by the observed
// CE81 telemetry, so these values are intentionally always inactive.
bool CameraBLE::isPureShotCountdown() const
{
    return false;
}


String CameraBLE::getPureShotValue() const
{
    return "";
}


String CameraBLE::getModeCode() const
{
    return _status.modeCode;
}


unsigned long CameraBLE::getLastPacketTime() const
{
    return _lastPacketTime;
}


bool CameraBLE::hasRecentPacket(
    unsigned long timeoutMs
) const
{
    if (_lastPacketTime == 0)
        return false;

    return
        millis() - _lastPacketTime <
        timeoutMs;
}


// ============================================================
// CAMERA SHOOTING-MODE FAMILY HELPERS
// ============================================================

static bool isPhotoShootingMode(const String &mode)
{
    return
        mode == "PHOTO" ||
        mode == "HDR PHOTO" ||
        mode == "INTERVAL PHOTO" ||
        mode == "STARLAPSE";
}


static bool isVideoShootingMode(const String &mode)
{
    return
        mode == "VIDEO" ||
        mode == "FREEFRAME VIDEO" ||
        mode == "TIMELAPSE" ||
        mode == "TIMESHIFT" ||
        mode == "SLOW MOTION" ||
        mode == "LOOP RECORDING" ||
        mode == "INTERVAL VIDEO";
}


// ============================================================

// CAMERA PACKET HANDLER
// ============================================================

void CameraBLE::handleCameraPacket(
    const String &value
)
{
    _cameraSeen = true;
    _lastPacketTime = millis();

    Serial.printf(
        "[CE81 RX] HEX: %s\n",
        value.c_str()
    );

    if (isBatteryPacket(value))
    {
        if (value.length() >= 20)
        {
            String stateHex = value.substring(18, 20);

            uint8_t state =
                (uint8_t)strtoul(
                    stateHex.c_str(),
                    nullptr,
                    16
                );

            decodeBatteryState(state);
        }

        return;
    }

    // --------------------------------------------------------
    // Capture / recording state
    // --------------------------------------------------------
    if (
        value.startsWith("FEEFFE058001") &&
        value.length() == 14
    )
    {
        String stateHex = value.substring(12, 14);

        uint8_t state =
            (uint8_t)strtoul(
                stateHex.c_str(),
                nullptr,
                16
            );

        if (state == 0x01)
        {
            if (isPhotoShootingMode(_status.shootingMode))
            {
                _status.photoCaptureActive = true;
                _status.recording = false;
                _status.recordingKnown = true;

                Serial.println(
                    "[PHOTO] CAPTURE ACTIVE"
                );
            }
            else if (isVideoShootingMode(_status.shootingMode))
            {
                const bool wasRecording = _status.recording;

                _status.recording = true;
                _status.recordingKnown = true;
                _status.photoCaptureActive = false;

                if (!wasRecording || !g_localRecordingTimerActive)
                {
                    g_recordingStartedMillis = millis();
                    g_localRecordingTimerActive = true;
                    _status.recordingTimer = "00:00:00";
                }

                Serial.println(
                    "[RECORDING] ACTIVE"
                );
            }
            else
            {
                // The capture packet is authoritative for the fact that
                // a capture is active. The shooting-mode packet can arrive
                // a few milliseconds later, so do not throw this state away
                // merely because the mode is not known yet. If the later
                // mode packet identifies VIDEO, decodeType02Mode() converts
                // this temporary state into recording=true.
                _status.recording = false;
                _status.recordingKnown = false;
                _status.photoCaptureActive = true;

                Serial.println(
                    "[CAPTURE] ACTIVE - shooting mode unknown (pending mode)"
                );
            }
        }
        else if (state == 0x00)
        {
            _status.photoCaptureActive = false;
            _status.recording = false;
            _status.recordingKnown = true;
            _status.recordingTimer = "";
            g_localRecordingTimerActive = false;
            g_recordingStartedMillis = 0;

            if (isPhotoShootingMode(_status.shootingMode))
            {
                Serial.println(
                    "[PHOTO] CAPTURE INACTIVE"
                );
            }
            else
            {
                Serial.println(
                    "[RECORDING] INACTIVE"
                );
            }
        }

        return;
    }

    // --------------------------------------------------------
    // Type 0x02: status / shooting-mode code
    // --------------------------------------------------------
    // Battery packets are handled above. The remaining CE81 type 0x02
    // packets carry the mode selector byte at offset 18..19.
    if (value.startsWith("FEEFFE02") && value.length() >= 22)
    {
        decodeType02Mode(value);
        return;
    }

    // --------------------------------------------------------
    // Type 0x10: configuration / remaining time / timer
    // --------------------------------------------------------
    if (value.startsWith("FEEFFE10"))
    {
        decodeType10(value);
        return;
    }
}


// ============================================================
// TYPE 0x10 DECODER
// ============================================================

// Forward declaration: the helper implementation is kept with the
// other mode-validation helpers below, but decodeType10() needs it.
static bool isNumericString(const String &value);

void CameraBLE::decodeType10(
    const String &value
)
{
    String ascii = extractAscii(value, 16);
    ascii.trim();

    if (ascii.length() == 0)
        return;

    Serial.printf(
        "[CE81 ASCII] %s\n",
        ascii.c_str()
    );

    // Configuration strings are decoded before generic timer values.
    if (ascii.indexOf("|") >= 0)
    {
        decodeMode(ascii);
        return;
    }

    if (isRecordingTimer(ascii))
    {
        _status.recordingTimer = ascii;

        Serial.printf(
            "[RECORDING TIMER] %s\n",
            _status.recordingTimer.c_str()
        );

        return;
    }

    // Photo-count values are NOT remaining runtime.
    // The GO 3S can report "999+" when the count exceeds the
    // display range, or a plain numeric value such as "998",
    // "997", etc.
    if (ascii == "999+" || isNumericString(ascii))
    {
        _status.remainingPhotos = ascii;

        Serial.printf(
            "[PHOTOS REMAINING] %s\n",
            _status.remainingPhotos.c_str()
        );

        return;
    }

    // Values such as 2h16m / 1h42m / 25h37m are remaining runtime.
    if (isRemainingValue(ascii))
    {
        _status.remainingRuntime = ascii;
        _status.remainingTime = ascii; // Legacy API compatibility.

        // Keep the photo counter independent. The OSD selects either
        // remainingPhotos or remainingRuntime from the current shooting mode.

        Serial.printf(
            "[REMAINING RUNTIME] %s\n",
            _status.remainingRuntime.c_str()
        );

        return;
    }
}


// ============================================================
// MODE DECODER HELPERS
// ============================================================

static bool isNumericString(const String &value)
{
    if (value.length() == 0)
        return false;

    for (unsigned int i = 0; i < value.length(); i++)
    {
        if (!isdigit(value.charAt(i)))
            return false;
    }

    return true;
}


static bool isPhotoTimerValue(const String &value)
{
    if (value.length() < 2)
        return false;

    char last = value.charAt(value.length() - 1);

    if (last != 's' && last != 'S')
        return false;

    return isNumericString(
        value.substring(0, value.length() - 1)
    );
}


static bool isAspectRatioValue(const String &value)
{
    int colon = value.indexOf(':');

    if (colon <= 0 || colon >= (int)value.length() - 1)
        return false;

    return
        isNumericString(value.substring(0, colon)) &&
        isNumericString(value.substring(colon + 1));
}


static bool isVideoResolutionValue(const String &value)
{
    int k = value.indexOf('K');

    if (k < 1)
        k = value.indexOf('k');

    if (k < 1)
        return false;

    return isNumericString(
        value.substring(0, k)
    );
}


// ============================================================
// MODE DECODER
// ============================================================

void CameraBLE::decodeMode(
    const String &modeString
)
{
    _status.modeString = modeString;

    int first = modeString.indexOf("|");
    int second = (first >= 0)
        ? modeString.indexOf("|", first + 1)
        : -1;

    String field1;
    String field2;
    String field3;

    if (first >= 0)
    {
        field1 = modeString.substring(0, first);

        if (second >= 0)
        {
            field2 = modeString.substring(first + 1, second);
            field3 = modeString.substring(second + 1);
        }
        else
        {
            field2 = modeString.substring(first + 1);
        }
    }

    _status.resolution = field1;
    _status.setting = field2;
    _status.lensMode = field3;

    // IMPORTANT:
    // The exact shooting mode comes from CE81 type 0x02. The type 0x10
    // string is configuration/display data and may be identical for
    // Video, FreeFrame, Timelapse, TimeShift and Loop Recording.
    // Therefore this function MUST NOT infer VIDEO from "4K|30|MEGA".

    if (isPhotoShootingMode(_status.shootingMode))
    {
        // Configuration packets must never cancel an active photo capture.
        // The capture-state packet (FEEFFE058001xx) is the authoritative
        // source for photoCaptureActive.
        _status.recording = false;
        _status.recordingKnown = true;
        _status.recordingTimer = "";
        g_localRecordingTimerActive = false;
        g_recordingStartedMillis = 0;

        if (isAspectRatioValue(field1) &&
            isPhotoTimerValue(field2))
        {
            _status.photoTimer = field2;
        }
        else
        {
            _status.photoTimer = "";
        }
    }
    else if (isVideoShootingMode(_status.shootingMode))
    {
        _status.photoTimer = "";
        // If the capture packet arrived before the mode packet, the
        // temporary photoCaptureActive flag represents an active capture.
        // Once the actual shooting family is known to be video, convert
        // that state into recording instead of losing it.
        if (_status.photoCaptureActive)
        {
            _status.photoCaptureActive = false;
            _status.recording = true;
            _status.recordingKnown = true;
            g_recordingStartedMillis = millis();
            g_localRecordingTimerActive = true;
            _status.recordingTimer = "00:00:00";
        }
    }
    else
    {
        _status.photoTimer = "";
    }

    Serial.printf(
        "[MODE] Shooting mode: %s\n",
        _status.shootingMode.length()
            ? _status.shootingMode.c_str()
            : "UNKNOWN"
    );

    Serial.println(
        "[MODE] ------------------------------"
    );

    Serial.printf(
        "[MODE] Full: %s\n",
        _status.modeString.c_str()
    );

    Serial.printf(
        "[MODE] Field 1: %s\n",
        _status.resolution.c_str()
    );

    Serial.printf(
        "[MODE] Field 2: %s\n",
        _status.setting.c_str()
    );

    Serial.printf(
        "[MODE] Lens/FOV: %s\n",
        _status.lensMode.length()
            ? _status.lensMode.c_str()
            : ""
    );

    Serial.printf(
        "[MODE] Shooting mode: %s\n",
        _status.shootingMode.length()
            ? _status.shootingMode.c_str()
            : "UNKNOWN"
    );

    if (_status.photoTimer.length() > 0)
    {
        Serial.printf(
            "[MODE] Photo timer: %s\n",
            _status.photoTimer.c_str()
        );
    }

    Serial.println(
        "[MODE] ------------------------------"
    );
}


// ============================================================
// TYPE 0x02 SHOOTING-MODE DECODER
// ============================================================

void CameraBLE::decodeType02Mode(
    const String &value
)
{
    if (value.length() < 22 ||
        !value.startsWith("FEEFFE02"))
    {
        return;
    }

    const String modeCode =
        value.substring(18, 20);

    const String familyCode =
        value.substring(20, 22);

    _status.modeCode = modeCode;

    // Observed packets with family 0x50 occur during photo capture and
    // are not a new shooting-mode selection. They must not overwrite the
    // already known PHOTO/VIDEO mode with UNKNOWN.
    if (familyCode == "50")
    {
        Serial.printf(
            "[CAPTURE SUBSTATE] code=0x%s family=0x%s\n",
            modeCode.c_str(),
            familyCode.c_str()
        );
        return;
    }

    String modeName = "UNKNOWN";

    if (modeCode == "01")
    {
        if (familyCode == "70")
            modeName = "PHOTO";
        else if (familyCode == "60")
            modeName = "VIDEO";
    }
    else if (modeCode == "02" && familyCode == "70")
    {
        modeName = "HDR PHOTO";
    }
    else if (modeCode == "03")
    {
        if (familyCode == "60")
            modeName = "LOOP RECORDING";
        else if (familyCode == "70")
            modeName = "INTERVAL PHOTO";
    }
    else if (modeCode == "04" && familyCode == "60")
    {
        modeName = "TIMELAPSE";
    }
    else if (modeCode == "05")
    {
        if (familyCode == "60")
            modeName = "TIMESHIFT";
        else if (familyCode == "70")
            modeName = "STARLAPSE";
    }
    else if (modeCode == "06" && familyCode == "60")
    {
        modeName = "SLOW MOTION";
    }
    else if (modeCode == "13" && familyCode == "60")
    {
        modeName = "FREEFRAME VIDEO";
    }

    _status.shootingMode = modeName;
    _status.mode = modeName;

    if (isPhotoShootingMode(modeName))
    {
        // Type 0x02 identifies the shooting family, but it does not tell
        // us that a photo capture has ended. Only FEEFFE05800100 does that.
        _status.recording = false;
        _status.recordingKnown = true;
        _status.recordingTimer = "";
        g_localRecordingTimerActive = false;
        g_recordingStartedMillis = 0;
    }
    else if (isVideoShootingMode(modeName))
    {
        _status.photoTimer = "";

        // If a capture-state packet arrived before this mode packet, the
        // pending active state is actually a video recording.
        if (_status.photoCaptureActive)
        {
            _status.photoCaptureActive = false;
            _status.recording = true;
            _status.recordingKnown = true;
            g_recordingStartedMillis = millis();
            g_localRecordingTimerActive = true;
            _status.recordingTimer = "00:00:00";
        }
    }
    else
    {
        _status.photoTimer = "";
    }

    Serial.printf(
        "[MODE STATUS] code=0x%s family=0x%s -> %s\n",
        modeCode.c_str(),
        familyCode.c_str(),
        modeName.c_str()
    );
}


// ============================================================
// BATTERY DECODER
// ============================================================

void CameraBLE::decodeBatteryState(
    uint8_t state
)
{
    _status.batteryKnown = true;
    _status.batteryState = state;

    switch (state)
    {
        case 0x0A:
            _status.batteryBars = 4;
            _status.batteryDescription =
                "4 bars";
            break;

        case 0x09:
            _status.batteryBars = 3;
            _status.batteryDescription =
                "3 bars";
            break;

        case 0x08:
            _status.batteryBars = 2;
            _status.batteryDescription =
                "2 bars";
            break;

        case 0x07:
            _status.batteryBars = 1;
            _status.batteryDescription =
                "1 bar";
            break;

        case 0x06:
            _status.batteryBars = 0;
            _status.batteryDescription =
                "Critical";
            break;

        case 0x05:
            _status.batteryBars = 0;
            _status.batteryDescription =
                "Final / near shutdown";
            break;

        case 0x0B:
            _status.batteryBars = 4;
            _status.batteryDescription =
                "Docked / in pod";
            break;

        default:
            _status.batteryBars = 0;
            _status.batteryDescription =
                "Unknown";
            _status.batteryKnown = false;
            break;
    }

    Serial.printf(
        "[BATTERY] state=0x%02X bars=%u description=%s\n",
        state,
        _status.batteryBars,
        _status.batteryDescription.c_str()
    );
}


// ============================================================
// BATTERY PACKET DETECTION
// ============================================================

bool CameraBLE::isBatteryPacket(
    const String &value
)
{
    if (
        value.length() != 22
    )
    {
        return false;
    }

    if (
        !value.startsWith(
            "FEEFFE0280050054"
        )
    )
    {
        return false;
    }

    if (
        !value.endsWith(
            "50"
        )
    )
    {
        return false;
    }

    return true;
}


// ============================================================
// ASCII EXTRACTION
// ============================================================

String CameraBLE::extractAscii(
    const String &value,
    int start
)
{
    if (
        start < 0 ||
        start >= value.length()
    )
    {
        return "";
    }

    // Type 0x10 packets contain a binary header followed by one or more
    // printable ASCII runs. In the GO 3S protocol the useful value is not
    // necessarily the first run: for example a packet can contain:
    //
    //   5E 00 34 4B 7C 33 30 7C 4D 45 47 41
    //    ^      4K|30|MEGA
    //
    // The old implementation stopped at the first non-printable byte and
    // therefore returned only "^". Keep all printable runs and return
    // the longest one, which is the actual human-readable payload.
    String bestRun;
    String currentRun;

    for (
        int i = start;
        i + 1 < value.length();
        i += 2
    )
    {
        String byteHex =
            value.substring(
                i,
                i + 2
            );

        char c =
            (char)strtoul(
                byteHex.c_str(),
                nullptr,
                16
            );

        if (
            c >= 32 &&
            c <= 126
        )
        {
            currentRun += c;
        }
        else
        {
            if (
                currentRun.length() >
                bestRun.length()
            )
            {
                bestRun = currentRun;
            }

            currentRun = "";
        }
    }

    if (
        currentRun.length() >
        bestRun.length()
    )
    {
        bestRun = currentRun;
    }

    return bestRun;
}


// ============================================================
// TIMER VALIDATION
// ============================================================

bool CameraBLE::isRecordingTimer(
    const String &s
)
{
    // Normal GO 3S recording timer: HH:MM:SS.
    // Also accept H:MM:SS in case the camera drops the leading zero.
    int first = s.indexOf(':');
    int second = (first >= 0)
        ? s.indexOf(':', first + 1)
        : -1;

    if (
        first <= 0 ||
        second <= first + 1 ||
        second >= s.length() - 1
    )
    {
        return false;
    }

    // Hours may be one or more digits; minutes and seconds must be two.
    if (
        s.length() - second - 1 != 2 ||
        second - first - 1 != 2
    )
    {
        return false;
    }

    for (int i = 0; i < s.length(); i++)
    {
        if (
            i == first ||
            i == second
        )
        {
            continue;
        }

        if (
            !isdigit(
                s.charAt(i)
            )
        )
        {
            return false;
        }
    }

    return true;
}


// ============================================================
// REMAINING VALUE
// ============================================================

bool CameraBLE::isRemainingValue(
    const String &s
)
{
    if (s.length() == 0)
        return false;

    if (
        s.indexOf("h") >= 0 ||
        s.indexOf("H") >= 0
    )
    {
        return true;
    }

    return false;
}


// ============================================================
// PRINT HEX
// ============================================================

void CameraBLE::printHex(
    const String &value
)
{
    Serial.print(
        "[CE81] HEX: "
    );

    for (
        unsigned int i = 0;
        i < value.length();
        i++
    )
    {
        uint8_t b =
            (uint8_t)value[i];

        Serial.printf(
            "%02X ",
            b
        );
    }

    Serial.println();
}


// ============================================================
// PRINT ASCII
// ============================================================

void CameraBLE::printAscii(
    const String &value
)
{
    Serial.print(
        "[CE81] ASCII: "
    );

    for (
        unsigned int i = 0;
        i < value.length();
        i++
    )
    {
        uint8_t c =
            (uint8_t)value[i];

        if (
            c >= 32 &&
            c <= 126
        )
        {
            Serial.write(
                c
            );
        }
        else
        {
            Serial.print(
                '.'
            );
        }
    }

    Serial.println();
}


// ============================================================
// PRINT TX HEX
// ============================================================

void CameraBLE::printTxHex(
    const uint8_t *data,
    size_t length
)
{
    Serial.print(
        "[TX] CE82 HEX: "
    );

    for (
        size_t i = 0;
        i < length;
        i++
    )
    {
        Serial.printf(
            "%02X ",
            data[i]
        );
    }

    Serial.println();
}


// ============================================================
// GENERIC TX COMMAND
// ============================================================

bool CameraBLE::sendCommand(
    uint8_t button,
    const char *label
)
{
    if (!_deviceConnected)
    {
        Serial.printf(
            "[ERROR] %s: camera not connected\n",
            label
        );

        return false;
    }

    if (_txCharacteristic == nullptr)
    {
        Serial.printf(
            "[ERROR] %s: CE82 characteristic is NULL\n",
            label
        );

        return false;
    }


    uint8_t sequence =
        g_txSequence;

    uint8_t packet[] =
    {
        0xFC,
        0xEF,
        0xFE,
        0x86,
        sequence,
        0x03,
        0x01,
        button,
        0x00
    };


    g_txSequence += 2;


    _txCharacteristic->setValue(
        packet,
        sizeof(packet)
    );

    _txCharacteristic->notify();


    Serial.printf(
        "[TX] %s\n",
        label
    );

    Serial.printf(
        "[TX] Session: %lu\n",
        (unsigned long)g_sessionNumber
    );

    Serial.printf(
        "[TX] GATT handle: %u\n",
        g_connHandle
    );

    Serial.printf(
        "[TX] Sequence: 0x%02X\n",
        sequence
    );

    Serial.printf(
        "[TX] Button: 0x%02X\n",
        button
    );

    Serial.printf(
        "[TX] CE82 subscription before notify: 0x%04X (%s)\n",
        g_ce82SubscriptionValue,
        g_ce82NotifyEnabled
            ? "NOTIFY ENABLED"
            : "NOTIFY DISABLED"
    );

    printTxHex(
        packet,
        sizeof(packet)
    );

    Serial.println(
        "[TX] CE82 notify() executed"
    );

    return true;
}


// ============================================================
// SHUTTER
// ============================================================

void CameraBLE::shutter()
{
    sendCommand(
        0x02,
        "SHUTTER"
    );
}


// ============================================================
// MODE
// ============================================================

void CameraBLE::mode()
{
    sendCommand(
        0x01,
        "MODE"
    );
}


// ============================================================
// SCREEN TOGGLE
// ============================================================

void CameraBLE::screenToggle()
{
    sendCommand(
        0x00,
        "SCREEN TOGGLE"
    );
}


// ============================================================
// POWER OFF
// ============================================================

void CameraBLE::powerOff()
{
    // GPS Remote protocol: a real power-off is NOT a single
    // POWER click. It is a held POWER button represented by
    // repeated CE82 frames with STATE=0x03.
    //
    // The reverse-engineered remote protocol specifies roughly
    // 20 ms between frames for about 1-2 seconds. We use a
    // conservative 2 second hold. Every frame gets a fresh
    // sequence number.

    if (!_deviceConnected)
    {
        Serial.println("[ERROR] POWER OFF: camera not connected");
        return;
    }

    if (_txCharacteristic == nullptr)
    {
        Serial.println("[ERROR] POWER OFF: CE82 characteristic is NULL");
        return;
    }

    // Set the lock BEFORE the first POWER frame. The camera normally
    // disconnects as a consequence of this command; the disconnect
    // callback must therefore know that this is an intentional power-off
    // and must not restart normal advertising.
    g_manualPowerOffAdvertisingLock = true;

    // Stop any normal advertising that may still be active.
    BLEDevice::stopAdvertising();

    Serial.println();
    Serial.println("========================================");
    Serial.println("[TX] POWER OFF - HELD POWER COMMAND");
    Serial.println("[TX] Sending STATE=0x03 frames every 20 ms");
    Serial.println("========================================");

    static const uint8_t BUTTON_POWER = 0x00;
    static const uint8_t STATE_HELD = 0x03;
    static const uint8_t FRAME[8] =
    {
        0xFC,
        0xEF,
        0xFE,
        0x86,
        0x00, // sequence replaced below
        0x03,
        0x01,
        BUTTON_POWER
    };

    const unsigned long HOLD_TIME_MS = 2000UL;
    const unsigned long FRAME_INTERVAL_MS = 20UL;
    const unsigned long start = millis();
    unsigned long lastFrame = 0;
    unsigned long framesSent = 0;

    while (_deviceConnected &&
           (millis() - start) < HOLD_TIME_MS)
    {
        const unsigned long now = millis();

        if (framesSent == 0 ||
            now - lastFrame >= FRAME_INTERVAL_MS)
        {
            uint8_t packet[9];
            memcpy(packet, FRAME, sizeof(FRAME));
            packet[4] = g_txSequence;
            packet[8] = STATE_HELD;

            _txCharacteristic->setValue(
                packet,
                sizeof(packet)
            );

            _txCharacteristic->notify();

            Serial.printf(
                "[TX] POWER HOLD #%lu seq=0x%02X\n",
                framesSent + 1,
                packet[4]
            );

            printTxHex(
                packet,
                sizeof(packet)
            );

            g_txSequence += 2;
            framesSent++;
            lastFrame = now;
        }

        delay(1);
    }

    Serial.printf(
        "[TX] POWER OFF hold finished - %lu frames sent\n",
        framesSent
    );
    Serial.println();
}


// ============================================================
// WAKE-ON
// ============================================================

void CameraBLE::wake()
{
    // An explicit wake is the only operation that clears the manual
    // power-off advertising lock. From this point the normal/wake BLE
    // lifecycle is allowed again.
    g_manualPowerOffAdvertisingLock = false;

    g_wakeActive = true;

    // ========================================================
    // GO 3S WAKE BEACON
    //
    // This follows the working pattern used by the independent
    // insta360_m5StickC_remote project: ordinary Arduino BLE
    // advertising with the 26-byte Insta360/iBeacon manufacturer
    // payload. No static-random identity is forced here.
    // ========================================================
    static const uint8_t wakeManufacturerData[] =
    {
        0x4C, 0x00, 0x02, 0x15,
        0x09, 0x4F, 0x52, 0x42, 0x49, 0x54,
        0x09, 0xFF, 0x0F, 0x00,
        0x30, 0x38, 0x58, 0x52, 0x34, 0x45,
        0x00, 0x00, 0x00, 0x00,
        0xE4, 0x01
    };

    g_connHandle = BLE_HS_CONN_HANDLE_NONE;
    g_ce82NotifyEnabled = false;
    g_ce82SubscriptionValue = 0;

    Serial.println("[WAKE] Sending GO 3S wake beacon...");

    BLEDevice::stopAdvertising();
    delay(100);

    BLEAdvertising *advertising = BLEDevice::getAdvertising();
    BLEAdvertisementData adData;

    String manufacturerData;
    manufacturerData.reserve(sizeof(wakeManufacturerData));

    for (size_t i = 0; i < sizeof(wakeManufacturerData); ++i)
    {
        manufacturerData += (char)wakeManufacturerData[i];
    }

    adData.setManufacturerData(manufacturerData);
    adData.setName("Insta360 GPS Remote");

    // Same practical advertising setup used by the M5StickC remote.
    // Do not add service UUIDs here: the 26-byte manufacturer payload
    // is the part the camera needs for wake-up and must fit intact.
    advertising->setScanResponse(false);

    advertising->setAdvertisementData(adData);
    advertising->setMinPreferred(0x0);
    advertising->start();

    Serial.println("[WAKE] Beacon active (10 s window)");

    const unsigned long started = millis();

    while (millis() - started < 10000UL)
    {
        if (_deviceConnected)
        {
            Serial.println("[WAKE] Camera connected");
            g_wakeActive = false;
            return;
        }

        delay(10);
    }

    advertising->stop();

    if (_deviceConnected)
    {
        Serial.println("[WAKE] Camera connected");
        g_wakeActive = false;
        return;
    }

    // Restore the normal GATT advertising configuration.
    advertising->addServiceUUID(CE80_UUID);
    advertising->addServiceUUID(D0FF_UUID);
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMinPreferred(0x12);

    BLEDevice::startAdvertising();

    Serial.println("[WAKE] No connection - normal advertising restored");
    g_wakeActive = false;
}


// ============================================================
// CONNECTION DIAGNOSTICS
// ============================================================

void CameraBLE::printConnectionDiagnostics(
    const char *label
)
{
    // Kept as a callable compatibility/debug hook, but intentionally
    // quiet during normal operation. Detailed diagnostics are no longer
    // printed automatically on every connection/subscription event.
    (void)label;
}


// ============================================================
// STATUS
// ============================================================

void CameraBLE::printStatus()
{
    Serial.println();
    Serial.println(
        "========================================"
    );

    Serial.println(
        "          CAMERA STATUS"
    );

    Serial.println(
        "========================================"
    );

    Serial.printf(
        "GATT connected : %s\n",
        _deviceConnected
            ? "YES"
            : "NO"
    );

    Serial.printf(
        "CE82 notify    : %s\n",
        g_ce82NotifyEnabled
            ? "ENABLED"
            : "DISABLED"
    );

    Serial.printf(
        "CE82 sub value : 0x%04X\n",
        g_ce82SubscriptionValue
    );

    Serial.printf(
        "Camera seen    : %s\n",
        _cameraSeen
            ? "YES"
            : "NO"
    );

    Serial.printf(
        "Session        : %lu\n",
        (unsigned long)g_sessionNumber
    );

    Serial.printf(
        "GATT handle    : %u\n",
        g_connHandle
    );

    Serial.printf(
        "TX sequence    : 0x%02X\n",
        g_txSequence
    );

    if (g_peerAddressValid)
    {
        Serial.print(
            "Peer           : "
        );

        printBLEAddress(
            g_peerAddress
        );

        Serial.println();
    }
    else
    {
        Serial.println(
            "Peer           : NONE"
        );
    }


    if (_lastPacketTime == 0)
    {
        Serial.println(
            "CE81 telemetry : NEVER"
        );
    }
    else
    {
        Serial.printf(
            "CE81 telemetry : %lu ms ago\n",
            millis() - _lastPacketTime
        );
    }


    Serial.printf(
        "Recording known: %s\n",
        _status.recordingKnown
            ? "YES"
            : "NO"
    );

    Serial.printf(
        "Recording      : %s\n",
        _status.recording
            ? "ACTIVE"
            : "INACTIVE"
    );

    Serial.printf(
        "Timer          : %s\n",
        _status.recordingTimer.c_str()
    );

    Serial.printf(
        "Remaining      : %s\n",
        _status.remainingTime.c_str()
    );


    Serial.printf(
        "Battery known  : %s\n",
        _status.batteryKnown
            ? "YES"
            : "NO"
    );

    Serial.printf(
        "Battery state  : 0x%02X\n",
        _status.batteryState
    );

    Serial.printf(
        "Battery bars   : %u\n",
        _status.batteryBars
    );

    Serial.printf(
        "Battery desc   : %s\n",
        _status.batteryDescription.c_str()
    );


    Serial.printf(
        "Mode full      : %s\n",
        _status.modeString.c_str()
    );

    Serial.printf(
        "Resolution     : %s\n",
        _status.resolution.c_str()
    );

    Serial.printf(
        "Setting        : %s\n",
        _status.setting.c_str()
    );

    Serial.printf(
        "Mode           : %s\n",
        _status.mode.c_str()
    );

    Serial.printf(
        "Shooting mode  : %s\n",
        _status.shootingMode.c_str()
    );

    Serial.printf(
        "Lens/FOV       : %s\n",
        _status.lensMode.c_str()
    );

    Serial.printf(
        "Photo timer    : %s\n",
        _status.photoTimer.c_str()
    );

    Serial.printf(
        "Photo capture  : %s\n",
        _status.photoCaptureActive
            ? "ACTIVE"
            : "NO"
    );


    Serial.println(
        "========================================"
    );

    printConnectionDiagnostics(
        "PRINT STATUS"
    );
}