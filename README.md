# FrogKill 🐸

A lightweight, Windows-like task manager for Linux:
- Process list (PID / Name / CPU% / RSS / User)
- **DEL** to terminate selected process (with confirmation)
- **Shift+DEL** force (SIGKILL)
- **Ctrl+DEL** terminate **process tree** (with confirmation)
- Global shortcut recommended: **Ctrl+Shift+Esc** (configured in your desktop environment)
- Runs in background from login (**systemd --user** unit included)
- Root helper via **pkexec/polkit** for processes requiring admin rights (asks for password)

## Build (Ubuntu)

Dependencies (Ubuntu package names):
```bash
sudo apt update
sudo apt install -y \
  build-essential cmake pkg-config \
  qt6-base-dev qt6-base-dev-tools qt6-tools-dev qt6-tools-dev-tools \
  polkitd pkexec \
  clang lld
```

### Build with clang++ + lld (recommended)

Option A: script
```bash
bash scripts/build_clang_lld.sh
```

Option B: CMake presets
```bash
cmake --preset clang-lld-release
cmake --build --preset clang-lld-release
```

Option C: toolchain file
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=toolchains/clang-lld.cmake
cmake --build build -j"$(nproc)"
```

### Build with your default compiler (gcc)

Option A: script
```bash
bash scripts/build.sh
```

Option B: CMake
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

## Install (system-wide)

This installs:
- `/usr/local/bin/frogkill`
- `/usr/libexec/frogkill-helper`
- `/usr/share/polkit-1/actions/org.frogkill.helper.policy`

Install (pick the build dir you used):
```bash
# if you built with clang
sudo cmake --install build-clang

# or, if you built with gcc/default
sudo cmake --install build
```

> After installing the polkit policy, you may need to log out/in for the policy cache to refresh.

## Start on login (recommended: systemd --user)

Copy the unit:
```bash
mkdir -p ~/.config/systemd/user
cp install/systemd-user/frogkill.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now frogkill.service
```

Check it's running:
```bash
systemctl --user status frogkill.service
```

Logs:
```bash
journalctl --user -u frogkill.service -f
```

## Global shortcut (Ctrl+Shift+Esc)

### GNOME (UI)
Settings → Keyboard → Custom Shortcuts:
- Command: `frogkill --toggle`
- Shortcut: `Ctrl+Shift+Esc`

### GNOME (script)
A helper script is included:
```bash
bash install/gnome_shortcut.sh
```

## Usage

- Start daemon (background, no window): `frogkill --daemon`
- Toggle the window (used by global shortcut): `frogkill --toggle`
- Start GUI immediately: `frogkill`

In the process table:
- Select a row and press **DEL** → confirm → terminate.
- **Shift+DEL** → force kill (SIGKILL) with confirmation.
- **Ctrl+DEL** → terminate the whole tree (children first) with confirmation.
- If permission is denied, FrogKill offers to re-run the action with admin rights (polkit password prompt).

## Security notes

The root helper is deliberately minimal:
- Accepts only `TERM` and `KILL`
- Validates PID and refuses PID <= 1
- For tree operations, builds the tree from `/proc` and kills children first
- Performs `kill(2)` and exits
