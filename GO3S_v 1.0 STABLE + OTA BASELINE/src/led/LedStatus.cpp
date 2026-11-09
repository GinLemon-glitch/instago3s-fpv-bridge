#include "LedStatus.h"

#include <cstring>
#include <math.h>

// ============================================================
// PWM LED
// ============================================================

static constexpr uint32_t LED_PWM_FREQUENCY = 5000;
static constexpr uint8_t  LED_PWM_RESOLUTION = 8;

void LedStatus::begin()
{
    /*
     * GPIO8 = LED blu onboard.
     *
     * Il LED del SuperMini è ACTIVE LOW:
     *
     *   duty 0   = massimo
     *   duty 255 = spento
     *
     * Usiamo direttamente LEDC hardware.
     * In questo modo non mischiamo digitalWrite()
     * e analogWrite() sul GPIO.
     */

    pinMode(LED_PIN, OUTPUT);

    ledcAttach(
        LED_PIN,
        LED_PWM_FREQUENCY,
        LED_PWM_RESOLUTION
    );

    // LED spento
    ledcWrite(LED_PIN, 255);

    _patternStartedAt = millis();
    _lastBootUpdate = 0;
    _lastPattern = nullptr;
    _bootFinished = false;
    _ledState = false;
}

void LedStatus::setLed(bool on)
{
    _ledState = on;

    /*
     * LED ACTIVE LOW.
     */

    if (on)
    {
        // Massima luminosità
        ledcWrite(LED_PIN, 0);
    }
    else
    {
        // Spento
        ledcWrite(LED_PIN, 255);
    }
}

void LedStatus::resetPattern(const char *pattern)
{
    if (_lastPattern != pattern)
    {
        _lastPattern = pattern;
        _patternStartedAt = millis();

        _ledState = false;
        setLed(false);
    }
}

void LedStatus::update(
    const CameraBLE &camera,
    const AutoRun &autorun
)
{
    const char *state = autorun.getStateName();

    // ============================================================
    // BOOT
    // ============================================================

    if (!_bootFinished)
    {
        updateBoot();
        return;
    }

    // ============================================================
    // MANUAL POWER OFF
    // ============================================================

    if (autorun.isManualPowerOff())
    {
        updateManualPowerOff();
        return;
    }

    // ============================================================
    // RECORDING
    // ============================================================

    if (camera.isRecording())
    {
        updateRecording();
        return;
    }

    // ============================================================
    // WAKE
    // ============================================================

    if (camera.isWakeActive())
    {
        updateWake();
        return;
    }

    // ============================================================
    // READY
    //
    // AutoRun READY è la fonte di verità.
    // NON usiamo camera.isReady().
    // ============================================================

    if (strcmp(state, "READY") == 0)
    {
        updateReady();
        return;
    }

    // ============================================================
    // GATT OFFLINE
    // ============================================================

    if (strcmp(state, "CAMERA_AWAKE_GATT_OFFLINE") == 0)
    {
        updateGattOffline();
        return;
    }

    // ============================================================
    // RECONNECT
    // ============================================================

    if (
        strcmp(state, "RECONNECT_WAIT") == 0 ||
        strcmp(state, "WAIT_CONNECTION") == 0 ||
        strcmp(state, "CONNECTION_SETTLE") == 0
    )
    {
        updateReconnect();
        return;
    }

    // ============================================================
    // CAMERA OFFLINE
    // ============================================================

    if (strcmp(state, "CAMERA_OFFLINE") == 0)
    {
        updateError();
        return;
    }

    // ============================================================
    // UNKNOWN / ERROR
    // ============================================================

    updateError();
}

// ============================================================
// BOOT
// ============================================================

void LedStatus::updateBoot()
{
    /*
     * Tre brevi lampeggi all'avvio.
     *
     * 150 ms ON
     * 150 ms OFF
     */

    const unsigned long elapsed =
        millis() - _patternStartedAt;

    if (elapsed < 900)
    {
        const unsigned long phase =
            elapsed % 300;

        if (phase < 150)
        {
            setLed(true);
        }
        else
        {
            setLed(false);
        }

        return;
    }

    setLed(false);

    _bootFinished = true;
    _lastPattern = nullptr;
    _patternStartedAt = millis();
}

// ============================================================
// MANUAL POWER OFF
// ============================================================

void LedStatus::updateManualPowerOff()
{
    /*
     * Camera spenta manualmente:
     *
     * breve flash ogni 3 secondi.
     */

    resetPattern("MANUAL_POWER_OFF");

    const unsigned long elapsed =
        millis() - _patternStartedAt;

    if (elapsed < 150)
    {
        setLed(true);
    }
    else
    {
        setLed(false);
    }

    if (elapsed >= 3000)
    {
        _patternStartedAt = millis();
    }
}

// ============================================================
// RECORDING
// ============================================================

void LedStatus::updateRecording()
{
    /*
     * ============================================================
     * RECORDING - BREATHING LED
     * ============================================================
     *
     * Ciclo completo: 2,5 secondi.
     *
     *   spento
     *      ↓
     *   aumenta lentamente
     *      ↓
     *   luminosità massima
     *      ↓
     *   diminuisce lentamente
     *      ↓
     *   spento
     *
     * Curva sinusoidale per un effetto morbido.
     */

    resetPattern("RECORDING");

    constexpr unsigned long BREATH_PERIOD = 2500UL;

    const unsigned long elapsed =
        millis() - _patternStartedAt;

    const float phase =
        (float)(elapsed % BREATH_PERIOD) /
        (float)BREATH_PERIOD;

    /*
     * Curva:
     *
     * 0.0 → 1.0 → 0.0
     */

    const float brightness =
        0.5f -
        0.5f * cosf(
            phase * 2.0f * PI
        );

    /*
     * LED ACTIVE LOW:
     *
     * brightness 0.0 = spento
     * brightness 1.0 = massimo
     *
     * Quindi invertiamo il duty PWM.
     */

    const uint8_t duty =
        (uint8_t)(
            255.0f -
            (brightness * 255.0f)
        );

    ledcWrite(LED_PIN, duty);

    _ledState = brightness > 0.01f;
}

// ============================================================
// WAKE
// ============================================================

void LedStatus::updateWake()
{
    /*
     * Wake camera:
     *
     * 300 ms ON
     * 700 ms OFF
     */

    resetPattern("WAKE");

    const unsigned long elapsed =
        millis() - _patternStartedAt;

    const unsigned long phase =
        elapsed % 1000;

    if (phase < 300)
    {
        setLed(true);
    }
    else
    {
        setLed(false);
    }
}

// ============================================================
// GATT OFFLINE
// ============================================================

void LedStatus::updateGattOffline()
{
    /*
     * Camera sveglia ma GATT non disponibile:
     *
     * 100 ms ON
     * 100 ms OFF
     */

    resetPattern("GATT_OFFLINE");

    const unsigned long elapsed =
        millis() - _patternStartedAt;

    const unsigned long phase =
        elapsed % 200;

    if (phase < 100)
    {
        setLed(true);
    }
    else
    {
        setLed(false);
    }
}

// ============================================================
// READY
// ============================================================

void LedStatus::updateReady()
{
    /*
     * Sistema pronto:
     * LED fisso acceso.
     */

    resetPattern("READY");

    setLed(true);
}

// ============================================================
// RECONNECT
// ============================================================

void LedStatus::updateReconnect()
{
    /*
     * Riconnessione:
     *
     * ON  120 ms
     * OFF 120 ms
     * ON  120 ms
     * pausa
     */

    resetPattern("RECONNECT");

    const unsigned long elapsed =
        millis() - _patternStartedAt;

    const unsigned long phase =
        elapsed % 1200;

    if (
        (phase < 120) ||
        (phase >= 240 && phase < 360)
    )
    {
        setLed(true);
    }
    else
    {
        setLed(false);
    }
}

// ============================================================
// ERROR
// ============================================================

void LedStatus::updateError()
{
    /*
     * Stato inatteso / errore:
     *
     * 75 ms ON
     * 75 ms OFF
     */

    resetPattern("ERROR");

    const unsigned long elapsed =
        millis() - _patternStartedAt;

    const unsigned long phase =
        elapsed % 150;

    if (phase < 75)
    {
        setLed(true);
    }
    else
    {
        setLed(false);
    }
}