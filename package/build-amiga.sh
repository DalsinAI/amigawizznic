#!/bin/sh
# Probe: what the stove offers for OpenGPU SDL games.
set -u
P="$STOVE"; [ -d "$STOVE/prefix" ] && P="$STOVE/prefix"
echo "prefix=$P"
ls "$P/bin" | grep -i -E 'sdl|gcc$' || true
ls "$P/m68k-amigaos/lib" | grep -i -E 'sdl|png|libz' || true
ls -d "$P/m68k-amigaos/include/SDL"* 2>/dev/null || true
cat "$P/../STOVE.json" 2>/dev/null | head -50 || true
ls "$P/../kits" 2>/dev/null || true
for t in python3 gst-launch-1.0 lha; do command -v $t || echo "no $t"; done
python3 -c "import gi; print('gi ok')" 2>&1 | tail -1
mkdir -p "$OUT"; echo probe > "$OUT/probe.txt"
