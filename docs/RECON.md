# RECON — Fase 0

Verificato sul codice sorgente clonato in `vendor/Felucca` (commit corrente di `main`, con submodule
`firmware/bsp` e `firmware/engines`) e `vendor/fm1_open` (README `V14`). Dove le mie conclusioni
correggono la spec (`fm1-ear-trainer-spec.md`), lo segnalo esplicitamente con **[DISCREPANZA]**.

## 1. Confronto Felucca vs FM1 Open e scelta

| | Felucca | FM1 Open |
|---|---|---|
| Licenza | GPL-3.0-only (app) + MPL-2.0 (`firmware/bsp`) + MIT (`firmware/engines`) | GPLv3 **con eccezione di linking per l'SDK FM1** (`LICENSE.md`); il motore sonoro è un **SDK binario** con termini propri (`THIRD_PARTY.md`) |
| Build hardware | macOS: Docker (`linux/amd64` debian) + toolchain JieLi Linux + AC79 SDK Apache-2.0. Linux x86-64: nativo | **Richiede Windows x64 + PowerShell**, toolchain JieLi Windows 2.5.3; nessun percorso macOS/Linux documentato per il build del firmware |
| Sviluppo senza hardware | Sì: `tests/run_tests.sh` (unit/regression test su host) e `web/emu` (Emscripten → WebAssembly, demo browser) | Preview web (`Build-Web.ps1`), ma costruita con tooling PowerShell/WASI SDK; non ho trovato una suite di unit test host-side equivalente |
| Motore audio | Sorgente completo (FM6 derivato da Dexed/msfa, più DRUM, PHYS, GRAIN, SLICE, SAMPLE, FORMANT, ADPCM, DIGITAL) | Binario chiuso (SDK); solo lo strato app è open |
| Architettura UI | Pagine per famiglia (`FAM_*`) + "quick layer" (HOLD/SET) su 7 pulsanti; codice in stile "unity build" (molti `.c` si `#include`-ano a vicenda) | Non verificata in profondità (vedi §7, rischio) |
| Documentazione | `BUILDING.md` dettagliato; **non esistono** `ARCHITECTURE.md`, `PORTING.md`, `PLUGINS.md`, `COMPATIBILITY.md` top-level citati dalla spec | `README.md` + `app/instrument.h` (API di tuning) |

**[DISCREPANZA]** La spec (sezione 2) descrive per Felucca un livello host con `platforms/host`,
`felucca_host.h`, build `./build.sh gen|web|sdl|host|test`. Non esiste nulla di tutto ciò nel
repo attuale: non c'è cartella `platforms/`, non c'è `felucca_host.h`, `build.sh` non accetta
sottocomandi (fa sempre il build firmware completo via `tools/build.py`), non c'è target SDL2 né
libretro (verificato con una ricerca a tappeto, zero risultati). Esistono invece:
- `tests/run_tests.sh` — unit/regression test su host (quello che la spec chiamava "build.sh test");
- `web/emu/` — demo browser via Emscripten/WebAssembly (quello che la spec chiamava "build.sh web"),
  già pubblicata su `https://hugelton.github.io/Felucca/webapp/try/`.

Nessun equivalente SDL2 desktop nativo: lo sviluppo "senza hardware" previsto dalla spec per la
Fase 2 va fatto o nel browser (`web/emu`) o estendendo i test host (`tests/`), non in una build
SDL separata.

**Scelta: Felucca**, confermata. Motivi, in ordine di peso:
1. Motore sonoro e BSP interamente open (GPL/MPL/MIT): possiamo leggere e, se necessario, estendere
   qualunque livello, cosa impossibile con l'SDK binario di FM1 Open.
2. Build riproducibile su macOS (Docker) senza Windows.
3. Esiste già un percorso di sviluppo/test senza hardware (`tests/run_tests.sh` + `web/emu`), anche
   se con nomi e strumenti diversi da quelli ipotizzati in spec.
4. Codice e commit message estremamente documentati riga per riga (ogni file spiega il "perché"),
   il che riduce il rischio di rompere qualcosa per Trainer.

## 2. Come si aggiunge una nuova modalità/schermata

Non esiste un sistema a "schermate" plugin-like. L'architettura reale (`firmware/src/`):

- **Loop principale** (`main.c`, `fm1_main`): dopo boot e init, gira per sempre chiamando ogni
  frame (~60/s) `ui_input()` → `ui_leds()` → `ui_draw()`. Esistono già overlay che "rubano" il
  loop principale (es. il countdown UBOOT in `main.c`, disegnato direttamente sopra tutto), quindi
  c'è un precedente per una modalità che sostituisce temporaneamente input/draw normali.
- **UI normale**: pagine organizzate per famiglia (`FAM_HOME`, `FAM_SCL`, `FAM_SEQ`, ...) definite
  come tabelle (`page_t`), più 7 "quick layer" attivabili tenendo premuto un pulsante pagina
  (`ui_layer.c`, tabella `LAYERS[]`): FX, GLO, SCL, EDIT, SEQ, REC. Ogni pulsante pagina ha già un
  ruolo fisso.
- **Stile del codice**: molti `.c` si includono a vicenda (es. `main.c` fa `#include "timer5.c"`,
  `ui_layer.c` fa `#include "ui_tools.c"`) e le funzioni sono quasi tutte `static` — è un "unity
  build", non moduli con header pubblici. Le funzioni di note on/off (`trk_note_on`, `trk_note_off`,
  `trk_all_off` in `voice.c`) sono `static`, non esposte.

**Conseguenza per l'architettura richiesta (sezione 4 della spec):**
- Il **nucleo didattico** (`trainer/`) può essere scritto in C puro, senza header di Felucca, esattamente
  come richiesto: non deve conoscere nulla dell'hardware o dello stile unity-build.
- Il **livello audio/UI** dentro Felucca, invece, non potrà limitarsi a "chiamare delle API
  esistenti": quelle API (note on/off) oggi non sono pubbliche. Andranno aggiunte poche funzioni
  ponte non-`static` (nuove, minime, isolate in un file proprio, es. `trainer_bridge.c`) che
  richiamano `trk_note_on`/`trk_note_off`/`trk_all_off` sul `track_t` scelto per il Trainer. Questo
  resta comunque un cambiamento piccolo e localizzato, coerente col vincolo "codice nuovo separato
  dalla base" (sezione 4), ma è un rischio/costo che la spec non menzionava: va tenuto d'occhio nei
  merge con l'upstream Felucca.
- La **modalità Trainer** come "schermo" terrà lo stesso schema degli overlay esistenti: un flag
  (es. `ui.trainer`) controllato in testa a `ui_input()`/`ui_draw()`/`ui_leds()` che, quando attivo,
  salta la UI normale e usa routine di input/draw proprie (coerente con come UBOOT già si comporta).

## 3. API di display, input, audio, salvataggio (riferimenti nel codice)

- **Input**: `firmware/bsp/boards/fm1/fm1_input.h` — matrice 11 colonne × 6 righe, 14 pulsanti
  (id 0..13) + 27 tasti nota (id 14..40, nota 0 = F3 … nota 26 = G5) + 7 encoder in quadratura con
  rilevamento "detent". API principali: `fm1_input_scan()`/`fm1_input_tick()` (polling/IRQ),
  `fm1_input_edges()` (fronti pulsanti), `fm1_input_note_edges()` (fronti tasti),
  `fm1_enc_take(ruolo)` (passi encoder dall'ultima lettura), `fm1_led_key(id, on)`.
  `panel.c` mappa gli id di matrice ai 14 **nomi reali** dei pulsanti e ai 7 ruoli degli encoder
  (vedi §4) — mappatura calibrabile sull'hardware vero (OCT- + OCT+ al boot), quindi i numeri sono
  "best guess" finché non verificati su un FM-1 reale.
- **Display**: `lcd.c` (driver TFT), `gfx.c`/`ui_draw.c`/`ui_graph.c` (primitive di disegno),
  `ui.c`/`ui_menu.c`/`ui_help.c` (menu e help). TFT a colori, confermato 240×240 come area utile
  (vedi `lcd_fill(0, 0, 240, 240, ...)` usato ovunque in `main.c`) — la spec dice "1,54\"" ma non
  specifica la risoluzione in pixel; 240×240 è coerente con un pannello da 1,54" comune.
- **Audio**: `audio.c` (avvio/stop, `fm1_audio_stop()`), `voice.c` (gestione voci per traccia,
  `NVOICE` = budget condiviso), `engines.c`/`eng_fm6.c`/`fm6_core.c` (motore FM, 32 tabelle di
  algoritmo, derivato da msfa/Dexed — confermato). Note on/off oggi `static` in `voice.c`
  (`trk_note_on`, `trk_note_off`, `trk_all_off`): da esporre con un bridge (vedi §2).
- **Salvataggio**: `storage.c`/`storage_hw.c` (flash), `settings_persist.c` (record di impostazioni
  con "magic" + migrazione versione su versione, es. `PERSIST_MAGIC`), `project.c` (progetti/preset).
  Il meccanismo è già pensato per sopravvivere a spegnimenti improvvisi e ad aggiornamenti di
  firmware (vecchi formati vengono migrati, non scartati) — adatto a ospitare un blocco dati
  Trainer (impostazioni + statistiche) seguendo lo stesso schema a "magic number" versionato.
  Verifica approfondita di `storage.c`/`storage_hw.c` rimandata alla Fase 1, quando si progetta il
  formato dati del Trainer.

## 4. Controlli reali e proposta di mappatura (da approvare)

**[DISCREPANZA]** La spec (sezione 2) parla di "circa 12 pulsanti di modalità" e "4 encoder nella
parte parametri". Il codice (`panel.c`, `fm1_input.h`) dà numeri precisi e diversi:

- **14 pulsanti reali** (matrice id 0..13, nomi da `panel.c`):
  `FX`, `SCL`, `ENV`, `LFO`, `EDIT`, `GLO`, `HOME`, `SAVE`, `ARP`, `SEQ`, `PLAY`, `REC`, `OCT-`, `OCT+`.
- **7 encoder reali** (non 4): `SELECT`, `ALGORITHM`, `PRESETS`, `KNOB 1`, `KNOB 2`, `KNOB 3`, `KNOB 4`.
- **27 tasti nota** (F3..G5), di cui **16 tasti bianchi** (F3 G3 A3 B3 C4 D4 E4 F4 G4 A4 B4 C5 D5 E5
  F5 G5) — questo numero **coincide** con la spec ("16 tasti bianchi... usati come step").

Tutti i 14 pulsanti hanno già un ruolo nella UI normale di Felucca (pagine o quick layer); **nessuno
è libero**. Per entrare/uscire dal Trainer senza rubare un pulsante già in uso, propongo (da
discutere e approvare prima di implementare, come richiesto dalla spec):

- **Ingresso**: una voce di menu (`HOME` → `MENU` → nuova voce `TRAINER`), più eventualmente una
  combinazione "hold" coerente con lo stile esistente (es. `HOME` tenuto premuto a lungo, sul
  modello delle altre quick-layer), da verificare che non collida con la pressione lunga già
  usata da `HOME` nella UI normale.
- **Uscita**: stesso percorso (`MENU` → `EXIT TRAINER`), oppure `HOME` di nuovo se in Trainer si
  intercetta prima della UI normale.
- **Dentro al Trainer** (funzioni da sezione 5 della spec), proposta di riuso dei ruoli già
  "naturali": `PLAY` = ripeti/conferma, `REC` = mostra risposta, `SAVE` = (non usato o reset stats
  con conferma), `OCT-`/`OCT+` = livello su/giù, `KNOB 1..4` = scorrere opzioni/impostazioni,
  `SELECT`/`PRESETS` = scegliere esercizio/livello. Nessuna di queste assegnazioni è definitiva:
  vanno confermate col codice `ui_input.c` reale (quali pulsanti generano eventi "hold" vs "tap")
  e, idealmente, su hardware vero, prima della Fase 2.

## 5. Toolchain per la build hardware

- **macOS** (ambiente dell'utente): Python 3 + Pillow + fontTools; Docker Desktop (il toolchain
  JieLi è Linux x86-64 only, girato in container `linux/amd64 debian:bookworm-slim`, via Rosetta su
  Apple Silicon); toolchain JieLi (`tools/get_toolchain.sh` → `~/.jieli/toolchain`); JieLi AC79 SDK
  Apache-2.0 (`git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13
  https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK`); Node.js opzionale (test
  web); Emscripten opzionale (`web/emu`).
- Build: `./build.sh` (debug) o `./build.sh --release X.Y` (release, assegna anche l'identity).
- Nessuno di questi strumenti è ancora installato in questa sessione: andranno scaricati quando si
  arriverà alla Fase 3 (o prima, se si vuole verificare la build reale già in Fase 1/2). Non ho
  eseguito download né installazioni in questa fase.

## 6. Identity del firmware

Il meccanismo reale (`tools/build.py`, `tools/fm1pkg_make.py`, `firmware/src/felucca.c`,
`tools/fm1_install.py`):
- L'identity è una stringa di 8 caratteri tipo `FM-1_9XY`, incorporata sia nel binario
  (`FELUCCA_ID` in `felucca.c`, default `"FM-1_900"`) sia nel pacchetto `.fwsc`
  (`PRODUCT` in `build.py`, stesso default; con `--release X.Y` diventa `FM-1_9` + le due cifre).
  È scritta come marcatore ripetuto nei primi 20 blocchi del pacchetto (`fm1pkg_make.py`).
- L'installer (`fm1_install.py`) legge l'identity del dispositivo connesso e, dopo l'installazione,
  **verifica** che l'identity scritta corrisponda a quella del pacchetto (uscita 6 = "wrong model",
  un codice diverso = "another identity after install"); è un controllo di coerenza, non un
  meccanismo di sicurezza bloccante lato firmware.
- Il firmware ufficiale stock è riconosciuto per hash SHA-256 esatto (due costanti nello script),
  non per identity — tornare al firmware ufficiale M-Updater non dipende dalla nostra identity.

**Proposta per il progetto**: usare un'identity propria, distinta sia dal default Felucca
(`FM-1_9XY`, riservato agli aggiornamenti upstream) sia da quella del firmware ufficiale, per non
essere scambiati per un aggiornamento Felucca o per il firmware di fabbrica. Serve una stringa di
esattamente 8 caratteri; propongo qualcosa come `FM-1_EA1` (EA = Ear trainer) — **da confermare con
l'utente** prima di impostarla, perché cambia cosa il web-installer e `fm1_install.py` accettano
come "lo stesso firmware" per i futuri aggiornamenti incrementali del progetto.

## 7. Rischi individuati

1. **API audio non pubbliche**: le funzioni di note on/off sono `static` nel file monolitico
   `voice.c`. Richiede un piccolo bridge nuovo (rischio basso, ma non nullo per i merge upstream).
2. **Nessuna build SDL/desktop nativa**: lo sviluppo "senza hardware" previsto per la Fase 2 dovrà
   usare `web/emu` (richiede Emscripten) o i test host (`tests/run_tests.sh`), non un eseguibile
   desktop come ipotizzato in spec. Va verificato che `web/emu` sia comodo per iterare una UI
   interattiva (il browser demo, dalla doc, non emula USB/MIDI/TRS/sample utente — accettabile per
   il Trainer che non ne ha bisogno).
3. **Tutti i pulsanti fisici sono già assegnati**: entrare/uscire dal Trainer richiede
   necessariamente un nuovo gesto o una voce di menu; nessuna scelta è "ovvia" e serve conferma
   utente (sezione 4 sopra) prima di implementare, come richiesto dalla spec stessa.
4. **Toolchain hardware non banale su macOS**: Docker + toolchain Linux + SDK Apache-2.0 di terze
   parti da scaricare da Gitee; da verificare che il download funzioni dalla rete dell'utente
   quando si arriva alla Fase 3 (non ancora provato).
5. **Build FM-1 Open non verificata**: essendo Windows-only, non ho potuto verificarne la build;
   l'informazione sul binario SDK viene dal README, non da un tentativo di build reale.
6. **Divergenza spec/codice sui controlli**: la spec sottostima encoder (4 vs 7 reali) e non elenca
   i nomi reali dei 14 pulsanti; corretto qui, ma la sezione 2 del file di spec dovrebbe essere
   aggiornata (vedi nota finale).
7. **Verifica incompleta**: questa ricognizione ha letto la documentazione principale e le parti di
   codice più rilevanti per l'architettura (input, panel, main loop, voice, settings, identity), ma
   non l'intero albero (76 file in `firmware/src/`, più BSP ed engines). Dettagli su `project.c`,
   `storage_hw.c` e sulla UI di menu (`ui_menu.c`) andranno approfonditi all'inizio della Fase 1,
   quando servirà il formato dati esatto per statistiche/impostazioni persistenti.

## 8. Nota sulla spec

Come richiesto dalla sezione 8 della spec ("quando un'informazione... risulta sbagliata rispetto al
codice, vale il codice"): la sezione 2 di `fm1-ear-trainer-spec.md` (tabella basi software, in
particolare la riga Felucca) contiene affermazioni non verificate nel codice attuale
(`platforms/host`, `felucca_host.h`, target `web|sdl|host|test` di `build.sh`, "fino a 12 voci").
Non l'ho modificata in questa fase per non alterare un documento di riferimento senza conferma
esplicita; segnalo qui le correzioni e aspetto indicazioni se aggiornarla.
