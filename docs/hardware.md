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

## Salida de video VGA (segundo ESP32, rama `vga-out`)

Board aparte (WROOM-32, sin PSRAM) que saca el juego por **SVGA 640x480@60** monocromo.
El CRT compuesto queda intacto en su placa. Detalle de la arquitectura: `docs/PLAN_VGA.md`.

- **Driver**: `bitluni/ESP32Lib` (CC BY-SA 4.0) embebido en `src/esp32lib/`; fork de
  `VGA8BitDACI` con frame store externo (320x240, escalado 2x en el ISR de línea).
- **Pines del board VGA**:

| Señal | GPIO | Notas |
|-------|------|-------|
| Video (DAC) | GPIO25 | DAC1 → divisor 270Ω×3 en paralelo a R/G/B |
| HSYNC | GPIO32 | digital |
| VSYNC | GPIO33 | digital |
| I2C nunchuck SDA/SCL | GPIO21/GPIO22 | igual que el composite |
| Pot / Botón start / Audio | GPIO34 / GPIO13 / GPIO26 | igual que el composite |

- **Circuito del conector VGA (DE-15)** — solo 7 pines:

```
GPIO25 ──[270Ω]──┬────────► VGA pin 1 (R)
GPIO25 ──[270Ω]─┤────────► VGA pin 2 (G)
GPIO25 ──[270Ω]─┴────────► VGA pin 3 (B)
GPIO32 ────────────────────► VGA pin 13 (HSYNC)
GPIO33 ────────────────────► VGA pin 14 (VSYNC)
GND ───────────────────────► VGA pines 5/6/7/8/10
```

Las resistencias + los 75 Ω de terminación interna del monitor forman el divisor de voltaje
(mismo voltaje a los tres colores → gris, `voltageDivider=true` en `init()`). El **270 Ω/rama**
da `V_blanco = 3.3·75/(270+75) = 0.717 V` (spec VGA 0.7 V) y `V_negro = 0 V`; ~9.6 mA por rama
(~29 mA total). **Importante**: usar las MISMAS resistencias en las tres ramas — el circuito
`100/100/220` del ejemplo de bitluni es para **dos DACs** (GPIO25→R/G, GPIO26→B); con un solo
DAC da 1.414 V en R/G (recorte, "blanco" saturado) y tinte por desbalance. Alternativas:
280 Ω (0.697 V, exacto) o 330 Ω (0.611 V, con margen de corriente). GPIO32/33 son XTAL_32K_P/N
en algunos DevKit — verificar que la placa no tenga el cristal de 32 kHz montado. No conectar
el pin 9 (+5 V) ni los DDC (4/11/12/15); sync directo a 13/14 (TTL, sin terminación).
- **Memoria**: no caben dos buffers de 76.8 KB en DRAM estática → `fbBack` es estático y
  `fbFront` (el que lee el ISR) se `malloc`ea al inicio de `setup()` (heap ~216 KB libres).
- **Test antes del juego**: `#define VGA_TEST_PATTERN 1` dibuja rampa de grises + rejilla +
  marco; verificado, pasar a `0`.

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
