#pragma once

#include <Arduino.h>

#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>

#include "../camera/CameraBLE.h"
#include "../autorun/AutoRun.h"
#include "../fc/FCInterface.h"
#include "../bridge/CameraBridge.h"
#include "../Version.h"


// ============================================================
// WEB UI
// ============================================================

class WebUI
{
public:

    void begin(
        CameraBLE &camera,
        FCInterface &fc,
        CameraBridge &bridge,
        AutoRun &autorun
    );

    void update();


private:

    CameraBLE *_camera = nullptr;
    FCInterface *_fc = nullptr;
    CameraBridge *_bridge = nullptr;
    AutoRun *_autorun = nullptr;

    WebServer _server;


    // --------------------------------------------------------
    // WIFI
    // --------------------------------------------------------

    static constexpr const char *AP_SSID =
        "GO3S-FPV-BRIDGE";

    static constexpr const char *AP_PASSWORD =
        "12345678";


    // --------------------------------------------------------
    // HTTP PAGES
    // --------------------------------------------------------

    void handleRoot();
    void handleCamera();
    void handleLink();
    void handleOta();
    void handleSystem();

    void sendPageStart(
        const char *title
    );

    void sendPageEnd();

    void sendCameraPage();
    void sendLinkPage();
    void sendOtaPage();
    void sendSystemPage();


    // --------------------------------------------------------
    // API
    // --------------------------------------------------------

    void handleStatus();
    void handleShutter();
    void handleMode();
    void handleScreen();
    void handlePower();
    void handleWake();

    void handleNotFound();


    // --------------------------------------------------------
    // OTA UPDATE
    // --------------------------------------------------------

    void handleUpdateUpload();
    void handleUpdateFinished();

    bool _updateStarted = false;
    bool _updateError = false;
    size_t _updateSize = 0;


    // --------------------------------------------------------
    // JSON
    // --------------------------------------------------------

    String buildStatusJson();

    String jsonEscape(
        const String &value
    );
};