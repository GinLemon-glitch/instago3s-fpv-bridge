# Insta360 GO 3S — BLE CE81

**Canale camera → remote**

Documentazione delle acquisizioni e delle scoperte.

| Campo | Valore |
|---|---|
| Dispositivo analizzato | Insta360 GO 3S |
| Seriale osservato | `08XXXX` (redatto per privacy) |
| Identificativo ASCII | `30 38 58 58 58 58` (redatto) |
| Periodo delle acquisizioni recuperate | settembre 2026 |
| Obiettivo | documentare la comunicazione BLE della GO 3S, con particolare attenzione ai dati trasmessi dalla camera, allo stato operativo e alle informazioni utilizzabili da un telecomando BLE personalizzato |

> **Nota sulla redazione**: il seriale reale del dispositivo di test è stato sostituito con `08XXXX`. La struttura, la posizione e la codifica ASCII del campo sono quelle osservate. La parte variabile è stata rimossa per privacy; il prefisso `08` è mantenuto perché potrebbe essere un identificativo di modello utile a chi confronta più GO 3S.

---

## 1. Architettura BLE generale

Durante il reverse engineering è stato individuato un servizio BLE principale:
CE80

Caratteristiche associate:

| UUID breve | Ruolo attribuito | Stato |
|---|---|---|
| CE81 | Comunicazione relativa allo stato della camera | Analizzata |
| CE82 | Comunicazione dei comandi e notifiche | Analizzata insieme a CE81 |
| CE83 | Caratteristica informativa | Individuata, approfondimento incompleto |

È stato inoltre individuato un servizio secondario:
0000D0FF-3C17-D293-8E48-14FE2E4DA212


Le sue caratteristiche sono riportate nella sezione dedicata ai servizi correlati.

> **Attenzione**: durante le prime analisi, le denominazioni TX/RX e le proprietà GATT attribuite alle caratteristiche non sono sempre risultate coerenti tra documentazione GPS Remote, implementazioni reperite e test sulla GO 3S. Per questo motivo, il nome CE81 non deve essere considerato una garanzia della direzione di trasmissione.

---

## 2. Framing dei messaggi

Nei dati analizzati sono emerse due famiglie di prefissi:

**Prefisso associato ai messaggi in uscita**

FC EF FE

**Prefisso associato ai messaggi ricevuti**

FE EF FE

La struttura generale osservata per i messaggi ricevuti è stata interpretata come:

FE EF FE TYPE ...

In una prima analisi era stata proposta una struttura più dettagliata:

FE EF FE TYPE B4 LEN PAYLOAD


Questa struttura non deve essere considerata definitivamente dimostrata per tutti i tipi di messaggio.

Il parser sviluppato successivamente ha riconosciuto diversi tipi di pacchetto e sottotipi, ma non tutti gli offset e le lunghezze sono stati verificati con acquisizioni complete.

---

## 3. Identificazione della camera — TYPE 0x07

Durante le acquisizioni è stato identificato il seriale:

08XXXX

Rappresentazione HEX:

30 38 58 58 58 58


Il seriale è stato osservato durante la connessione e utilizzato per identificare la GO 3S.

Il decoder sviluppato ha associato il tipo `0x07` alle informazioni identificative della camera.

**Stato**: seriale osservato direttamente; struttura completa del messaggio da verificare sui dump originali.

---

## 4. Informazioni sulla modalità — TYPE 0x02

Il tipo `0x02` è stato associato alle informazioni di modalità e famiglia di acquisizione.

Tra i valori osservati nel decoder e nelle successive interpretazioni figurano:

- codici di modalità;
- codici relativi alla famiglia di acquisizione;
- valori associati a stati di cattura.

È stata inoltre individuata una famiglia `0x50`, trattata nel decoder come capture substate.

Il codice attuale implementa una mappatura dei valori, ma non tutti i codici sono accompagnati da una sequenza completa di dump che ne dimostri il significato.

> **Da non confondere**: il fatto che il firmware riconosca un codice non significa che il suo significato sia stato verificato indipendentemente.

---

## 5. Informazioni display e modalità — TYPE 0x10

Il tipo `0x10` è uno dei messaggi più interessanti perché contiene informazioni utilizzate per rappresentare lo stato della camera.

Sono stati individuati diversi sottotipi.

### 5.1 Sottotipo 0x5E — informazioni sulla modalità

Stringhe osservate:

4K|30|MEGA
4K|30|ACT
2.7K|100|MEGA
16:9|3s|DEW
16:9|3s|UW

Queste stringhe contengono informazioni relative a risoluzione, frequenza, modalità o impostazioni di acquisizione.

Interpretazione dei componenti:

- `4K`: risoluzione;
- `2.7K`: risoluzione;
- `30`: frequenza;
- `100`: frequenza;
- `MEGA`, `ACT`, `DEW`, `UW`: identificativi o abbreviazioni delle modalità/impostazioni.

Non è stata ricostruita una tabella ufficiale che associ ogni sigla alla relativa funzione.

Il decoder evita deliberatamente di dedurre automaticamente che una modalità sia VIDEO soltanto perché contiene `4K|30|MEGA`.

### 5.2 Sottotipo 0x46 — timer e informazioni residue

Sono state osservate stringhe come:

00:00:00
1h42m
1h18m
1h16m
999+

Queste informazioni sono state associate a timer, tempo residuo o capacità residua.

Nel decoder sono state previste interpretazioni differenti in funzione del contenuto.

La semantica esatta di ogni campo non è ancora completamente documentata.

In particolare:

- `00:00:00` è stato associato a un timer azzerato;
- valori come `1h42m` sono stati associati a tempo residuo;
- `999+` è stato interpretato come indicazione di capacità residua superiore alla soglia mostrata.

Non è dimostrato che tutti questi valori abbiano sempre la stessa semantica in ogni modalità di ripresa.

### 5.3 Sottotipo 0x2C — PureShot

È stato individuato un messaggio associato al countdown PureShot.

Valori osservati:

3
2
1

Il countdown è stato gestito nel decoder come informazione separata dalle altre stringhe di modalità.

Non sono disponibili in questo riepilogo tutti i dump grezzi che permettono di ricostruire la struttura completa del messaggio.

---

## 6. Batteria — TYPE 0x02 / sottocodice 0x54

È stata individuata una famiglia di messaggi associata alla batteria.

Un pacchetto osservato:

FE EF FE 02 80 05 00 54 00 0A 50

La parte:

00 54

è stata associata al sottotipo batteria.

Durante i test sono stati osservati valori di batteria indicati anche in percentuale:

- 82%;
- 76%;
- fino all'1%.

La mappatura empirica utilizzata dal decoder è:

| Valore | Interpretazione |
|---|---|
| 0x0A | 4 tacche |
| 0x09 | 3 tacche |
| 0x08 | 2 tacche |
| 0x07 | 1 tacca |
| 0x06 | Batteria critica |
| 0x05 | Stato finale / prossimità allo spegnimento |
| 0x0B | Camera in carica o dock |

Questa è una mappatura empirica, non una specifica ufficiale.

In particolare, il valore `0x05` è stato osservato prima della disconnessione durante lo spegnimento.

Il valore `0x0B` è stato associato allo stato di ricarica.

---

## 7. Stato registrazione e cattura — TYPE 0x05

Durante le analisi è stata individuata una famiglia di messaggi `0x05`, associata a eventi e stati operativi.

È stata inoltre implementata nel decoder la gestione della sequenza:

FEEFFE058001xx

Interpretazione adottata:

- `01`: stato attivo;
- `00`: stato inattivo.

La sequenza è stata utilizzata per rappresentare lo stato di registrazione/cattura.

La struttura e la lunghezza devono essere riconfrontate con i dump originali prima di considerare definitiva l'interpretazione.

---

## 8. Altri tipi di messaggio

Durante le prime analisi sono stati individuati anche:

- `0x0E`;
- `0x0F`.

Sono stati inseriti tra i tipi riconosciuti o interpretati dal decoder.

Non è disponibile, nel materiale recuperato, una descrizione sufficientemente completa e verificabile del loro payload.

Non vengono quindi attribuite loro funzioni definitive.

---

## 9. Acquisizioni durante lo spegnimento

In una sessione del 8 settembre 2026 è stata acquisita una sequenza significativa.

Durante lo spegnimento della GO 3S sono stati osservati:

- messaggi CE81 contenenti informazioni come `1h18m`;
- eventi di tipo `0x05`;
- messaggi relativi alla batteria;
- successiva disconnessione BLE.

Tra i pacchetti riportati:

FE EF FE 02 80 05 00 54 00 0A 50

Durante la successiva procedura di wake sono stati osservati messaggi CE81, ma lo stato BLE risultava:

BLE DISCONNECTED

Dopo la riconnessione sono ricomparse informazioni identificative:

08XXXX

e informazioni di modalità:

2.7K|100|MEGA
999+
16:9|3s|DEW

con batteria:

0x09

Questa acquisizione è importante perché dimostra che la ricezione di informazioni CE81 e la disponibilità della connessione GATT non devono essere considerate la stessa cosa.

---

## 10. Comportamento CE81 durante il wake

Durante i test è stato osservato un comportamento apparentemente anomalo:

- CE81 veniva ricevuto durante la fase di wake;
- `getConnectedCount()` rimaneva a zero;
- anche dopo il ripristino dell'advertising il conteggio risultava zero;
- la callback `onConnect()` riportava comunque `Connected clients: 0`.

Il contatore dei client non è quindi stato considerato affidabile come unico indicatore di connessione.

È stata adottata una distinzione tra:

1. camera sveglia;
2. advertising attivo;
3. connessione GATT;
4. sottoscrizione alle notifiche;
5. canale comandi effettivamente utilizzabile.

I comandi MODE e SHUTTER sono stati verificati funzionanti soltanto dopo il messaggio:

||| GO 3S CONNECTED <<<

---

## 11. Servizio secondario D0FF

È stato individuato il servizio:

0000D0FF-3C17-D293-8E48-14FE2E4DA212

Caratteristiche presenti nell'implementazione:

| UUID breve | Proprietà/ruolo riportato |
|---|---|
| FFD1 | WRITE |
| FFD2 | WRITE |
| FFD3 | READ |
| FFD4 | READ |
| FFD5 | NOTIFY |
| FFD8 | NOTIFY |
| FFF1 | READ / WRITE |
| FFF2 | NOTIFY |
| FFE0 | READ / WRITE |

Queste sono le caratteristiche individuate nel codice e nella ricognizione GATT.

Non è disponibile una mappatura completa e verificata dei rispettivi payload.

Non bisogna attribuire automaticamente a queste caratteristiche le stesse funzioni delle caratteristiche CE80.

---

## 12. CE83 e GPS Remote

Nelle prime ricerche è stata esaminata anche l'architettura dei telecomandi GPS Remote Insta360.

In quella documentazione CE83 era indicata come caratteristica informativa.

Sono state trovate informazioni relative a messaggi GPS e dati NMEA RMC, con frequenza indicativa di 10 Hz, ma non costituiscono una scoperta verificata sulla GO 3S.

Successivamente il progetto ESP32 è stato deliberatamente limitato alle funzioni di telecomando, escludendo:

- GPS;
- NMEA;
- coordinate;
- fix;
- trasmissione GPS `0x83`.

Pertanto questi elementi sono riportati come contesto storico, non come funzionalità accertate della GO 3S.

---

## 13. Risultati utilizzabili nel firmware

Le informazioni CE81 sono state utilizzate nel progetto ESP32-C3 per:

- identificare la camera;
- leggere la batteria;
- rappresentare lo stato di registrazione;
- visualizzare il timer;
- mostrare la modalità;
- mostrare il tempo residuo;
- mostrare le informazioni PureShot.

Il decoder è stato progettato per separare la decodifica dei pacchetti dalla loro rappresentazione nell'OSD.

La ricezione CE81 non viene considerata sufficiente per dichiarare automaticamente la camera connessa.

---

## 14. TODO — CE81

- [ ] Recuperare i dump HEX integrali originali.
- [ ] Ricostruire la struttura completa di ogni tipo di pacchetto.
- [ ] Verificare lunghezze e offset.
- [ ] Verificare tutti i valori TYPE `0x02`.
- [ ] Ricostruire la tabella completa dei sottotipi `0x10`.
- [ ] Verificare il significato di `0x46`.
- [ ] Verificare il significato di `0x2C`.
- [ ] Confermare la mappatura batteria.
- [ ] Verificare la sequenza `FEEFFE058001xx`.
- [ ] Decodificare TYPE `0x0E` e `0x0F`.
- [ ] Ricostruire gli eventi durante boot, wake, registrazione, stop e shutdown.
- [ ] Verificare la relazione tra CE81, advertising e connessione GATT.
- [ ] Documentare eventuali notifiche duplicate o periodiche.

---

## 15. Attendibilità

**Dati osservati direttamente o riportati da acquisizioni reali:**

- seriale (redatto);
- stringhe di modalità;
- valori batteria;
- countdown PureShot;
- timer e valori residui;
- pacchetto batteria riportato;
- messaggi durante shutdown e wake;
- ricezione CE81 durante BLE DISCONNECTED.

**Interpretazioni implementate nel firmware, da verificare ulteriormente:**

- mappatura completa dei TYPE;
- struttura interna di alcuni payload;
- significato di tutti i sottotipi;
- semantica di alcuni valori batteria;
- relazione precisa tra i messaggi e lo stato effettivo della camera.

**Non documentato in modo sufficiente:**

- dump completi di ogni sequenza;
- tabella completa dei byte;
- specifica ufficiale del protocollo.

---

*Documento basato su reverse engineering empirico del protocollo BLE della Insta360 GO 3S. Nessuna affiliazione con Insta360. Il seriale del dispositivo di test è stato redatto per privacy.*