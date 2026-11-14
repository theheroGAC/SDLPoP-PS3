# SDLPoP PS3

Native **Sony PlayStation 3** port of **SDLPoP** (v1.24 RC), the open-source reimplementation of *Prince of Persia* based on the disassembly of the original DOS version.

Built with the **ps3dev / PSL1GHT** toolchain (`ppu-gcc` Cell PPU), **SDL2**, **SDL2_image**, and the native Cell `libaudio` backend.

---

## Features

- **RSX Hardware Rendering**: Fullscreen output at the native XMB video resolution (720p / 1080p) with 4:3 aspect ratio (`use_correct_aspect_ratio = true`) and `sharp` scaling.
- **TV Overscan Compensation**: The active viewport is scaled to `90%` and centered (`5%` margin on each side) so the bottom HUD text and health hearts remain fully visible even on TVs with overscan enabled.
- **Native 48 kHz Stereo PS3 Audio** (`ps3_audio.c`): Direct output via `libaudio` (`audioInit` / `audioPortOpen`, 2 channels, 8 blocks) with a dedicated thread for sound effects, OPL3 music, and OGG tracks.
- **Ready-to-Use DualShock 3 / Sixaxis Mapping**: No manual setup required.
- **In-Game Options Menu** (`Select` / `Start`): Video, audio, saves, cheats, and starting level settings.
- **Instant Quicksave / Quickload** (`L1` / `R1`) and **PNG Screenshots** (`R3`).
- **Game Data Bundled in the PKG**: `data/` and `SDLPoP.ini` are installed directly into `USRDIR` (no manual DOS files required).

---

## Controls

| PS3 Button | Action |
|---|---|
| **D-Pad / Left Stick** | Move the Prince / navigate menus |
| **Cross (`X`)** | Action (`Shift`): grab ledges, draw/swing sword, drink potions, careful step |
| **Square** | Confirm in menus (`Enter`) |
| **Triangle** | Space (show remaining time / advance screens) |
| **Select / Start** | Open/close pause and **Options Menu** (`Esc`) |
| **L1** | Quicksave (`QUICKSAVE.SAV`) |
| **R1** | Quickload |
| **R3** | PNG Screenshot (saved to `USRDIR/screenshots/`) |
| **L2 / R2** | Volume down / up |

Use the **D-Pad** or **Left Stick** to navigate menus, **Square** (or **Triangle**) to confirm, and **Select** or **Start** to go back / close the menu.

---

## PS3 Installation

1. Copy `dist/SDLPoP_PS3.gnpdrm.pkg` (recommended for **CFW / HEN**) or `dist/SDLPoP_PS3.pkg` onto a **FAT32**-formatted USB drive.
2. On your PS3, open **Package Manager > Install Package Files** and install the package.
3. Launch **SDLPoP PS3** from the XMB.

Install path: `/dev_hdd0/game/SDLP30001/USRDIR/`

### PS3 Runtime Directory Layout

```text
/dev_hdd0/game/SDLP30001/USRDIR/
  EBOOT.BIN
  SDLPoP.ini
  data/            graphics, levels, sounds, and music
  screenshots/     PNG screenshots
  replays/         recorded replays
  QUICKSAVE.SAV    quick save
  PRINCE.SAV       game save
  PRINCE.HOF       hall of fame high scores
  SDLPoP.cfg       settings saved from the options menu
```

---

## Building from Source

### Prerequisites

- **WSL / Linux** environment with the **ps3dev / PSL1GHT** toolchain (defaults to `/mnt/d/ps3dev-sdk`, overridable via `PS3DEV`)
- `portlibs/ppu` libraries: `SDL2`, `SDL2_image`, `libpng`, `libjpeg`, `zlib`
- SDK tools in `$PS3DEV/bin` and `$PS3DEV/ppu/bin`: `ppu-gcc`, `powerpc64-ps3-elf-strip`, `sprxlinker`, `make_self_npdrm`, `sfo`, `pkg`, `package_finalize`

### Build & Packaging

```bash
export PS3DEV=/mnt/d/ps3dev-sdk   # optional if installed at the default path
./build.sh                        # builds build/sdlpop.elf
./package.sh                      # signs EBOOT.BIN and creates .pkg files in dist/
```

Generated outputs:
- `build/sdlpop.elf` — Cell PPU ELF executable
- `dist/SDLPoP_PS3.pkg` / `dist/SDLPoP PS3.pkg` — standard PKG package
- `dist/SDLPoP_PS3.gnpdrm.pkg` / `dist/SDLPoP PS3.gnpdrm.pkg` — finalized package for CFW/HEN (`Content ID: UP0001-SDLP30001_00-0000000000000000`)

---

## Repository Structure

```text
.
├── LICENSE               GNU GPL v3 license
├── Makefile              Cell PPU compile and link rules (ppu-gcc)
├── README.md             Project documentation
├── SDLPoP.ini            Default configuration bundled in the PKG
├── build.sh              Build wrapper for WSL / Linux
├── package.sh            ELF strip, sprxlinker, make_self_npdrm, PARAM.SFO & PKG
├── sfo.xml               PS3 title metadata (SDLP30001)
├── assets/               ICON0.PNG, PIC1.PNG, and make_assets.py script
├── platform/ps3/         PS3 platform layer (input, paths, libaudio, SDL_PSL1GHTrender.o)
├── src/                  SDLPoP 1.24 RC source and assets (with #ifdef PS3 integrations)
└── tools/                Fallback Python utilities for SELF/SFO/PKG
```

---

## PS3 Technical Notes

- **Compile & Link (`Makefile`)**: Uses PSL1GHT machine flags (`-mhard-float -fmodulo-sched -ffunction-sections -fdata-sections`), `-O2 -mcpu=cell -std=gnu99 -fcommon -DPS3 -DSDL_MAIN_HANDLED` without `-m64` or extraneous C++/pthread link groups, keeping `.ctors` aligned with the standard PSL1GHT CRT startup order.
- **Video (`src/src/seg009.c`)**: Converts the 24-bit framebuffer to `ARGB8888` for `SDL_UpdateTexture` (as expected by `SDL_PSL1GHTrender.o`) and applies a centered `90%` viewport to compensate for TV overscan.
- **Audio (`platform/ps3/ps3_audio.c`)**: Opens the PS3 audio port at `48000 Hz` stereo and converts S16 samples into normalized floats inside a ring buffer serviced by a PPU thread.
- **Boot Debug Log (`platform/ps3/ps3_platform.c`)**: In `ps3_platform_init()`, opens the debug log on USB (`/dev_usb000/sdlpop_debug.log`, `/dev_usb001/sdlpop_debug.log`) or HDD (`/dev_hdd0/game/SDLP30001/USRDIR/debug.log`, `/dev_hdd0/tmp/sdlpop_debug.log`).

---

## Credits & Licenses

- **David Nagy (NagyD)** and contributors for **SDLPoP** ([GNU GPL v3](LICENSE)).
- **Jordan Mechner** for *Prince of Persia*.
- **ps3dev / PSL1GHT** for the PS3 homebrew toolchain and libraries.
- **SDL2 / SDL2_image** and the **PSL1GHT** driver.

*Prince of Persia* is a registered trademark of Ubisoft. This is an unofficial homebrew project not affiliated with Sony Computer Entertainment or Ubisoft.
