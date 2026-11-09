#pragma once

#include <Arduino.h>

class CameraBLE;
class FCInterface;

class CameraOSD
{
public:
    void begin(
        CameraBLE &camera,
        FCInterface &fc
    );

    void update();

private:

    CameraBLE *_camera = nullptr;
    FCInterface *_fc = nullptr;

    String _lastMessage1;
    String _lastMessage2;
    String _lastMessage3;

    unsigned long _lastUpdate = 0;

    static constexpr unsigned long UPDATE_INTERVAL = 1000;

    unsigned long _lastBlink = 0;
    bool _blinkState = true;

    static constexpr unsigned long BLINK_INTERVAL = 500;

    String buildCameraStatusMessage();
    String buildCameraInfoMessage();
    String buildCameraRemainingMessage();

    void updateMessage1(
        const String &message
    );

    void updateMessage2(
        const String &message
    );

    void updateMessage3(
        const String &message
    );

    char batteryGlyph();
};