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
| `moons.h` | **Lunares de nivel (14/8/2026; gravedad 15/8/2026; géiseres 16/8/2026; volcanes 16/8/2026)**: tabla `MoonInfo {name, gravity}` con 8 lunas (LUNA, IO, EUROPA, GANYMEDES, CALLISTO, TITAN, ENCELADUS, TRITON) y `moonIndex(level)`/`moonName(level)`/`moonGravity(level)`/`moonHasGeysers(level)`/`moonHasVolcanoes(level)` (índice `(level-1) % 8`). **PENDIENTE (19/8/2026)**: personalidad/efectos propios por luna (dificultad, cielo, título). Hoy cada luna solo aporta su gravedad y, según índice, sus efectos ambientales ya integrados: géiseres (Encélado), volcanes (Ío), niebla (Titán). Faltan efectos propios para LUNA, EUROPA, GANYMEDES, CALLISTO y TRITON, y queda por definir la dificultad y el cielo/título por luna. Se ampliará en ramas posteriores |
| `geysers.h/cpp` | **Géiseres de Encélado (16/8/2026, rama `moon-flavor`)**: activo en niveles de Encélado (`moonHasGeysers(level)` → `moonIndex==6`, i.e. nivel 7, 15, 23…). `reset(level, terrain)` coloca `GEYSER_VENTS=5` respiraderos **en las bases de las laderas junto a las plataformas de aterrizaje** (`GEYSER_VENT_OFFSET=14 u` del borde del pad) para que estén en la trayectoria de vuelo, `update(dt)` + `draw(r,viewX,viewY,viewScale)`. Cada respiradero erupciona cíclicamente (burst `GEYSER_BURST=3 s` + pausa `GEYSER_GAP_MIN..MAX=3..9 s`, desincronizados):     emite partículas (máx `GEYSER_MAX_PARTS=250`, ~50 % de los ticks) que ascienden con `GEYSER_PART_SPEED=24` y arco por `GEYSER_PART_GRAV=7` (~38 u de altura) y se desvanecen (`pixelShade` 200→60); mientras erupciona dibuja una **columna cónica** (relleno por filas con degradado radial: brillo 215 en el centro de la base → desvanecido hacia arriba/bordes; **doblez en S** `sin(t·1.7)·1.5·t` y "puffos" lentos `sin(t·5+age·1.5)`, sin parpadeo; paso 2 px en zoom si la columna mide ≥40 px, alto `GEYSER_SPOUT_H=40 u`) + brillo de tobera en cruz/plus de 5 px (220). **Física (térmica, 16/8/2026)**: `inPlume(x,y)` detecta si la nave está en el chorro (|x−vent|≤`GEYSER_RADIUS=8`, y entre suelo y `GEYSER_PLUME_H=40`); `Game::update()` aplica `ship.velY -= GEYSER_PUSH=0.00025` por tick (~50 % de la gravedad de Encélado) → la nave recibe un pequeño empuje hacia arriba al cruzar la columna. Se añadieron `ventCount()`/`ventX(i)` para tests |
| `geyser_demo.cpp` | Prueba de visualización en PC: terreno generado + géiseres → PPM en `frames/` (selftest `active` + `maxAlive>0`) |
| `volcanoes.h/cpp` | **Volcanes de Ío (16/8/2026, rama `moon-flavor`; validado en CRT 17/8/2026)**: activo en niveles de Ío (`moonHasVolcanoes(level)` → `moonIndex==1`, i.e. nivel 2, 10, 18, 26…). `reset(level, terrain)` escanea el terreno (muestreo cada 6 u, desnivel `VOLCANO_MIN_DROP=14 u` sobre `VOLCANO_FLOW_LEN=60 u`, sin pisar zonas landable) y coloca `VOLCANO_VENTS=4` volcanes repartidos en laderas descendentes; `update(dt)` + `draw(r,viewX,viewY,viewScale)`. Cada volcán muestra **flujo de lava** (línea brillante `200–255` que sigue el terreno ladera abajo en pasos `VOLCANO_FLOW_STEP=4 u`, dibujada 1 px sobre la superficie para que no la tape el blanco del terreno, desvanecida 200→227 hacia el final, con pulso `0.85+0.15·sin(t·2+phase)`) + **brillo de cráter pulsante** (cruz/plus 190–230). Erupciones **casi continuas** (burst `VOLCANO_BURST=9 s` + pausa corta `VOLCANO_GAP_MIN..MAX=0.5..1.5 s`, desincronizadas; el demo de PC muestra llama visible el 100 % del tiempo): **destello radial** inicial (`VOLCANO_FLASH=0.25 s`, radio 3, 230→0) y partículas en arco (máx `VOLCANO_MAX_PARTS=200`, ~50 % de ticks, `VOLCANO_ERUPT_SPEED=26`, `VOLCANO_PART_GRAV=10`, vida `VOLCANO_PART_LIFE=1.3` s) que se desvanecen (`pixelShade` 220→40). **Mecánica de lava (16-17/8/2026)**: `computeLava()`/`clampFlows()` calculan los **rangos reales** donde el flujo cubre la plataforma de aterrizaje (`LavaRange {x1,x2}`, franja segura garantizada `VOLCANO_SAFE_STRIP=8 u` de ancho en el centro del pad, i.e. el pad nunca queda 100 % cubierto; el rango se recorta al **alcance real del flujo** — antes sobredimensionaba). `landOnLava(x1,x2)` detecta si el box de la nave pisa lava. **Cualquier colisión sobre lava quema** (`checkCollisions`: `result != 0 && landOnLava(...)` → `lavaBurn=true`, incluido hang-off-pad): aterrizar bien sobre lava no da score y muestra el final **"YOU BURNED" / "LAVA DESTROYED THE SHIP"**. API para tests: `active()`, `volcanoCount()`, `particlesAlive()`, `lavaRangeCount/X1/X2`, `landOnLava` |
| `volcano_demo.cpp` | Prueba de visualización en PC: terreno generado + volcanes → PPM en `frames/` (selftest `active` + `maxAlive>0`; nivel por defecto 2 = Ío) |
| `atmosphere.h/cpp` | **Atmósfera de Titán (19/8/2026; bandas de niebla 19/8/2026)**: activa en niveles de Titán (`moonHasTitan(level)`, `moonIndex==5`, i.e. nivel 6, 14, 22, 30…). `reset(level)` + `update(dt)` + `drawSky` (halo tenue ~20/9 luma de 2 px sobre la silueta del terreno + **bandas de niebla vivas**: `FOG_BAND_COUNT=3` franjas horizontales... ). **Niebla dinámica**: `centerY()`/`halfAt()` fuente única; **`hidesShip(x,y)`** oculta la nave. **Física**: arrastre `velX *= ATMOS_DRAG=0.9992` y corriente descendente `velY += ATMOS_DOWN=0.00008`. **Viento reactivado en Titán**; tormenta apagada (`storm.setEnabled(false)`). Ver detalle en "Proceso de trabajo" #20 |
| `titan_demo.cpp` | Prueba de visualización en PC: terreno generado + atmósfera → PPM en `frames/` (nivel por defecto 6 = Titán) |

### Mundo y pantalla

- Mundo 800×600, pantalla 320×240. Física y colisiones en coordenadas de mundo; el dibujado escala.
- Vista normal `viewScale = SCREEN_H/700`. Nave `scale = 1.0` (vista normal) / `0.32` (zoom).

### Constantes del port (config.h)

- `GRAVITY=0.0005`, `THRUST_ACCEL=0.0018`, `DRAG=0.9997`, `TOP_SPEED=0.35`.
- **Gravedad por luna (15/8/2026, rama `moon-gravity`)**: cada luna tiene un multiplicador
  (`moonGravity(level)`), la gravedad efectiva es `GRAVITY · moonGravity(level)`. Valores: LUNA
  1.00, IO 1.10, EUROPA 0.85, GANYMEDES 0.95, CALLISTO 0.90, TITAN 0.90, ENCELADUS 0.70, TRITON
  0.75. `Game::update()` propaga `ship.gravity` cada frame (nuevo campo en `Ship`, default
  `GRAVITY`); `Ship::update()` aplica `velY += gravity` (único punto de la física). El HUD muestra
  el multiplicador como **`G 0.85`** en `(250,52)` (bajo `VY`), con glitch al impacto de rayo.
  Los umbrales de aterrizaje (`LAND_PERFECT_VY`/`LAND_HARD_VY`) no cambian.
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
- **Viento (visual desde 5/8/2026; física desde 9/8/2026)**: se activa desde el nivel
  `WIND_START_LEVEL` (**4**) y **aleatoriamente por nivel** (desde 13/8/2026: 50 % de
  probabilidad por nivel ≥ 4, `WIND_CHANCE_PERCENT`; ver sección de efectos aleatorios);
  ráfagas y dirección aleatorias.
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
  HUD: `WIND nn>` (o `<`) en `(250,62)`, en la columna derecha debajo de `G` (los avisos
  `LOW FUEL`/`TOO FAST` se desplazan según haya WIND; ver sección Sketch ESP32); glifos `<` y `>`
  añadidos a la fuente 5x7.
  Config: `WIND_START_LEVEL=4` y `WIND_CHANCE_PERCENT=50` (viento **aleatorio por nivel** desde el
  nivel 4: 50 % de probabilidad por nivel; ráfagas y dirección aleatorias) y `DEMO_LEVEL_FORCE=0`
  (el demo elige nivel al azar `1..DEMO_MAX_LEVEL`).
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
- **C + stick del nunchuck → pasos de potencia (5/8/2026; rediseñado 17/8/2026)**: mientras se
  mantiene **C**, mover el stick **arriba sube / abajo baja** el `powerLevel` de forma continua
  (no pasos discretos), con rampa `PWR_STICK_RATE=0.008`/tick (~2 s de barrido completo con el
  stick a tope). `readStickYDev()` devuelve `+` arriba / `−` abajo (invertido: Y raw bajo =
  arriba); calibración adaptativa del eje Y (`stickCenterY=128` inicial, idle 40, dead zone 10).
  El **pot sigue funcionando** ("last-used wins"): al empezar el modo stick se guarda
  `potAtCycle` y si el pot se mueve >120 cuentas ADC retoma el control (`pwrStickActive=false`,
  `powerLevel=potLevel`). Sustituye al antiguo ciclo por pasos `{0,25,50,75,100}%` (`powerStep`).
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
  (pot o paso de C). **`G x.xx` (15/8/2026)**: multiplicador de gravedad de la luna
  (`ship.gravity / GRAVITY`) en `(250,52)`, debajo de `VY` (con glitch al impacto de rayo).
  **`WIND nn>`/`<` (11/8/2026)** en `(250,62)`, debajo de `G`, solo si
  `game.windEnabled` (viento aleatorio por nivel ≥ 4). Los avisos de la derecha **se desplazan según haya WIND** para no
  dejar franja en blanco: si WIND está mostrado, `LOW FUEL`/`OUT OF FUEL` van en `(250,72)` y `TOO
  FAST` en `(250,82)`; si no, quedan en `(250,62)` y `(250,72)`.
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
  **Nombre de luna (14/8/2026, rama `moon-names`)**: debajo de `LEVEL N` se dibuja el nombre de la
  luna del nivel (`moonName(level)`, escala 2, mismo fade), por ejemplo `LEVEL 3` / `EUROPA`.
  Validado en CRT; se ampliará en ramas posteriores.
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
- **Demo / attract mode (8/8/2026; re-trabajado para viento 9/8/2026; niveles al azar 1..12 desde
  13/8/2026)**: tras `DEMO_START_DELAY=13 s`
  en el título, `Game::startDemo()` lanza un nivel al azar (1..12; `DEMO_MAX_LEVEL=12`) jugado por un
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
  (con niveles 1..12 y efectos aleatorios: **~45 %** con `./demo_sim` 60 seeds, 0 timeouts,
  ~90 s/vuelo; con niveles 1..4 y viento físico ~54 %). Al aterrizar/estrellarse muestra el
  resultado (`CRASH_RESET_DELAY`) y vuelve al
  título; `DEMO` se muestra en el HUD **debajo de `PWR`** en `(22,62)`. Cualquier
  `startPressed` cancela el demo y arranca partida real (`demo=false`). `srand(esp_random())` en
  `setup()`. Validado en PC: `test_pc` (50 checks) + `demo_sim`.
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
| Nunchuck (botón C) | Mientras se mantiene C, el stick sube/baja `powerLevel` (continuo, rampa 0.008/tick) | `pwrStickActive`; pot retoma si se mueve >120 ADC |
| Botón (GPIO13) | Inicio / reinicio de partida | Equivale a tecla "P"; **sin autostart** (espera el botón) |

El motor se enciende/apaga con el botón Z del nunchuck (como el resorte del gatillo: soltado =
motor apagado). La potencia se fija con el pot (continuo) o el botón C (pasos de 25 %); "last-used
wins": al mover el pot >120 cuentas ADC, vuelve a mandar el pot.

## Hardware eléctrico / pinado

Las conexiones eléctricas, esquemas, mediciones y el detalle de flash/memoria están en
**`docs/hardware.md`** (lo consulta el `hardware` agent). Resumen de pines:

| Señal | GPIO |
|-------|------|
| I2C nunchuck SDA / SCL | GPIO21 / GPIO22 |
| Pot (nivel de potencia) | GPIO34 |
| Gatillo (LEGACY, desconectado) | GPIO35 |
| Botón start | GPIO13 |
| Video compuesto (DAC) | GPIO25 |
| Audio (LEDC PWM) | GPIO26 |

La **lógica de lectura/mapeo** de estos pines (dead zone, calibración del stick, botones
Z/C, "last-used wins" del pot) es código de juego y se documenta en la sección "Entrada".

## Salida de video (compuesta a CRT B/N)

Librería aquaticus `esp32_composite_video_lib` (GPL) embebida como `src/video.h/c`:
`video_graphics(NTSC_320x240, FB_FORMAT_GREY_8BPP)`, DAC GPIO25 → RCA, B/N luma 255.
El renderer escribe en `video_get_frame_buffer_address()`; `video_wait_frame()` sincroniza.
Cableado y detalle del framebuffer: `docs/hardware.md`.

## Sonido (COMPLETADA 4/8/2026)

- **Vía: PWM por LEDC + timer ISR en GPIO26** (NO I2S). Motivo: la librería de video
  (aquaticus) usa I2S0 + DAC1 (GPIO25) y `dac_i2s_enable()` fuerza DAC2 (GPIO26) a modo DMA,
  así que GPIO26 no estaba realmente libre para I2S. Solución: `dac_output_disable(DAC_CHANNEL_2)`
  libera la almohadilla y se usa **LEDC (canal 0, HS mode) como PWM portador a 312.5 kHz
  (resolución 8-bit = máx)**.
- `src/audio.h/cpp`: timer gptimer a **16 kHz** con ISR (`IRAM_ATTR`) que mezcla
  `THRUST_SOUND` (loop) + `WIND_SOUND` (loop) + `EXPLOSION_SOUND` (one-shot) + `LIGHTNING_SOUND`
  (one-shot) + **voz de quemado** (17/8/2026, one-shot de 4 s) en RAM (copiados desde PROGMEM al
  arrancar) y escribe el duty directo al registro
  `LEDC.channel_group[0].channel[0].duty.duty` (`val<<4`) + handshake `duty_start`. API:
  `Audio::begin()`, `Audio::setThrust(0..1)`, `Audio::setWind(0..1)`, `Audio::playExplosion()`,
  `Audio::playBurn()`, `Audio::playLightning()`.
- Datos: `src/audio_data.h` generado (PROGMEM) desde `sounds/rocket_thrust.wav` (32 k, loop),
  `sounds/explosion.wav` (27.4 k, one-shot), `sounds/wind.wav` (32 k, loop) y
  `sounds/lightning.wav` (19.2 k, one-shot) — mono 8-bit / 16 kHz. Generados por
  `sounds/gen_storm_sounds.py` (viento y rayo; los reales de motor/explosión no se tocan) y
  combinados por `sounds/convert_wav.py`.
- Origen de los sonidos: **reales**, extraídos de `tblazevic/moonlander` (clon arcade JS)
  `audio/rocket.mp3` (loop de motor) + `audio/crash.mp3`. Pipeline en `sounds/real_sounds.py`
  (extrae el segmento 2 s más estable del mp3, hace **loop sin clic** cruzando la continuación
  natural hacia la cabeza, sube ganancia con `tanh`, convierte a 8-bit). Los mp3 se convierten
  primero a PCM16 16 kHz con ffmpeg (`/tmp/opencode/rocket16.wav`).
- RAM: los sonidos se copian a RAM al arrancar (~116 KB incl. beep) para lectura segura desde el ISR.
- Disparo en el `.ino`: transición a `STATE_CRASHED` → explosión; `thrustBuild` durante
  `STATE_PLAYING` → motor; `game.windEnabled` (viento activo en el nivel y jugando) → viento;
  `game.storm.takeNewBolt()` → rayo.
- **Suavizado del sonido (12/8/2026)**: el volumen de motor y viento usa **rampa de ataque/release**
  con easing **entero** (`envEase`, `ENV_DIV=24`) en el ISR (sin FPU: punto flotante en el IRAM ISR
  del núcleo Arduino → `LoadProhibited`/reset) para que entren/salgan sin clic, y el tono del motor
  se **suaviza con un low-pass** (alpha 0.45) al copiarlo a RAM. El **viento se mantiene sutil**
  (`WIND_GAIN=140`, tope `WIND_MAX=160` sobre 256) y **proporcional a la velocidad del viento**
  (`setWind(windStrength)`). El rayo es un one-shot (crack + trueno) al formarse cada rayo.
  Validado: `test_pc` 50 checks, sketch compila 486 KB / RAM 7%, monitor serial estable (sin reset).
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

Resumen: ESP32 Dev Module flash 4 MB (QIO 80 MHz), core 3.3.10, esquema de partición `no_ota`
(2 MB app). Sketch ≈ 425 KB (20% del app slot), RAM 7%. Layout de particiones y cómo forzar
`no_ota` al flashear (vs `arduino-cli upload` que revierte a `default`): `docs/hardware.md`.

## Proceso de trabajo

1. ~~Medir el reóstato y documentar resultado~~ **COMPLETADA (2/8/2026)** — ver docs/hardware.md.
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
    `ship.draw()`, con `update(dt, terrain)` cada tick (excluye el título).
    **`STORM_START_LEVEL=3` y activación aleatoria por nivel** (`STORM_CHANCE_PERCENT=50`,
    13/8/2026; antes `1` temporal para probarla). Validado en PC: `test_pc` 50 checks,
    `./storm_demo 3 9` (rayo OK), mediana de cielo = 0 (sin wash). **Sync completado** a
    `esp32LanderComposite/src/`; subido a placa (433 KB, 7%). Confirmar en CRT.
    **Física del impacto (11/8/2026)**: `Storm::strikes(sx, sy, STORM_HIT_RADIUS)` detecta si el
    camino del rayo (distancia punto-segmento) pasa a < `STORM_HIT_RADIUS` (70 u) del centro de la
    nave (`Bolt::hit` evita dobles golpes del mismo rayo). Al golpe: **−combustible**
    (`STORM_HIT_FUEL=60`) y **pérdida temporal de control** (`STORM_CONTROL_LOSS=1.5 s`: motor
    cortado y ángulo con jitter aleatorio ±0.5 rad). **HUD con glitch (13/8/2026)**: durante el
    golpe los instrumentos se "cubren" de lecturas scrambled (`glitchChars`, dígitos+letras
    aleatorios que bailan) en `ANG`/`PWR`/`ALT`/`VX`/`VY`; el aviso de texto `LIGHTNING` se quitó
    (el glitch lo sustituye).
    `stormHitTimer` se resetea al iniciar partida/nivel/demo.
    **Llama de exhaust (12/8/2026; perfil teardrop 14/8/2026)**: cono relleno (cuerdas con
    degradado `pixelShade` + núcleo y aristas) en `ship.cpp`, que crece gradualmente con
    `thrustBuild` (sin `flicker` temporal → sin strobe). `flameLen = thrustBuild*24.0f`. Desde
    14/8/2026 la sección usa **perfil teardrop** `halfW = 0.5·width0·tt(1−tt)·4` (angosto en tobera
    y punta, ancho en el medio) y se quitaron las dos líneas del contorno exterior: la llama
    empieza pegada al cuerpo pero nunca lo solapa al rotar la nave.
14. **Efectos aleatorios por nivel (13/8/2026)** — la demo, la tormenta y el viento pasan a ser
    **aleatorios por nivel**:
    - **Demo**: `DEMO_MAX_LEVEL=12` — `Game::startDemo()` elige nivel al azar `1..12`
      (`DEMO_LEVEL_FORCE=0`); el nivel juega con sus efectos aleatorios propios (tormenta/viento).
    - **Tormenta**: `STORM_START_LEVEL=3` + `STORM_CHANCE_PERCENT=50` — `Storm::reset(level)`
      activa `enabled_` (nuevo campo; `active()` = `enabled_`) con 50 % de probabilidad solo si
      `level >= STORM_START_LEVEL`. Se decide de nuevo en cada `reset()` (nueva partida/nivel/demo);
      `restartLevel()` (mismo nivel) conserva el estado. La demo **ya no fuerza** la tormenta
      (`startDemo()` usa `storm.reset(level)`). **Los rayos caen al azar** (en el demo también): se
      eliminó `Storm::aimAt` (14/8/2026), que apuntaba cada rayo a la nave en el demo; el rayo solo
      golpea si `strikes()` detecta que su camino pasa cerca de la nave.
    - **Viento**: `WIND_START_LEVEL=4` + `WIND_CHANCE_PERCENT=50` — `Game::windEnabled` (nuevo
      campo público) se decide con 50 % de probabilidad en `newGame()` (nivel 1 → siempre off),
      `nextLevel()` y `startDemo()`. Todos los gate del viento (`spawnWind`, `spawnDust`,
      `updateWind`, `drawWind`, `ship.windStrength`, HUD `WIND`, y `Audio::setWind` en el `.ino`)
      usan `windEnabled` en vez de `level >= WIND_START_LEVEL`.
    - **Ambos efectos pueden coincidir** en un mismo nivel (decisiones independientes).
    Validado en PC: `test_pc` **50 checks ALL PASSED**, `demo_sim` 60 seeds (**45 % win, 0 timeouts,
    ~90 s/vuelo**, niveles 1..12), `storm_demo 3 9` y `5 4` (rayo OK). **Sync completado** a
    `esp32LanderComposite/src/`; sketch compila (486 KB, 37 % del slot default). Confirmar en CRT.
15. **Gravedad por luna (15/8/2026, rama `moon-gravity`)** — cada luna tiene un multiplicador de
    gravedad (`moons.h`: `MoonInfo {name, gravity}` + `moonGravity(level)`). La gravedad efectiva
    es `GRAVITY · moonGravity(level)`; `Game::update()` propaga `ship.gravity` cada frame (nuevo
    campo en `Ship`, default `GRAVITY`) y `Ship::update()` aplica `velY += gravity` (único punto
    de la física). El HUD muestra el multiplicador como **`G x.xx`** en `(250,52)`, bajo `VY`, con
    glitch al impacto de rayo; `WIND` se movió a `(250,62)` y los avisos de la derecha se
    desplazan según haya WIND. Los umbrales de aterrizaje no cambian. Validado en PC: `test_pc`
    **58 checks ALL PASSED** (nuevos checks de `moonGravity`/propagación/aplicación), `demo_sim`
    20 seeds (**55 % win, 0 timeouts**, autopilot compensa la gravedad variable).
    **Sync completado** a `esp32LanderComposite/src/`; sketch compila (486 KB, 37 %).
    Confirmar en CRT.
16. **Géiseres de Encélado (16/8/2026, rama `moon-flavor`)**: en niveles
    de Encélado (`moonHasGeysers(level)`, `moonIndex==6` → nivel 7, 15, 23…) `Geysers::reset(level,
    terrain)` coloca `GEYSER_VENTS=5` respiraderos **en las bases de las laderas junto a las
    plataformas de aterrizaje** (`GEYSER_VENT_OFFSET=14 u` del borde del pad) y cada uno erupciona
    cíclicamente (burst `GEYSER_BURST=3 s` + pausa `GEYSER_GAP_MIN..MAX=3..9 s`, desincronizados).
    Durante la erupción emite partículas (máx `GEYSER_MAX_PARTS=250`, ~50 % de ticks) que ascienden
    (`GEYSER_PART_SPEED=24`, arco por `GEYSER_PART_GRAV=7`) y se desvanecen (`pixelShade` 200→60),
    y dibuja una **columna cónica** (relleno por filas con degradado radial: brillo 215 en el centro
    de la base → desvanecido hacia arriba/bordes; **doblez en S** `sin(t·1.7)·1.5·t` y "puffos"
    lentos `sin(t·5+age·1.5)`, sin parpadeo; paso 2 px en zoom, alto `GEYSER_SPOUT_H=40 u`) + brillo
    de tobera en cruz/plus de 5 px (220). **Física (térmica)**: `inPlume(x,y)` detecta si la nave
    está en el chorro
    (|x−vent|≤`GEYSER_RADIUS=8`, y entre suelo y `GEYSER_PLUME_H=40`); `Game::update()` aplica
    `ship.velY -= GEYSER_PUSH=0.00025` por tick (~50 % de la gravedad de Encélado). Integrado en
    `Game` junto a `storm`: `reset` en constructor/newGame/nextLevel/startDemo, `update` tras
    `storm` (excluye título), `draw` tras `terrain.draw` (antes de la nave). Validado en PC:
    `test_pc` **68 checks ALL PASSED** (nuevos de `moonHasGeysers`/actividad/`inPlume`/pluma),
    `geyser_demo 3 7` (máx 250 partículas visibles), `demo_sim` 20 seeds en nivel 7 (**50 % win,
    0 timeouts**, el autopilot aguanta el empuje). **Sync completado** a `esp32LanderComposite/src/`;
    sketch compila (492 KB, 37 %). **Validado en CRT** (16/8/2026): se afinó el ancho de la columna
    (pico 2.3 px de media anchura) y se añadió punta cónica; `DEMO_LEVEL_FORCE` y `START_LEVEL`
    (temporal 7) revertidos a 0/1 tras validar.
17. **Volcanes de Ío (16/8/2026, rama `moon-flavor`)**: en niveles
    de Ío (`moonHasVolcanoes(level)`, `moonIndex==1` → nivel 2, 10, 18, 26…) `Volcanoes::reset(level,
    terrain)` escanea el terreno (muestreo cada 6 u, desnivel `VOLCANO_MIN_DROP=14 u` sobre
    `VOLCANO_FLOW_LEN=60 u`, sin pisar zonas landable) y coloca `VOLCANO_VENTS=4` volcanes repartidos
    en laderas descendentes. Cada volcán muestra **flujo de lava** que sigue el terreno ladera abajo
    (pasos `VOLCANO_FLOW_STEP=4 u`, línea brillante `200–255` dibujada 1 px sobre la superficie para
    que no la tape el blanco del terreno, desvanecida hacia el final, pulso lento `0.85+0.15·sin(t·2)`)
    + **brillo de cráter pulsante** (cruz/plus). Erupciones **casi continuas** (burst
    `VOLCANO_BURST=9 s` + pausa `VOLCANO_GAP_MIN..MAX=0.5..1.5 s`, desincronizadas): destello radial
    (`VOLCANO_FLASH=0.25 s`) y partículas en arco (máx `VOLCANO_MAX_PARTS=200`,
    `VOLCANO_ERUPT_SPEED=26`, `VOLCANO_PART_GRAV=10`, vida `VOLCANO_PART_LIFE=1.3 s`) que se
    desvanecen. **Mecánica de lava (16-17/8/2026)**: `LavaRange {x1,x2}` recortado al **alcance real
    del flujo**, franja segura garantizada `VOLCANO_SAFE_STRIP=8 u` en el centro del pad; cualquier
    colisión sobre lava quema (`lavaBurn`, incluido hang-off-pad) → final **"YOU BURNED"**. **Final de
    nave quemada (17/8/2026)**: en `STATE_CRASHED` con `lavaBurn` la nave se **derrite** (límite de
    fusión que sube de las patas al techo recortando las aristas en `Ship::draw(melt)`, con borde
    ondulado), glow radial pulsante creciente, arista fundida brillante, brasas que ascienden y gotas de
    metal que caen, hasta deshacerse en ~4 s (CRASH_RESET_DELAY). **Sonido de quemado (17/8/2026)**:
    `Audio::playBurn()` — el sample de explosión reproducido a ~17 % de volumen (peak 19/128 vs 79),
    con jitter LFSR que crepita, envolvente de 4 s (fade-in 0.2 s, fade-out 1.2 s) y crossfade en el
    loop; disparado por el `.ino` al pasar a `STATE_CRASHED` con `lavaBurn` (el choque normal sigue con
    `playExplosion()`). Integrado en `Game` junto a `geysers`: `reset` en constructor/newGame/nextLevel/startDemo,
    `update` tras `geysers` (excluye título), `draw` tras `geysers.draw` (antes de la nave). Validado
    en PC: `test_pc` **738 checks ALL PASSED** (nuevos de `moonHasVolcanoes`/actividad/partículas/
    lava), `volcano_demo 3 2`/`1 2`/`5 2` (máx 78–126 partículas), `demo_sim` 10 seeds (**40 % win,
    0 timeouts**, niveles 1..12). **Sync completado** a `esp32LanderComposite/src/`; sketch compila
    (500 KB, 38 %); subido a placa. **Validado en CRT (17/8/2026)**: flame casi continua, final
    "YOU BURNED" con derretido + sonido crepitante confirmados. Temporales revertidos
    (`DEMO_LEVEL_FORCE=0`, `START_LEVEL=1`).
18. **PENDIENTE — Boca de volcán con patrón "U" aserrado (19/8/2026, sugerido por el usuario)**: al
    dibujar el volcán, la boca del cráter debería dibujarse siguiendo un patrón de **"U" aserrado**
    (convexa hacia abajo), en vez del labio elíptico irregular actual
    (`volcanoes.cpp` ~línea 352: bucle de 14 puntos con `rr = 2.6 + 0.9·sin(ang·3+phase)`, dibujado
    como anillo alrededor del vent). Referencia para retomar: el dibujo de la boca está en
    `Volcanoes::draw()` en `esp32Lander/volcanoes.cpp` (y su copia en
    `esp32LanderComposite/src/volcanoes.cpp`). No implementado todavía.
19. **Máximo 3 volcanes por viewport (19/8/2026, rama `moon-flavor`)**: `Volcanoes::draw()` ya no
    dibuja los `VOLCANO_VENTS=4` sin filtrar: `pickVisible(viewX, viewScale, out)` (helper testeable)
    cierra los volcanes fuera de pantalla (margen `80·viewScale` px para alcanzar flujos y fireballs)
    y, si quedan más de `VOLCANO_MAX_VISIBLE=3` en vista, conserva los 3 más cercanos al centro de
    pantalla. `countInView(viewX, viewScale)` expone el conteo (cap ≤ 3 verificado en `test_pc` por
    barrido de cámaras en vista normal y zoom; zoom centrado en un volcán → ≥ 1). Config en
    `config.h`. Sync completado a `esp32LanderComposite/src/`.
20. **Atmósfera de Titán (19/8/2026, rama `moon-flavor`)**: módulo `Atmosphere`
    (`atmosphere.h/cpp`, PC + composite) activo en niveles de Titán (`moonHasTitan(level)`,
    `moonIndex==5` → nivel 6, 14, 22, 30…). **Visual**: halo tenue sobre la silueta del terreno
    (`drawSky`, luma ~20/9, muestreo por columna cada 2 px) + **bandas de niebla vivas**
    (`FOG_BAND_COUNT=3` franjas horizontales en el corredor de descenso, desde `FOG_BAND_START=185`,
    media altura `FOG_BAND_HALF_MIN..MAX=35..50 u`, hueco `FOG_BAND_GAP_MIN=70+ u`, velo a luma
    `FOG_BRIGHT=32` con gradiente pico-en-el-centro, muestreado cada 2 px, **solo en el cielo**).
    **Niebla dinámica (19/8/2026)**: cada banda **deriva verticalmente** (`FOG_DRIFT_A=20 u`,
    velocidad 0.12–0.20 rad/s) y su **espesor ondula** en x y en el tiempo (`FOG_WAVE_A=0.35·half`,
    `FOG_WAVE_K=0.02`, `FOG_WAVE_SPEED=0.15`, fases por banda) → las zonas ciegas **no se pueden
    memorizar**; `centerY()`/`halfAt(x)` son la fuente única (visual y lógica idénticas).
    **`hidesShip(x,y)`**: si la nave cae dentro de una banda, `Game::draw()` **no la dibuja** en
    `STATE_PLAYING` → vuelas "a ciegas" por el HUD (ALT/VX/VY/ANG) hasta salir. **La niebla no
    cubre el HUD (19/8/2026)**: `FOG_SCREEN_TOP=100` px — `drawSky` recorta la franja superior
    (deja libres `LOW FUEL`=72 y `TOO FAST`=82 con viento) y **recorta también el halo del terreno**
    (`sy ≥ FOG_SCREEN_TOP+2`); test pixel que verifica 0 píxeles de niebla/halo sobre el HUD.
    **Física**: arrastre
    lateral `velX *= ATMOS_DRAG=0.9992` por tick (~7.7 %/s) y corriente descendente
    `velY += ATMOS_DOWN=0.00008` por tick (~23 % de la gravedad de Titán). **Viento reactivado en
    Titán (19/8/2026)**: el gate `windEnabled` vuelve a ser aleatorio por nivel (ya no excluye
    Titán) → mientras vas ciego no sabes hacia dónde te empuja y sales de la niebla en un sitio
    inesperado; **la tormenta sigue apagada** en Titán (`storm.setEnabled(false)` tras cada
    `storm.reset`, nuevo API público en `Storm`). Integrado en `Game` (reset en
    constructor/newGame/nextLevel/startDemo, update excluye título, drawSky tras storm, ocultado de
    nave en draw). Validado en PC: `test_pc` **848 checks ALL PASSED** (nuevos `testAtmosphere`:
    `moonHasTitan`, activación por nivel, `hidesShip` dentro/fuera de banda, el patrón de ocultación
    **cambia con el tiempo**, `Game::nextLevel()` → nivel 6 activo con `storm` inactiva),
    `titan_demo 3/5/9 6` (la nave queda oculta 239–379 frames del descenso, variable por seed),
    `demo_sim` 10 seeds en nivel 6 (**40 % win, 0 timeouts**, el autopilot aguanta niebla+viento).
    Sync completado; sketch compila (502 KB, 38 %); subido a placa con `DEMO_LEVEL_FORCE=6` (TEMP)
    para probar en CRT. **Pendiente de prueba en CRT y de afinar deriva/ondulación.**
    **BUG PENDIENTE — los controles pestañean/se pierden en la fase de aproximación con zoom en
    Titán (19/8/2026)**: con `FOG_SCREEN_TOP=100` y el halo recortado, todavía hay momentos en la
    segunda fase (zoom-in) donde los indicadores del HUD **pestañean y hasta desaparecen por
    completo**. Se deja como bug pendiente (la parte es perfectamente jugable aun así). Hipótesis a
    investigar: algo se dibuja **después** del HUD y lo pisa (minimapa / indicadores de aterrizaje /
    glitch de rayo / intro), o el oscilador de niebla aún alcanza la franja HUD en algún estado de
    cámara. Referencia: `Game::draw()` en `esp32Lander/game.cpp` (orden: clear → storm.drawSky →
    atmosphere.drawSky → terreno → nave → minimapa → HUD).
