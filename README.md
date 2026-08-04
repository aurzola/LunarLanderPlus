# Lunar Lander ESP32

Port del juego **moonlander.seb.ly** (JavaScript) a un **ESP32** con salida de **video
compuesto (AV)** hacia un **CRT blanco y negro** con entrada compuesta (video + sonido).

Controles físicos arcade:

- **Nunchuck (Wii)** → dirección (joystick X) y botón disparador del motor.
- **Potenciómetro** → nivel de potencia de motores (thrust).
- **Botón** → inicio / reinicio de partida.

## Código

| Ruta | Contenido |
|------|-----------|
| `esp32Lander/` | Port C++ std del juego (sin hardware): física, terreno, nave. Validado en PC (`make && ./test_pc`). |
| `esp32LanderComposite/` | Sketch Arduino del ESP32 (video compuesto, audio, controles). Compilar con `arduino-cli` (core esp32 ≥ 3.x). |
| `sounds/` | Pipeline de generación de los sonidos (real_sounds.py) + WAV fuente. |

Ver `AGENTS.md` para la documentación técnica completa (física, terreno, audio, decisiones).

## Pinout completo

| Pin ESP32 | Señal | Uso |
|-----------|-------|-----|
| **GPIO21** | I2C SDA | Datos del nunchuck (Wii), 50 kHz, con pull-up interno explícito. |
| **GPIO22** | I2C SCL | Clock del nunchuck (Wii), 50 kHz, con pull-up interno explícito. |
| **GPIO34** | ADC pot (thrust level) | Potenciómetro 10 kΩ → **nivel de potencia** de motores `0.0–1.0`. Solo entrada (ADC1). |
| **GPIO13** | Botón START | Botón a GND con `INPUT_PULLUP` (flanco) → inicia / reinicia la partida. **Sin autostart.** |
| **GPIO25** | Video compuesto | Salida NTSC `320x240` B/N (DAC interno, librería aquaticus) → RCA **amarillo** del TV. |
| **GPIO26** | Audio | PWM LEDC + timer ISR 16 kHz (motor + explosión) → RCA **blanco** del TV. |
| **3.3 V** | Alimentación sensores | Nunchuck + extremo del pot. |
| **GND** | Referencia | Término común de todos los circuitos + RCA del TV. |
| **USB** | Alimentación + flash | Programación y monitoreo serial (115200 baud). |

> **GPIO35 (gatillo reóstato)**: quedó **desconectado** — sustituido por nunchuck + pot.
> El código del mapeo por voltaje se conserva como legacy en el `.ino` bajo `#if 0`.

### Notas de los pines

- **GPIO34 y GPIO35 son solo-entrada** (sin pull-up/pull-down): ideales para ADC. No se
  pueden usar como salida.
- **GPIO25/GPIO26** son los DAC del ESP32. La librería de video usa el DAC1 (GPIO25).
  Para audio se libera GPIO26 con `dac_output_disable(DAC_CHANNEL_2)` + `rtc_gpio_deinit()`
  y se usa como PWM LEDC (ver `AGENTS.md` → Sonido). No usar esos pines para otra cosa.
- Botón: GPIO13 con `INPUT_PULLUP` interno, conectado a GND (el pulso pone la línea a 0).
- Nunchuck: el cable del nunchuck tiene 6 almohadillas: 3.3 V, GND, SDA, SCL (las otras dos
  son del acelerómetro y no se usan). Se conecta directo (lógica 3.3 V).

## Cableado

### Nunchuck → dirección + disparador (GPIO21/GPIO22)

```
3.3 V ──► nunchuck VCC
GND  ──► nunchuck GND
GPIO21 ──► nunchuck SDA (pull-up interno)
GPIO22 ──► nunchuck SCL (pull-up interno)
```

- Joystick X → ángulo de la nave; botón **Z** → enciende/apaga el motor.
- I2C 100 kHz, dirección `0x52`, datos cifrados con clave `0x17`.

### Potenciómetro → nivel de potencia (GPIO34)

```
3.3 V ──┬──[extremo 1]
        │
   [pot 10 kΩ]
        │
      [cursor] ──► GPIO34
        │
   [extremo 2]
        │
 GND ───┴───
```

- Opcional: condensador 0.1 µF del cursor a GND para limpiar ruido.
- El pot **solo fija el nivel** de thrust; el motor se enciende/apaga con el botón Z del
  nunchuck (soltado = motor apagado).

### Video y audio → TV

```
GPIO25 ─────────────► RCA amarillo (video compuesto NTSC)
GPIO26 ──[1–10 µF]──► RCA blanco (audio, acople en serie)
GND    ─────────────► GND / masa del TV
```

- El condensador de acople en serie en el audio quita la componente DC (lógica 3.3 V).
- Video: B/N con luma alta (255); se usa la entrada de video compuesto del TV.

## Compilar y subir

```sh
# Validar el port en PC (sin hardware)
cd esp32Lander && make && ./test_pc

# Compilar el sketch
arduino-cli compile --fqbn esp32:esp32:esp32 esp32LanderComposite/esp32LanderComposite.ino

# Subir
arduino-cli upload --fqbn esp32:esp32:esp32 --port /dev/ttyUSB0 esp32LanderComposite/esp32LanderComposite.ino
```
