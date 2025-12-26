#!/usr/bin/env bash
set -euo pipefail

KEYPATH="/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/frogkill/"
SCHEMA="org.gnome.settings-daemon.plugins.media-keys.custom-keybinding"

existing="$(gsettings get org.gnome.settings-daemon.plugins.media-keys custom-keybindings)"

# If not present, append
if [[ "$existing" == "@as []" || "$existing" == "[]" ]]; then
  gsettings set org.gnome.settings-daemon.plugins.media-keys custom-keybindings "['$KEYPATH']"
else
  if ! grep -q "$KEYPATH" <<<"$existing"; then
    # append to array string
    updated="${existing%]}"
    if [[ "$updated" == "[" ]]; then
      updated="['$KEYPATH'"
    else
      updated="$updated, '$KEYPATH'"
    fi
    updated="$updated]"
    gsettings set org.gnome.settings-daemon.plugins.media-keys custom-keybindings "$updated"
  fi
fi

gsettings set "$SCHEMA:$KEYPATH" name "FrogKill"
gsettings set "$SCHEMA:$KEYPATH" command "frogkill --toggle"
gsettings set "$SCHEMA:$KEYPATH" binding "<Control><Shift>Escape"

echo "Atalho registrado: Ctrl+Shift+Esc -> frogkill --toggle"
