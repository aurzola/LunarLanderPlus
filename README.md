# Lunar Lander ESP32

Port del juego **moonlander.seb.ly** (JavaScript) a un **ESP32** con salida de **video
compuesto (AV)** hacia un **CRT blanco y negro** con entrada compuesta (video + sonido).

Controles físicos arcade:

- **Potenciómetro** → ángulo de la nave.
- **Gatillo (reóstato de pista de autos)** → potencia de motores (thrust).
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
| **GPIO34** | ADC pot (ángulo) | Potenciómetro 10 kΩ → ángulo de la nave `[-90°, +90°]`. Solo entrada (ADC1). |
| **GPIO35** | ADC gatillo (thrust) | Divisor de voltaje del reóstato del gatillo → potencia de motores `0.0–1.0`. Solo entrada (ADC1). |
| **GPIO13** | Botón START | Botón a GND con `INPUT_PULLUP` (flanco) → inicia / reinicia la partida. **Sin autostart.** |
| **GPIO25** | Video compuesto | Salida NTSC `320x240` B/N (DAC interno, librería aquaticus) → RCA **amarillo** del TV. |
| **GPIO26** | Audio | PWM LEDC + timer ISR 16 kHz (motor + explosión) → RCA **blanco** del TV. |
| **3.3 V** | Alimentación sensores | Extremo del pot, divisor del gatillo. |
| **GND** | Referencia | Término común de todos los circuitos + RCA del TV. |
| **USB** | Alimentación + flash | Programación y monitoreo serial (115200 baud). |

### Notas de los pines

- **GPIO34 y GPIO35 son solo-entrada** (sin pull-up/pull-down): ideales para ADC. No se
  pueden usar como salida.
- **GPIO25/GPIO26** son los DAC del ESP32. La librería de video usa el DAC1 (GPIO25).
  Para audio se libera GPIO26 con `dac_output_disable(DAC_CHANNEL_2)` + `rtc_gpio_deinit()`
  y se usa como PWM LEDC (ver `AGENTS.md` → Sonido). No usar esos pines para otra cosa.
- Botón: GPIO13 con `INPUT_PULLUP` interno, conectado a GND (el pulso pone la línea a 0).

## Cableado

### Potenciómetro → ángulo (GPIO34)

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
- Si el ángulo sale invertido, invertir en software o cambiar los extremos.

### Gatillo (reóstato) → potencia (GPIO35)

El gatillo de pista de autos es un **reóstato de baja resistencia** (500 → 30 Ω, rango útil
de 2.5 → 2.9 V); no se puede leer directo con el ADC. Se usa divisor de voltaje:

```
3.3 V ──[reóstato 500→30Ω]──┬──[120 Ω]── GND
                            └──┬──[0.1 µF]── GND
                               └── GPIO35
```

- Reposo (suelto) = circuito abierto → 0 V → motor apagado (thrust 0).
- Apretado a fondo ≈ 2.9 V → potencia máxima. Barrido continuo (suavizado en software).

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
