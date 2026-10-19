# Zugfish ♟️

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Qt 6 Quick](https://img.shields.io/badge/Qt-6.4%2B%20QML-green.svg)](https://www.qt.io/)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows%20%7C%20macOS-informational.svg)]()
[![Stockfish](https://img.shields.io/badge/Engine-Stockfish%20UCI-orange.svg)](https://stockfishchess.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

**Zugfish** is an open-source, high-performance chess game reviewer and analysis suite with a modern **Chess.com-style UI**. Powered by a native C++20 engine core, Qt 6 QML, and Stockfish UCI, Zugfish provides instant, deep game analysis, move classifications, win probability graphs, accuracy scores, and direct imports from Chess.com and Lichess.

Runs natively with zero lag on **Linux**, **Windows**, and **macOS**.

---

## 📸 Screenshots

| Game Review & Accuracy | Blunder & Mistake Breakdown |
| :---: | :---: |
| ![Review Screen](gui/docs/review.png) | ![Blunder Analysis](gui/docs/blunder.png) |

| Welcome Screen & Built-in Demos |
| :---: |
| ![Welcome Screen](gui/docs/welcome.png) |

---

## ✨ Features

- **Chess.com-Style Game Review**:
  - Full move classification: **Brilliant (`!!`)**, **Great (`!`)**, **Best (`[*]`)**, **Excellent (`[!]`)**, **Good (`[+]`)**, **Book (`[B]`)**, **Inaccuracy (`[?!]`)**, **Mistake (`[?]`)**, **Blunder (`[??]`)**, **Missed Win (`[X]`)**, and **Forced (`[]`)**.
  - Strict classification rules: real piece sacrifices required for brilliant moves, blunder detection based on win probability swings.
  - Overall accuracy ratings (0–100%) and Average Centipawn Loss (ACPL) for both players.
  - Key moments summary with one-click navigation to mistakes and missed wins.
- **Interactive Board & Visuals**:
  - Responsive, scalable chess board with high-DPI vector SVG pieces.
  - Animated move transitions and on-board move quality badges.
  - Color-coded engine recommendation arrows (best moves in teal, played moves tinted by quality).
  - Dynamic evaluation bar (centipawns & mate-in-N).
  - Interactive, scrubbable evaluation graph displaying winning probability over the course of the game.
- **Online Game Importer**:
  - Connect directly to the **Chess.com API** and **Lichess.org API**.
  - Fetch recent rapid, blitz, bullet, or classical games by username with one click.
- **Cross-Platform & Native**:
  - Asynchronous background analysis thread ensures the UI remains smooth (60+ FPS) during evaluation.
  - Full native Windows support via Win32 process handling (`CreateProcessA`/`CreatePipe`) and POSIX on Linux/macOS.
- **CLI & GUI**:
  - Includes both a fast terminal tool (`zugfish-cli`) and a rich desktop application (`zugfish-qt`).

---

## ⚡ Quick Start (Copy-Paste)

### Automated Install Script

#### Linux & macOS
```bash
git clone https://github.com/your-username/zugfish.git
cd zugfish
./install.sh
```

#### Windows (PowerShell)
```powershell
git clone https://github.com/your-username/zugfish.git
cd zugfish
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\install.ps1
```

---

## 💻 Installation by Platform

### 1. Arch Linux / Manjaro

Install all required packages and build with Ninja:

```bash
# Install dependencies from official Arch repos
sudo pacman -S --needed base-devel cmake ninja gcc qt6-base qt6-declarative qt6-svg curl nlohmann-json

# Stockfish is in the AUR (or download from stockfishchess.org)
# yay -S stockfish   (or paru -S stockfish)

# Configure and compile
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run
./build/gui/zugfish-qt    # GUI
./build/zugfish-cli       # CLI
```

### 2. Ubuntu 22.04 / 24.04 & Debian 12

```bash
# Install dependencies
sudo apt update
sudo apt install -y build-essential cmake ninja-build \
    qt6-base-dev qt6-declarative-dev libqt6svg6-dev \
    qml6-module-qtquick qml6-module-qtquick-controls2 \
    qml6-module-qtquick-layouts qml6-module-qtquick-templates2 \
    qml6-module-qtquick-shapes \
    libcurl4-openssl-dev nlohmann-json3-dev stockfish

# Configure and compile
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run
./build/gui/zugfish-qt
```

### 3. Fedora 38+ / RHEL 9

```bash
# Install dependencies
sudo dnf install -y cmake ninja-build gcc-c++ \
    qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtsvg-devel \
    libcurl-devel nlohmann-json-devel stockfish

# Configure and compile
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run
./build/gui/zugfish-qt
```

### 4. macOS (Homebrew)

```bash
# Install dependencies
brew install cmake ninja qt@6 stockfish curl nlohmann-json

# Configure and compile
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
cmake --build build

# Run
./build/gui/zugfish-qt
```

---

## 🪟 Windows Support

**Does Zugfish work on Windows? Yes, 100% natively!**

The core engine communication uses native Win32 APIs (`CreateProcessA`, `CreatePipe`, `WriteFile`, `ReadFile`), and the GUI is built on standard cross-platform Qt 6 Quick.

### Option A: Automated PowerShell Installer (Recommended)
Run PowerShell (with script execution policy bypass):
```powershell
powershell -ExecutionPolicy Bypass -File .\install.ps1
```
If Qt 6 is installed in a custom location, specify `-QtPath`:
```powershell
powershell -ExecutionPolicy Bypass -File .\install.ps1 -QtPath "C:\Qt\6.8.0\msvc2022_64"
```
Or build only the CLI tool without requiring Qt 6:
```powershell
powershell -ExecutionPolicy Bypass -File .\install.ps1 -CliOnly
```
The script will:
1. Verify CMake and Stockfish (with optional `winget` installation).
2. Auto-detect Visual Studio 2022 / MinGW and Qt 6.
3. Automatically download and build missing networking dependencies (`curl` via Windows Schannel SSL and `nlohmann-json`) via CMake `FetchContent`.
4. Build `zugfish-qt.exe` and/or `zugfish-cli.exe` in Release mode.
5. Automatically bundle Qt runtime DLLs using `windeployqt`.
6. Install to `%LOCALAPPDATA%\Programs\Zugfish`.
7. Create Desktop and Start Menu shortcuts and add Zugfish to your user `PATH`.

### Option B: Manual Build with MSVC or MinGW
If you have Visual Studio 2022 (with "Desktop development with C++") and Qt 6:
```powershell
# Install Stockfish, CMake, Ninja (via winget if needed)
winget install Stockfish.Stockfish Kitware.CMake Ninja-build.Ninja

# Configure CMake with your Qt6 path
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:\Qt\6.8.0\msvc2022_64"
cmake --build build --config Release

# Bundle Qt DLLs for a standalone executable
windeployqt --qmldir gui\qml build\gui\Release\zugfish-qt.exe

# Run
.\build\gui\Release\zugfish-qt.exe
```

---

## 🎮 Usage Guide

### Desktop GUI (`zugfish-qt`)

Launch the GUI by simply typing `zugfish` in your terminal or clicking the desktop shortcut.

#### Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| `←` / `→` | Step one move backwards / forwards |
| `Home` / `End` | Jump to game start / game end |
| `Space` | Play / Pause autoplay playback |
| `F` | Flip board perspective (White ⇄ Black) |
| `J` / `K` | Jump to previous / next Key Moment (mistake, blunder, miss) |
| `R` | Run engine review |
| `Ctrl + O` | Open local PGN file |
| `Ctrl + I` | Open Chess.com / Lichess import dialog |
| `Ctrl + V` | Paste PGN text from clipboard |
| `Ctrl + ,` | Open Settings (Engine depth, Stockfish path, board theme) |
| **Scroll Wheel** | Hover over board and scroll up/down to step through moves |

#### Importing Online Games
1. Press `Ctrl + I` or click **Import** in the top bar.
2. Select **Chess.com** or **Lichess.org**.
3. Type any username (e.g. `hikaru`, `magnuscarlsen`, or your own username).
4. Browse the recent games list and click **Load Game** to instantly start reviewing!

---

### Command-Line Interface (`zugfish-cli`)

For headless review, scripts, or terminal output:

```bash
# Display help and options
zugfish-cli --help

# Analyze a local PGN file at depth 14
zugfish-cli game.pgn --depth 14

# Fetch and review the latest game of a Chess.com user
zugfish-cli --chesscom hikaru --depth 12

# Fetch and review the latest game of a Lichess user
zugfish-cli --lichess magnuscarlsen --depth 12
```

#### Example CLI Output:
```
White: Paul Morphy                      Black: Duke of Brunswick
Opening:  C41 - Philidor Defense: Morphy Variation
Accuracy: 83.6%                           Accuracy: 77.2%
ACPL: 34.8                               ACPL: 62.0
--------------------------------------------------------------------------------
 Move Classification                       White              Black
--------------------------------------------------------------------------------
 !! Brilliant Moves                        2                  0
 ! Great Moves                             0                  0
 [*] Best Moves                            10                 5
 [!] Excellent Moves                       0                  1
 [+] Good Moves                            1                  3
 [B] Book Moves                            3                  3
 [?!] Inaccuracies                         0                  3
 [?] Mistakes                              1                  0
 [??] Blunders                             0                  0
 [X] Missed Wins                           0                  0
 [] Forced Moves                           0                  1
================================================================================
 Key Mistakes & Blunders:
   15. Bxd7+   (Mistake)        Loss: -20 % | Best was: Bxf6
================================================================================
```

---

## 📊 Classification & Accuracy Logic

Zugfish uses a win-probability model modeled after modern chess review engines:

1. **Win Probability Formula**:
   $$\text{WinProb}(cp) = \frac{1}{1 + e^{-0.00368208 \cdot cp}}$$

2. **Accuracy Rating**:
   Computed from the player's win-probability loss over all moves:
   $$\text{Accuracy} = 103.1668 \cdot e^{-0.04354 \cdot \Delta\text{WinPct}} - 3.1669$$
   Bounded in $[0\%, 100\%]$.

3. **Classification Thresholds**:
   - **Brilliant (`!!`)**: The player must sacrifice a piece (material loss), the position must remain equal or winning, and the move must be the top engine choice.
   - **Great (`!`)**: The only move that preserves an advantage or avoids losing in a critical position.
   - **Best (`[*]`)**: The engine's highest-ranked move or within 0.10 centipawns of the top move.
   - **Excellent (`[!]`)**: Minimal loss ($\le 2\%$ win probability loss).
   - **Good (`[+]`)**: Slight loss ($\le 5\%$ win probability loss).
   - **Book (`[B]`)**: Recognized opening theory from ECO classification.
   - **Inaccuracy (`[?!]`)**: $5\% - 10\%$ win probability loss.
   - **Mistake (`[?]`)**: $10\% - 20\%$ win probability loss.
   - **Blunder (`[??]`)**: $> 20\%$ win probability loss.
   - **Miss (`[X]`)**: Overlooking a forced win or tactical opportunity where the opponent had blundered.

---

## 🛠️ Architecture

```
┌────────────────────────────────────────────────────────┐
│                   Qt 6 QML Front End                   │
│   Board (SVG) · Eval Bar · Eval Graph · Review Panel   │
└──────────────────────────┬─────────────────────────────┘
                           │ QObject Signals & Slots
┌──────────────────────────▼─────────────────────────────┐
│                 AppController (zq::App)                │
│    Background Worker Threads · Settings · Navigation   │
└──────────────────────────┬─────────────────────────────┘
                           │ C++ Interfaces
┌──────────────────────────▼─────────────────────────────┐
│                  Zugfish Core Engine                   │
│  ├─ UciEngine (Cross-Platform Win32/POSIX Stockfish)   │
│  ├─ GameReviewer (Chess.com Classification & Stats)    │
│  ├─ PgnParser (Robust PGN & SAN notation parser)       │
│  ├─ OpeningBook (ECO code & opening classification)    │
│  └─ OnlineImporter (Chess.com & Lichess REST APIs)     │
└────────────────────────────────────────────────────────┘
```

---

## ❓ FAQ & Troubleshooting

### Stockfish not found
If Stockfish is not in your system path, Zugfish will notify you. You can:
1. Install it via your package manager:
   - Arch: `yay -S stockfish` (or `paru -S stockfish` / download binary)
   - Ubuntu/Debian: `sudo apt install stockfish`
   - macOS: `brew install stockfish`
   - Windows: `winget install Stockfish.Stockfish`
2. Or set the custom executable path in the GUI settings modal (`Ctrl + ,`).

### QML modules missing on Ubuntu
If you see an error like `module "QtQuick.Controls" is not installed`, install the QML plugins:
```bash
sudo apt install qml6-module-qtquick-controls2 qml6-module-qtquick-layouts qml6-module-qtquick-shapes
```

---

## 📜 License

Zugfish is open-source under the MIT License. See [LICENSE](LICENSE) for details.
Pieces SVG assets and fonts are subject to their respective open-source licenses (see `gui/THIRD_PARTY.md`).
