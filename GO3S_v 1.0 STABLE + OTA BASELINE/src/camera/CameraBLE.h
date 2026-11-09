#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>
#include <host/ble_gap.h>
#include <host/ble_hs_adv.h>

int wakeGapEventHandler(struct ble_gap_event *event, void *arg);

class CameraRxCallbacks;
class CameraTxCallbacks;
class MyServerCallbacks;

class CameraBLE
{
public:
    void begin();
    void update();

    bool isConnected() const;
    bool isWakeActive() const;
    void toggleDebug();
    bool isDebugEnabled() const;
    bool isCommandChannelReady() const;
    bool isReady() const;

    bool isRecordingKnown() const;
    bool isRecording() const;

    String getRecordingTimer() const;
    String getRemainingTime() const;
    String getRemainingRuntime() const;
    String getRemainingPhotos() const;

    uint8_t getBatteryBars() const;
    bool isBatteryKnown() const;
    String getBatteryDescription() const;

    String getModeString() const;
    String getResolution() const;
    String getSetting() const;
    String getModeName() const;

    // Shooting-mode telemetry
    String getShootingMode() const;
    String getLensMode() const;
    String getPhotoTimer() const;
    bool isPhotoCaptureActive() const;
    bool isPureShotCountdown() const;
    String getPureShotValue() const;
    String getModeCode() const;

    unsigned long getLastPacketTime() const;
    bool hasRecentPacket(unsigned long timeoutMs) const;

    void shutter();
    void mode();
    void screenToggle();
    void powerOff();
    void wake();
    void printStatus();

private:
    struct Go3SStatus
    {
        bool recording;
        bool recordingKnown;

        String recordingTimer;
        String remainingTime;
        String remainingRuntime;
        String remainingPhotos;

        // Raw telemetry configuration string, e.g.
        // "4K|30|MEGA" or "16:9|5s|MEGA".
        String modeString;

        // First field of modeString.
        // Video: resolution, e.g. "4K"
        // Photo: aspect ratio, e.g. "16:9"
        String resolution;

        // Second field of modeString.
        // Video: frame rate, e.g. "30"
        // Photo: timer, e.g. "5s"
        String setting;

        // Legacy field kept for compatibility with existing WebUI.
        // It now contains the decoded shooting mode name.
        String mode;

        // Explicit shooting mode decoded from CE81 type 0x02.
        String shootingMode;

        // Empirical GO 3S mode code from CE81 type 0x02.
        // Examples: 01=Photo/Video, 02=HDR Photo, 03=Loop/Interval,
        // 04=Timelapse, 05=TimeShift/Starlapse, 06=Slow Motion,
        // 13=FreeFrame.
        String modeCode;

        // Third field of modeString, e.g. MEGA.
        String lensMode;

        // Photo timer extracted from the second field in Photo mode.
        String photoTimer;

        // True only while a photo capture event is active.
        bool photoCaptureActive;

        // PureShot countdown telemetry.
        bool pureShotCountdown;
        String pureShotValue;

        bool batteryKnown;
        uint8_t batteryState;
        String batteryDescription;
        uint8_t batteryBars;
    };

    BLEServer *_server;
    BLECharacteristic *_txCharacteristic;

    bool _deviceConnected;
    bool _oldDeviceConnected;
    bool _cameraSeen;

    unsigned long _lastPacketTime;

    Go3SStatus _status;

    void setupBLE();

    void handleCameraPacket(const String &value);
    void decodeType10(const String &value);
    void decodeType02Mode(const String &value);
    void decodeMode(const String &modeString);
    bool decodePureShot(const String &value);
    void decodeBatteryState(uint8_t state);
    bool isBatteryPacket(const String &value);

    void printHex(const String &value);
    void printAscii(const String &value);
    void printTxHex(const uint8_t *data, size_t length);

    String extractAscii(const String &value, int start);
    bool isRecordingTimer(const String &s);
    bool isRemainingValue(const String &s);

    void handleNativeGapConnection(
        bool connected,
        uint16_t handle,
        const struct ble_gap_conn_desc *desc
    );

    void printConnectionDiagnostics(const char *label);

    bool sendCommand(uint8_t button, const char *label);

    void handleCE82Subscribe(
        uint16_t connHandle,
        uint16_t subValue
    );

    void handleCE82Status(
        BLECharacteristicCallbacks::Status status,
        uint32_t code
    );

    friend class CameraRxCallbacks;
    friend class CameraTxCallbacks;
    friend class MyServerCallbacks;
    friend int wakeGapEventHandler(struct ble_gap_event *event, void *arg);
};
