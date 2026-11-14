# SDLPoP PS3

Port nativo per **Sony PlayStation 3** di **SDLPoP** (v1.24 RC), la reimplementazione open source di *Prince of Persia* basata sul disassemblato della versione DOS originale.

Compilato con toolchain **ps3dev / PSL1GHT** (`ppu-gcc` Cell PPU), **SDL2**, **SDL2_image** e backend audio nativo Cell `libaudio`.

---

## Caratteristiche

- **Rendering hardware RSX**: output fullscreen alla risoluzione nativa impostata su XMB (720p / 1080p) con aspect ratio 4:3 (`use_correct_aspect_ratio = true`) e scaling `sharp`.
- **Compensazione overscan TV**: l'area utile viene scalata al `90%` centrata (`5%` di margine per lato), così le scritte inferiori e i cuori restano sempre visibili anche su TV con overscan attivo.
- **Audio nativo PS3 a 48 kHz stereo** (`ps3_audio.c`): driver diretto tramite `libaudio` (`audioInit` / `audioPortOpen`, 2 canali, 8 blocchi) con thread dedicato per effetti sonori, musica OPL3 e tracce OGG.
- **Mappatura DualShock 3 / Sixaxis pronta all'uso**: nessuna configurazione richiesta.
- **Menu opzioni in-game** (`Select` / `Start`): impostazioni grafiche, audio, salvataggi, trucchi e livello iniziale.
- **Quicksave / Quickload** immediati (`L1` / `R1`) e **Screenshot PNG** (`R3`).
- **Dati di gioco inclusi nel PKG**: `data/` e `SDLPoP.ini` vengono installati direttamente in `USRDIR` (nessun file DOS da copiare manualmente).

---

## Comandi

| Pulsante PS3 | Azione |
|---|---|
| **D-Pad / Stick sinistro** | Muovi il principe / naviga nei menu |
| **Croce (`X`)** | Azione (`Shift`): afferra sporgenze, estrai/colpisci con la spada, bevi, passo cauto |
| **Quadrato** | Conferma nei menu (`Invio`) |
| **Triangolo** | Spazio (mostra tempo rimasto / avanza schermate) |
| **Select / Start** | Apri/chiudi pausa e **Menu Opzioni** (`Esc`) |
| **L1** | Quicksave (`QUICKSAVE.SAV`) |
| **R1** | Quickload |
| **R3** | Screenshot PNG (salvato in `USRDIR/screenshots/`) |
| **L2 / R2** | Volume giù / su |

Nei menu si naviga con **D-Pad** o **stick sinistro**, si conferma con **Quadrato** (o **Triangolo**) e si torna indietro / si chiude con **Select** o **Start**.

---

## Installazione su PS3

1. Copia `dist/SDLPoP_PS3.gnpdrm.pkg` (consigliato per **CFW / HEN**) oppure `dist/SDLPoP_PS3.pkg` su una chiavetta USB formattata in **FAT32**.
2. Sulla PS3 apri **Package Manager > Install Package Files** e installa il pacchetto.
3. Avvia **SDLPoP PS3** dalla XMB.

Percorso di installazione: `/dev_hdd0/game/SDLP30001/USRDIR/`

### Struttura runtime su PS3

```text
/dev_hdd0/game/SDLP30001/USRDIR/
  EBOOT.BIN
  SDLPoP.ini
  data/            grafica, livelli, suoni e musica
  screenshots/     screenshot PNG
  replays/         replay registrati
  QUICKSAVE.SAV    salvataggio rapido
  PRINCE.SAV       salvataggio di gioco
  PRINCE.HOF       classifica punteggi
  SDLPoP.cfg       impostazioni salvate dal menu opzioni
```

---

## Compilazione da sorgente

### Prerequisiti

- Ambiente **WSL / Linux** con toolchain **ps3dev / PSL1GHT** (per default cercata in `/mnt/d/ps3dev-sdk`, sovrascrivibile con `PS3DEV`)
- Librerie `portlibs/ppu`: `SDL2`, `SDL2_image`, `libpng`, `libjpeg`, `zlib`
- Strumenti SDK in `$PS3DEV/bin` e `$PS3DEV/ppu/bin`: `ppu-gcc`, `powerpc64-ps3-elf-strip`, `sprxlinker`, `make_self_npdrm`, `sfo`, `pkg`, `package_finalize`

### Build e Packaging

```bash
export PS3DEV=/mnt/d/ps3dev-sdk   # opzionale se installato nel percorso di default
./build.sh                        # compila build/sdlpop.elf
./package.sh                      # firma EBOOT.BIN e genera i .pkg in dist/
```

Output generati:
- `build/sdlpop.elf` — eseguibile ELF Cell PPU
- `dist/SDLPoP_PS3.pkg` / `dist/SDLPoP PS3.pkg` — pacchetto PKG standard
- `dist/SDLPoP_PS3.gnpdrm.pkg` / `dist/SDLPoP PS3.gnpdrm.pkg` — pacchetto finalizzato per CFW/HEN (`Content ID: UP0001-SDLP30001_00-0000000000000000`)

---

## Struttura del repository

```text
.
├── LICENSE               Licenza GNU GPL v3
├── Makefile              Regole di compilazione e link Cell PPU (ppu-gcc)
├── README.md             Documentazione del progetto
├── SDLPoP.ini            Configurazione predefinita inclusa nel PKG
├── build.sh              Wrapper build per WSL / Linux
├── package.sh            Strip ELF, sprxlinker, make_self_npdrm, PARAM.SFO e PKG
├── sfo.xml               Metadati titolo PS3 (SDLP30001)
├── assets/               ICON0.PNG, PIC1.PNG e script make_assets.py
├── platform/ps3/         Livello piattaforma PS3 (input, path, audio libaudio, SDL_PSL1GHTrender.o)
├── src/                  Sorgenti e asset di SDLPoP 1.24 RC (con integrazioni #ifdef PS3)
└── tools/                Utility Python fallback per SELF/SFO/PKG
```

---

## Note tecniche PS3

- **Compilazione e link (`Makefile`)**: usa i flag macchina PSL1GHT (`-mhard-float -fmodulo-sched -ffunction-sections -fdata-sections`), `-O2 -mcpu=cell -std=gnu99 -fcommon -DPS3 -DSDL_MAIN_HANDLED` senza `-m64` né gruppi C++/pthread spuri, allineando i costruttori CRT (`.ctors`) allo schema standard di PSL1GHT.
- **Video (`src/src/seg009.c`)**: converte il framebuffer 24-bit in `ARGB8888` per `SDL_UpdateTexture` (come richiesto dal renderer `SDL_PSL1GHTrender.o`) e applica il viewport centrato al `90%` per compensare l'overscan delle TV.
- **Audio (`platform/ps3/ps3_audio.c`)**: apre la porta audio PS3 a `48000 Hz` stereo convertendo i campioni S16 in float normalizzati su un ring buffer gestito da thread PPU.
- **Log di avvio (`platform/ps3/ps3_platform.c`)**: in `ps3_platform_init()` apre il log di debug su USB (`/dev_usb000/sdlpop_debug.log`, `/dev_usb001/sdlpop_debug.log`) oppure su HDD (`/dev_hdd0/game/SDLP30001/USRDIR/debug.log`, `/dev_hdd0/tmp/sdlpop_debug.log`).

---

## Crediti e Licenze

- **David Nagy (NagyD)** e contributori per **SDLPoP** ([GNU GPL v3](LICENSE)).
- **Jordan Mechner** per *Prince of Persia*.
- **ps3dev / PSL1GHT** per toolchain e librerie homebrew PS3.
- **SDL2 / SDL2_image** e driver **PSL1GHT**.

*Prince of Persia* è un marchio registrato di Ubisoft. Progetto homebrew non affiliato a Sony Computer Entertainment né a Ubisoft.
