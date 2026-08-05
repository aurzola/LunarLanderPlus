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
| `storm.h/cpp` | **Tormenta eléctrica (solo visual, 11/8/2026)**: rayos (polilínea con jitter + glow `pixelShade`) de brillo moderado y breves, destello único con `fade`. Sin nubes ni flash/lavado de pantalla (se quitaron el 12/8/2026 por efecto estroboscópico en CRT). `reset(level)`, `update(dt, terrain)`, `drawSky` (no-op)/`drawBolts`. Sin física todavía |
| `storm_demo.cpp` | Prueba de visualización en PC: terreno + nave estática (sin física) + tormenta → PPM en `frames/` |

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
- **Viento (visual desde 5/8/2026; física desde 9/8/2026)**: se activa a partir de `WIND_START_LEVEL`
  (ahora **4**, siempre presente desde ese nivel; ráfagas y dirección aleatorias).
  Ráfagas: `windStrength = WIND_MIN(0.35) + (1-WIND_MIN)*gust` con `gust = 0.5+0.5*sin(windPhase*0.6)`;
  `windDir` (+1/-1) cambia cada 8–20 s. **Física (9/8/2026)**: el viento ahora **empuja la nave**
  (`Ship::update()`: `velX += windDir·windStrength·WIND_ACCEL` por tick, sin `GAME_DT`, como el resto
  de la física; `WIND_ACCEL=0.0004` ≈ 80% de la gravedad a ráfaga máxima). Afecta la trayectoria y
  el aterrizaje: mantener rumbo contra el viento cuesta empuje lateral (y por tanto combustible). Los
  tres efectos visuales siguen como estaban:
  - **Trazos de fondo (sutiles)**: campo de `WIND_STREAK_COUNT=18` guiones en el cielo que derivan
    en la dirección del viento. Cada trazo: guión principal delgado de 1 px (luma 180, longitud
    `(WIND_STREAK_MIN=20..WIND_STREAK_MAX=60)·viewScale·f1` con `f1` aleatorio 0.55–0.99 fijo al
    spawn) + 2 estelas atenuadas (`pixelShade` 90/45) detrás. **Fix 11/8/2026 — fork solo en la
    segunda fase**: en la **primera fase** (crucero/alta altitud) se dibuja **una sola línea
    delgada** (como el original, que se veía bien); en la **segunda fase** (aproximación, cuando
    `altFactor > WIND_FORK_ALT=0.5`, i.e. `alt<125`) el trazo se convierte en un **fork de dos
    líneas delgadas de 1 px** separadas 1 px con **longitudes diferentes** (`f1`/`f2`), con la
    segunda rama desvaneciéndose gradualmente (`180·forkIn`) mientras baja la altitud. Antes era
    una barra de `thick = 1+2·altFactor` px (hasta 3 px cerca del suelo) que se veía mal al
    aterrizar; el fork elimina el grosor en la aproximación y conserva la línea única en vuelo. Los
    trazos **viven en una banda de cielo dinámica que sigue la vista** (`skyTop=(0-viewY)/viewScale`,
    `skyBot=(SCREEN_H·0.55-viewY)/viewScale`, recortada por el terreno `terrainYAt-4` por trazo): al
    salir de la banda se **reubican al azar dentro de ella** (con `vy∈[-6,6]`) y hacen wrap
    horizontal en `[0, tileWidth]`. Así son **siempre visibles en vista normal y en zoom** (antes se
    reciclaban a ±20/720 u fuera de pantalla y casi nunca se veían). **Cantidad según
    altitud**: solo se dibujan los primeros `N = max(WIND_STREAK_MIN_VISIBLE(3), count·altFactor)`
    trazos con `altFactor = 1-alt/WIND_ALT_MAX(250)`. En zoom se limita `N≤8` para no saturar.
    **Fix 9/8/2026**: la intro de nivel (`introTimer`) congelaba la física y `ship.altitude`
    quedaba en 0 → `altFactor=1` → líneas de 3 px durante el anuncio `LEVEL N`. Ahora la intro
    recalcula `ship.altitude` (terreno bajo la nave). **Fix 11/8/2026**: además la intro actualiza
    el **box real de la nave** (`left/right/bottom/top` desde `posX/posY/scale`), porque con la
    física congelada `ship.bottom` quedaba stale del barco del título → la altitud de la intro podía
    salir distinta a la del arranque del nivel; con esto los trazos se ven **idénticos** al
    anunciar `LEVEL N` y al empezar a volar.
  - **Polvo en el suelo**: `DUST_COUNT=24` motas (`DustParticle {x,y,vy,life}`) que derivan con el
    viento (`x += speed*DUST_SPEED(0.7)`) pegadas al terreno (`terrainYAt()`, y entre 3 y 35 u
    sobre la superficie, nunca por debajo), con deriva vertical `vy∈[-2,2]`. **Se generan cerca de
    la nave** (`ship.posX ± DUST_RANGE=120`; **fix 8/8/2026**: antes el spawn era
    `rand()%2000/10 - DUST_RANGE` = solo 60–260 u a la **izquierda**, por eso el polvo no aparecía
    en el zoom; ahora es simétrico ±DUST_RANGE, y ±100 u quedan dentro del zoom ±93) y se reciclan
    (`DUST_LIFE=5 s`). **Puntito muy sutil lejos del pad** (mitad de motas a luma 45); la
    intensidad sube con una **rampa cuadrática** `t=(nearF-0.15)/0.85`,
    `b = 45+205·t²·windStrength·flick` (máx 250) → **casi invisible durante toda la fase de
    acercamiento** y solo se aclara de verdad al llegar al landing spot, donde se dibuja un
    **cúmulo en cruz de 5 px** (solo si `nearF>0.7` y `b>120`, vecinos a `b/2`) con **salto hacia
    arriba** (`vy -= nearF·2.5` al reciclar si `nearF>0.6`) → se ve claro **cuando la nave llega
    al landing spot**. `landingProximity()` es público (Game).
  - **Llama del motor desviada**: `Ship::draw()` inclina la llama (triángulo de la tobera) en la
    dirección del viento (`bend = windDir*windStrength*flameLen*sc*0.7`, base inclinada ×0.3) y le
    añade un rastro atenuado `(pixelShade` 90→0)` en la dirección del viento. `Ship` tiene
    `windStrength`/`windDir` que `Game::update()` le propaga cada frame (0 en niveles
    < `WIND_START_LEVEL`).
    Verificado en PC con test determinista: sin viento centrada, viento 1.0 → +3.5 px de sesgo.
  HUD: `WIND nn>` (o `<`) en `(250,52)`, en la columna derecha debajo de `VY` (los avisos
  `LOW FUEL`/`TOO FAST` se desplazan según haya WIND; ver sección Sketch ESP32); glifos `<` y `>`
  añadidos a la fuente 5x7.
  Config: `WIND_START_LEVEL=4` (viento siempre presente desde el nivel 4; ráfagas y dirección
  aleatorias) y `DEMO_LEVEL_FORCE=0` (el demo elige nivel al azar `1..DEMO_MAX_LEVEL`).
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
  (pot o paso de C). **`WIND nn>`/`<` (11/8/2026)** en `(250,52)`, debajo de `VY`, solo si
  `level >= WIND_START_LEVEL`. Los avisos de la derecha **se desplazan según haya WIND** para no
  dejar franja en blanco: si WIND está mostrado, `LOW FUEL`/`OUT OF FUEL` van en `(250,62)` y `TOO
  FAST` en `(250,72)`; si no, quedan en `(250,52)` y `(250,62)`.
  Aviso parpadeante `LOW FUEL` (o `OUT OF FUEL`) alineado con los indicadores de la derecha.
  **Aviso `TOO FAST` (7/8/2026)**: parpadeante (debajo de `LOW FUEL`) cuando la
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
  (`STICK: ROTATION`, `Z: ENGINE ON/OFF`, `C: POWER STEPS`, `POT: POWER LEVEL`). La línea de crédito
  `Copyright Alex Urzola 2026/Opencode` se dibuja **centrada debajo del título** (`y=40`, con
  `centerText`). El fondo es el de juego
  (estrellas + nave entrando **por la derecha** con deriva lenta a la izquierda
  (`setupTitleShip()`, `velX=-0.35`, `posX=(SCREEN_W-20)/viewScale`)).
- **Demo / attract mode (8/8/2026; re-trabajado para viento 9/8/2026)**: tras `DEMO_START_DELAY=13 s`
  en el título, `Game::startDemo()` lanza un nivel (1..4; `DEMO_MAX_LEVEL=4`) jugado por un
  **autopilot** (`Game::runDemoAI()`) hacia una plataforma (`demoTargetX/Y`). **Control desacoplado
  (9/8/2026)**: en vez del antiguo "ángulo por `asin(want)` + empuje acoplado", el AI calcula una
  aceleración horizontal `aX = (desVX−velX)·0.02 − windDir·windPush` (PD sobre la posición + contra
  el viento feedforward) y una vertical `aY = (velY−desVY)·0.03` limitada a `0.00075` (≈1.5·gravedad)
  con `desVY` según fase (0.12 en crucero, 0.04 cerca del suelo, 0.03 en aproximación), y deriva
  `angle = atan2(aX, aY)`, `thrust = hypot(aX,aY)/THRUST_ACCEL`. Así mantiene el rumbo contra el
  viento **sin subir** (el empuje se inclina a ~90° cuando no hace falta freno vertical). Cerca del
  suelo `aX` se atenúa (`alt<12 → ×0.6`, `alt<2.5 → ×0.05`) para aterrizar erguido; freno de ascenso
  suave (`velY<-0.01 → thrust=0`). La potencia (PWR) se rampea a velocidad humana
  (`DEMO_POWER_RATE=0.4/s`). **A veces gana, a veces pierde**: ~50 % de demos son "torpes"
  (`demoSkill` 0.00–0.35) y apuntan desviado (offset de hasta ±110 u) → aterrizan en la ladera; el
  resto (skill 0.60–1.00) aterriza casi siempre salvo que el viento cambie en la aproximación final.
  Ruido por-frame `(rand−0.5)·(1−skill)` en ángulo/empuje. Win-rate validado en PC
  (sin viento ~70–76 %; **con física de viento ~54 %** con `./demo_sim`, 200 seeds, sin timeouts,
  ~100 s/vuelo). Al aterrizar/estrellarse muestra el resultado (`CRASH_RESET_DELAY`) y vuelve al
  título; `DEMO` se muestra en el HUD **debajo de `PWR`** en `(22,62)`. Cualquier
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
- Compila validado con `arduino-cli compile --fqbn esp32:esp32:esp32`: ~425 KB flash
  (20% del app slot), RAM 7%. **Esquema de partición `no_ota`** (ver "Flash"), app slot de 2 MB.
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
- **El agente NO hace commit ni push salvo que el usuario lo pida explícitamente.** Los cambios
  quedan en el working tree; solo se commitea cuando el usuario lo ordena en el chat o al ejecutar
  el comando `/flash` (que hace el commit como parte de su flujo documentado en
  `.opencode/commands/flash.md`; "hacer flash" ≠ "que el agente commitee por su cuenta").
  Subir a la placa (upload) sí está permitido para probar en CRT sin commitear.

## Comandos útiles

- Validar port en PC: `make && ./test_pc` en `esp32Lander/` (todos los checks pasan).
  Demo visual: `./main_pc` (PPM en `frames/`). Win-rate del autopilot de demo:
  `./demo_sim <seeds>`; render de un demo: `./demo_render <seed>` (PPM en `frames/`).
  Tormenta: `./storm_demo <seed> <level>` (terreno + nave estática + rayos, sin física,
  PPM en `frames/`; selftest `bolts>0` + `maxAlive>0`).
- Compilar sketch: `arduino-cli compile --fqbn esp32:esp32:esp32 esp32LanderComposite/esp32LanderComposite.ino`.
- Subir: `arduino-cli upload --fqbn esp32:esp32:esp32 --port /dev/ttyUSB0 ...` (o Arduino IDE).
- Sync PC↔ESP32: `diff esp32Lander/<f> esp32LanderComposite/src/<f>`.

## Flash / memoria

- Placa: ESP32 Dev Module, flash **4 MB** (QIO 80 MHz), core 3.3.10.
- **Esquema de partición `no_ota`** ("No OTA (2MB APP/2MB SPIFFS)", `tools/partitions/no_ota.csv`),
  usado para aprovechar todo el flash de programa al máximo (no se hace OTA). Layout leído de la
  flash: `nvs 20K` (`0x9000`), `otadata 8K` (`0xe000`), **`app0 2 MB`** (`0x10000`),
  `spiffs 1.9 MB` (`0x210000`), `coredump 64K` (`0x3f0000`).
- Sketch ≈ **425 KB → 20% del app slot (2 MB)**; RAM: 24.8 KB estáticos (**7%**) y quedan 302.9 KB
  (92.4%) para stack/heap. En cualquier caso no hay presión de memoria.
- Para volver a flashear manteniendo el esquema `no_ota` (el `arduino-cli upload` simple usa el
  esquema `default` de 1.25 MB, que también es suficiente), se puede forzar el particionado en la
  compilación con:
  `arduino-cli compile --config-file …/arduino-cli.yaml --fqbn esp32:esp32:esp32 --build-property build.partitions=no_ota --build-property upload.maximum_size=2097152 esp32LanderComposite/esp32LanderComposite.ino`
  y luego flashear el binario resultante (`…ino.merged.bin`/`…ino.bin`) con `esptool.py`. Si se
  reflashea con el comando simple de `arduino-cli upload`, se revierte al esquema `default`
  (1.25 MB app), que sigue con abundante margen.

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
11. ~~Añadir viento desde LEVEL 4~~ **COMPLETADA (9/8/2026)**
    — **Visual** (5/8/2026): (a) campo sutil de `WIND_STREAK_COUNT=18` trazos con
    estelas (`pixelShade` 180/90/45) que derivan con la dirección del viento, **siempre visibles**
    (banda de cielo dinámica que sigue la vista; `N = max(3, count·altFactor)` trazos, grosor
    `1+2·altFactor` px, máx 8 en zoom; la intro recalcula la altitud para que no salgan gruesos);
    (b) `DUST_COUNT=24` puntitos de polvo pegados al terreno que derivan con el viento,
    **casi invisibles en toda la fase de acercamiento** (rampa cuadrática `t=(nearF-0.15)/0.85`,
    `b=45+205·t²·strength·flick`) y claros solo al llegar al landing spot (cúmulo en cruz de 5 px
    con `nearF>0.7` + salto hacia arriba si `nearF>0.6`); (c) llama del motor
    **desviada** en la dirección del viento en `Ship::draw()` (+ rastro atenuado). HUD `WIND nn>`/
    `<` en `(250,52)`, debajo de `VY`; glifos `<`/`>` en la fuente 5x7. **Física (9/8/2026)**: `WIND_ACCEL=0.0004` ≈ 80% de la gravedad a
    ráfaga máxima; `Ship::update()` aplica `velX += windDir·windStrength·WIND_ACCEL` por tick (sin
    `GAME_DT`). El **autopilot del demo** se re-trabajó a control desacoplado
    (ángulo↔horizontal, empuje↔vertical, con feedforward contra el viento) para seguir jugable con
     viento. Validado en PC: `test_pc` (45 checks ALL PASSED), `demo_sim` 200 seeds (54% win, 0
     timeouts, ~100 s/vuelo; sin viento ~70–76%), `wind_test` (viento 1.0 acelera `velX` 0.350 vs
     0.259 en caída libre 10 s) y análisis de PPM. **Sync completado** a
     `esp32LanderComposite/src/`; compila (429 KB, 20%). **Pendiente**: revisar el efecto en CRT y
     subir la build con la física.
12. ~~Restaurar demo y viento a valores de producción~~ **COMPLETADA (11/8/2026)**
    — `DEMO_LEVEL_FORCE=0` (el demo vuelve a elegir nivel al azar 1..4) y `WIND_START_LEVEL=4`
    (viento siempre presente desde el nivel 4), en `esp32Lander/` y `esp32LanderComposite/src/`.
    `test_pc` (45 checks) OK.
13. **Tormenta eléctrica — integración visual (11/8/2026)**
    — módulo `Storm` (`storm.h/cpp`, solo visual, sin física) + `storm_demo <seed> <level>` (PC):
    terreno + nave estática + rayos → PPM en `frames/` (selftest `bolts>0` + `maxAlive>0`).
    Rayo: polilínea con jitter + glow (`pixelShade`), destello único con `fade` al final de vida
    (sin parpadeo aleatorio). **Sin nubes, sin flash/lavado de pantalla**: se quitaron el 12/8/2026
    porque el lavado de fondo (`STORM_FLASH_MAX=48`) y las nubes hacían que "toda la pantalla"
    apareciera/desapareciera estroboscópicamente en CRT. El rayo ahora es **fino, de brillo moderado
    (~185) y breve** (`STORM_BOLT_LIFE=0.22`, sin ramas ni brillo de impacto), ocasional
    (`STORM_BOLT_MIN/MAX=4/8 s`) para no entorpecer la aproximación. `Storm` está **integrado en
    `Game`** (visual, sin física): `drawSky` (no-op) tras el `clear()` y `drawBolts` tras
    `ship.draw()`, con `update(dt, terrain)` cada tick (excluye el título). **`STORM_START_LEVEL=1`
    temporal** (para verla al probar; pendiente restaurar a 5). Validado en PC: `test_pc` 45 checks,
    `./storm_demo 3 9` (rayo OK), mediana de cielo = 0 (sin wash). **Sync completado** a
    `esp32LanderComposite/src/`; subido a placa (433 KB, 7%). Confirmar en CRT.
