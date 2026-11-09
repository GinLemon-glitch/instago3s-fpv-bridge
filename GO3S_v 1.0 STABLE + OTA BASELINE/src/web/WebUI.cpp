#include "WebUI.h"
#include "Version.h"


// ============================================================
// BEGIN
// ============================================================

void WebUI::begin(
    CameraBLE &camera,
    FCInterface &fc,
    CameraBridge &bridge,
    AutoRun &autorun
)
{
    _camera = &camera;
    _fc = &fc;
    _bridge = &bridge;
    _autorun = &autorun;

    Serial.println();
    Serial.println("[WEB] Starting Wi-Fi access point...");

    WiFi.mode(WIFI_OFF);
    delay(300);

    WiFi.mode(WIFI_AP);
    delay(300);

    WiFi.setTxPower(WIFI_POWER_8_5dBm);

    bool apStarted = WiFi.softAP(
        AP_SSID,
        AP_PASSWORD,
        6,
        0,
        4
    );

    if (!apStarted)
    {
        Serial.println("[WEB] ERROR: Wi-Fi AP start failed");
    }
    else
    {
        Serial.println("[WEB] Wi-Fi AP started");

        Serial.print("[WEB] SSID: ");
        Serial.println(AP_SSID);

        Serial.print("[WEB] IP: ");
        Serial.println(WiFi.softAPIP());
    }


    // --------------------------------------------------------
    // PAGES
    // --------------------------------------------------------

    _server.on("/", HTTP_GET, [this]()
    {
        handleRoot();
    });

    _server.on("/camera", HTTP_GET, [this]()
    {
        handleCamera();
    });

    _server.on("/link", HTTP_GET, [this]()
    {
        handleLink();
    });

    _server.on("/ota", HTTP_GET, [this]()
    {
        handleOta();
    });

    _server.on("/system", HTTP_GET, [this]()
    {
        handleSystem();
    });


    // --------------------------------------------------------
    // API
    // --------------------------------------------------------

    _server.on("/api/status", HTTP_GET, [this]()
    {
        handleStatus();
    });

    _server.on("/api/shutter", HTTP_POST, [this]()
    {
        handleShutter();
    });

    _server.on("/api/mode", HTTP_POST, [this]()
    {
        handleMode();
    });

    _server.on("/api/screen", HTTP_POST, [this]()
    {
        handleScreen();
    });

    _server.on("/api/power", HTTP_POST, [this]()
    {
        handlePower();
    });

    _server.on("/api/wake", HTTP_POST, [this]()
    {
        handleWake();
    });


    // --------------------------------------------------------
    // OTA
    // --------------------------------------------------------

    _server.on(
        "/api/update",
        HTTP_POST,
        [this]()
        {
            handleUpdateFinished();
        },
        [this]()
        {
            handleUpdateUpload();
        }
    );


    _server.onNotFound([this]()
    {
        handleNotFound();
    });


    _server.begin();

    Serial.println("[WEB] HTTP server started");
    Serial.println();
}


// ============================================================
// UPDATE
// ============================================================

void WebUI::update()
{
    _server.handleClient();
}


// ============================================================
// ROOT
// ============================================================

void WebUI::handleRoot()
{
    Serial.println("[WEB] ROOT REQUEST");

    sendCameraPage();
}


// ============================================================
// CAMERA
// ============================================================

void WebUI::handleCamera()
{
    Serial.println("[WEB] CAMERA PAGE REQUEST");

    sendCameraPage();
}


// ============================================================
// LINK
// ============================================================

void WebUI::handleLink()
{
    Serial.println("[WEB] LINK PAGE REQUEST");

    sendLinkPage();
}


// ============================================================
// OTA PAGE
// ============================================================

void WebUI::handleOta()
{
    Serial.println("[WEB] OTA PAGE REQUEST");

    sendOtaPage();
}


// ============================================================
// SYSTEM PAGE
// ============================================================

void WebUI::handleSystem()
{
    Serial.println("[WEB] SYSTEM PAGE REQUEST");

    sendSystemPage();
}


// ============================================================
// PAGE START
// ============================================================

void WebUI::sendPageStart(
    const char *title
)
{
    _server.setContentLength(
        CONTENT_LENGTH_UNKNOWN
    );

    _server.send(
        200,
        "text/html; charset=utf-8",
        ""
    );

    _server.sendContent(
        F(
R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="format-detection" content="telephone=no">

<meta http-equiv="Cache-Control" content="no-cache,no-store,must-revalidate">
<meta http-equiv="Pragma" content="no-cache">
<meta http-equiv="Expires" content="0">

<title>)rawliteral"
        )
    );

    _server.sendContent(title);

    _server.sendContent(
        F(
R"rawliteral(</title>

<style>

* {
    box-sizing:border-box;
}

html,
body {
    margin:0;
    padding:0;
    background:#0b0d10;
    color:#e9edf2;
    font-family:
        -apple-system,
        BlinkMacSystemFont,
        "Segoe UI",
        sans-serif;
}

body {
    min-height:100vh;
}

.wrap {
    max-width:900px;
    margin:auto;
    padding:16px;
}

header {
    display:flex;
    align-items:center;
    justify-content:space-between;
    margin-bottom:14px;
}

h1 {
    margin:0;
    font-size:20px;
}

h2 {
    margin:0 0 12px 0;
    font-size:12px;
    text-transform:uppercase;
    letter-spacing:.08em;
    color:#89929d;
}

nav {
    display:grid;
    grid-template-columns:repeat(4,1fr);
    gap:7px;
    margin-bottom:14px;
}

nav a {
    display:block;
    text-align:center;
    text-decoration:none;
    color:#aab3bd;
    background:#15191e;
    border:1px solid #252b32;
    border-radius:10px;
    padding:10px 4px;
    font-size:11px;
    font-weight:700;
}

nav a:hover {
    background:#20262d;
    color:#fff;
}

.dot {
    width:10px;
    height:10px;
    border-radius:50%;
    background:#68717b;
}

.dot.ok {
    background:#35d07f;
    box-shadow:0 0 10px rgba(53,208,127,.5);
}

.card {
    background:#12161a;
    border:1px solid #252b32;
    border-radius:14px;
    padding:15px;
    margin-bottom:11px;
}

.grid {
    display:grid;
    grid-template-columns:repeat(2,1fr);
    gap:9px;
}

.item {
    background:#0d1013;
    border-radius:10px;
    padding:10px;
}

.label {
    font-size:10px;
    color:#7f8994;
    text-transform:uppercase;
    letter-spacing:.06em;
}

.value {
    margin-top:4px;
    font-size:15px;
    font-weight:650;
    word-break:break-word;
}

.small {
    color:#8f98a2;
    font-size:12px;
}

.big {
    font-size:23px;
    font-weight:750;
}

.buttons {
    display:grid;
    grid-template-columns:repeat(2,1fr);
    gap:8px;
}

button {
    width:100%;
    border:0;
    border-radius:10px;
    padding:13px 8px;
    background:#27313a;
    color:#fff;
    font-size:12px;
    font-weight:750;
}

button:disabled {
    opacity:.4;
}

.okbtn {
    background:#1f7b4a;
}

.warnbtn {
    background:#8a5c1c;
}

input[type=file] {
    width:100%;
    padding:10px;
    background:#0d1013;
    border:1px solid #252b32;
    border-radius:10px;
    color:#cbd2d9;
}

progress {
    width:100%;
    height:12px;
}

pre {
    background:#090b0d;
    padding:12px;
    border-radius:10px;
    font-size:10px;
    white-space:pre-wrap;
    word-break:break-word;
    max-height:400px;
    overflow:auto;
}

footer {
    text-align:center;
    color:#68727c;
    font-size:10px;
    padding:8px 0;
}

#apiState {
    margin-top:8px;
    text-align:center;
    font-size:10px;
    color:#68727c;
}

#overlay {
    display:none;
    position:fixed;
    inset:0;
    z-index:100;
    align-items:center;
    justify-content:center;
    background:rgba(5,7,9,.94);
}

.modal {
    width:min(420px,90vw);
    background:#14191e;
    border:1px solid #303840;
    border-radius:16px;
    padding:22px;
    text-align:center;
}

@media(max-width:560px) {

    .wrap {
        padding:11px;
    }

    .grid {
        grid-template-columns:1fr;
    }

    .buttons {
        grid-template-columns:1fr;
    }

    nav a {
        font-size:10px;
        padding:9px 2px;
    }
}

</style>
</head>

<body>

<div class="wrap">

<header>
<h1>GO3S FPV BRIDGE</h1>
<span id="dot" class="dot"></span>
</header>

<nav>
<a href="/camera">CAMERA</a>
<a href="/link">LINK</a>
<a href="/ota">OTA</a>
<a href="/system">SYSTEM</a>
</nav>

)rawliteral"
        )
    );
}


// ============================================================
// PAGE END
// ============================================================

void WebUI::sendPageEnd()
{
    _server.sendContent(
        F(
R"rawliteral(

<div id="apiState">
JavaScript not started
</div>

<div id="overlay">

<div class="modal">

<h2 style="color:#fff">
FIRMWARE UPDATE
</h2>

<div id="ovText" class="small">
Uploading...
</div>

<br>

<progress
    id="ovProgress"
    max="100"
    value="0">
</progress>

<div
    id="ovPercent"
    style="margin-top:10px;font-size:24px;font-weight:750">
0%
</div>

</div>

</div>

</div>

</body>
</html>

)rawliteral"
        )
    );
}


// ============================================================
// CAMERA PAGE
// ============================================================

void WebUI::sendCameraPage()
{
    sendPageStart("Camera");

    _server.sendContent(
        F(
R"rawliteral(

<style>

/* ============================================================
   CAMERA PAGE
   ============================================================ */

.cameraHero {
    position:relative;
    overflow:hidden;
}

.cameraHeroTop {
    display:flex;
    align-items:center;
    justify-content:space-between;
    gap:12px;
}

.cameraStateBlock {
    min-width:0;
}

.cameraState {
    margin-top:5px;
    font-size:30px;
    line-height:1.05;
    font-weight:800;
    letter-spacing:-.02em;
}

.cameraStateSub {
    margin-top:7px;
    color:#89929d;
    font-size:12px;
}

.cameraStateIndicator {
    flex:0 0 auto;
    width:54px;
    height:54px;
    border-radius:50%;
    display:flex;
    align-items:center;
    justify-content:center;
    background:#1a2026;
    border:1px solid #303840;
    color:#68717b;
    font-size:20px;
}

.cameraStateIndicator.ok {
    color:#35d07f;
    border-color:#296746;
    box-shadow:
        0 0 18px rgba(53,208,127,.16);
}

.cameraStateIndicator.rec {
    color:#ff5f5f;
    border-color:#713434;
    background:#241416;
    box-shadow:
        0 0 20px rgba(255,70,70,.18);
}

.cameraHeroBottom {
    display:grid;
    grid-template-columns:1fr 1fr;
    gap:9px;
    margin-top:15px;
}

.heroMetric {
    background:#0d1013;
    border-radius:11px;
    padding:11px;
}

.heroMetric .label {
    margin-bottom:5px;
}

.heroMetricValue {
    font-size:16px;
    font-weight:750;
}

.batteryVisual {
    display:flex;
    align-items:center;
    gap:3px;
    margin-bottom:4px;
}

.batteryBar {
    width:12px;
    height:20px;
    border-radius:3px;
    background:#252b32;
}

.batteryBar.on {
    background:#35d07f;
}

.batteryText {
    margin-left:6px;
    font-size:15px;
    font-weight:750;
}

.sectionTitleRow {
    display:flex;
    align-items:center;
    justify-content:space-between;
    gap:8px;
    margin-bottom:12px;
}

.sectionTitleRow h2 {
    margin:0;
}

.sectionBadge {
    border:1px solid #303840;
    background:#0d1013;
    border-radius:999px;
    padding:4px 8px;
    font-size:9px;
    font-weight:750;
    color:#89929d;
    text-transform:uppercase;
    letter-spacing:.06em;
}

.infoGrid {
    display:grid;
    grid-template-columns:repeat(2,1fr);
    gap:9px;
}

.infoItem {
    background:#0d1013;
    border-radius:10px;
    padding:11px;
    min-width:0;
}

.infoItem .value {
    overflow:hidden;
    text-overflow:ellipsis;
}

.recordingGrid {
    display:grid;
    grid-template-columns:repeat(2,1fr);
    gap:9px;
}

.recordingMain {
    grid-column:1 / -1;
    background:#0d1013;
    border-radius:11px;
    padding:13px;
    border:1px solid #252b32;
}

.recordingMain.recordingActive {
    border-color:#713434;
    background:#1a1113;
}

.recordingLabel {
    color:#89929d;
    font-size:10px;
    text-transform:uppercase;
    letter-spacing:.07em;
}

.recordingValue {
    margin-top:5px;
    font-size:22px;
    font-weight:800;
}

.recordingDetail {
    color:#8f98a2;
    font-size:11px;
    margin-top:4px;
}

.connectionGrid {
    display:grid;
    grid-template-columns:repeat(3,1fr);
    gap:9px;
}

.connectionItem {
    background:#0d1013;
    border-radius:10px;
    padding:10px;
}

.connectionItem .value {
    font-size:13px;
}

.controlMain {
    display:grid;
    grid-template-columns:1.5fr 1fr;
    gap:9px;
    margin-bottom:9px;
}

.controlSecondary {
    display:grid;
    grid-template-columns:1fr 1fr;
    gap:9px;
}

.cameraButton {
    min-height:54px;
    font-size:13px;
    border-radius:11px;
}

.cameraShutter {
    min-height:68px;
    font-size:15px;
    background:#1f7b4a;
}

.cameraMode {
    background:#27313a;
}

.cameraPowerWake {
    background:#27313a;
}

.cameraPowerWake.off {
    background:#8a5c1c;
}

.cameraButton:active {
    transform:scale(.985);
}

.firmwareLine {
    display:flex;
    align-items:baseline;
    justify-content:space-between;
    gap:10px;
    flex-wrap:wrap;
}

.firmwareName {
    font-size:15px;
    font-weight:750;
}

@media(max-width:560px) {

    .cameraState {
        font-size:27px;
    }

    .cameraHeroBottom {
        grid-template-columns:1fr;
    }

    .infoGrid,
    .recordingGrid {
        grid-template-columns:1fr 1fr;
    }

    .connectionGrid {
        grid-template-columns:1fr;
    }

    .controlMain {
        grid-template-columns:1.3fr 1fr;
    }
}

@media(max-width:380px) {

    .infoGrid,
    .recordingGrid {
        grid-template-columns:1fr;
    }

    .controlMain,
    .controlSecondary {
        grid-template-columns:1fr;
    }

    .cameraShutter {
        min-height:58px;
    }
}

</style>


<!-- ========================================================
     CAMERA HERO
     ======================================================== -->

<div class="card cameraHero">

<div class="cameraHeroTop">

<div class="cameraStateBlock">

<div class="label">
CAMERA STATUS
</div>

<div
    id="camState"
    class="cameraState">
DISCONNECTED
</div>

<div
    id="camStateSub"
    class="cameraStateSub">
Waiting for camera...
</div>

</div>

<div
    id="cameraStateIndicator"
    class="cameraStateIndicator">
●
</div>

</div>


<div class="cameraHeroBottom">

<div class="heroMetric">

<div class="label">
BATTERY
</div>

<div
    id="battery"
    class="heroMetricValue">
--
</div>

<div
    id="batteryDescription"
    class="small">
--
</div>

</div>


<div class="heroMetric">

<div class="label">
MODE
</div>

<div
    id="modeName"
    class="heroMetricValue">
--
</div>

<div
    id="modeDetail"
    class="small">
--
</div>

</div>

</div>

</div>


<!-- ========================================================
     RECORDING
     ======================================================== -->

<div class="card">

<div class="sectionTitleRow">

<h2>Recording</h2>

<div
    id="recordingBadge"
    class="sectionBadge">
IDLE
</div>

</div>


<div class="recordingGrid">

<div
    id="recordingMain"
    class="recordingMain">

<div class="recordingLabel">
Recording status
</div>

<div
    id="recording"
    class="recordingValue">
NOT RECORDING
</div>

<div
    id="recordingTimer"
    class="recordingDetail">
--
</div>

</div>


<div class="infoItem">

<div class="label">
Remaining
</div>

<div
    id="remaining"
    class="value">
--
</div>

</div>


<div class="infoItem">

<div class="label">
Photos
</div>

<div
    id="remainingPhotos"
    class="value">
--
</div>

</div>


<div class="infoItem">

<div class="label">
Runtime
</div>

<div
    id="remainingRuntime"
    class="value">
--
</div>

</div>


<div class="infoItem">

<div class="label">
Telemetry
</div>

<div
    id="telemetry"
    class="value">
--
</div>

</div>

</div>

</div>


<!-- ========================================================
     CAMERA SETTINGS
     ======================================================== -->

<div class="card">

<div class="sectionTitleRow">

<h2>Camera</h2>

<div
    id="cameraReadyBadge"
    class="sectionBadge">
NOT READY
</div>

</div>


<div class="infoGrid">

<div class="infoItem">

<div class="label">
Mode
</div>

<div
    id="cameraMode"
    class="value">
--
</div>

</div>


<div class="infoItem">

<div class="label">
Resolution
</div>

<div
    id="resolution"
    class="value">
--
</div>

</div>


<div class="infoItem">

<div class="label">
Setting
</div>

<div
    id="setting"
    class="value">
--
</div>

</div>


<div class="infoItem">

<div class="label">
Lens
</div>

<div
    id="lens"
    class="value">
--
</div>

</div>

</div>

</div>


<!-- ========================================================
     CONNECTION
     ======================================================== -->

<div class="card">

<div class="sectionTitleRow">

<h2>Connection</h2>

<div
    id="connectionBadge"
    class="sectionBadge">
OFFLINE
</div>

</div>


<div class="connectionGrid">

<div class="connectionItem">

<div class="label">
BLE
</div>

<div
    id="connected"
    class="value">
--
</div>

</div>


<div class="connectionItem">

<div class="label">
Command
</div>

<div
    id="command"
    class="value">
--
</div>

</div>


<div class="connectionItem">

<div class="label">
Telemetry
</div>

<div
    id="connectionTelemetry"
    class="value">
--
</div>

</div>

</div>

</div>


<!-- ========================================================
     CONTROLS
     ======================================================== -->

<div class="card">

<div class="sectionTitleRow">

<h2>Controls</h2>

<div
    id="controlState"
    class="sectionBadge">
READY
</div>

</div>


<div class="controlMain">

<button
    id="shutter"
    class="cameraButton cameraShutter"
    onclick="shutter()">
SHUTTER
</button>


<button
    id="modeButton"
    class="cameraButton cameraMode"
    onclick="mode()">
MODE
</button>

</div>


<div class="controlSecondary">

<button
    id="powerWakeButton"
    class="cameraButton cameraPowerWake"
    onclick="power()">
WAKE CAMERA
</button>

</div>

</div>


<!-- ========================================================
     FIRMWARE
     ======================================================== -->

<div class="card">

<div class="sectionTitleRow">

<h2>Firmware</h2>

</div>

<div class="firmwareLine">

<div
    id="fw"
    class="firmwareName">
--
</div>

<div
    id="build"
    class="small">
--
</div>

</div>

</div>


<footer id="foot">
--
</footer>

)rawliteral"
        )
    );


    // ========================================================
    // JAVASCRIPT
    // ========================================================

    _server.sendContent(
        F(
R"rawliteral(

<script>

(function()
{

"use strict";


var modeSelecting = false;
var pollCount = 0;


/* ==========================================================
   HELPERS
   ========================================================== */

function el(id)
{
    return document.getElementById(id);
}


function text(value)
{
    if (
        value === undefined ||
        value === null ||
        value === ""
    )
        return "--";

    return String(value);
}


function setApiState(value)
{
    var e = el("apiState");

    if (e)
        e.textContent = value;
}


console.log("[WEB] CAMERA JS START");

setApiState(
    "JavaScript active - polling..."
);


/* ==========================================================
   API
   ========================================================== */

function apiPost(url)
{
    var xhr = new XMLHttpRequest();


    xhr.open(
        "POST",
        url,
        true
    );


    xhr.onreadystatechange =
        function()
        {
            if (
                xhr.readyState !== 4
            )
                return;


            if (
                xhr.status < 200 ||
                xhr.status >= 300
            )
            {
                setApiState(
                    "API error HTTP " +
                    xhr.status
                );

                return;
            }


            setApiState(
                "COMMAND SENT"
            );
        };


    xhr.onerror =
        function()
        {
            setApiState(
                "API connection error"
            );
        };


    xhr.send();
}


/* ==========================================================
   CONTROLS
   ========================================================== */

window.shutter = function()
{
    if (modeSelecting)
    {
        modeSelecting = false;

        updateModeButton();

        apiPost(
            "/api/shutter"
        );

        return;
    }


    apiPost(
        "/api/shutter"
    );
};


window.mode = function()
{
    apiPost(
        "/api/mode"
    );

    modeSelecting = true;

    updateModeButton();
};


window.wake = function()
{
    apiPost(
        "/api/wake"
    );
};


window.power = function()
{
    apiPost(
        "/api/power"
    );
};


function updateModeButton()
{
    var b =
        el("modeButton");


    if (!b)
        return;


    b.textContent =
        modeSelecting
            ? "NEXT MODE"
            : "MODE";


    var s =
        el("shutter");


    if (s)
    {
        s.textContent =
            modeSelecting
                ? "CONFIRM MODE"
                : "SHUTTER";
    }

    function updatePowerWakeButton(camera)
{
    var b =
        el("powerWakeButton");

    if (!b)
        return;

    if (camera.connected)
    {
        b.textContent =
            "POWER OFF";

        b.className =
            "cameraButton cameraPowerWake off";
    }
    else
    {
        b.textContent =
            "WAKE CAMERA";

        b.className =
            "cameraButton cameraPowerWake";
    }
}
    

    var state =
        el("controlState");


    if (state)
    {
        state.textContent =
            modeSelecting
                ? "SELECTING"
                : "READY";
    }
}


/* ==========================================================
   BATTERY
   ========================================================== */

function formatBattery(s)
{
    if (
        !s.camera ||
        !s.camera.batteryKnown
    )
    {
        return "--";
    }


    var bars =
        Number(
            s.camera.batteryBars || 0
        );


    if (bars < 0)
        bars = 0;


    if (bars > 4)
        bars = 4;


    var result =
        '<div class="batteryVisual">';


    for (
        var i = 0;
        i < 4;
        i++
    )
    {
        result +=
            '<span class="batteryBar ' +
            (
                i < bars
                    ? "on"
                    : ""
            ) +
            '"></span>';
    }


    result +=
        '<span class="batteryText">' +
        bars +
        "/4</span>";


    result +=
        "</div>";


    return result;
}


/* ==========================================================
   STATE
   ========================================================== */

function updateCameraState(camera)
{
    var state =
        el("camState");

    var sub =
        el("camStateSub");

    var indicator =
        el("cameraStateIndicator");


    if (
        !state ||
        !sub ||
        !indicator
    )
        return;


    indicator.className =
        "cameraStateIndicator";


    if (
        camera.recordingKnown &&
        camera.recording
    )
    {
        state.textContent =
            "RECORDING";


        sub.textContent =
            camera.recordingTimer
                ? camera.recordingTimer
                : "Recording active";


        indicator.className +=
            " rec";


        return;
    }


    if (camera.ready)
    {
        state.textContent =
            "READY";


        sub.textContent =
            "Camera connected and ready";


        indicator.className +=
            " ok";


        return;
    }


    if (camera.connected)
    {
        state.textContent =
            "CONNECTED";


        sub.textContent =
            camera.commandChannel
                ? "BLE connected"
                : "BLE connected · command channel unavailable";


        indicator.className +=
            " ok";


        return;
    }


    state.textContent =
        "DISCONNECTED";


    sub.textContent =
        "Waiting for camera";
}


/* ==========================================================
   RECORDING
   ========================================================== */

function updateRecording(camera)
{
    var main =
        el("recordingMain");

    var badge =
        el("recordingBadge");

    var status =
        el("recording");

    var timer =
        el("recordingTimer");


    if (
        !main ||
        !badge ||
        !status ||
        !timer
    )
        return;


    main.className =
        "recordingMain";


    if (
        camera.recordingKnown &&
        camera.recording
    )
    {
        main.className +=
            " recordingActive";


        badge.textContent =
            "RECORDING";


        status.textContent =
            "RECORDING";


        timer.textContent =
            camera.recordingTimer ||
            "Timer unavailable";


        return;
    }


    if (camera.recordingKnown)
    {
        badge.textContent =
            "IDLE";


        status.textContent =
            "NOT RECORDING";


        timer.textContent =
            "Camera is not recording";


        return;
    }


    badge.textContent =
        "UNKNOWN";


    status.textContent =
        "UNKNOWN";


    timer.textContent =
        "Waiting for camera telemetry";
}


/* ==========================================================
   CONNECTION
   ========================================================== */

function updateConnection(camera)
{
    var connected =
        el("connected");

    var command =
        el("command");

    var telemetry =
        el("connectionTelemetry");

    var badge =
        el("connectionBadge");


    if (connected)
    {
        connected.textContent =
            camera.connected
                ? "CONNECTED"
                : "OFFLINE";
    }


    if (command)
    {
        command.textContent =
            camera.commandChannel
                ? "READY"
                : "NOT READY";
    }


    if (telemetry)
    {
        telemetry.textContent =
            camera.recentTelemetry
                ? "ACTIVE"
                : "NO RECENT DATA";
    }


    if (badge)
    {
        if (camera.connected)
        {
            badge.textContent =
                camera.commandChannel
                    ? "ONLINE"
                    : "BLE ONLY";
        }
        else
        {
            badge.textContent =
                "OFFLINE";
        }
    }
}


/* ==========================================================
   PAGE UPDATE
   ========================================================== */

function updatePage(s)
{
    if (!s)
    {
        setApiState(
            "STATUS EMPTY OBJECT"
        );

        return;
    }


    var camera =
        s.camera || {};


    var firmware =
        s.firmware || {};


    var system =
        s.system || {};


    /* --------------------------------------------------------
       HEADER DOT
       -------------------------------------------------------- */

    el("dot").className =
        "dot " +
        (
            camera.connected
                ? "ok"
                : ""
        );


    /* --------------------------------------------------------
       CAMERA STATE
       -------------------------------------------------------- */

    updateCameraState(
        camera
    );


    /* --------------------------------------------------------
       BATTERY
       -------------------------------------------------------- */

    var battery =
        el("battery");


    if (battery)
    {
        battery.innerHTML =
            formatBattery(s);
    }


    el("batteryDescription").textContent =
        text(
            camera.batteryDescription
        );


    /* --------------------------------------------------------
       MODE
       -------------------------------------------------------- */

    var currentMode =
        camera.modeName ||
        camera.mode ||
        "";


    el("modeName").textContent =
        text(currentMode);


    el("modeDetail").textContent =
        text(
            camera.mode
        );


    el("cameraMode").textContent =
        text(currentMode);


    /* --------------------------------------------------------
       SETTINGS
       -------------------------------------------------------- */

    el("resolution").textContent =
        text(
            camera.resolution
        );


    el("setting").textContent =
        text(
            camera.setting
        );


    el("lens").textContent =
        text(
            camera.lensMode
        );


    /* --------------------------------------------------------
       RECORDING
       -------------------------------------------------------- */

    updateRecording(
        camera
    );


    el("remaining").textContent =
        text(
            camera.remaining
        );


    el("remainingPhotos").textContent =
        text(
            camera.remainingPhotos
        );


    el("remainingRuntime").textContent =
        text(
            camera.remainingRuntime
        );


    el("telemetry").textContent =
        camera.recentTelemetry
            ? "ACTIVE"
            : "NO RECENT DATA";


    /* --------------------------------------------------------
       CONNECTION
       -------------------------------------------------------- */

    updateConnection(
        camera
    );


    /* --------------------------------------------------------
       READY BADGE
       -------------------------------------------------------- */

    var readyBadge =
        el("cameraReadyBadge");


    if (readyBadge)
    {
        readyBadge.textContent =
            camera.ready
                ? "READY"
                : (
                    camera.connected
                        ? "CONNECTED"
                        : "OFFLINE"
                );
    }


    /* --------------------------------------------------------
       FIRMWARE
       -------------------------------------------------------- */

    el("fw").textContent =
        (
            firmware.name ||
            "--"
        ) +
        " " +
        (
            firmware.version ||
            ""
        );


    el("build").textContent =
        (
            firmware.buildDate ||
            ""
        ) +
        " " +
        (
            firmware.buildTime ||
            ""
        );


    /* --------------------------------------------------------
       FOOTER
       -------------------------------------------------------- */

    el("foot").textContent =
        "IP " +
        (
            system.ip ||
            "--"
        ) +
        " · clients " +
        (
            system.clients !== undefined
                ? system.clients
                : "--"
        ) +
        " · heap " +
        (
            system.freeHeap !== undefined
                ? system.freeHeap
                : "--"
        );


    pollCount++;


    setApiState(
        "STATUS OK · poll " +
        pollCount
    );


    updateModeButton();
}


/* ==========================================================
   POLLING
   ========================================================== */

function poll()
{
    var xhr =
        new XMLHttpRequest();


    xhr.open(
        "GET",
        "/api/status?t=" +
        Date.now(),
        true
    );


    xhr.setRequestHeader(
        "Cache-Control",
        "no-cache"
    );


    xhr.onreadystatechange =
        function()
        {
            if (
                xhr.readyState !== 4
            )
                return;


            if (
                xhr.status < 200 ||
                xhr.status >= 300
            )
            {
                setApiState(
                    "STATUS HTTP ERROR " +
                    xhr.status
                );


                console.error(
                    "[WEB] STATUS HTTP ERROR",
                    xhr.status
                );


                return;
            }


            var raw =
                xhr.responseText;


            if (
                !raw ||
                raw.length === 0
            )
            {
                setApiState(
                    "STATUS EMPTY RESPONSE"
                );


                console.error(
                    "[WEB] STATUS EMPTY RESPONSE"
                );


                return;
            }


            try
            {
                var data =
                    JSON.parse(raw);


                updatePage(
                    data
                );
            }
            catch(e)
            {
                setApiState(
                    "STATUS JSON ERROR"
                );


                console.error(
                    "[WEB] STATUS JSON ERROR",
                    e
                );


                console.log(
                    "[WEB] RAW STATUS:",
                    raw
                );
            }
        };


    xhr.onerror =
        function()
        {
            setApiState(
                "STATUS CONNECTION ERROR"
            );


            console.error(
                "[WEB] STATUS CONNECTION ERROR"
            );
        };


    xhr.ontimeout =
        function()
        {
            setApiState(
                "STATUS TIMEOUT"
            );


            console.error(
                "[WEB] STATUS TIMEOUT"
            );
        };


    xhr.timeout = 5000;


    xhr.send();
}


/* ==========================================================
   START
   ========================================================== */

setInterval(
    poll,
    700
);


poll();


})();

</script>

)rawliteral"
        )
    );

    sendPageEnd();
}


// ============================================================
// LINK PAGE
// ============================================================

void WebUI::sendLinkPage()
{
    sendPageStart("Link");

    _server.sendContent(
        F(
R"rawliteral(

<div class="card">

<h2>FC link</h2>

<div class="grid">

<div class="item">
<div class="label">MSP link</div>
<div id="fcLink" class="value">--</div>
</div>

<div class="item">
<div class="label">Armed</div>
<div id="armed" class="value">--</div>
</div>

<div class="item">
<div class="label">RC</div>
<div id="rc" class="value">--</div>
</div>

<div class="item">
<div class="label">RC channels</div>
<div id="rcc" class="value">--</div>
</div>

<div class="item">
<div class="label">Shutter channel</div>
<div id="sch" class="value">--</div>
</div>

<div class="item">
<div class="label">Shutter value</div>
<div id="sv" class="value">--</div>
</div>

</div>
</div>


<div class="card">

<h2>Camera link</h2>

<div class="grid">

<div class="item">
<div class="label">BLE</div>
<div id="ble" class="value">--</div>
</div>

<div class="item">
<div class="label">Command channel</div>
<div id="cmd" class="value">--</div>
</div>

<div class="item">
<div class="label">Telemetry</div>
<div id="tel" class="value">--</div>
</div>

<div class="item">
<div class="label">Camera ready</div>
<div id="ready" class="value">--</div>
</div>

</div>
</div>


<div class="card">

<h2>AutoRun</h2>

<div class="grid">

<div class="item">
<div class="label">State</div>
<div id="arState" class="value">--</div>
</div>

<div class="item">
<div class="label">Power-off lock</div>
<div id="lock" class="value">--</div>
</div>

</div>
</div>


<footer id="foot">
--
</footer>

)rawliteral"
        )
    );

    _server.sendContent(
        F(
R"rawliteral(

<script>

(function()
{

"use strict";


var pollCount = 0;


function el(id)
{
    return document.getElementById(id);
}


console.log("[WEB] LINK JS START");


function updatePage(s)
{
    var camera =
        s.camera || {};

    var fc =
        s.fc || {};

    var autorun =
        s.autorun || {};

    var system =
        s.system || {};


    el("dot").className =
        "dot " +
        (
            camera.connected
                ? "ok"
                : ""
        );


    el("fcLink").textContent =
        fc.link
            ? "CONNECTED"
            : "OFFLINE";


    el("armed").textContent =
        fc.armed
            ? "ARMED"
            : "DISARMED";


    el("rc").textContent =
        fc.rcValid
            ? "VALID"
            : "INVALID";


    el("rcc").textContent =
        fc.rcCount !== undefined
            ? fc.rcCount
            : "--";


    el("sch").textContent =
        fc.shutterChannel !== undefined
            ? "CH" + fc.shutterChannel
            : "--";


    el("sv").textContent =
        fc.shutterValue !== undefined
            ? fc.shutterValue
            : "--";


    el("ble").textContent =
        camera.connected
            ? "CONNECTED"
            : "OFFLINE";


    el("cmd").textContent =
        camera.commandChannel
            ? "READY"
            : "NOT READY";


    el("tel").textContent =
        camera.recentTelemetry
            ? "ACTIVE"
            : "NO DATA";


    el("ready").textContent =
        camera.ready
            ? "YES"
            : "NO";


    el("arState").textContent =
        autorun.state ||
        "--";


    el("lock").textContent =
        autorun.manualPowerOff
            ? "LOCKED"
            : "NORMAL";


    el("foot").textContent =
        "IP " +
        (
            system.ip || "--"
        ) +
        " · clients " +
        (
            system.clients !== undefined
                ? system.clients
                : "--"
        ) +
        " · heap " +
        (
            system.freeHeap !== undefined
                ? system.freeHeap
                : "--"
        );


    pollCount++;


    var state =
        document.getElementById(
            "apiState"
        );

    if (state)
    {
        state.textContent =
            "STATUS OK · poll " +
            pollCount;
    }
}


function poll()
{
    var xhr =
        new XMLHttpRequest();


    xhr.open(
        "GET",
        "/api/status?t=" +
        Date.now(),
        true
    );


    xhr.setRequestHeader(
        "Cache-Control",
        "no-cache"
    );


    xhr.onreadystatechange =
        function()
        {
            if (
                xhr.readyState !== 4
            )
                return;


            if (
                xhr.status < 200 ||
                xhr.status >= 300
            )
            {
                var state =
                    document.getElementById(
                        "apiState"
                    );

                if (state)
                {
                    state.textContent =
                        "STATUS HTTP ERROR " +
                        xhr.status;
                }

                console.error(
                    "[WEB] LINK STATUS HTTP ERROR",
                    xhr.status
                );

                return;
            }


            if (
                !xhr.responseText
            )
            {
                var state =
                    document.getElementById(
                        "apiState"
                    );

                if (state)
                {
                    state.textContent =
                        "STATUS EMPTY RESPONSE";
                }

                return;
            }


            try
            {
                updatePage(
                    JSON.parse(
                        xhr.responseText
                    )
                );
            }
            catch(e)
            {
                var state =
                    document.getElementById(
                        "apiState"
                    );

                if (state)
                {
                    state.textContent =
                        "STATUS JSON ERROR";
                }

                console.error(
                    "[WEB] LINK STATUS JSON ERROR",
                    e
                );

                console.log(
                    "[WEB] RAW STATUS:",
                    xhr.responseText
                );
            }
        };


    xhr.onerror =
        function()
        {
            var state =
                document.getElementById(
                    "apiState"
                );

            if (state)
            {
                state.textContent =
                    "STATUS CONNECTION ERROR";
            }

            console.error(
                "[WEB] LINK STATUS CONNECTION ERROR"
            );
        };


    xhr.ontimeout =
        function()
        {
            var state =
                document.getElementById(
                    "apiState"
                );

            if (state)
            {
                state.textContent =
                    "STATUS TIMEOUT";
            }
        };


    xhr.timeout = 5000;

    xhr.send();
}


setInterval(
    poll,
    700
);

poll();

})();

</script>

)rawliteral"
        )
    );

    sendPageEnd();
}


// ============================================================
// OTA PAGE
// ============================================================

void WebUI::sendOtaPage()
{
    sendPageStart("OTA");

    _server.sendContent(
        F(
R"rawliteral(

<div class="card">

<h2>Current firmware</h2>

<div class="big">
)rawliteral"
        )
    );

    _server.sendContent(FIRMWARE_NAME);
    _server.sendContent(" ");
    _server.sendContent(FIRMWARE_VERSION);

    _server.sendContent(
        F(
R"rawliteral(
</div>

<div class="small">
Build
)rawliteral"
        )
    );

    _server.sendContent(FIRMWARE_BUILD_DATE);
    _server.sendContent(" ");
    _server.sendContent(FIRMWARE_BUILD_TIME);

    _server.sendContent(
        F(
R"rawliteral(
</div>

</div>


<div class="card">

<h2>Firmware upload</h2>

<input
    id="file"
    type="file"
    accept=".bin">

<br><br>

<button
    id="upload"
    class="okbtn"
    onclick="upload()">
UPLOAD FIRMWARE
</button>

<br><br>

<progress
    id="progress"
    max="100"
    value="0">
</progress>

<div
    id="status"
    class="small"
    style="margin-top:8px">
Select a .bin file.
</div>

</div>


<footer>
OTA port 3232 also available via ArduinoOTA
</footer>

)rawliteral"
        )
    );

    _server.sendContent(
        F(
R"rawliteral(

<script>

(function()
{

"use strict";


function el(id)
{
    return document.getElementById(id);
}


var uploading = false;


console.log("[WEB] OTA JS START");


function lock(v)
{
    uploading = v;

    document
        .querySelectorAll(
            "button,input"
        )
        .forEach(
            function(x)
            {
                x.disabled = v;
            }
        );

    el("overlay").style.display =
        v
            ? "flex"
            : "none";
}


window.upload = function()
{
    if (uploading)
        return;


    var file =
        el("file").files[0];


    if (!file)
    {
        el("status").textContent =
            "Select a .bin file.";

        return;
    }


    if (
        !file.name
            .toLowerCase()
            .endsWith(".bin")
    )
    {
        el("status").textContent =
            "Invalid firmware file.";

        return;
    }


    lock(true);


    el("ovText").textContent =
        "Uploading " +
        file.name;


    var xhr =
        new XMLHttpRequest();


    xhr.open(
        "POST",
        "/api/update",
        true
    );


    xhr.upload.onprogress =
        function(event)
        {
            if (!event.lengthComputable)
                return;


            var p =
                Math.round(
                    event.loaded *
                    100 /
                    event.total
                );


            el("progress").value = p;
            el("ovProgress").value = p;
            el("ovPercent").textContent =
                p + "%";
        };


    xhr.onreadystatechange =
        function()
        {
            if (
                xhr.readyState !== 4
            )
                return;


            if (
                xhr.status >= 200 &&
                xhr.status < 300
            )
            {
                el("ovText").textContent =
                    "Update successful. Rebooting...";

                el("ovProgress").value =
                    100;

                el("ovPercent").textContent =
                    "100%";


                setTimeout(
                    function()
                    {
                        waitBoot(0);
                    },
                    1800
                );
            }
            else
            {
                lock(false);

                el("status").textContent =
                    "Update failed: HTTP " +
                    xhr.status;
            }
        };


    xhr.onerror =
        function()
        {
            el("ovText").textContent =
                "Connection lost. Checking reboot...";

            setTimeout(
                function()
                {
                    waitBoot(0);
                },
                1500
            );
        };


    xhr.send(file);
};


function waitBoot(n)
{
    var xhr =
        new XMLHttpRequest();


    xhr.open(
        "GET",
        "/api/status?t=" +
        Date.now(),
        true
    );


    xhr.timeout = 2000;


    xhr.onreadystatechange =
        function()
        {
            if (
                xhr.readyState !== 4
            )
                return;


            if (
                xhr.status >= 200 &&
                xhr.status < 300
            )
            {
                window.location.replace(
                    "/?t=" +
                    Date.now()
                );

                return;
            }


            retry();
        };


    xhr.onerror = retry;
    xhr.ontimeout = retry;


    function retry()
    {
        if (n < 30)
        {
            setTimeout(
                function()
                {
                    waitBoot(n + 1);
                },
                500
            );
        }
        else
        {
            lock(false);

            el("status").textContent =
                "Bridge did not come back online.";
        }
    }


    xhr.send();
}

})();

</script>

)rawliteral"
        )
    );

    sendPageEnd();
}


// ============================================================
// SYSTEM PAGE
// ============================================================

void WebUI::sendSystemPage()
{
    sendPageStart("System");

    _server.sendContent(
        F(
R"rawliteral(

<div class="card">

<h2>System</h2>

<div class="grid">

<div class="item">
<div class="label">IP</div>
<div id="ip" class="value">--</div>
</div>

<div class="item">
<div class="label">Wi-Fi clients</div>
<div id="clients" class="value">--</div>
</div>

<div class="item">
<div class="label">Uptime</div>
<div id="uptime" class="value">--</div>
</div>

<div class="item">
<div class="label">Free heap</div>
<div id="heap" class="value">--</div>
</div>

</div>
</div>


<div class="card">

<h2>Firmware</h2>

<div class="grid">

<div class="item">
<div class="label">Name</div>
<div id="name" class="value">--</div>
</div>

<div class="item">
<div class="label">Version</div>
<div id="version" class="value">--</div>
</div>

<div class="item">
<div class="label">Build date</div>
<div id="date" class="value">--</div>
</div>

<div class="item">
<div class="label">Build time</div>
<div id="time" class="value">--</div>
</div>

</div>
</div>


<div class="card">

<h2>Raw status</h2>

<pre id="raw">
Loading...
</pre>

</div>


<footer>
GO3S CAM BRIDGE
</footer>

)rawliteral"
        )
    );

    _server.sendContent(
        F(
R"rawliteral(

<script>

(function()
{

"use strict";


var pollCount = 0;


function el(id)
{
    return document.getElementById(id);
}


console.log("[WEB] SYSTEM JS START");


function updatePage(s)
{
    var system =
        s.system || {};

    var firmware =
        s.firmware || {};


    el("dot").className =
        "dot " +
        (
            s.camera &&
            s.camera.connected
                ? "ok"
                : ""
        );


    el("ip").textContent =
        system.ip || "--";


    el("clients").textContent =
        system.clients !== undefined
            ? system.clients
            : "--";


    el("uptime").textContent =
        system.uptime !== undefined
            ? (
                Math.floor(
                    Number(system.uptime) /
                    1000
                ) +
                " s"
            )
            : "--";


    el("heap").textContent =
        system.freeHeap !== undefined
            ? system.freeHeap + " bytes"
            : "--";


    el("name").textContent =
        firmware.name || "--";


    el("version").textContent =
        firmware.version || "--";


    el("date").textContent =
        firmware.buildDate || "--";


    el("time").textContent =
        firmware.buildTime || "--";


    el("raw").textContent =
        JSON.stringify(
            s,
            null,
            2
        );


    pollCount++;


    var state =
        document.getElementById(
            "apiState"
        );

    if (state)
    {
        state.textContent =
            "STATUS OK · poll " +
            pollCount;
    }
}


function poll()
{
    var xhr =
        new XMLHttpRequest();


    xhr.open(
        "GET",
        "/api/status?t=" +
        Date.now(),
        true
    );


    xhr.setRequestHeader(
        "Cache-Control",
        "no-cache"
    );


    xhr.onreadystatechange =
        function()
        {
            if (
                xhr.readyState !== 4
            )
                return;


            if (
                xhr.status < 200 ||
                xhr.status >= 300
            )
            {
                var state =
                    document.getElementById(
                        "apiState"
                    );

                if (state)
                {
                    state.textContent =
                        "STATUS HTTP ERROR " +
                        xhr.status;
                }

                console.error(
                    "[WEB] SYSTEM STATUS HTTP ERROR",
                    xhr.status
                );

                return;
            }


            if (
                !xhr.responseText
            )
            {
                var state =
                    document.getElementById(
                        "apiState"
                    );

                if (state)
                {
                    state.textContent =
                        "STATUS EMPTY RESPONSE";
                }

                return;
            }


            try
            {
                updatePage(
                    JSON.parse(
                        xhr.responseText
                    )
                );
            }
            catch(e)
            {
                var state =
                    document.getElementById(
                        "apiState"
                    );

                if (state)
                {
                    state.textContent =
                        "STATUS JSON ERROR";
                }

                console.error(
                    "[WEB] SYSTEM STATUS JSON ERROR",
                    e
                );

                console.log(
                    "[WEB] RAW STATUS:",
                    xhr.responseText
                );
            }
        };


    xhr.onerror =
        function()
        {
            var state =
                document.getElementById(
                    "apiState"
                );

            if (state)
            {
                state.textContent =
                    "STATUS CONNECTION ERROR";
            }

            console.error(
                "[WEB] SYSTEM STATUS CONNECTION ERROR"
            );
        };


    xhr.ontimeout =
        function()
        {
            var state =
                document.getElementById(
                    "apiState"
                );

            if (state)
            {
                state.textContent =
                    "STATUS TIMEOUT";
            }
        };


    xhr.timeout = 5000;

    xhr.send();
}


setInterval(
    poll,
    1000
);

poll();

})();

</script>

)rawliteral"
        )
    );

    sendPageEnd();
}


// ============================================================
// STATUS
// ============================================================

void WebUI::handleStatus()
{
    String status =
        buildStatusJson();

    _server.send(
        200,
        "application/json",
        status
    );
}


// ============================================================
// SHUTTER
// ============================================================

void WebUI::handleShutter()
{
    if (!_camera)
    {
        _server.send(
            500,
            "application/json",
            "{\"ok\":false,\"error\":\"camera unavailable\"}"
        );

        return;
    }

    Serial.println("[WEB] SHUTTER");

    _camera->shutter();

    _server.send(
        200,
        "application/json",
        "{\"ok\":true,\"command\":\"shutter\"}"
    );
}


// ============================================================
// MODE
// ============================================================

void WebUI::handleMode()
{
    if (!_camera)
    {
        _server.send(
            500,
            "application/json",
            "{\"ok\":false,\"error\":\"camera unavailable\"}"
        );

        return;
    }

    Serial.println("[WEB] MODE");

    _camera->mode();

    _server.send(
        200,
        "application/json",
        "{\"ok\":true,\"command\":\"mode\"}"
    );
}


// ============================================================
// SCREEN
// ============================================================

void WebUI::handleScreen()
{
    if (!_camera)
    {
        _server.send(
            500,
            "application/json",
            "{\"ok\":false,\"error\":\"camera unavailable\"}"
        );

        return;
    }

    Serial.println("[WEB] SCREEN");

    _camera->screenToggle();

    _server.send(
        200,
        "application/json",
        "{\"ok\":true,\"command\":\"screen\"}"
    );
}


// ============================================================
// POWER
// ============================================================

void WebUI::handlePower()
{
    if (!_autorun)
    {
        _server.send(
            500,
            "application/json",
            "{\"ok\":false,\"error\":\"autorun unavailable\"}"
        );

        return;
    }

    Serial.println("[WEB] POWER / WAKE TOGGLE");

    _autorun->manualPowerToggle();

    _server.send(
        200,
        "application/json",
        "{\"ok\":true,\"command\":\"power\"}"
    );
}


// ============================================================
// WAKE
// ============================================================

void WebUI::handleWake()
{
    if (!_autorun)
    {
        _server.send(
            500,
            "application/json",
            "{\"ok\":false,\"error\":\"autorun unavailable\"}"
        );

        return;
    }

    Serial.println("[WEB] FORCE WAKE");

    _autorun->manualWake();

    _server.send(
        200,
        "application/json",
        "{\"ok\":true,\"command\":\"wake\"}"
    );
}


// ============================================================
// NOT FOUND
// ============================================================

void WebUI::handleNotFound()
{
    _server.send(
        404,
        "text/plain",
        "404 - Not Found"
    );
}


// ============================================================
// OTA UPLOAD
// ============================================================

void WebUI::handleUpdateUpload()
{
    HTTPUpload& upload =
        _server.upload();


    if (
        upload.status ==
        UPLOAD_FILE_START
    )
    {
        Serial.println();
        Serial.println(
            "[WEB] OTA upload started"
        );

        _updateStarted = true;
        _updateError = false;
        _updateSize = 0;


        String filename =
            upload.filename;


        Serial.print(
            "[WEB] Filename: "
        );

        Serial.println(
            filename
        );


        if (
            !filename.endsWith(
                ".bin"
            )
        )
        {
            Serial.println(
                "[WEB] ERROR: invalid firmware file"
            );

            _updateError = true;

            return;
        }


        if (
            !Update.begin(
                UPDATE_SIZE_UNKNOWN,
                U_FLASH
            )
        )
        {
            Serial.println(
                "[WEB] ERROR: Update.begin failed"
            );

            Update.printError(
                Serial
            );

            _updateError = true;

            return;
        }


        Serial.println(
            "[WEB] OTA update ready"
        );
    }


    else if (
        upload.status ==
        UPLOAD_FILE_WRITE
    )
    {
        if (_updateError)
            return;


        if (
            upload.currentSize > 0
        )
        {
            size_t written =
                Update.write(
                    upload.buf,
                    upload.currentSize
                );


            if (
                written !=
                upload.currentSize
            )
            {
                Serial.println(
                    "[WEB] ERROR: Update.write failed"
                );

                Update.printError(
                    Serial
                );

                _updateError = true;

                return;
            }


            _updateSize += written;


            Serial.print(
                "[WEB] OTA received: "
            );

            Serial.print(
                _updateSize
            );

            Serial.println(
                " bytes"
            );
        }
    }


    else if (
        upload.status ==
        UPLOAD_FILE_END
    )
    {
        if (_updateError)
            return;


        if (
            Update.end(true)
        )
        {
            Serial.print(
                "[WEB] OTA upload complete: "
            );

            Serial.print(
                upload.totalSize
            );

            Serial.println(
                " bytes"
            );
        }
        else
        {
            Serial.println(
                "[WEB] ERROR: Update.end failed"
            );

            Update.printError(
                Serial
            );

            _updateError = true;
        }
    }


    else if (
        upload.status ==
        UPLOAD_FILE_ABORTED
    )
    {
        Serial.println(
            "[WEB] OTA upload aborted"
        );

        _updateError = true;

        Update.abort();
    }
}


// ============================================================
// OTA FINISHED
// ============================================================

void WebUI::handleUpdateFinished()
{
    if (!_updateStarted)
    {
        _server.send(
            400,
            "application/json",
            "{\"ok\":false,\"error\":\"no update\"}"
        );

        return;
    }


    if (_updateError)
    {
        _server.send(
            500,
            "application/json",
            "{\"ok\":false,\"error\":\"firmware update failed\"}"
        );

        _updateStarted = false;

        return;
    }


    if (!Update.hasError())
    {
        Serial.println(
            "[WEB] Firmware update successful"
        );


        _server.send(
            200,
            "application/json",
            "{\"ok\":true,\"message\":\"update successful, rebooting\"}"
        );


        delay(1500);

        ESP.restart();

        return;
    }


    _server.send(
        500,
        "application/json",
        "{\"ok\":false,\"error\":\"firmware update failed\"}"
    );

    _updateStarted = false;
}


// ============================================================
// JSON ESCAPE
// ============================================================

String WebUI::jsonEscape(
    const String &value
)
{
    String out;

    out.reserve(
        value.length() + 8
    );


    for (
        size_t i = 0;
        i < value.length();
        i++
    )
    {
        char c =
            value[i];


        switch (c)
        {
            case '"':
                out += "\\\"";
                break;

            case '\\':
                out += "\\\\";
                break;

            case '\b':
                out += "\\b";
                break;

            case '\f':
                out += "\\f";
                break;

            case '\n':
                out += "\\n";
                break;

            case '\r':
                out += "\\r";
                break;

            case '\t':
                out += "\\t";
                break;

            default:
                out += c;
                break;
        }
    }


    return out;
}


// ============================================================
// STATUS JSON
// ============================================================

String WebUI::buildStatusJson()
{
    String json;

    json.reserve(
        3600
    );


    // --------------------------------------------------------
    // CAMERA
    // --------------------------------------------------------

    bool cameraConnected =
        _camera &&
        _camera->isConnected();


    bool commandChannel =
        _camera &&
        _camera->isCommandChannelReady();


    bool cameraReady =
        _camera &&
        _camera->isReady();


    bool recordingKnown =
        _camera &&
        _camera->isRecordingKnown();


    bool recording =
        _camera &&
        _camera->isRecording();


    bool batteryKnown =
        _camera &&
        _camera->isBatteryKnown();


    uint8_t batteryBars =
        _camera
            ? _camera->getBatteryBars()
            : 0;


    bool recentTelemetry =
        _camera &&
        _camera->hasRecentPacket(
            15000
        );


    // --------------------------------------------------------
    // FC
    // --------------------------------------------------------

    bool fcLink =
        _fc &&
        _fc->isLinkUp();


    bool armed =
        _fc &&
        _fc->isArmed();


    bool rcValid =
        _fc &&
        _fc->isRcValid();


    // --------------------------------------------------------
    // SHUTTER RC
    // --------------------------------------------------------

    uint8_t shutterChannel =
        _bridge
            ? _bridge->getShutterChannel()
            : 0;


    uint16_t shutterValue =
        0;


    bool shutterValid =
        false;


    if (
        _fc &&
        _bridge
    )
    {
        shutterValid =
            _fc->getRcChannel(
                shutterChannel,
                shutterValue
            );
    }


    // --------------------------------------------------------
    // ROOT
    // --------------------------------------------------------

    json += "{";


    // --------------------------------------------------------
    // FIRMWARE
    // --------------------------------------------------------

    json += "\"firmware\":{";

    json += "\"name\":\"";
    json += jsonEscape(FIRMWARE_NAME);
    json += "\",";

    json += "\"version\":\"";
    json += jsonEscape(FIRMWARE_VERSION);
    json += "\",";

    json += "\"buildDate\":\"";
    json += jsonEscape(FIRMWARE_BUILD_DATE);
    json += "\",";

    json += "\"buildTime\":\"";
    json += jsonEscape(FIRMWARE_BUILD_TIME);
    json += "\"},";


    // --------------------------------------------------------
    // CAMERA
    // --------------------------------------------------------

    json += "\"camera\":{";

    json += "\"connected\":";
    json += cameraConnected ? "true" : "false";
    json += ",";

    json += "\"commandChannel\":";
    json += commandChannel ? "true" : "false";
    json += ",";

    json += "\"ready\":";
    json += cameraReady ? "true" : "false";
    json += ",";

    json += "\"recordingKnown\":";
    json += recordingKnown ? "true" : "false";
    json += ",";

    json += "\"recording\":";
    json += recording ? "true" : "false";
    json += ",";

    json += "\"recordingTimer\":\"";

    if (_camera)
        json += jsonEscape(
            _camera->getRecordingTimer()
        );

    json += "\",";

    json += "\"remaining\":\"";

    if (_camera)
        json += jsonEscape(
            _camera->getRemainingTime()
        );

    json += "\",";

    json += "\"remainingPhotos\":\"";

    if (_camera)
        json += jsonEscape(
            _camera->getRemainingPhotos()
        );

    json += "\",";

    json += "\"remainingRuntime\":\"";

    if (_camera)
        json += jsonEscape(
            _camera->getRemainingRuntime()
        );

    json += "\",";

    json += "\"batteryKnown\":";
    json += batteryKnown ? "true" : "false";
    json += ",";

    json += "\"batteryBars\":";
    json += String(batteryBars);
    json += ",";

    json += "\"batteryDescription\":\"";

    if (_camera)
        json += jsonEscape(
            _camera->getBatteryDescription()
        );

    json += "\",";

    json += "\"mode\":\"";

    if (_camera)
        json += jsonEscape(
            _camera->getModeString()
        );

    json += "\",";

    json += "\"resolution\":\"";

    if (_camera)
        json += jsonEscape(
            _camera->getResolution()
        );

    json += "\",";

    json += "\"setting\":\"";

    if (_camera)
        json += jsonEscape(
            _camera->getSetting()
        );

    json += "\",";

    json += "\"modeName\":\"";

    if (_camera)
        json += jsonEscape(
            _camera->getModeName()
        );

    json += "\",";

    json += "\"lensMode\":\"";

    if (_camera)
        json += jsonEscape(
        _camera->getLensMode()
    );

    json += "\",";

    json += "\"debug\":";

    json += (
        _camera &&
        _camera->isDebugEnabled()
    )
        ? "true"
        : "false";

    json += ",";

    json += "\"recentTelemetry\":";
    json += recentTelemetry ? "true" : "false";

    json += "},";


    // --------------------------------------------------------
    // FC
    // --------------------------------------------------------

    json += "\"fc\":{";

    json += "\"link\":";
    json += fcLink ? "true" : "false";
    json += ",";

    json += "\"armed\":";
    json += armed ? "true" : "false";
    json += ",";

    json += "\"rcValid\":";
    json += rcValid ? "true" : "false";
    json += ",";

    json += "\"rcCount\":";

    if (_fc)
        json += String(
            _fc->getRcChannelCount()
        );
    else
        json += "0";

    json += ",";

    json += "\"shutterChannel\":";
    json += String(shutterChannel + 1);
    json += ",";

    json += "\"shutterValue\":";
    json += String(shutterValue);
    json += ",";

    json += "\"shutterValid\":";
    json += shutterValid ? "true" : "false";

    json += "},";


    // --------------------------------------------------------
    // AUTORUN
    // --------------------------------------------------------

    json += "\"autorun\":{";

    json += "\"state\":\"";

    if (_autorun)
        json += jsonEscape(
            _autorun->getStateName()
        );

    json += "\",";

    json += "\"manualPowerOff\":";

    json += (
        _autorun &&
        _autorun->isManualPowerOff()
    )
        ? "true"
        : "false";

    json += "},";


    // --------------------------------------------------------
    // SYSTEM
    // --------------------------------------------------------

    json += "\"system\":{";

    json += "\"uptime\":";
    json += String(millis());
    json += ",";

    json += "\"clients\":";
    json += String(
        WiFi.softAPgetStationNum()
    );
    json += ",";

    json += "\"ip\":\"";
    json += jsonEscape(
        WiFi.softAPIP().toString()
    );
    json += "\",";

    json += "\"freeHeap\":";
    json += String(
        ESP.getFreeHeap()
    );

    json += "},";


    // --------------------------------------------------------
    // TIMESTAMP
    // --------------------------------------------------------

    json += "\"timestamp\":";
    json += String(millis());

    json += "}";


    return json;
}