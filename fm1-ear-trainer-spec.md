# FM-1 Ear Trainer: specifiche per lo sviluppo

Documento di riferimento per sviluppare con Claude Code un firmware custom di ear training per il sintetizzatore M-VAVE FM-1.

Nome di lavoro del progetto: **FM-1 Ear Trainer** (modificabile).

---

## 1. Obiettivo

Trasformare l'FM-1 in un dispositivo autonomo per l'allenamento dell'orecchio musicale. Il synth suona un quesito (intervallo, accordo, grado della scala…), l'utente risponde sulla tastiera o con i controlli, e il display mostra l'esito e le statistiche. Non servono computer o telefono durante l'uso.

Requisiti di fondo:
- il motore sonoro e le funzioni di base del synth restano disponibili (possibilmente si torna alla modalità "suona" con un tasto);
- gli esercizi vanno usati con le mani, sul dispositivo, senza menu profondi;
- deve funzionare per uno studente principiante come per un musicista esperto (livelli configurabili).

---

## 2. Contesto tecnico (da verificare sul codice)

Le informazioni seguenti vengono da fonti pubbliche. **Claude Code deve verificarle sul codice sorgente prima di basarci il lavoro** e segnalare le differenze.

### Hardware FM-1
- SoC JieLi AC791N / WL82, CPU pi32v2.
- Motore FM a 6 operatori derivato da Dexed/msfa, 32 algoritmi, fino a 12 voci nel firmware di fabbrica.
- Tastiera di 27 tasti in silicone; i 16 tasti bianchi sono usati come step nei sequencer.
- Display TFT a colori da 1,54".
- Encoder rotativi (4 nella parte parametri, secondo il manuale), pulsanti OCT− / OCT+, circa 12 pulsanti di modalità.
- Speaker integrato, uscita jack 3,5 mm, batteria.
- MIDI su USB-C, BLE e TRS da 3,5 mm (solo IN). Su USB è un dispositivo composito audio + MIDI.

### Basi software disponibili
| Base | Licenza | Note |
|---|---|---|
| **Felucca** (`hugelton/Felucca`) | GPL-3.0-only | Firmware multi-motore molto maturo. Ha un livello host (`platforms/host`, API `felucca_host.h`) con build web (`platforms/web`), SDL2 desktop e libretro, quindi si sviluppa e si testa senza hardware. `./build.sh gen` genera gli header senza la toolchain dell'FM-1; `./build.sh web|sdl|host|test` per gli altri target. Strato hardware separato in `felucca-bsp` (submodule). Documentazione in `docs/` (ARCHITECTURE.md, PORTING.md, PLUGINS.md, COMPATIBILITY.md, BUILDING.md). Strumenti: `tools/fm1_install.py`, web installer, editor web. |
| **FM1 Open** (`m-vave/fm1_open`) | GPLv3 (verificare i dettagli) | Firmware open source ufficiale M-VAVE, con SDK e Workbench web (`open.m-vave.com/fm1-workbench`). Sequencer a 8 tracce, 6 motori. |
| **Hortator**, **JIANT** | GPL-3.0 | Fork di Felucca: esempi utili di come si trasforma Felucca in un'applicazione con un'altra interfaccia. |

**Scelta consigliata: fork di Felucca**, perché la build web/SDL permette di sviluppare quasi tutto senza flashare il dispositivo e ci sono già fork che ne cambiano completamente lo scopo. Claude Code deve comunque confrontare le due basi nella fase 0 e motivare la scelta.

### Installazione e recovery
- Il pacchetto installabile è un file `.fwsc`; ogni firmware ha una "identity" (es. `FM-1_910` per Felucca). **Il progetto deve usare una identity propria** che non vada in conflitto con altri firmware (verificare come si sceglie).
- Installazione via web installer (Chrome/Edge, WebUSB) o `tools/fm1_install.py`.
- Per tornare al firmware ufficiale c'è l'M-Updater di M-VAVE.
- Se un aggiornamento si interrompe, il chip resta in boot mode (dispositivo USB `WL80UBOOT`, ID `4C4A:8057`). Il recupero è documentato da Felucca (FM-1 Transporter con un Seeed XIAO RP2040).

---

## 3. Funzionalità

### 3.1 Tipi di esercizio (MVP)

1. **Intervalli**
   - Modalità: ascendente, discendente, armonico (simultaneo), misto.
   - Insieme selezionabile: da 2ª minore a ottava, poi opzionalmente fino alla 15ª.
   - Nota di partenza casuale entro un'estensione configurabile.
2. **Qualità degli accordi**
   - Triadi: maggiore, minore, diminuita, aumentata, sus2, sus4.
   - Settime: maj7, 7, m7, m7b5, dim7 (livello avanzato).
   - Opzionale: stato fondamentale / rivolti; esecuzione a blocco o arpeggiata.
3. **Gradi della scala in contesto**
   - Il synth stabilisce la tonalità (cadenza I–IV–V–I oppure bordone sulla tonica), poi suona una nota; l'utente deve dire che grado è.
   - Maggiore all'inizio, poi minore e cromatismi.

### 3.2 Esercizi successivi (dopo l'MVP)
- Dettato melodico breve (3–5 note, da ripetere sulla tastiera).
- Progressioni armoniche (riconoscere I–IV–V–vi ecc.).
- Riconoscimento di scale e modi.
- Opzionale: riferimento di intonazione ("suona un La").

### 3.3 Modalità di risposta
- **Suonando**: l'utente suona la risposta sulla tastiera (es. la seconda nota dell'intervallo). Opzione per accettare la nota in qualsiasi ottava.
- **Scegliendo**: le risposte possibili vengono assegnate ai tasti (es. 12 tasti = 12 intervalli, con etichetta a display) oppure si sceglie da lista con un encoder.
- Ogni esercizio dichiara quali modalità supporta.

### 3.4 Flusso di una domanda
1. Il synth suona il quesito.
2. L'utente può riascoltarlo (pulsante dedicato) quante volte vuole; i riascolti vengono contati.
3. L'utente risponde.
4. Feedback immediato: giusto/sbagliato, a display e con un breve segnale sonoro opzionale; se è sbagliato si mostra e si fa sentire la risposta corretta, con la possibilità di confrontarla con quella data.
5. Si passa alla domanda successiva (automaticamente o con un tasto, configurabile).

### 3.5 Sessioni, livelli, statistiche
- Sessione di N domande (10/20/50/infinito) con riepilogo finale: percentuale, tempo medio di risposta, errori più frequenti.
- Statistiche persistenti per singolo elemento (es. "6ª minore discendente: 14/20").
- **Selezione adattiva**: gli elementi sbagliati più spesso escono più di frequente (pesatura semplice, documentata).
- Livelli predefiniti per esercizio (es. Intervalli L1 = 3ª M, 5ª G, 8ª; L2 = aggiunge 4ª G, 6ª M…) più un livello "personalizzato".
- Reset delle statistiche dal menu, con conferma.

### 3.6 Suono
- Scelta del timbro tra pochi preset FM adatti all'ascolto (piano, piano elettrico, organo, sinusoide pura).
- Durata delle note e tempo regolabili.
- Volume del segnale di feedback separato da quello del quesito.
- Velocity fissa per i quesiti, per non dare indizi.

### 3.7 Impostazioni persistenti
Ultimo esercizio, livello, modalità di risposta, estensione, timbro, tempo, statistiche. Il salvataggio deve essere robusto a spegnimenti improvvisi (batteria): verificare come lo gestisce la base scelta.

### 3.8 Lingua
Testi a display in inglese per default, con struttura che permetta di aggiungere l'italiano (tabella di stringhe). Nomi delle note selezionabili: C D E… oppure Do Re Mi…

---

## 4. Architettura richiesta

- **Nucleo didattico separato** (`trainer/` o simile), in C puro, senza dipendenze dall'hardware:
  - generatore di quesiti (per ogni tipo di esercizio);
  - valutazione delle risposte;
  - macchina a stati della sessione;
  - statistiche e selezione adattiva;
  - generatore pseudo-casuale con seed impostabile (test riproducibili).
- **Livello interfaccia**: rendering a display e mappatura dei controlli, sopra le API grafiche e di input già presenti nella base.
- **Livello audio**: usa il motore FM esistente tramite le sue API di note on/off; nessun nuovo motore sonoro.
- Le API del nucleo devono essere documentate negli header.
- Mantenere separato il più possibile il codice nuovo da quello della base, per facilitare l'aggiornamento al codice upstream (merge di nuove versioni di Felucca/FM1 Open).

---

## 5. Mappatura dei controlli

**Non inventare nomi di pulsanti.** Nella fase 0 Claude Code deve ricavare dal codice (e dalle pagine Controls/Layout della wiki di Felucca) l'elenco reale di tasti, pulsanti ed encoder, poi proporre una tabella di mappatura per la modalità Trainer, da approvare prima di implementarla.

Funzioni che la mappatura deve coprire:
- entrare/uscire dalla modalità Trainer;
- scegliere esercizio e livello;
- riascoltare il quesito;
- confermare / passare alla domanda successiva;
- mostrare la risposta;
- aprire impostazioni e statistiche;
- scorrere le opzioni (encoder).

---

## 6. Piano di lavoro a fasi

Alla fine di ogni fase Claude Code si ferma, riassume cosa ha fatto e aspetta conferma prima di proseguire.

### Fase 0: ricognizione (nessun codice)
- Clonare Felucca (`git clone --recurse-submodules`) e FM1 Open.
- Leggere README, BUILDING.md, docs/ARCHITECTURE.md, PORTING.md, licenze.
- Produrre `docs/RECON.md` con:
  - confronto tra le due basi e scelta motivata;
  - come si aggiunge una nuova modalità/schermata;
  - API di display, input, audio, salvataggio;
  - elenco reale dei controlli e proposta di mappatura;
  - toolchain necessaria per la build hardware e come ottenerla;
  - come si assegna l'identity del firmware;
  - rischi individuati.

### Fase 1: nucleo didattico + test
- Implementare il nucleo (sezione 4) per i tre esercizi MVP.
- Test unitari eseguibili sull'host (`./build.sh test` o equivalente): generazione dei quesiti, valutazione, selezione adattiva, serializzazione delle statistiche.

### Fase 2: interfaccia nell'emulatore
- Integrare la modalità Trainer nella build web o SDL.
- Deve essere possibile usare tutto il flusso (sezione 3.4) nel browser o sul desktop.

### Fase 3: build e installazione sull'hardware
- Build del `.fwsc` con identity propria.
- Procedura di installazione documentata, **con backup del dispositivo prima del flash**.
- Checklist di prova manuale sul dispositivo.

### Fase 4: rifinitura
- Statistiche avanzate, stringhe in italiano, esercizi successivi (sezione 3.2), web editor/installer se utile.

---

## 7. Criteri di accettazione (MVP)

- I tre esercizi MVP funzionano sul dispositivo con entrambe le modalità di risposta previste.
- La modalità synth normale è ancora utilizzabile.
- Impostazioni e statistiche sopravvivono a spegnimento e riaccensione.
- Nessun crash né blocco audio in una sessione di 100 domande.
- Test unitari verdi sull'host.
- Si torna al firmware ufficiale con M-Updater (procedura verificata e documentata).
- README con installazione, controlli, esercizi e procedura di recovery.

---

## 8. Vincoli e regole

- **Licenza**: il progetto derivato resta GPL-3.0; mantenere copyright e attribuzioni della base.
- **Sicurezza del dispositivo**: non eseguire mai comandi di flash o di scrittura sul dispositivo senza conferma esplicita. Prima di ogni flash ricordare di fare il backup.
- Non modificare bootloader né aree di flash fuori da quelle che usa normalmente la base scelta.
- Commit piccoli e descrittivi; un branch per fase.
- Quando un'informazione di questo documento risulta sbagliata rispetto al codice, vale il codice: segnalarlo e aggiornare questo file.

---

## 9. Riferimenti

- Felucca: https://github.com/hugelton/Felucca (wiki: Installation, Controls, Layout, Troubleshooting)
- Felucca-Engines (ABI dei plugin, MIT): submodule di Felucca
- FM1 Open: https://github.com/m-vave/fm1_open
- FM-1 Workbench: https://open.m-vave.com/fm1-workbench
- Hortator (fork di Felucca, esempio di riuso): https://github.com/DeadActive/hortator
- JIANT (fork di Felucca): https://github.com/juanjiant-bit/JIANT
- Elenco dei firmware della community: https://fm1-editor.com/firmware/
- Pagina prodotto e firmware ufficiale: https://www.m-vave.com/product?id=fm-1
