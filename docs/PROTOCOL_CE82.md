# Insta360 GO 3S — BLE CE82

**Canale remote → camera**

Documentazione dei comandi, delle acquisizioni e dei test.

| Campo | Valore |
|---|---|
| Dispositivo analizzato | Insta360 GO 3S |
| Seriale osservato | 08XXXX (redatto per privacy) |
| Periodo delle acquisizioni recuperate | settembre 2026 |
| Obiettivo | documentare il protocollo di comando BLE utilizzato per controllare la camera, comprese le sequenze di inizializzazione, i comandi, la gestione delle notifiche e il wake |

**Nota sulla redazione**: il seriale reale del dispositivo di test è stato sostituito con `08XXXX`. La struttura, la posizione e la codifica ASCII del campo sono quelle osservate. La parte variabile è stata rimossa per privacy; il prefisso `08` è mantenuto perché potrebbe essere un identificativo di modello utile a chi confronta più GO 3S.

---

## 1. Architettura

Servizio principale:

CE80

Caratteristiche:

| UUID breve | Ruolo attribuito |
|---|---|
| CE81 | Informazioni e stato camera |
| CE82 | Canale di comando/notifica |
| CE83 | Informazioni aggiuntive |

La caratteristica CE82 è stata utilizzata nel progetto ESP32-C3 per inviare i comandi alla GO 3S.

Nel firmware è stata configurata la gestione delle notifiche e della sottoscrizione BLE.

---

## 2. Formato dei comandi

Il formato implementato per i comandi è:

FC EF FE 86 [SEQ] 03 01 [BUTTON] [STATE]

Struttura:

| Byte | Significato |
|---|---|
| FC | Prefisso |
| EF | Prefisso |
| FE | Prefisso |
| 86 | Identificativo del messaggio |
| SEQ | Numero di sequenza |
| 03 | Campo fisso |
| 01 | Campo fisso |
| BUTTON | Identificativo comando |
| STATE | Stato o parametro |

Lunghezza del frame:

9 byte

Il firmware incrementa il numero di sequenza di 2 a ogni comando.

Non è stato dimostrato che l'incremento di 2 sia obbligatorio in ogni implementazione del protocollo.

Non è stata recuperata una verifica completa del comportamento di wraparound.

---

## 3. Comando SHUTTER

Identificativo:

BUTTON = 0x02

Esempi di frame:

FC EF FE 86 00 03 01 02 00

Il comando viene utilizzato per azionare lo shutter.

È stato verificato il funzionamento con la GO 3S reale.

Nel progetto è utilizzato sia per il controllo manuale sia per le funzioni automatiche associate allo stato ARM/DISARM del drone.

---

## 4. Comando MODE

Identificativo:

BUTTON =0x01

Il comando è stato implementato per il cambio modalità.

È stato verificato nel contesto del canale di comando BLE.

Non è stata recuperata una documentazione completa di tutte le modalità selezionabili e delle risposte della camera.

Il comportamento preciso dipende dallo stato operativo della GO 3S.

---

## 5. Comando SCREEN

Identificativo:

BUTTON 0x00

Il comando è stato implementato come comando di controllo dello schermo.

La funzione è presente nel firmware ESP32.

Non è disponibile nel materiale recuperato una serie completa di acquisizioni che documenti tutte le conseguenze del comando nelle diverse condizioni della camera.

---

## 6. Spegnimento manuale — POWER HOLD

Una delle scoperte più importanti è la procedura di spegnimento tramite pressione prolungata del comando POWER.

Frame:

FC EF FE 86 [SEQ] 03 01 00 03

Parametri:

| Campo | Valore |
|---|---|
| BUTTON | 0x00 |
| STATE | 0x03 |
| Intervallo | circa 20 ms |
| Durata | circa 2 secondi |

Il frame viene ripetuto continuamente per simulare una pressione prolungata.

Durante i test sono stati riportati cicli con:

- 100 frame;
- 42 frame;
- 41 frame.

La procedura è stata implementata come operazione bloccante nel firmware.

La continuità temporale è considerata importante: non va trasformata in una sequenza intermittente senza prima effettuare test sulla camera.

Dopo lo spegnimento, la camera si disconnette.

Il firmware imposta inoltre un blocco per evitare che il wake automatico riaccenda la camera dopo uno spegnimento richiesto manualmente.

---

## 7. Wake tramite advertising

È stata individuata e testata una procedura di wake basata su advertising BLE e manufacturer data.

Payload osservato:

4C 00 02 15 09 4F 52 42 49 54 09 FF 0F 00
30 38 58 58 58 58 00 00 00 00 E4 01

I byte centrali del payload (offset 16-21) contengono il seriale ASCII della camera (redatto: `08XXXX`).

Elementi riconoscibili:

- `4C 00`
- `02 15`
- `ORBIT`
- `09 FF`
- `0F 00`
- seriale ASCII (redatto)
- `E4 01`

Stringa ORBIT in HEX:

4F 52 42 49 54

Seriale in HEX (redatto):

30 38 58 58 58 58


Questo payload è stato utilizzato per generare il beacon di wake.

**Importante**: non si tratta di un frame CE82. È un payload advertising contenente manufacturer data.

Il wake è stato testato con la GO 3S reale.

Nel firmware la finestra di advertising dedicata al wake dura circa 10 secondi.

---

## 8. Connessione e sottoscrizione CE82

La connessione GATT e la sottoscrizione alle notifiche sono due condizioni differenti.

Durante i test è stato osservato che:

- la connessione BLE può risultare attiva;
- CE82 può non essere ancora sottoscritta;
- un tentativo di inviare una notifica può generare `ERROR_NO_CLIENT`.

La sottoscrizione al CCCD è stata gestita con:

| Valore | Significato |
|---|---|
| 0x0001 | Notify abilitato |
| 0x0000 | Notify disabilitato |

Handle osservati in una sessione:

| Elemento | Handle |
|---|---|
| CE82 | 0x0012 |
| CCCD CE82 | 0x0013 |

Questi handle non devono essere considerati costanti universali.

In altre documentazioni della famiglia GPS Remote sono stati riportati handle differenti, per esempio `0x0013` e `0x0014`.

La differenza può dipendere dall'ordine dei servizi e delle caratteristiche esposti dal dispositivo.

---

## 9. Gestione della connessione

Durante il reverse engineering sono emersi diversi stati da distinguere:

1. advertising attivo;
2. camera sveglia;
3. connessione GATT stabilita;
4. CE82 sottoscritta;
5. canale comandi pronto;
6. camera effettivamente operativa.

Il firmware utilizza una funzione equivalente a:

isCommandChannelReady()

La condizione richiede:

- GATT connected;
- caratteristica CE82 disponibile;
- notifiche abilitate.

Questa distinzione è importante perché il solo stato GATT connected non garantisce che il canale di comando sia pronto.

Durante i test è stato osservato anche il messaggio:

ERROR_NO_CLIENT

quando si tentava di inviare una notifica senza un client correttamente sottoscritto.

---

## 10. Handle e indirizzi osservati

Durante le acquisizioni sono comparsi:

- handle CE82 `0x0012`;
- CCCD `0x0013`;
- peer address `D8:38:8B:71:CC:54`;
- connection handle `1`;
- connection handle `2`;
- valore `65535` in una condizione di connessione non valida/non disponibile.

Questi sono valori osservati durante specifiche sessioni, non identificativi permanenti della GO 3S.

---

## 11. Prime acquisizioni Android — 5 settembre 2026

Durante le prime sessioni di reverse engineering è stato utilizzato un file:

ble_values.txt

Il file conteneva:

- timestamp relativi;
- opcode ATT;
- handle;
- valori HEX.

Nei record analizzati comparivano ATT Write Request:

0x12

verso handle:

0x0009

sono stati osservati diversi frame con prefisso

FF 0704

Esempio:

FF 07 40 07 00 07 00 00 00 05 00 6B 46

Un'altra variante riportata:

FF 07 40 07 00 07 00 00 00 05 00 00 6B 46

Frequenza osservata:

circa un messaggio ogni 2 secondi

Sono stati osservati anche valori ricorrenti:

- `19`;
- `33`;
- `10`;
- `2C`;
- `38`;
- `07`.

Questi pacchetti erano presenti durante le acquisizioni Android.

La loro funzione non è stata identificata con certezza.

Sono state considerate le possibilità di:

- inizializzazione;
- keep-alive;
- sincronizzazione;
- aggiornamento periodico dello stato.

Non è stato dimostrato che si tratti di comandi REC/STOP.

Il contatore e il possibile checksum sono rimasti ipotesi.

**Nota fondamentale**: questi pacchetti appartengono a una delle prime fasi del reverse engineering e non devono essere confusi con il formato CE82 `FC EF FE 86`.

Non è stata recuperata una correlazione dimostrata tra handle `0x0009` e CE82.

---

## 12. Sequenze di inizializzazione

Durante le acquisizioni Android è stata osservata una serie di ATT Write Request dirette all'handle `0x0009`.

Le sequenze mostravano diversi valori ricorrenti.

Non è disponibile nel materiale recuperato il dump integrale di tutti i pacchetti e dei relativi timestamp.

Non è quindi possibile ricostruire fedelmente l'intera sequenza di inizializzazione.

Non bisogna assumere che i pacchetti `FF 07 40` siano equivalenti ai comandi CE82.

---

## 13. Wake, disconnessione e riconnessione

Nella sessione dell'8 settembre 2026 è stato osservato un comportamento importante.

Durante il wake:

- CE81 trasmetteva informazioni;
- il contatore dei client restava a zero;
- il messaggio BLE risultava DISCONNECTED;
- la camera non era ancora pronta a ricevere i comandi.

Dopo la connessione effettiva:

||| GO 3S CONNECTED<<<


i comandi SHUTTER e MODE hanno iniziato a funzionare.

Questo ha portato a separare nel firmware la fase di wake dalla fase di connessione.

---

## 14. Spegnimento e blocco del wake automatico

Il comportamento richiesto per il firmware è:

- spegnimento manuale: bloccare il wake automatico;
- wake manuale: riattivare la camera;
- normale ARM/DISARM: consentire il funzionamento automatico;
- disconnessione imprevista: consentire il tentativo di riconnessione.

La distinzione tra spegnimento manuale e disconnessione accidentale è stata implementata nel livello AutoRun.

---

## 15. GPS Remote e CE83: contesto storico

Le prime ricerche sul protocollo si sono basate anche sulla documentazione del telecomando GPS Insta360.

Da quella documentazione sono stati ricavati:

- prefissi dei frame;
- architettura CE80;
- caratteristiche CE81, CE82 e CE83;
- struttura dei comandi `0x86`;
- incremento del sequence number;
- identificativi dei pulsanti.

Sono state inoltre trovate informazioni su GPS e NMEA.

Queste informazioni non devono essere automaticamente considerate valide per la GO 3S.

Nel progetto finale GPS, NMEA, coordinate, fix e trasmissione `0x83` sono stati esclusi deliberatamente.

---

## 16. Stato del protocollo nel firmware ESP32-C3

La logica implementata comprende:

- connessione BLE alla GO 3S;
- riconoscimento del seriale;
- sottoscrizione CE82;
- comando shutter;
- comando mode;
- comando screen;
- spegnimento tramite POWER HOLD;
- wake tramite advertising;
- gestione della sequenza di connessione;
- blocco del wake automatico dopo spegnimento manuale.

Il canale comandi è stato verificato con la camera reale.

---

## 17. TODO — CE82

- [ ] Recuperare il file Android `ble_values.txt` originale.
- [ ] Recuperare eventuali file `.pcapng` e `btsnoop_hci.log`.
- [ ] Ricostruire tutte le sequenze di inizializzazione.
- [ ] Identificare il significato dei pacchetti `FF 07 40`.
- [ ] Verificare se i pacchetti periodici siano keep-alive.
- [ ] Verificare il possibile contatore e checksum.
- [ ] Correlare handle `0x0009` con UUID e servizio.
- [ ] Confermare tutti i campi del frame `0x86`.
- [ ] Verificare il sequence number e il wraparound.
- [ ] Verificare MODE in tutte le modalità.
- [ ] Verificare SCREEN in tutti gli stati.
- [ ] Documentare tutte le risposte e gli eventuali errori.
- [ ] Verificare il comportamento del comando POWER HOLD.
- [ ] Verificare il payload advertising su più cicli di wake.
- [ ] Verificare il comportamento dopo disconnessione improvvisa.
- [ ] Verificare le differenze tra CE82 e implementazioni GPS Remote.
- [ ] Ricostruire la sequenza completa di connessione e sottoscrizione.

---

## 18. Attendibilità

**Verificato su GO 3S reale**

- comando SHUTTER;
- comando MODE nel contesto dei test;
- procedura POWER HOLD;
- wake tramite advertising;
- necessità della connessione effettiva per inviare comandi;
- comportamento di disconnessione dopo lo spegnimento.

**Osservato direttamente, ma non completamente decodificato**

- pacchetti `FF 07 40`;
- sequenze periodiche Android;
- handle `0x0009`;
- alcuni valori di connessione;
- payload advertising completo.

**Implementato, ma non verificato in ogni condizione**

- comando SCREEN;
- comportamento completo di MODE;
- wraparound del sequence number;
- significato di tutti i campi del protocollo;
- gestione di tutte le risposte della camera.

**Non dimostrato**

- significato definitivo dei pacchetti periodici `FF 07 40`;
- checksum;
- funzione precisa dei valori ricorrenti;
- equivalenza tra implementazioni GPS Remote e GO 3S;
- specifica ufficiale CE82.

---

*Documento basato su reverse engineering empirico del protocollo BLE della Insta360 GO 3S. Nessuna affiliazione con Insta360. Il seriale del dispositivo di test è stato redatto per privacy.*