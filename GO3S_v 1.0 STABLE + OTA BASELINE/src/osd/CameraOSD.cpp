#include "CameraOSD.h"
#include "../camera/CameraBLE.h"
#include "../fc/FCInterface.h"

static constexpr uint8_t SYM_BATT_FULL  = 0x90;
static constexpr uint8_t SYM_BATT_3     = 0x93;
static constexpr uint8_t SYM_BATT_2     = 0x94;
static constexpr uint8_t SYM_BATT_1     = 0x95;
static constexpr uint8_t SYM_BATT_EMPTY = 0x96;

void CameraOSD::begin(CameraBLE &camera, FCInterface &fc)
{
    _camera = &camera;
    _fc = &fc;
    _lastMessage1 = "";
    _lastMessage2 = "";
    _lastMessage3 = "";
    _lastUpdate = 0;
    _lastBlink = millis();
    _blinkState = true;
    Serial.println("[OSD] CameraOSD started");
}

void CameraOSD::update()
{
    if (_camera == nullptr || _fc == nullptr) return;
    unsigned long now = millis();

    if (now - _lastBlink >= BLINK_INTERVAL)
    {
        _lastBlink = now;
        _blinkState = !_blinkState;
    }

    if (now - _lastUpdate < UPDATE_INTERVAL) return;
    _lastUpdate = now;

    String message1 = buildCameraStatusMessage();
    String message2 = buildCameraInfoMessage();
    String message3 = buildCameraRemainingMessage();

    updateMessage1(message1);
    updateMessage2(message2);
    updateMessage3(message3);
}

String CameraOSD::buildCameraStatusMessage()
{
    if (!_camera->isConnected()) return "CAM CONNECT";

    if (_camera->isPhotoCaptureActive()) return "SHOT";

    if (_camera->isRecording())
    {
        String timer = _camera->getRecordingTimer();
        if (timer.length() > 0) return "REC " + timer;
        return "REC";
    }

    String mode = _camera->getModeName();
    mode.trim();
    if (mode.length() > 0)
    {
        mode.toUpperCase();
        if (mode.length() > 16) mode = mode.substring(0, 16);
        return mode;
    }
    return "CAM READY";
}

String CameraOSD::buildCameraInfoMessage()
{
    String message;
    message += batteryGlyph();
    message += " ";

    String modeString = _camera->getModeString();
    modeString.trim();

    if (modeString.length() > 0)
        message += modeString;
    else
    {
        String resolution = _camera->getResolution();
        String setting = _camera->getSetting();
        String lens = _camera->getLensMode();

        if (resolution.length() > 0) message += resolution;
        if (setting.length() > 0)
        {
            if (message.length() > 1) message += "|";
            message += setting;
        }
        if (lens.length() > 0)
        {
            if (message.length() > 1) message += "|";
            message += lens;
        }
    }

    if (message.length() > 16) message = message.substring(0,16);
    return message;
}

String CameraOSD::buildCameraRemainingMessage()
{
    // During an active photo capture, the photo count is authoritative.
    // Never show the video runtime in this state.
    if (_camera->isPhotoCaptureActive())
    {
        String photos = _camera->getRemainingPhotos();
        photos.trim();

        if (photos.length() == 0)
            return "REM --";

        String message = "REM " + photos;
        if (message.length() > 16)
            message = message.substring(0,16);

        return message;
    }

    String mode = _camera->getModeName();
    mode.trim();
    mode.toUpperCase();

    // Photo modes use the remaining-photo counter.
    if (mode == "PHOTO" || mode == "HDR PHOTO" || mode == "INTERVAL PHOTO")
    {
        String photos = _camera->getRemainingPhotos();
        photos.trim();

        if (photos.length() == 0)
            return "REM --";

        String message = "REM " + photos;
        if (message.length() > 16)
            message = message.substring(0,16);

        return message;
    }

    // Video / other modes use the remaining recording runtime.
    String remaining = _camera->getRemainingRuntime();
    remaining.trim();

    if (remaining.length() == 0)
        return "REM --";

    remaining.toUpperCase();

    String message = "REM " + remaining;
    if (message.length() > 16)
        message = message.substring(0,16);

    return message;
}

char CameraOSD::batteryGlyph()
{
    int bars = _camera->getBatteryBars();

    if (bars >= 4) return (char)SYM_BATT_FULL;
    if (bars == 3) return (char)SYM_BATT_3;
    if (bars == 2) return (char)SYM_BATT_2;
    if (bars == 1) return (char)SYM_BATT_1;

    return (char)SYM_BATT_EMPTY;
}

void CameraOSD::updateMessage1(const String &message)
{
    if (message == _lastMessage1) return;

    _lastMessage1 = message;
    _fc->setOsdMessage(0, message);

    Serial.printf("[OSD] MSG1: %s\\n", message.c_str());
}

void CameraOSD::updateMessage2(const String &message)
{
    if (message == _lastMessage2) return;

    _lastMessage2 = message;
    _fc->setOsdMessage(1, message);

    Serial.printf("[OSD] MSG2: %s\\n", message.c_str());
}

void CameraOSD::updateMessage3(const String &message)
{
    if (message == _lastMessage3) return;

    _lastMessage3 = message;
    _fc->setOsdMessage(2, message);

    Serial.printf("[OSD] MSG3: %s\\n", message.c_str());
}
