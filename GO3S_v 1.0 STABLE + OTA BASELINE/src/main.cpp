#include <Arduino.h>
#include <ArduinoOTA.h>

#include "Version.h"

#include "camera/CameraBLE.h"
#include "autorun/AutoRun.h"
#include "fc/FCInterface.h"
#include "bridge/CameraBridge.h"
#include "osd/CameraOSD.h"
#include "web/WebUI.h"
#include "led/LedStatus.h"


// ============================================================
// OBJECTS
// ============================================================

CameraBLE camera;
FCInterface fc;
CameraBridge bridge;
AutoRun autorun;
CameraOSD osd;
WebUI web;
LedStatus led;


// ============================================================
// OTA
// ============================================================

void setupOTA()
{
    // --------------------------------------------------------
    // OTA hostname
    // --------------------------------------------------------

    ArduinoOTA.setHostname("GO3S-CAM-BRIDGE");


    // --------------------------------------------------------
    // OTA password
    // Same password used by the WebUI AP
    // --------------------------------------------------------

    ArduinoOTA.setPassword("12345678");


    // --------------------------------------------------------
    // OTA callbacks
    // --------------------------------------------------------

    ArduinoOTA.onStart([]()
    {
        Serial.println();
        Serial.println("========================================");
        Serial.println("[OTA] Update started");
        Serial.println("========================================");
    });


    ArduinoOTA.onEnd([]()
    {
        Serial.println();
        Serial.println();
        Serial.println("[OTA] Update finished");
        Serial.println("[OTA] Rebooting...");
    });


    ArduinoOTA.onProgress([](unsigned int progress,
                              unsigned int total)
    {
        static unsigned int lastPercent = 999;

        unsigned int percent =
            (progress * 100U) / total;

        if (percent != lastPercent)
        {
            lastPercent = percent;

            Serial.printf(
                "[OTA] Progress: %u%%\r",
                percent
            );
        }
    });


    ArduinoOTA.onError([](ota_error_t error)
    {
        Serial.println();
        Serial.println();

        Serial.printf(
            "[OTA] Error[%u]: ",
            error
        );

        switch (error)
        {
            case OTA_AUTH_ERROR:
                Serial.println("Authentication failed");
                break;

            case OTA_BEGIN_ERROR:
                Serial.println("Begin failed");
                break;

            case OTA_CONNECT_ERROR:
                Serial.println("Connect failed");
                break;

            case OTA_RECEIVE_ERROR:
                Serial.println("Receive failed");
                break;

            case OTA_END_ERROR:
                Serial.println("End failed");
                break;

            default:
                Serial.println("Unknown error");
                break;
        }
    });


    // --------------------------------------------------------
    // Start OTA service
    // --------------------------------------------------------

    ArduinoOTA.begin();


    Serial.println();
    Serial.println("[OTA] OTA service started");
    Serial.println("[OTA] Hostname: GO3S-CAM-BRIDGE");
    Serial.println("[OTA] IP: 192.168.4.1");
    Serial.println("[OTA] Port: 3232");
    Serial.println();
}


// ============================================================
// SERIAL COMMAND HANDLER
// ============================================================

void handleSerial()
{
    if (!Serial.available())
        return;


    char c = Serial.read();


    switch (c)
    {
        case 't':
        case 'T':
            camera.shutter();
            break;


        case 'm':
        case 'M':
            camera.mode();
            break;


        case 's':
        case 'S':
            camera.screenToggle();
            break;


        case 'x':
        case 'X':
            autorun.manualPowerToggle();
            break;


        case 'w':
        case 'W':
            autorun.manualWake();
            break;


        case 'p':
        case 'P':
            camera.printStatus();
            break;


        case 'v':
        case 'V':
            camera.toggleDebug();
            break;


        case 'a':
        case 'A':
            fc.simulateArm();
            break;


        case 'd':
        case 'D':
            fc.simulateDisarm();
            break;


        case '?':
            Serial.println();
            Serial.println("Commands:");
            Serial.println("  t = SHUTTER");
            Serial.println("  m = MODE");
            Serial.println("  s = SCREEN");
            Serial.println("  x = POWER / WAKE TOGGLE");
            Serial.println("  w = FORCE WAKE-ON");
            Serial.println("  p = PRINT STATUS");
            Serial.println("  v = DEBUG ON/OFF");
            Serial.println();
            Serial.println("Simulation:");
            Serial.println("  a = SIMULATE ARM");
            Serial.println("  d = SIMULATE DISARM");
            Serial.println();
            Serial.println("  ? = HELP");
            Serial.println();
            break;


        default:
            break;
    }
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);


    delay(1000);


    Serial.println();

    Serial.println(
        "========================================"
    );

    Serial.println(
        "  Insta360 GO 3S Remote"
    );

    Serial.println(
        "  CAMERA BRIDGE + AUTORUN + FC MSP"
    );

    Serial.print(
        "  Firmware "
    );

    Serial.println(
        FIRMWARE_VERSION
    );

    Serial.println(
        "========================================"
    );

    Serial.println();


    Serial.println(
        "Camera: GO 3S (08XR4E)"
    );

    Serial.println();


    // --------------------------------------------------------
    // Camera
    // --------------------------------------------------------

    camera.begin();


    // --------------------------------------------------------
    // Flight controller
    // --------------------------------------------------------

    fc.begin();


    // --------------------------------------------------------
    // Camera Bridge
    // --------------------------------------------------------

    bridge.begin(
        fc
    );


    // --------------------------------------------------------
    // AutoRun
    // --------------------------------------------------------

    autorun.begin(
        camera,
        fc,
        bridge
    );


    // --------------------------------------------------------
    // OSD
    // --------------------------------------------------------

    osd.begin(
        camera,
        fc
    );


    // --------------------------------------------------------
    // Web UI
    // This starts the GO3S-CAM-BRIDGE access point.
    // OTA is initialized AFTER the AP exists.
    // --------------------------------------------------------

    web.begin(
        camera,
        fc,
        bridge,
        autorun
    );


    // --------------------------------------------------------
    // LED STATUS
    // --------------------------------------------------------

    led.begin();


    // --------------------------------------------------------
    // OTA
    // --------------------------------------------------------

    setupOTA();


    Serial.println();

    Serial.println(
        "[READY] Camera BLE initialized"
    );

    Serial.println(
        "[READY] FC MSP interface initialized"
    );

    Serial.println(
        "[READY] Camera Bridge initialized"
    );

    Serial.println(
        "[AUTORUN] Automatic startup sequence enabled"
    );

    Serial.println(
        "[OSD] Camera OSD initialized"
    );

    Serial.println(
        "[WEB] Web interface initialized"
    );

    Serial.println(
        "[LED] Status LED initialized"
    );

    Serial.println(
        "[OTA] OTA interface initialized"
    );

    Serial.println();


    Serial.println(
        "Commands:"
    );

    Serial.println(
        "  t = SHUTTER"
    );

    Serial.println(
        "  m = MODE"
    );

    Serial.println(
        "  s = SCREEN"
    );

    Serial.println(
        "  x = POWER / WAKE TOGGLE"
    );

    Serial.println(
        "  w = FORCE WAKE-ON"
    );

    Serial.println(
        "  p = PRINT STATUS"
    );

    Serial.println(
        "  v = DEBUG ON/OFF"
    );

    Serial.println();


    Serial.println(
        "Simulation:"
    );

    Serial.println(
        "  a = SIMULATE ARM"
    );

    Serial.println(
        "  d = SIMULATE DISARM"
    );

    Serial.println(
        "  ? = HELP"
    );

    Serial.println();


    Serial.println(
        "[WEB] Connect to Wi-Fi:"
    );

    Serial.println(
        "      GO3S-CAM-BRIDGE"
    );

    Serial.println(
        "      http://192.168.4.1"
    );

    Serial.println();


    Serial.println(
        "[OTA] Connect to OTA:"
    );

    Serial.println(
        "      GO3S-CAM-BRIDGE"
    );

    Serial.println(
        "      192.168.4.1:3232"
    );

    Serial.println();


    Serial.println(
        "[SNIFFER] CE81 RX active"
    );

    Serial.println();
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    handleSerial();


    // --------------------------------------------------------
    // OTA
    // Must be serviced continuously.
    // --------------------------------------------------------

    ArduinoOTA.handle();


    // --------------------------------------------------------
    // Camera BLE
    // --------------------------------------------------------

    camera.update();


    // --------------------------------------------------------
    // Flight controller MSP
    // --------------------------------------------------------

    fc.update();


    // --------------------------------------------------------
    // Camera Bridge
    // --------------------------------------------------------

    bridge.update();


    // --------------------------------------------------------
    // OSD
    // --------------------------------------------------------

    osd.update();


    // --------------------------------------------------------
    // AutoRun
    // --------------------------------------------------------

    autorun.update();


    // --------------------------------------------------------
    // Web UI
    // --------------------------------------------------------

    web.update();


    // --------------------------------------------------------
    // LED STATUS
    // Non-blocking
    // --------------------------------------------------------

    led.update(
        camera,
        autorun
    );


    delay(5);
}