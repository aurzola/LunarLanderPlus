# PLAN — Puerto VGA del Lunar Lander ESP32 (proyecto paralelo)

> Estado: **implementado (rama `vga-out`, 23/8/2026)**. `esp32LanderVGA/` existe y compila
> (552 KB flash / RAM 112 KB estáticos + fbFront en heap, `no_ota`). Falta: cablear el segundo
> ESP32, probar el patrón de prueba en monitor (`VGA_TEST_PATTERN=1`) y pasar a `0` (juego).
> Plan original (factibilidad, 15/8/2026) abajo.

## Objetivo

Portar la salida de video del juego (hoy **compuesto NTSC 320x240** vía aquaticus en
`esp32LanderComposite/`) a **VGA**, como proyecto **paralelo** que NO toca
`esp32LanderComposite/` ni la lógica de juego. Nuevo sketch `esp32LanderVGA/`.

## Decisiones tomadas

| Tema | Decisión |
|------|----------|
| Librería | **bitluni/ESP32Lib** (`VGA8BitDACI`), CC BY-SA 4.0, embebida como fuente (igual que aquaticus GPL) |
| Fuente de señal | **DAC interno del ESP32** (GPIO25), salida analógica mono → R/G/B en paralelo = **255 niveles de gris**, mapeo 1:1 con `RendererCanvas` (el juego es B/N) |
| Resolución | `MODE640x480@60` (25.175 MHz) por defecto, con **escalado 2x del canvas 320x240 en el ISR de línea** (sin FB de 300 KB, no cabe en WROOM-32 sin PSRAM). `#define` alternativo `MODE320x240` (estira el monitor) |
| Placa | WROOM-32 DevKit, **sin PSRAM** (320 KB SRAM) |
| Cableado | DAC mono + HS/VS en 2 GPIOs (mínimo) |
| Audio | GPIO26 **intacto** para LEDC (el driver VGA solo usa DAC1/I2S0 en GPIO25) |
| Repos | `esp32LanderComposite/` y `esp32Lander/` sin cambios; trabajo en rama `vga-out` |

Descartadas: `VGA6Bit` (8 hilos, 2-2-2, solo 4 grises, doble buffer), `VGA14Bit`
(16 hilos, 32 grises, bufer único), **FabGL** (framework pesado, salida RGB por escalera
R-2R de 12-16 hilos, envuelve el juego con su propio Display/Graphics, fricción con cores
Arduino nuevos).

## Verificación técnica hecha (15/8/2026)

- `VGA8BitDACI` usa `GraphicsW8` = **FB de 1 byte/píxel en gris (0-255)** = 76.8 KB a 320x240.
- ISR por línea (`interruptPixelLine`) rellena line buffers → sin tearing, sin FB VGA grande.
- `outputPin=25` → solo DAC1 → **GPIO26 libre** para audio LEDC.
- `MODE320x240(8,48,24,320, 11,2,31,480, vDiv=2, 12587500, 1, 1)`: 31.47 kHz / 60.05 Hz
  (mismos sync que 640x480) → cualquier monitor VGA hace lock; el monitor estira a pantalla completa.
- Clon local de ESP32Lib en `/tmp/opencode/ESP32Lib` (referencia; a embeber subárbol en el sketch).
- Ejemplo `examples/8BitDACMode/8BitDACMode.ino`: `init(VGAMode::MODE320x240, hsyncPin, vsyncPin, outputPin, voltageDivider)`.
- RAM: shadow 76.8K + FB VGA 76.8K ≈ perfil del compuesto (~100-110 KB total). Todo el juego
  /audio/nunchuck/demo queda idéntico; solo cambia el renderer y se elimina aquaticus.

## Circuito de conexión VGA (según ejemplo de bitluni)

El conector VGA (15 pines, DE-15) — solo nos importan 7 pines:

| Pin VGA | Función | Qué le mandamos |
|---|---|---|
| 1 | RED (analógico) | el video (brillo) |
| 2 | GREEN (analógico) | el video (brillo) |
| 3 | BLUE (analógico) | el video (brillo) |
| 13 | HSYNC | señal digital (GPIO32) |
| 14 | VSYNC | señal digital (GPIO33) |
| 5, 6, 7, 8, 10 | GND | masa común |

Concepto B/N: el monitor mezcla R/G/B por voltaje; mandando **el mismo voltaje a los tres**
se obtiene gris (blanco a máximo, negro a cero). El DAC de GPIO25 saca 0-3.3 V = luminosidad
del píxel.

Variante elegida (**con divisor, 255 tonos**, `voltageDivider=true` en `init()`):

```
GPIO25 ──[270Ω]──┬────────► VGA pin 1 (R)
GPIO25 ──[270Ω]─┤────────► VGA pin 2 (G)
GPIO25 ──[270Ω]─┴────────► VGA pin 3 (B)
GPIO32 ────────────────────► VGA pin 13 (HSYNC)
GPIO33 ────────────────────► VGA pin 14 (VSYNC)
GND ───────────────────────► VGA pines 5/6/7/8/10
```

> **Corrección 23/8/2026 (agente hardware)**: el `100/100/220` original de este plan es el
> circuito de bitluni para **dos DACs** (GPIO25→R/G, GPIO26→B). Con un solo GPIO25 en mono da
> `3.3·75/(100+75)=1.414 V` en R/G (el spec VGA es 0.7 V → recorte de la rampa de grises) y
> tinte por desbalance. **270 Ω en las tres ramas** = `0.717 V` en blanco, 0 V en negro,
> gris neutro, ~29 mA totales. Alternativas: 280 Ω (0.697 V) o 330 Ω (0.611 V).

Nota: GPIO25 + GPIO26 son los DAC del ESP32. Usamos solo GPIO25 (GPIO26 = audio LEDC).
Las resistencias + los 75 Ω de terminación interna del monitor forman el divisor de voltaje.
HSYNC/VSYNC van directos (TTL, sin terminación). No conectar el pin 9 (+5 V) ni los DDC
(4/11/12/15). Verificar que el DevKit no use GPIO32/33 como XTAL_32K (algunos con RTC).

## Cambios de código (mínimos)

Juego, física, audio, nunchuck, calibración, demo = **idénticos**. Cambia únicamente:

- **`.ino`**: quitar aquaticus (`src/video.h/c`) y `RendererESP32`; init `RendererVGA` + driver VGA.
- **`src/renderer_vga.{h,cpp}`**: `RendererVGA : RendererCanvas`, escribe gris 0-255 en el shadow;
  `flush()` = esperar vSync + `memcpy` shadow→FB visible (mismo patrón de doble buffer del compuesto).
- **`src/esp32lib/`**: subárboles `VGA/ Graphics/ I2S/ Tools/` de bitluni/ESP32Lib embebidos
  + driver adaptado (con atribución de licencia CC BY-SA 4.0 en cabecera).
- Se **borra** `src/video.h/c` (aquaticus) del sketch VGA.

### Driver adaptado (fork mínimo dentro del sketch)

- `allocateFrameBuffer()` apunta `frontBuffer`/`backBuffer` a nuestro `fbShadow` (76.8 KB)
  → sin malloc de 300 KB (no cabe sin PSRAM).
- `interruptPixelLine()`: en `MODE640x480` hace el escalado 2x horizontal
  (`shadow[y>>1][x>>1]`) y 2x vertical; en `MODE320x240` pasa directo. Bits 6/7 del sample = HSYNC/VSYNC.
- Doble buffer como el compuesto: dibujar en `fbBack`, `memcpy` → `fbFront` (lee el ISR) en vblank
  → sin tearing. RAM total ≈ 175 KB (2×76.8 + line buffers + juego), similar al compuesto.

## Estructura de archivos (por crear)

```
esp32LanderVGA/
  esp32LanderVGA.ino          # setup()/loop(): video VGA, audio, nunchuck, botón, loop fijo GAME_DT
  src/
    renderer_vga.{h,cpp}      # RendererVGA : RendererCanvas
    esp32lib/                 # bitluni/ESP32Lib embebido (subárboles + driver adaptado)
    ...                       # copia del resto de src/ de esp32LanderComposite (game, terrain, ship, audio, nunchuck, renderer_canvas, config)
    (video.h/c eliminados)
```

## Siguientes pasos

1. Crear rama `vga-out`.
2. Copiar `esp32LanderComposite/` → `esp32LanderVGA/`; borrar `video.h/c` y `renderer_esp32.*`.
3. Embeber subárboles de `/tmp/opencode/ESP32Lib` en `src/esp32lib/`.
4. Escribir `RendererVGA` + driver adaptado (`LanderVGA8BitDACI`).
5. Compilar: `arduino-cli compile --fqbn esp32:esp32:esp32 --build-property build.partitions=no_ota esp32LanderVGA/esp32LanderVGA.ino`.
   Verificar RAM/Flash (≈ compuesto, ~525 KB flash / ~110 KB RAM).
6. En hardware: primero un **patrón de prueba** (rampa de grises + rejilla) antes del juego,
   para verificar niveles/255 grises/sync.
7. Docs: `docs/hardware.md` (sección "Salida de video VGA"), `AGENTS.md` (estructura `esp32LanderVGA/`
   + comandos), `docs/WORKLOG.md` (entrada del ítem).

## Pendientes de confirmar al retomar

- Aprobación del usuario para arrancar (implementación).
- Default de resolución: `MODE640x480` (recomendado) vs `MODE320x240`.
- Cableado físico real (revisión del agente `hardware`) y monitor/cable VGA disponible.

## Riesgos aceptados

- Suavizado de bordes por ancho de banda del DAC (look retro; idéntico en 320 estirado o 640 software-2x).
- Aceptación del monitor en 320x240 (si se elige) vs 640x480 (a prueba de balas).
- Un ISR VGA por línea adicional (IRAM, CPU despreciable, coexiste con el ISR de audio a 16 kHz).
- Licencia CC BY-SA 4.0 de bitluni/ESP32Lib → atribución en cabecera + docs.
