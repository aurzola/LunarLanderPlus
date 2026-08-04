# AGENTS.md — Lunar Lander ESP32

## Objetivo

Portar el juego **moonlander.seb.ly** (JavaScript) a un **ESP32** con salida **video compuesto (AV)**
hacia un **CRT blanco y negro con entrada compuesta (video + sonido)**. Controles físicos:
**potenciómetro (ángulo) + gatillo de reóstato de pista de autos (potencia de motores)**.

Estética objetivo: arcade / retro auténtico.

## Estado actual del código

- `pyLander/` — original en Python/Pygame Zero. **NO se usa para el ESP32.**
- `cppLander/` — versión C++ previa del juego (base antigua). Reemplazada por el port de moonlander.
- `esp32Lander/` — port C++ std del juego moonlander, **validado en PC** (ver "Port a ESP32").
- `esp32LanderComposite/` — sketch Arduino del ESP32 (ver "Sketch ESP32").

## Port a ESP32 (ESTADO 4/8/2026)

El juego portado es **moonlander.seb.ly** (JS): física, terreno fijo y nave hexagonal.
Estructura en `esp32Lander/` (C++ std, sin dependencias de hardware):

| Archivo | Contenido |
|---------|-----------|
| `ship.h/cpp` | Nave hexagonal (6 shapes: cuerpo, cabina, patas, toberas). Física, rotación suave, `draw(Renderer&, viewX, viewY, viewScale)`. Explosión al chocar |
| `terrain.h/cpp` | Terreno fijo (154 puntos, S=1.35, OY=130), zonas de aterrizaje con multiplicadores y `labelX` (label único por zona), estrellas, colisión línea-segmento |
| `game.h/cpp` | Estados, zoom + minimapa, scoring, `update()` + `draw(Renderer&)` |
| `renderer.h` | Interfaz abstracta (pixel/line/rect/circle/text/flush) |
| `renderer_canvas.h/cpp` | Primitivas compartidas (Bresenham con caso explícito dx=0/dy=0, círculo, rect, fuente 5x7) vía `pixel()` |
| `renderer_pc.h/cpp` | Renderer de validación en PC: framebuffer + PPM (extiende `RendererCanvas`) |
| `main_pc.cpp` | Demo en PC (genera snapshots PPM en `frames/`) |
| `test_pc.cpp` | Tests de validación (asserts) |

### Mundo y pantalla

- Mundo 800×600, pantalla 320×240. Física y colisiones en coordenadas de mundo; el dibujado escala.
- Vista normal `viewScale = SCREEN_H/700`. Nave `scale = 1.0` (vista normal) / `0.32` (zoom).

### Constantes del port (config.h)

- `GRAVITY=0.0005`, `THRUST_ACCEL=0.0018`, `DRAG=0.9997`, `TOP_SPEED=0.35`.
- `FUEL_MAX=1000`, `FUEL_PER_THRUST=0.2`, `GAME_DT=0.01`.
- Rotación `[-90°, +90°]`, lerp `ROTATION_LERP=0.3`, la nave **arranca en 0°** (boquilla abajo).
- Empuje: `velX += THRUST_ACCEL*thrustBuild*sin(rad)`, `velY -= THRUST_ACCEL*thrustBuild*cos(rad)`.
- `setThrust()`: lerp `thrustBuild += (power - thrustBuild) * 0.4`.
- Aterrizaje (config.h): perfecto `vy<0.075`, hard `<0.15`, tolerancia de rotación
  `LAND_MAX_ROTATION=5.0` (antes exacta `rotation==0`). `VX` no se valida.
- `checkLanding()` usa la **zona completa** (segmentos `landable` contiguos) en vez de un solo
  segmento: las plataformas quedan ~19–31 de ancho vs caja de la nave 6.4 (antes el segmento
  plano único medía 6.8 → crash por desbordar el borde con rot/vy válidos).
- Zoom: entra `alt<200`, sale `alt>350`; `viewScale` con zoom = `SCREEN_H/700*5`.
- Minimapa 96×54 en **arriba-centro (112,22)** dibujado cuando `zoomedIn` (terreno completo + marcador de nave).

### Entrada

`struct Input { bool startPressed; float angle; float thrust; }`.

- **Nunchuck (joystick X) → ángulo**: centro `128`, dead zone ±10, rampa lineal a
  `[-PI/2, PI/2]`. Stick izquierdo = giro a la izquierda. I2C: **SDA=GPIO21, SCL=GPIO22**,
  100 kHz, pull-ups internos explícitos (el core no los activa), dirección `0x52`, dato
  cifrado con clave `0x17` (`(b^0x17)+0x17`).
- **Pot (GPIO34) → nivel de potencia (thrust level)**: dead zone 2–98%, lineal a `0.0–1.0`.
  **Solo fija la potencia**; el motor se enciende/apaga con el botón del nunchuck
  (`Z` por defecto, configurable con `NUNCHUCK_TRIGGER_Z`): `thrust = motorOn ? potLevel : 0`.
- **Gatillo (GPIO35) → LEGACY**: el reóstato quedó **desconectado**; el código del mapeo
  por voltaje se conserva en el `.ino` bajo `#if 0` (decisión: cambiar a pot + botón).
- Botón start (GPIO13, INPUT_PULLUP, flanco) → `startPressed`. **No hay autostart**: la
  partida espera el botón start (antes había autostart a los 4 s; se eliminó por pedido).

### Terreno

- 154 puntos hardcodeados del original moonlander, escalados `x*S`, `y*S+OY`, con wrap-around.
- Zonas de aterrizaje: índices `{34, 63, 106, 133}` con multiplicadores `{4, 5, 5, 2}`, 4 segmentos c/u.
  `checkLanding()` trata cada grupo de segmentos `landable` contiguos como una plataforma entera.
- `labelX` se setea solo en el primer segmento de cada zona → el label "Nx" se dibuja una sola vez.

## Sketch ESP32 (`esp32LanderComposite/`)

Sketch Arduino autónomo (Arduino IDE o `arduino-cli`). Placa "ESP32 Dev Module" (core esp32 ≥ 3.x).

| Archivo | Contenido |
|---------|-----------|
| `esp32LanderComposite.ino` | `setup()`/`loop()`: video (aquaticus), `esp_pm_lock` CPU máx, ADC+nunchuck+botón, audio, loop fijo con `millis()` y `GAME_DT=0.01` |
| `src/video.h/c` | **Librería aquaticus `esp32_composite_video_lib`** (GPL): `video_graphics(NTSC_320x240, FB_FORMAT_GREY_8BPP)`, DAC en **GPIO25**, `video_wait_frame()` |
| `src/renderer_esp32.h/cpp` | `RendererESP32 : RendererCanvas`: `pixel→fb[py*w+px]=255`, `clear→memset`, `flush` no-op |
| `src/ship/terrain/game/renderer_canvas/renderer/config` | Mismas fuentes que `esp32Lander/` (copias; mantener en sync con `diff`) |
| `src/audio.h/cpp` + `src/audio_data.h` | Sonido por LEDC + timer ISR (ver sección Sonido); solo en el sketch ESP32 |
| `src/nunchuck.h/cpp` | Lectura Wii Nunchuck por I2C (init `0xF0:0x55`/`0xFB:0x00`, descifrado `0x17`); solo en el sketch ESP32 |

- **Pines:** GPIO21/22 = I2C nunchuck (SDA/SCL), GPIO34 = pot (nivel de thrust),
  GPIO35 = gatillo (legacy, desconectado), GPIO13 = botón start.
  Audio: GPIO26 (LEDC PWM; ver sección Sonido).
- HUD: `SCORE`/`FUEL`/`ANG` en `(22,22)`/`(22,32)`/`(22,42)`; `ALT`/`VX`/`VY` en
  `(250,22)`/`(250,32)`/`(250,42)` (desplazado a la derecha por overscan del CRT).
  `VY` mostrado = `velY*200`; `ANG` = rotación en grados.
- Video lib: `renderer_esp32` escribe en el framebuffer de `video_get_frame_buffer_address()`.
  El render lo hace la librería (DAC → GPIO25 → RCA del TV). B/N usa luma alta (255).
- Compila validado con `arduino-cli compile --fqbn esp32:esp32:esp32`: ~415 KB flash (31%), RAM 7%.
- Loop: `game.update()` cada 10 ms (acumulador sobre `millis()`); `game.draw(renderer)` por iteración.

## Controles físicos decididos

| Control | Mapeo del juego | Notas |
|---------|-----------------|-------|
| Nunchuck (joystick X) | Ángulo de la nave `[-PI/2, PI/2]` → rotación `[-90°, +90°]` | I2C GPIO21/GPIO22; dead zone ±10 |
| Potenciómetro A | Nivel de potencia de motores (thrust level 0.0–1.0) | ADC con suavizado, dead zone 2–98% |
| Nunchuck (botón Z) | Encendido/apagado del motor (thrust = botón ? potLevel : 0) | `NUNCHUCK_TRIGGER_Z`; alternativo C |
| Botón (GPIO13) | Inicio / reinicio de partida | Equivale a tecla "P"; **sin autostart** (espera el botón) |

El motor se enciende/apaga con el botón del nunchuck (como el resorte del gatillo: soltado =
motor apagado). El pot solo fija cuánta potencia se aplica al mantener el botón.

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
  completa al rango de thrust (ver "Entrada"). Si el gatillo se siente "todo o nada", opciones:
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
- **GPIO34 y 35 son solo-entrada** (sin pull-up/pull-down): ideales para ADC.
  Asignación: GPIO34 = ángulo (pot), GPIO35 = potencia (gatillo).
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

## Sonido (COMPLETADA 4/8/2026)

- **Vía: PWM por LEDC + timer ISR en GPIO26** (NO I2S). Motivo: la librería de video
  (aquaticus) usa I2S0 + DAC1 (GPIO25) y `dac_i2s_enable()` fuerza DAC2 (GPIO26) a modo DMA,
  así que GPIO26 no estaba realmente libre para I2S. Solución: `dac_output_disable(DAC_CHANNEL_2)`
  libera la almohadilla y se usa **LEDC (canal 0, HS mode) como PWM portador a 312.5 kHz
  (resolución 8-bit = máx)**.
- `src/audio.h/cpp`: timer gptimer a **16 kHz** con ISR (`IRAM_ATTR`) que mezcla
  `THRUST_SOUND` (loop) + `EXPLOSION_SOUND` (one-shot) en RAM (copiados desde PROGMEM al
  arrancar) y escribe el duty directo al registro `LEDC.channel_group[0].channel[0].duty.duty`
  (`val<<4`) + handshake `duty_start`. API: `Audio::begin()`, `Audio::setThrust(0..1)`,
  `Audio::playExplosion()`.
- Datos: `src/audio_data.h` generado (PROGMEM) desde `sounds/rocket_thrust.wav` (32 k muestras,
  2 s, loopable) y `sounds/explosion.wav` (27.4 k muestras, 1.71 s) — mono 8-bit / 16 kHz.
- Origen de los sonidos: **reales**, extraídos de `tblazevic/moonlander` (clon arcade JS)
  `audio/rocket.mp3` (loop de motor) + `audio/crash.mp3`. Pipeline en `sounds/real_sounds.py`
  (extrae el segmento 2 s más estable del mp3, hace **loop sin clic** cruzando la continuación
  natural hacia la cabeza, sube ganancia con `tanh`, convierte a 8-bit). Los mp3 se convierten
  primero a PCM16 16 kHz con ffmpeg (`/tmp/opencode/rocket16.wav`).
- RAM: los dos sonidos se copian a RAM al arrancar (~59 KB) para lectura segura desde el ISR.
- Disparo en el `.ino`: transición a `STATE_CRASHED` → explosión; `thrustBuild` durante
  `STATE_PLAYING` → motor.
- Debug (serial): `debugBeep()` emite un pitido 440 Hz (0.5 s) al arrancar para confirmar el
  audio; `debugIsrCount()` imprime `[audio] isr=%u` 1×/s (~16156 ISR/s → 16 kHz reales).
- Cableado: **GPIO26 → condensador de acople en serie (1–10 µF) → RCA blanco del TV**
  (quita el DC; lógica de 3.3 V). Verificado con parlante + amplificador.

## Escalado de pantalla

- Mundo 800×600 → pantalla 320×240. La física NO cambia, solo el dibujado.
- Zoom: multiplica `viewScale` ×5 y la nave pasa a `scale=0.32` (se ve más grande).
- Minimapa cuando hay zoom: 96×54 px arriba-centro, escala el terreno completo y marca la nave.

## Decisiones de arquitectura / convenciones

- Framework: **Arduino (arduino-esp32)** salvo que el usuario decida ESP-IDF.
- Port de **moonlander.seb.ly**: física y terreno del original JS, con nave hexagonal.
- Loop fijo con `millis()`, `GAME_DT=0.01`; el ritmo de video lo maneja la librería
  (`video_wait_frame()`), sin VSYNC explícito en el juego.
- Dibujado: interfaz `Renderer` (pixel/line/rect/circle/text/flush). `RendererCanvas` comparte
  las primitivas; PC y ESP32 implementan `pixel()`.
- `esp32LanderComposite/src/` es **copia** de `esp32Lander/` (mismas fuentes); mantener en sync
  con `diff` al cambiar física/dibujado.
- Idioma del código: inglés (coherente con el port). Respuestas al usuario: español.
- No usar librerías no verificadas antes de consultar. No añadir comentarios al código salvo que se pidan.

## Comandos útiles

- Validar port en PC: `make && ./test_pc` en `esp32Lander/` (todos los checks pasan).
  Demo visual: `./main_pc` (PPM en `frames/`).
- Compilar sketch: `arduino-cli compile --fqbn esp32:esp32:esp32 esp32LanderComposite/esp32LanderComposite.ino`.
- Subir: `arduino-cli upload --fqbn esp32:esp32:esp32 --port /dev/ttyUSB0 ...` (o Arduino IDE).
- Sync PC↔ESP32: `diff esp32Lander/<f> esp32LanderComposite/src/<f>`.

## Proceso de trabajo

1. ~~Medir el reóstato y documentar resultado~~ **COMPLETADA (2/8/2026)** — ver sección de medición.
2. ~~Portar moonlander.seb.ly y validar en PC~~ **COMPLETADA (4/8/2026)**
   — `esp32Lander/`, `make && ./test_pc` OK (21 checks).
3. ~~Integrar video compuesto (aquaticus) → CRT~~ **COMPLETADA (4/8/2026)** — imagen verificada en CRT.
4. ~~Integrar controles: pot (ángulo), gatillo (potencia), botón start~~ **COMPLETADA (4/8/2026)**
   — mapeo por ventana de voltaje del gatillo; **autostart eliminado** (espera el botón start).
5. ~~Integrar sonido por GPIO26~~ **COMPLETADA (4/8/2026)**
   — LEDC PWM + timer ISR 16 kHz (explosión + motor), ver sección de sonido.
6. ~~Pulir jugabilidad: tolerancia de aterrizaje + plataformas por zona completa~~ **COMPLETADA (4/8/2026)**
   — `LAND_MAX_ROTATION=5.0` y `checkLanding()` sobre la zona landable completa (plataformas
   ~19–31 de ancho); HUD con `ANG`. Verificado el fix del crash con parámetros válidos.
7. ~~Cambiar el hardware del thrust~~ **COMPLETADA (4/8/2026)**
   — el gatillo (reóstato 500→30 Ω) se sentía "todo o nada"; reemplazado por **nunchuck**
   (dirección + botón disparador) manteniendo el pot como nivel de potencia. Gatillo GPIO35
   desconectado (código legacy en el `.ino`). **Pendiente de prueba en CRT**.
8. **Probar el nunchuck en CRT (EN CURSO)** — validar mapeo de dirección y botón Z; ajustar
   dead zone del stick y curva de potencia del pot si hace falta.
