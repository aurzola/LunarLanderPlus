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
- Minimapa 96×49 en **arriba-centro (112,22)** dibujado cuando `zoomedIn` (terreno completo + marcador de nave).
- **Indicadores de aterrizaje (7/8/2026)** (detectados por `labelX >= 0`, único por zona):
  - **Minimapa**: una **flechita sólida** de 3×2 px (triángulo relleno 1-3) bajo cada zona,
    centrada en su `labelX` y ~5 px bajo la superficie del pad (recortada al borde del minimapa),
    que **parpadea on/off** con `(ship.counter/25)&1`.
  - **Vista principal** (vista normal y zoom): hilera de **cuadritos 2×2 que parpadean alternando**
    (`(k + ship.counter/20)&1`) bajo cada plataforma, centrada en el `labelX`, ~3 unidades de mundo
    bajo la superficie (pitch 5 u, 3–12 luces según el ancho) — efecto de luces de aproximación que
    **no tapa el plano de aterrizaje**.
  - **Etiqueta "Nx"** (`terrain.cpp`): se dibuja centrada en `labelX` a una distancia **fija de
    pantalla de ~7 px** bajo la superficie (`l.y1*viewScale + viewY + 7`), en vez del offset de
    mundo +20 que se alejaba 100 px bajo el plano con el zoom ×5. Así queda pegada al pad en
    ambas vistas.

### Entrada

`struct Input { bool startPressed; float angle; float thrust; float powerLevel; }`.

- **Nunchuck (joystick X) → ángulo**: **calibración adaptativa** (no hardcodeada). `calibrateStick()`
  en `setup()` promedia 40 lecturas → `stickCenterX`. `readStickAngle()` usa dead zone ±10 y
  desviaciones por lado (`stickLeftDev`/`stickRightDev`, mín 40, observadas con EMA; el máximo
  observado en boot se conserva) → rampa lineal a `[-PI/2, PI/2]`. Stick izquierdo = giro a la
  izquierda. I2C: **SDA=GPIO21, SCL=GPIO22**, **50 kHz**, `Wire.setTimeOut(100)`, pull-ups
  internos explícitos (`gpio_set_pull_mode`; el core no los activa), dirección `0x52`.
- **Cifrado nunchuck auto-detectado**: `Nunchuck::begin()` lee 20 muestras y compara la suma cruda
  vs la descifrada con `0x17`; usa la que más se aleja de 128. **Este nunchuck NO cifra**
  (`ENCRYPTED:0`), así que se lee en crudo. Rango medido: `x(35–229)`, `y(36–215)`,
  centro `(134,128)`, `ERR 0`. `read()` re-init + `flush()` (3 lecturas) si falla, y cuenta
  `readErrors()`.
- **Sketch de calibración `esp32NunchuckCal/`**: video CRT + nunchuck; muestra X/Y, botones C/Z,
  ERR, ENCRYPTED, min/max del stick y centro; C+Z resetea min/max. Usado para medir los valores
  reales del nunchuck.
- **Pot (GPIO34) → nivel de potencia (thrust level)**: dead zone 2–98%, lineal a `0.0–1.0`.
  **Solo fija la potencia**; el motor se enciende/apaga con el botón del nunchuck
  (`Z` por defecto, configurable con `NUNCHUCK_TRIGGER_Z`): `thrust = motorOn ? powerLevel : 0`.
- **Botón C del nunchuck → pasos de potencia (5/8/2026)**: cicla `powerLevel` por
  `{0, 25, 50, 75, 100}%` (flanco, `powerStep` 0..4, guarda `potAtCycle`). El **pot sigue
  funcionando**: si se mueve >120 cuentas ADC desde el valor al pulsar C, retoma el control
  (`powerStep=-1` → `powerLevel=potLevel`). "Last-used wins".
- **Gatillo (GPIO35) → LEGACY**: el reóstato quedó **desconectado**; el código del mapeo
  por voltaje se conserva en el `.ino` bajo `#if 0` (decisión: cambiar a pot + botón).
- Botón start (GPIO13, INPUT_PULLUP, flanco) → `startPressed`. **No hay autostart**: la
  partida espera el botón start (antes había autostart a los 4 s; se eliminó por pedido).

### Terreno

- 154 puntos hardcodeados del original moonlander, escalados `x*S`, `y*S+OY`, con wrap-around.
- Zonas de aterrizaje: índices `{34, 63, 106, 133}` con multiplicadores `{4, 5, 5, 2}`, 4 segmentos c/u.
  `checkLanding()` trata cada grupo de segmentos `landable` contiguos como una plataforma entera.
- `labelX` se setea solo en el primer segmento de cada zona → el label "Nx" se dibuja una sola vez.
- **Muros verticales de las plataformas (8/8/2026)**: al aplanar la zona (`init()` solo aplanaba
  los segmentos `idx..idx+3`) el punto `idx+4` conservaba su `y` original → quedaba un salto de
  altura en el mismo `x` que **no se dibujaba** (muro invisible en la vista y en el minimapa).
  Fix: `Terrain::draw()` (y el minimapa en `game.cpp`) dibujan un **conector vertical** cuando dos
  segmentos consecutivos comparten `x2==x1` y difieren en `y`. `generate()` no tenía el bug
  (aplana el punto de frontera `zoneStart..zoneStart+4`).
- **Niveles procedurales (5/8/2026)**: `Terrain::generate(level)` para nivel ≥ 2. Nivel 1 = terreno
  clásico (`init()`). Generación: random walk con deriva acotada (±40) + colinas sinusoidales
  (`freq`/`phase` por nivel) + 2 pasadas de suavizado; 150 puntos, ancho ~900. 4 zonas planas de 4
  segmentos (multiplicadores `{4,5,5,2}`), anchos 19–31 (caja de la nave 6.4). Dificultad: amplitud
  del random walk `4+level` (tope 12). Semilla `srand(esp_random())` en `setup()` del `.ino`.

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
- HUD: `L<n> SCORE`/`FUEL`/`ANG`/`PWR` en `(22,22)`/`(22,32)`/`(22,42)`/`(22,52)`;
  `ALT`/`VX`/`VY` en
  `(250,22)`/`(250,32)`/`(250,42)` (desplazado a la derecha por overscan del CRT).
  `VY` mostrado = `velY*200`; `ANG` = rotación en grados; `PWR` = nivel de potencia actual (%)
  (pot o paso de C). Aviso parpadeante `LOW FUEL` (o `OUT OF FUEL`) en `(250,52)`, debajo de `VY`,
  alineado con los indicadores de la derecha.
  **Aviso `TOO FAST` (7/8/2026)**: parpadeante en `(250,62)` (debajo de `LOW FUEL`) cuando la
  velocidad de descenso `velY > LAND_HARD_VY` o la velocidad horizontal `|velX| > LAND_HARD_VX`
  (no se podría aterrizar con seguridad).
  **No hay etiqueta `LVL`** (el nivel se anuncia con la intro).
- **Intro de nivel (5/8/2026)**: al iniciar partida o nivel (`introTimer = LEVEL_INTRO_TIME=2.4 s`)
  se congela la física y se dibuja `LEVEL N` centrado, **grande y en negrita** (`textScaled`,
  escala 3, 3 pasadas de dibujo para espesar), con **fade de luminancia**: entrada `INTRO_FADE_IN
  =0.35 s`, salida `INTRO_FADE_OUT=0.9 s` (escribe valores < 255 vía `pixelShade`). HUD oculto
  durante la intro. `Renderer` gana `pixelShade(x,y,brightness)` y `textScaled(...)`.
- **Pantalla de título (7/8/2026)** (`STATE_WAITING`): título **`LUNAR LANDER++`** en **negrita**
  (`textScaled` escala 2, 3 pasadas) sin marco (el marco y el disco lunar previos se quitaron por
  parpadeo/molestia). Bajo el título, aviso parpadeante `PRESS BUTTON TO PLAY`. A la izquierda de
  los controles, **dibujo en líneas del módulo lunar Apollo "Eagle"** (base octogonal con escalera
  central, etapa de ascenso con panel central/ventana, cajas laterales, plato de rastreo con cardán,
  antena omnidireccional con esfera, antena helicoidal, propulsores RCS y sombras bajo las patas),
  trazado con lambdas `SX(x)=x*1.2+26`, `SY(y)=y*1.2+56` en el área x26–145, y58–173 (spec de bajo
  nivel, trama cruzada + tramas de sombreado con `pixel`). Controles en letras pequeñas a la derecha
  en x=170
  (`STICK: ROTATION`, `Z: ENGINE ON/OFF`, `C: POWER STEPS`, `POT: POWER LEVEL`). Una sola línea
  de crédito abajo a la derecha: `COPYRIGHT ALEX URZOLA 2026/OPENCODE`. El fondo es el de juego
  (estrellas + nave entrando **por la derecha** con deriva lenta a la izquierda
  (`setupTitleShip()`, `velX=-0.35`, `posX=(SCREEN_W-20)/viewScale`)).
- **Demo / attract mode (8/8/2026)**: tras `DEMO_START_DELAY=13 s` en el título, `Game::startDemo()`
  lanza un nivel (1..3) jugado por un **autopilot** (`Game::runDemoAI()`): control horizontal PD
  hacia una plataforma (`demoTargetX/Y`), fase de crucero con descenso acotado (`maxVY` por
  altitud, guarda de altitud mínima con subida forzada `alt<60`) y fase de aproximación con frenado
  vertical (`vy≤0.075`) y enderezado cerca del suelo (`alt<12`). **A veces gana, a veces pierde**:
  ~50 % de demos son "torpes" (`demoSkill` 0.00–0.35) y apuntan desviado (offset de hasta ±110 u)
  → aterrizan en la ladera y se estrellan; el resto (skill 0.60–1.00) aterriza casi siempre. Ruido
  por-frame `(rand−0.5)·(1−skill)` en ángulo/empuje. La **potencia (PWR) se rampea** a velocidad
  humana (`DEMO_POWER_RATE=0.4/s`, en vez de saltar al valor del autopilot). Win-rate validado en PC
  (~70 % con `./demo_sim`, 200 seeds, sin timeouts, ~80 s/vuelo). Al aterrizar/estrellarse muestra el
  resultado (`CRASH_RESET_DELAY`) y vuelve al título; `DEMO` se muestra en el HUD **debajo de
  `PWR`** en `(22,62)`. Cualquier
  `startPressed` cancela el demo y arranca partida real (`demo=false`). `srand(esp_random())` en
  `setup()`. Validado en PC: `test_pc` (45 checks) + `demo_sim`.
- **Combustible (5/8/2026)**: **no se recarga entre niveles**; lo consumido queda consumido
  (`ship.fuel` se conserva en `nextLevel()`/`restartLevel()`, que antes lo reiniciaban vía
  `Ship::reset()`). El juego **NO termina al quedarse sin combustible en pleno vuelo**: se puede
  acabar el nivel (aterrizar sin motor). Al aterrizar: si `ship.fuel<=0` (tras el bonus de
  aterrizaje perfecto) → `endGame()` (`OUT OF FUEL`/`GAME OVER` y vuelta a la intro); si hay
  combustible → `nextLevel()`. Aterrizaje perfecto sigue dando +50 (con tope `FUEL_MAX`).
- Video lib: `renderer_esp32` escribe en el framebuffer de `video_get_frame_buffer_address()`.
  El render lo hace la librería (DAC → GPIO25 → RCA del TV). B/N usa luma alta (255).
- Compila validado con `arduino-cli compile --fqbn esp32:esp32:esp32`: ~415 KB flash (31%), RAM 7%.
- Loop: `game.update()` cada 10 ms (acumulador sobre `millis()`); `game.draw(renderer)` por iteración.

## Controles físicos decididos

| Control | Mapeo del juego | Notas |
|---------|-----------------|-------|
| Nunchuck (joystick X) | Ángulo de la nave `[-PI/2, PI/2]` → rotación `[-90°, +90°]` | I2C GPIO21/GPIO22; dead zone ±10 |
| Potenciómetro A | Nivel de potencia de motores (thrust level 0.0–1.0) | ADC con suavizado, dead zone 2–98% |
| Nunchuck (botón Z) | Encendido/apagado del motor (thrust = botón ? powerLevel : 0) | `NUNCHUCK_TRIGGER_Z`; alternativo C |
| Nunchuck (botón C) | Cicla `powerLevel` por `{0,25,50,75,100}%` (flanco) | `powerStep` 0..4; pot retoma si se mueve >120 ADC |
| Botón (GPIO13) | Inicio / reinicio de partida | Equivale a tecla "P"; **sin autostart** (espera el botón) |

El motor se enciende/apaga con el botón Z del nunchuck (como el resorte del gatillo: soltado =
motor apagado). La potencia se fija con el pot (continuo) o el botón C (pasos de 25 %); "last-used
wins": al mover el pot >120 cuentas ADC, vuelve a mandar el pot.

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
- Minimapa cuando hay zoom: 96×49 px arriba-centro, escala el terreno completo y marca la nave.

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
  Demo visual: `./main_pc` (PPM en `frames/`). Win-rate del autopilot de demo:
  `./demo_sim <seeds>`; render de un demo: `./demo_render <seed>` (PPM en `frames/`).
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
9. ~~Niveles con terreno procedural~~ **COMPLETADA (5/8/2026)**
   — `Terrain::generate(level)` (random walk + colinas + 4 pads), `Game::level`/`nextLevel()`
   (al aterrizar avanza de nivel, mantiene score), semilla `srand(esp_random())`; validado con
   tests PC (24 checks) + render PPM. **Pendiente de prueba en CRT.**
10. ~~Intro de nivel con fade + combustible persistente~~ **COMPLETADA (5/8/2026)**
    — quita la etiqueta `LVL`; al iniciar partida/nivel se congela la física y se muestra `LEVEL N`
    en negrita (escala 3) con fade de luminancia (`textScaled`+`pixelShade`); el combustible **no
    se recarga** entre niveles y el juego termina al agotarse (`endGame()` → `OUT OF FUEL` →
    intro) permitiendo acabar el nivel en vuelo sin motor. Tests PC 34 checks. **Pendiente de
    prueba en CRT.**
