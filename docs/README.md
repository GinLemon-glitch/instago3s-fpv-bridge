# GO3S FPV Bridge

Bridge BLE tra una **Insta360 GO 3S** e una **flight controller FPV**, basato su ESP32-C3. Il firmware simula un telecomando BLE Insta360 e automatizza la gestione della camera in base allo stato del drone letto via MSP.

> **Fuori di testa con un saldatore in mano: progetto cose inutili con ESP32, droni e luci wireless. Se funziona, era un effetto collaterale.**

---

## Cosa fa

- **Wake automatico**: all'accensione del drone, l'ESP32 invia il beacon BLE che risveglia la GO 3S dal deep sleep.
- **Mantenimento**: se la camera si spegne per inattività (~3 minuti), il firmware la ri-wakea automaticamente. Il ciclo continua finché il drone è alimentato e l'utente non spegne esplicitamente la camera.
- **ARM → registrazione**: quando la FC riporta ARM, la camera avvia la registrazione (verificata via telemetria CE81).
- **DISARM → stop**: all'atterraggio, la registrazione viene fermata. Se lo stato camera è sconosciuto (es. dopo un crash), il firmware **non invia nulla**: la camera resta accesa e localizzabile con l'app Insta360.
- **Comando radio**: un canale RC configurabile funge da shutter toggle in volo (stop/riprendi registrazione).
- **OSD Betaflight**: stato camera, batteria, modalità, timer e tempo residuo vengono inviati alla FC come custom messages MSP2.
- **WebUI + OTA**: interfaccia web su AP `GO3S-CAM-BRIDGE` per stato, comandi manuali, aggiornamento firmware.

---

## Hardware

| Componente | Note |
|---|---|
| ESP32-C3 SuperMini | MCU principale |
| Flight controller | Qualsiasi FC con MSP su UART |
| Insta360 GO 3S | Camera |

### Collegamento UART alla FC

| Segnale | GPIO ESP32-C3 |
|---|---|
| FC RX (ESP TX) | GPIO21 |
| FC TX (ESP RX) | GPIO20 |
| Baud | 115200 8N1 |

Configurabile in `FCInterface.h` (`FC_RX_PIN`, `FC_TX_PIN`, `FC_BAUD`).

---

## Come si usa

### Boot
1. Alimenta il drone. L'ESP32 si avvia e avvia il wake beacon.
2. La GO 3S si risveglia e si connette.
3. Quando il canale comandi è pronto, il firmware sincronizza lo stato con la FC.

### Comandi seriali (115200 baud)

| Tasto | Funzione |
|---|---|
| `t` | SHUTTER |
| `m` | MODE |
| `s` | SCREEN toggle |
| `x` | POWER / WAKE toggle |
| `w` | FORCE WAKE |
| `p` | PRINT STATUS |
| `v` | DEBUG ON/OFF |
| `a` | SIMULATE ARM |
| `d` | SIMULATE DISARM |
| `?` | HELP |

### WebUI
- SSID: `GO3S-CAM-BRIDGE`
- Password: `12345678`
- URL: `http://192.168.4.1`

### OTA
- Hostname: `GO3S-CAM-BRIDGE`
- Porta: `3232`
- Password: `12345678`
- In alternativa: upload `.bin` da `/ota` nella WebUI.

---

## Struttura del repository

├── README.md
├── LICENSE
├── src/
│ ├── main.cpp
│ ├── Version.h
│ ├── camera/
│ │ ├── CameraBLE.cpp
│ │ └── CameraBLE.h
│ ├── fc/
│ │ ├── FCInterface.cpp
│ │ └── FCInterface.h
│ ├── bridge/
│ │ ├── CameraBridge.cpp
│ │ └── CameraBridge.h
│ ├── autorun/
│ │ ├── AutoRun.cpp
│ │ └── AutoRun.h
│ ├── osd/
│ │ ├── CameraOSD.cpp
│ │ └── CameraOSD.h
│ ├── web/
│ │ ├── WebUI.cpp
│ │ └── WebUI.h
│ └── led/
│ ├── LedStatus.cpp
│ └── LedStatus.h
└── docs/
├── PROTOCOL_CE81.md
└── PROTOCOL_CE82.md


---

## Documentazione del protocollo

Il cuore del progetto è il reverse engineering del protocollo BLE della GO 3S:

- **[`docs/PROTOCOL_CE81.md`](docs/PROTOCOL_CE81.md)** — canale camera → remote: telemetria, stato registrazione, batteria, modalità, timer.
- **[`docs/PROTOCOL_CE82.md`](docs/PROTOCOL_CE82.md)** — canale remote → camera: comandi, sequenza, power hold, wake beacon.

Entrambi i documenti distinguono esplicitamente tra dati **osservati**, **interpretati** e **da verificare**. Sono il risultato di reverse engineering empirico, non di documentazione ufficiale.

---

## Crediti

- **Wake beacon**: pattern derivato dal progetto [`insta360_m5StickC_remote`](https://github.com/Serialhobbyism/insta360_m5StickC_remote) e confermato dall'SDK ufficiale Insta360 (`bleWakeUp`).
- **Protocollo CE81 / CE82**: reverse engineering originale dell'autore, basato su sniffing BLE e test sulla GO 3S reale.
- **Librerie**: [Arduino-ESP32](https://github.com/espressif/arduino-esp32), NimBLE.
- **Ispirazione community**: [Flipper Zero GO 3S remote](https://github.com/vinuthomas/flipper-instapro), [SyncField Go3SStream](https://github.com/SyncField/Go3SStream).

---

## Disclaimer

Questo progetto è **reverse engineering non ufficiale**. Non è affiliato, sponsorizzato o approvato da Insta360.

- Il protocollo BLE della GO 3S **non è documentato pubblicamente** e può cambiare con aggiornamenti firmware.
- L'uso del firmware è **a proprio rischio**. Verifica sempre il comportamento a banco prima di un volo reale.
- L'autore non è responsabile per danni alla camera, al drone o a terzi.

---

## Licenza

MIT — vedi [`LICENSE`](LICENSE).