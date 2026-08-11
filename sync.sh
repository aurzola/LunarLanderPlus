#!/bin/bash
# Sync the shared game sources from the canonical PC port (esp32Lander/) into
# the two sketch src/ trees (composite + VGA). Sketch-only files are NOT
# touched: esp32LanderComposite/src keeps renderer_esp32/video/audio/nunchuck;
# esp32LanderVGA/ keeps renderer_vga + esp32lib.
set -u

CANON=esp32Lander
TARGETS=(esp32LanderComposite/src esp32LanderVGA/src)

SHARED=(config.h
        ship.cpp ship.h
        terrain.cpp terrain.h
        game.cpp game.h
        renderer.h
        renderer_canvas.cpp renderer_canvas.h
        storm.cpp storm.h
        moons.h
        geysers.cpp geysers.h
        volcanoes.cpp volcanoes.h
        atmosphere.cpp atmosphere.h
        rings.cpp rings.h
        twister.cpp twister.h
        tanker.cpp tanker.h
        wormhole.cpp wormhole.h \
        acidrain.cpp acidrain.h
        explosion.h)

err=0
for t in "${TARGETS[@]}"; do
    for f in "${SHARED[@]}"; do
        if ! cp "$CANON/$f" "$t/$f"; then
            echo "ERROR copying $CANON/$f -> $t/$f" >&2
            err=1
        fi
    done
done

if [ $err -ne 0 ]; then
    exit 1
fi

for t in "${TARGETS[@]}"; do
    clean=1
    for f in "${SHARED[@]}"; do
        if ! cmp -s "$CANON/$f" "$t/$f"; then
            echo "DIFF: $t/$f"
            clean=0
        fi
    done
    if [ $clean -eq 1 ]; then
        echo "OK: $t shared files match $CANON"
    fi
done
