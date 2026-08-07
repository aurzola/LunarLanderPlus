# Lunar Lander ESP32 — Hardware / Cableado / Pinado

> Documento de referencia para hardware. **No se carga en el contexto del agente principal**
> durante el desarrollo normal de código (juego, física, render, UI). Lo usa el
> `hardware` agent (ver `.opencode/agent/hardware.md`) cuando se trabaja en electricidad,
> cableado, mediciones o pinout.

## Pinout del ESP32

| Señal | GPIO | Notas |
|-------|------|-------|
| I2C nunchuck SDA | GPIO21 | 50 kHz, `Wire.setTimeOut(100)`, pull-ups internos a mano (`gpio_set_pull_mode`; el core no los activa), dirección `0x52` |
| I2C nunchuck SCL | GPIO22 | |
| Potenciómetro (nivel de potencia / thrust) | GPIO34 | ADC, dead zone 2–98% |
| Gatillo (LEGACY, desconectado) | GPIO35 | ADC; reóstato quedó desconectado (ver "Medición del reóstato") |
| Botón start | GPIO13 | `INPUT_PULLUP`, flanco → `startPressed` |
| Video compuesto | GPIO25 | DAC interno de aquaticus `NTSC_320x240` → RCA (luma alta 255, B/N) |
| Audio | GPIO26 | LEDC PWM + timer ISR → condensador en serie → RCA blanco del TV |

**GPIO34 y 35 son solo-entrada** (sin pull-up/pull-down): ideales para ADC.

## Medición del reóstato (COMPLETADA 2/8/2026)

Contexto: el gatillo de pista de autos es un reóstato de **resistencia baja** (unos pocos Ω,
porque pasaba corriente al motor del auto). **No se puede leer directo con el ADC del ESP32**
(drena demasiada corriente y la lectura sería mala). Por eso se usa divisor de voltaje.

### RESULTADO DE LA MEDICIÓN (2/8/2026)

- **Gatillo suelto (reposo):** circuito abierto (sin lectura) → motor apagado (thrust 0).
- **Primer contacto al apretar:** ~**500 Ω** → ~2.5 V en el ADC.
- **Gatillo apretado al máximo:** **30 Ω** → ~2.9 V (potencia máxima).
- **Barrido 500 → 30 Ω es continuo/suave** (sin escalones discretos). Las lecturas
  "brincan" por **ruido de contacto** del cursor sobre el bobinado: se mitiga en software
  (suavizado) y con un condensador.
- Conclusión: es interruptor + reóstato con rango útil 500–30 Ω. El arranque (abierto→500)
  es un salto de "apagado a encendido" con dead zone natural.
- **La ventana útil es muy angosta (2.5→2.9 V = 0.4 V).** En software se mapea esa ventana
  completa al rango de thrust. Si el gatillo se siente "todo o nada", opciones:
  bajar la resistencia de carga a ~47–56 Ω para estirar la ventana a ~0.6 V, o cambiar de mecanismo.

#### Circuito del divisor (gatillo → ADC)

```
3.3 V ──[reóstato 1.8M→30Ω]──┬──[120 Ω]── GND
                            └──┬──[0.1 µF]── GND
                               └── ADC (GPIO35)
```

- Reposo (abierto) → V ≈ 0.0 V (apagado, thrust 0). Verificado con el multímetro.
- Medición final con 120 Ω (punto medio → GND): primer contacto **2.5 V**, a fondo **2.9 V**.
- El condensador de 0.1 µF forma un paso bajo RC (~10 µs) que filtra el ruido de contacto.
- En circuito el reóstato va de ~38 Ω (primer contacto) a ~17 Ω (a fondo): control tipo
  "on + acelerador" con salto natural de apagado a encendido. Se mapea en software.

#### Conexión del potenciómetro (ángulo → ADC)

```
3.3 V ──┬──[extremo 1]
        │
   [pot 10 kΩ]
        │
      [cursor] ──► ADC (GPIO34)
        │
   [extremo 2]
        │
 GND ───┴───
```

- Pin 1 (extremo) → 3.3 V; pin 2 (extremo) → GND; pin 3 (cursor/medio) → ADC.
- Reversible: si el ángulo sale invertido, se invierte en software o se cambian los extremos.
- Condensador opcional de 0.1 µF del cursor a GND para limpiar ruido.

## Salida de video (compuesta a CRT B/N)

- **Librería: `aquaticus/esp32_composite_video_lib`** (GPL, C), embebida como `src/video.h/c`.
  DAC interno **GPIO25** → RCA del TV. NTSC `NTSC_320x240`, `FB_FORMAT_GREY_8BPP`.
  B/N usa luma alta (255) en el framebuffer.
- `video_graphics(NTSC_320x240, FB_FORMAT_GREY_8BPP)` en `setup()`; el renderer escribe en
  `video_get_frame_buffer_address()` y `video_wait_frame()` sincroniza el draw.
- El framebuffer (320×240 × 1 byte ≈ 76 KB) se aloja en el heap de la librería.
- Alternativa bitluni (documentada antes) NO se usa: se migró a aquaticus porque integra
  `video_wait_frame()` y doble buffer por hardware.

## Sonido — cableado

- **GPIO26 → condensador de acople en serie (1–10 µF) → RCA blanco del TV**
  (quita el DC; lógica de 3.3 V). Verificado con parlante + amplificador.
- Vía: **PWM por LEDC + timer ISR en GPIO26** (NO I2S). Motivo: la librería de video
  (aquaticus) usa I2S0 + DAC1 (GPIO25) y `dac_i2s_enable()` fuerza DAC2 (GPIO26) a modo DMA,
  así que GPIO26 no estaba realmente libre para I2S. Solución: `dac_output_disable(DAC_CHANNEL_2)`
  libera la almohadilla y se usa **LEDC (canal 0, HS mode) como PWM portador a 312.5 kHz
  (resolución 8-bit = máx)**.
- Datos de audio y pipeline de generación: ver `sounds/` (no es hardware).

## Flash / memoria (resumen)

- Placa: ESP32 Dev Module, flash **4 MB** (QIO 80 MHz), core 3.3.10.
- **Esquema de partición `no_ota`** ("No OTA (2MB APP/2MB SPIFFS)", `tools/partitions/no_ota.csv`).
  Layout: `nvs 20K` (`0x9000`), `otadata 8K` (`0xe000`), **`app0 2 MB`** (`0x10000`),
  `spiffs 1.9 MB` (`0x210000`), `coredump 64K` (`0x3f0000`).
- Sketch ≈ **425 KB → 20% del app slot (2 MB)**; RAM: 24.8 KB estáticos (**7%**) + 302.9 KB
  (92.4%) para stack/heap. Sin presión de memoria.
- Para reflashear con `no_ota` (el `arduino-cli upload` simple usa el esquema `default` de
  1.25 MB, también suficiente), forzar particionado en compilación:
  `arduino-cli compile --config-file …/arduino-cli.yaml --fqbn esp32:esp32:esp32 --build-property build.partitions=no_ota --build-property upload.maximum_size=2097152 esp32LanderComposite/esp32LanderComposite.ino`
  y flashear el binario (`…ino.merged.bin`/`…ino.bin`) con `esptool.py`. Un `arduino-cli upload`
  simple revierte al esquema `default` (1.25 MB app), que sigue con margen.
