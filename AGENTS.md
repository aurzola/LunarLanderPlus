# AGENTS.md — Lunar Lander ESP32

## Objetivo

Portar el juego **moonlander.seb.ly** (JavaScript) a un **ESP32** con salida **video compuesto (AV)**
hacia un **CRT blanco y negro con entrada compuesta (video + sonido)**. Controles físicos:
**nunchuck (ángulo) + potenciómetro (potencia) + botón Z del nunchuck (motor)**.

Estética objetivo: arcade / retro auténtico.

## Estado actual del código

- `pyLander/` — original en Python/Pygame Zero. **NO se usa para el ESP32.**
- `cppLander/` — versión C++ previa del juego (base antigua). Reemplazada por el port de moonlander.
- `esp32Lander/` — port C++ std del juego moonlander, **validado en PC** (ver "Port a ESP32").
- `esp32LanderComposite/` — sketch Arduino del ESP32 (ver "Sketch ESP32").
- `esp32LanderVGA/` — port **VGA paralelo** a un segundo ESP32 (ver "Port a VGA").

## Port a ESP32 (ESTADO 4/8/2026)

El juego portado es **moonlander.seb.ly** (JS): física, terreno fijo y nave hexagonal.
Estructura en `esp32Lander/` (C++ std, sin dependencias de hardware):

| Archivo | Contenido |
|---------|-----------|
| `ship.h/cpp` | Nave hexagonal (6 shapes: cuerpo, cabina, patas, toberas). Física, rotación suave, `draw(Renderer&, viewX, viewY, viewScale)`. Explosión al chocar. **Paracaídas (23/8/2026)**: campos `chute`/`chuteOpen`, física de frenado hacia `PARACHUTE_SINK` y dibujo del dosel (ver sección "Paracaídas") |
| `terrain.h/cpp` | Terreno fijo (154 puntos, S=1.35, OY=130), zonas de aterrizaje con multiplicadores y `labelX` (label único por zona), estrellas, colisión línea-segmento |
| `game.h/cpp` | Estados, zoom + minimapa, scoring, `update()` + `draw(Renderer&)` |
| `renderer.h` | Interfaz abstracta (pixel/line/rect/circle/text/flush) |
| `renderer_canvas.h/cpp` | Primitivas compartidas (Bresenham con caso explícito dx=0/dy=0, círculo, rect, fuente 5x7) vía `pixel()` |
| `renderer_pc.h/cpp` | Renderer de validación en PC: framebuffer + PPM (extiende `RendererCanvas`) |
| `main_pc.cpp` | Demo en PC (genera snapshots PPM en `frames/`) |
| `test_pc.cpp` | Tests de validación (asserts) |
| `storm.h/cpp` | **Tormenta eléctrica (solo visual, 11/8/2026)**: rayos (polilínea con jitter + glow `pixelShade`) de brillo moderado y breves, destello único con `fade`. Sin nubes ni flash/lavado de pantalla (se quitaron el 12/8/2026 por efecto estroboscópico en CRT). `reset(level)`, `update(dt, terrain)`, `drawSky` (no-op)/`drawBolts`. Sin física todavía |
| `storm_demo.cpp` | Prueba de visualización en PC: terreno + nave estática (sin física) + tormenta → PPM en `frames/` |
| `moons.h` | **Lunares de nivel (14/8/2026; gravedad 15/8/2026; géiseres 16/8/2026; volcanes 16/8/2026)**: tabla `MoonInfo {name, gravity}` con 8 lunas (LUNA, IO, EUROPA, GANYMEDES, CALLISTO, TITAN, ENCELADUS, TRITON) y `moonIndex(level)`/`moonName(level)`/`moonGravity(level)`/`moonHasGeysers(level)`/`moonHasVolcanoes(level)`/`moonHasRings(level)`/`moonHasTwister(level)` (índice `(level-1) % 8`). **PENDIENTE (19/8/2026)**: personalidad/efectos propios por luna (dificultad, cielo, título). Hoy cada luna solo aporta su gravedad y, según índice, sus efectos ambientales ya integrados: géiseres (Encélado), volcanes (Ío), niebla (Titán), anillos de roca (Ganímedes), torbellino (Tritón). Faltan efectos propios para LUNA, EUROPA y CALLISTO, y queda por definir la dificultad y el cielo/título por luna. Se ampliará en ramas posteriores |
| `geysers.h/cpp` | **Géiseres de Encélado (16/8/2026, rama `moon-flavor`)**: activos en niveles de Encélado (`moonHasGeysers(level)` → `moonIndex==6`, i.e. nivel 7, 15, 23…). `reset(level, terrain)` coloca `GEYSER_VENTS=5` respiraderos en las bases de las laderas junto a los pads (`GEYSER_VENT_OFFSET=14 u`); erupcionan cíclicamente (burst `GEYSER_BURST=3 s` + pausa `GEYSER_GAP_MIN..MAX=3..9 s`, desincronizados) emitiendo partículas en arco y una **columna cónica** con doblez en S (alto `GEYSER_SPOUT_H=40 u`). **Física térmica**: `inPlume(x,y)` detecta el chorro (distancia horizontal ≤ `GEYSER_RADIUS=8`, entre suelo y `GEYSER_PLUME_H=40`); `Game::update()` aplica `ship.velY -= GEYSER_PUSH=0.00025` (~50 % de la gravedad de Encélado). API tests: `ventCount()`/`ventX(i)` |
| `geyser_demo.cpp` | Prueba de visualización en PC: terreno generado + géiseres → PPM en `frames/` (selftest `active` + `maxAlive>0`) |
| `volcanoes.h/cpp` | **Volcanes de Ío (16/8/2026, rama `moon-flavor`; validado en CRT 17/8/2026)**: activos en niveles de Ío (`moonHasVolcanoes(level)` → `moonIndex==1`, i.e. nivel 2, 10, 18, 26…). `reset(level, terrain)` escanea el terreno (muestreo cada 6 u, desnivel `VOLCANO_MIN_DROP=14 u` sobre `VOLCANO_FLOW_LEN=60 u`, sin pisar zonas landable) y coloca `VOLCANO_VENTS=4` volcanes en laderas descendentes. Flujo de lava (línea brillante 1 px sobre la superficie, pulso `0.85+0.15·sin(t·2+phase)`) + erupciones casi continuas (destello radial + partículas en arco, desvanecidas 220→40). **Mecánica de lava**: `computeLava()`/`clampFlows()` calculan los **rangos reales** que cubren la plataforma (`LavaRange {x1,x2}`, franja segura `VOLCANO_SAFE_STRIP=8 u` central; el pad nunca queda 100 % cubierto); `landOnLava(x1,x2)`. **Cualquier colisión sobre lava quema** → final `"YOU BURNED" / "LAVA DESTROYED THE SHIP"`. API tests: `active()`, `volcanoCount()`, `particlesAlive()`, `lavaRangeCount/X1/X2`, `landOnLava` |
| `volcano_demo.cpp` | Prueba de visualización en PC: terreno generado + volcanes → PPM en `frames/` (selftest `active` + `maxAlive>0`; nivel por defecto 2 = Ío) |
| `atmosphere.h/cpp` | **Atmósfera de Titán (19/8/2026; rediseño 22/8/2026)**: activa en niveles de Titán (`moonHasTitan(level)`, `moonIndex==5`, i.e. nivel 6, 14, 22, 30…). `reset(level)` + `update(dt)` + `drawSky`. **Niebla (rediseño 22/8/2026)**: adopta el mismo estilo de Ganímedes/rings — `FOG_BAND_COUNT=3` **bandas elípticas concéntricas** (`centerAt(i,x)` = arco `sqrt(1-t²)` + deriva vertical viva) con **degradado gaussiano por LUT de 256 entradas** (creado una vez, no expf por píxel) y `FOG_BRIGHT=44`. **`hidesShip(x,y)`** oculta la nave al cruzar cada banda. Halo sobre la silueta + clamp `FOG_SCREEN_TOP` para no invadir el HUD. **Física**: arrastre `velX *= ATMOS_DRAG=0.9992` + corriente descendente `velY += ATMOS_DOWN=0.00008`. Viento reactivado; tormenta apagada (`storm.setEnabled(false)`). Ver WORKLOG #20 |
| `titan_demo.cpp` | Prueba de visualización en PC: terreno generado + atmósfera → PPM en `frames/` (nivel por defecto 6 = Titán) |
| `rings.h/cpp` | **Anillos de roca de Ganímedes (rediseño 22/8/2026; banda única + gradiente 9/8/2026)**: activos en niveles de Ganímedes (`moonHasRings(level)`, `moonIndex==3`, i.e. nivel 4, 12, 20, 28…). **Una sola banda** (`RING_COUNT=1`, `RING_CY=420`, `RING_DRIFT=-5`) con **gradiente de densidad**: mitad superior = 30 rocas pequeñas decorativas (radio 1.2–3.8, huecas, 4–8 vértices, sin colisión) en `[-RING_Y_JITTER, 0]`; mitad inferior = 24 rocas grandes peligrosas (radio 5.0–13.0, rellenas con dither, 4–8 vértices, colisionan con `RING_ROCK_HIT·size + RING_SHIP_RADIUS`) en `[0, +RING_Y_JITTER]`. Dispersión vertical `RING_Y_JITTER=38` (banda apretada). `RING_GAP_MIN=34` garantiza paso. `RING_ROCK_HIT=0.55`, `RING_SHIP_RADIUS=6.5`. Pequeñas con X aleatorio (máxima entropía), peligrosas en grilla con jitter ±38%. Elipse `RING_ELLIPSE_CX=400/RAD=520`, arco `CURVE_A=55`. `draw(r,t,vx,vy,vs)` sin frame-skip (1 sola banda, ~54 rocas). **Zoom en Ganímedes (9/8/2026)**: doble trigger — al entrar en la banda (`posY` en `[bandTop, bandBot]`) → zoom-in (2x), y también al bajar cerca del suelo (`alt < ZOOM_IN_ALT=200`) → zoom-in de acercamiento final. `RING_FOG_BRIGHT=0` (niebla desactivada). API: `rocksInRing()`/`rockVisible(t,i,x,y)`/`rockDanger(i)`/`hitsShip(...)`/`centerBandY(t,x)`. Colisión con rocas grandes → `"YOU CRASHED" / "STRUCK BY ORBITAL DEBRIS"`. Ver WORKLOG #21, #31 |
| `rings_demo.cpp` | Prueba de visualización en PC: terreno generado + anillos → PPM en `frames/` (selftest `active` + `rocksVisible>0`; nivel por defecto 4 = Ganímedes) |
| `twister.h/cpp` | **Torbellino de nitrógeno de Tritón (rama `twister-circ`; física v5 + dibujo de resorte/cola 21/8/2026)**: activo en niveles de Tritón (`moonHasTwister(level)`, `moonIndex==7`, i.e. nivel 8, 16, 24…). `reset(level, terrain)` coloca un vórtice que **deambula** por el mundo (`TWISTER_DRIFT_SPEED=8 u/s`, rebota en `[40,760]`) con `strength` aleatoria y giro `swirl` ±1. **Física v5 (física real, no se toca en dibujo)**: `apply(ship,terrain,stickDeg)` se hookea en `Game::update()`; al cruzar `TWISTER_RADIUS=150` captura la nave **cabalga la pared del embudo cónico** (`coneR` = radio del cono a la altura h: `TWISTER_BASE_HALF` abajo → `TWISTER_TOP_HALF` arriba) con **zig-zag en onda triangular** `triWave(swirlAngle_)` que se **cierra al descender** (`posX = cx + dir·amp·tri`, `amp` con ease `TWISTER_CAPTURE_RAMP`), gira con `TWISTER_SPIRAL_RATE=200 °/s` y desciende `velY = TWISTER_DESCENT=45·strength`. **Giro continuo**: `rotation = wobble(t) ±60°` + joystick con autoridad reducida (cap ±80). **Escape físico por profundidad** (`depth=1−h/HEIGHT`): empuje radial sostenido > `escThr` y velocidad saliente > `escVel` durante `TWISTER_ESCAPE_TICKS` → la nave sale **lanzada** (`TWISTER_FLING`+`TWISTER_SPIN_KICK`) y el grip queda off hasta salir del radio. **Cualquier contacto con el suelo estando `captured()`** → final `"YOU CRASHED" / "TWISTER SMASHED THE SHIP"`. La tormenta y el viento se apagan en Tritón. **Dibujo (21/8/2026, diseño propio nuevo, física intacta)**: en `draw(r,terrain,viewX,viewY,viewScale,zoomedIn)` un **resorte / cola de cerdo en espiral** que **arranca fino en el suelo y se ensancha hacia arriba** (`r = 4+30·tt`) — **2 espirales** en vista normal y **3** en zoom-in (`zoomedIn`, además con **5 vueltas** vs 7 para separarlas). Cada espiral es una **hélice discontinua**: segmentos rotos por gaps pseudo-aleatorios (`prand`, determinista por frame con `phase_`), jitter radial, trazo fino + una línea tenue al lado (2/3 brillo), y **brillo que desvanece al fondo de la bobina** (`front = 0.5+0.5·cos(ang)`) para dar giro 3D. **Partículas (21/8/2026)**: motas brillantes que **viajan descendiendo por las espirales** (wrapping con `t_·speed`), más numerosas, rápidas y como **blobs con estela** en zoom (30, blob 3-4 px + estela) vs puntos finos en normal (12). Labio superior tenue, motas de giro y falda de polvo pequeña abajo. API tests: `active()`/`coreX()`/`coreY(terrain)`/`strength()`/`captured()`/`justEscaped()`. `draw` recibe `zoomedIn`. Ver WORKLOG #22 |
| `twister_demo.cpp` | Prueba de visualización en PC: terreno generado + torbellino → PPM en `frames/` (selftest `active`; la nave entra en el radio y es succionada; nivel por defecto 8 = Tritón) |
| `tanker.h/cpp` | **Nave cisterna aérea con repostaje en vuelo (ronda 5b refine 3: mini-juego de docking 8/8/2026)**: aparece en niveles ≥2 con `TANKER_CHANCE_PERCENT` (70 %) **y solo si el combustible está bajo** (`fuel < FUEL_MAX·TANKER_FUEL_FRACTION=0.5`; demo/attract `force=true` lo ignora). Aeronave **zeppelin**: globo elargado `lineShade`, góndola, aletas, faro, motor. Flota a `TANKER_HOVER_ALT=340 u` (dock ~307 u, entre `ZOOM_IN_ALT=200` y `ZOOM_OUT_ALT=350`) con deriva ±40 u @ 9 u/s y bob ±3 u. En Titán `baseY=TANKER_TITAN_Y=85`; excluida de Ganímedes (`moonHasRings`). Docking **probe-and-drogue** con **una manguera** (`TANKER_HOSE_LEN=20 u`) y cesta inferior (`drogueX/Y = bodyX+sway / portY+HOSE_LEN+sway`, sway ±3 u @ 1.6 rad/s). **Mini-juego de mantenimiento**: tras engancharse hay que mantener el probe dentro de `TANKER_DOCK_TOL_X=8`/`TANKER_DOCK_TOL_Y=5` durante `TANKER_DOCK_LOCK_TIME=1.0 s` para que fluya el combustible; salir de `TANKER_DOCK_BREAK_TOL_X=12`/`TANKER_DOCK_BREAK_TOL_Y=8` durante `TANKER_DOCK_BREAK_TIME=0.4 s` rompe el acople (`breakAway`). El joystick controla el offset horizontal del probe (`ship.velX` → nudge, muelle de centrado suave); el motor (Z) desengancha. Repostaje incremental `TANKER_REFUEL_RATE=200/s` hasta `FUEL_MAX`. Colisión con el casco = destrucción mutua. **Visual**: cesta triangular en vista general / tronco de cono invertido en PiP; probe con varilla fina + **flecha sólida triangular** (sin círculo brillante en punta); **anillo de estado** en el PiP (verde/amarillo/rojo); zeppelin dibujado a `viewScale·1.6` **solo en zoom-out** (hitbox sin cambios). **Visual plutónico (22/8/2026)** (rama `fuel-tanker`): globo elipsoide relleno por filas con `lineShade` (brillo 120→160) + contorno 255 + arco de resalte superior (200) + **franja oscura a media altura** (3 filas, brillo 60→80) que reemplaza a la antigua línea central resaltada; **góndola-cabina aerodinámica** con contorno `\___|` (nariz diagonal tocando el casco, panza plana, popa vertical), **rellena** como el globo (trapezoide con degradado) y **sin cables** de soporte (antes parecía colgando). Demo AI corrige offset durante dock. Ver WORKLOG #29 |
| `parachute_demo.cpp` | Prueba de visualización en PC (23/8/2026): terreno generado + nave con **paracaídas** en 5 estadios de inflado + rampa con física real (brake→sink) → PPM en `frames/` (selftest `open=1.00 velY≈sink`) |

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
  **En Ganímedes (`moonHasRings`)**: zoom de banda al entrar al anillo de rocas (`posY` en
  `[bandCY±jitter±30]`) + zoom de altitud al bajar (`alt<200`); `setZoom(true, 2.0f)` (2× en vez
  de 5×). Histeresis de ±30 u en entrada / ±60 u en salida de la banda para evitar flickering.
- **Viento (física desde 9/8/2026)**: se activa desde `WIND_START_LEVEL` (**4**) y **aleatoriamente
  por nivel** (50 % de probabilidad por nivel ≥ 4, `WIND_CHANCE_PERCENT`); ráfagas y dirección
  aleatorias. **Twister ↔ viento excluyentes (20/8/2026)**: `windEnabled` lleva además
  `&& !moonHasTwister(level)` en los 3 sitios (`newGame`/`nextLevel`/`startDemo`) → en niveles de
  Tritón **no hay viento** (ni streaks ni polvo; `spawnWind`/`spawnDust` salen antes con
  `windEnabled=false`). Ráfagas: `windStrength = WIND_MIN(0.35) + (1-WIND_MIN)*gust`,
  `gust = 0.5+0.5*sin(windPhase*0.6)`; `windDir` (+1/-1) cambia cada 8–20 s. **Física**:
  `Ship::update()` aplica `velX += windDir·windStrength·WIND_ACCEL` por tick (sin `GAME_DT`);
  `WIND_ACCEL=0.0004` ≈ 80 % de la gravedad a ráfaga máxima → mantener rumbo contra el viento cuesta
  empuje lateral (y combustible). Tres efectos visuales:
  - **Trazos de fondo**: `WIND_STREAK_COUNT=18` guiones en una banda de cielo dinámica que sigue la
    vista (reubicación al azar + wrap en `[0, tileWidth]` → visibles en normal y zoom). Guión de
    1 px (luma 180, longitud `(WIND_STREAK_MIN=20..WIND_STREAK_MAX=60)·viewScale·f1`) + 2 estelas
    atenuadas (`pixelShade` 90/45); en aproximación (`altFactor > WIND_FORK_ALT=0.5`, i.e. `alt<125`)
    se vuelve **fork de 2 líneas delgadas** con longitudes `f1`/`f2` (2da rama desvanecida
    `180·forkIn`). Cantidad según altitud: `N = max(WIND_STREAK_MIN_VISIBLE(3), count·altFactor)`
    con `altFactor = 1-alt/WIND_ALT_MAX(250)`; en zoom `N≤8`. La intro de nivel recalcula
    `ship.altitude` y el box real para que los trazos se vean idénticos al anunciar `LEVEL N`.
  - **Polvo en el suelo**: `DUST_COUNT=24` motas (`DustParticle {x,y,vy,life}`) pegadas al terreno
    (`terrainYAt()`, 3–35 u sobre la superficie) que derivan con el viento; spawn simétrico
    `ship.posX ± DUST_RANGE=120`, reciclado a los `DUST_LIFE=5 s`. Brillo con rampa cuadrática
    `b = 45+205·t²·windStrength·flick` (casi invisible hasta llegar al landing spot, donde dibuja un
    **cúmulo en cruz de 5 px** con salto hacia arriba). `landingProximity()` público (Game).
  - **Llama desviada**: `Ship::draw()` inclina la llama en la dirección del viento
    (`bend = windDir*windStrength*flameLen*sc*0.7`, base inclinada ×0.3) + rastro atenuado
    (`pixelShade` 90→0). `Ship` recibe `windStrength`/`windDir` desde `Game::update()` cada frame
    (0 en niveles < `WIND_START_LEVEL`).
  HUD: `WIND nn>` (o `<`) en `(250,62)`, debajo de `G` (los avisos `LOW FUEL`/`TOO FAST` se
  desplazan según haya WIND; ver Sketch ESP32); glifos `<` y `>` añadidos a la fuente 5x7.
  Config: `WIND_START_LEVEL=4`, `WIND_CHANCE_PERCENT=50`, `DEMO_LEVEL_FORCE=0` (el demo elige nivel
  al azar `1..DEMO_MAX_LEVEL`).
- **Paracaídas dirigible (23/8/2026, one-shot por nivel)**: se despliega con el **botón Start**
  en vuelo (no interfiere con arrancar el juego porque en `STATE_PLAYING` despliega el chute y en
  `STATE_WAITING` arranca la partida). **Motor permitido mientras está abierto**
  (variante B): sin motor = aterrizaje hard (sin bonus de fuel), un toque de motor al final (flare)
  da el perfecto (+50 fuel). La apertura se **ignora bajo `PARACHUTE_MIN_ALT=80`** con aviso
  `TOO LOW` parpadeante (el dosel no abriría a tiempo).
  - **Física** (`Ship::update()`, campos `chute`/`chuteOpen`): el dosel se infla en
    `PARACHUTE_OPEN_TIME=0.5 s`; frena la caída hacia `PARACHUTE_SINK=0.09` **solo si cae más rápido**
    (`velY += (SINK-velY)*0.15*chuteOpen - gravity`; el `-gravity` hace que la velocidad terminal sea
    exactamente el sink). El flare puede bajar de sink (perfecto posible). **Dirigible**: con la vela
    el stick ya no rota — `Game::update()` llama `setTargetRotation(0)` (auto-nivelado) y desvía
    lateralmente `ship.velX += sin(input.angle)*PARACHUTE_STEER*chuteOpen` (`PARACHUTE_STEER=0.0012`);
    tope horizontal `PARACHUTE_DRIFT_MAX=0.30`. El viento actúa como **vela ×2**
    (`PARACHUTE_WIND_GAIN=2.0`). Un rayo en modo vela revuelve el steering (en vez de la rotación)
    durante `STORM_CONTROL_LOSS`. `crash()` y `reset()` limpian `chute`/`chuteOpen` (un nivel = un uso).
  - **Visual** (`Ship::draw()`): paquete plegado 4×2 px sobre el casco mientras disponible;
    desplegado = **cúpula** en arco elíptico (vértice centrado arriba, rin en los bordes) +
    **borde festoneado** (4 chevrons) + 5 **líneas de suspensión** (`lineShade` 120) + **relleno de
    tela** por filas (`shadedLine`, brillo 30→15, media elipse `sqrt(1-t²)`). Todo escala desde el
    top del casco (dy=-5) con `chuteOpen` en coordenadas locales del ship; helper `shadedLine(r, cx,
    y, halfW, b)` (línea horizontal con `pixelShade`).
  - **HUD**: `CHUTE` sólido en `(22,220)` mientras disponible, parpadeante desplegado,
    `TOO LOW` parpadeante 1.2 s al rechazar. Línea de título `START: PARACHUTE (1/LEVEL)`.
    El **demo no despliega el chute** en v1 (herramienta solo del jugador).
  - Config: `PARACHUTE_OPEN_TIME=0.5f`, `PARACHUTE_SINK=0.09f`, `PARACHUTE_MIN_ALT=80.0f`,
    `PARACHUTE_STEER=0.0012f`, `PARACHUTE_DRIFT_MAX=0.30f`, `PARACHUTE_WIND_GAIN=2.0f`. Nota en
    `config.h` sobre la variante A (motor apagado con vela) como opción futura.
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

- **Nunchuck (joystick X) → ángulo**: calibración adaptativa + **modo de calibración manual
  (20/8/2026)**: `calibrateStick()`
  en `setup()` promedia 40 lecturas → `stickCenterX`. `readStickAngle()` usa dead zone ±10 y
  desviaciones por lado (`stickLeftDev`/`stickRightDev`, mín 40, observadas con EMA; el máximo
  observado en boot se conserva) → rampa lineal a `[-PI/2, PI/2]`. Stick izquierdo = giro a la
  izquierda. I2C: **SDA=GPIO21, SCL=GPIO22**, **50 kHz**, `Wire.setTimeOut(100)`, pull-ups
  internos explícitos (`gpio_set_pull_mode`; el core no los activa), dirección `0x52`.
  **Modo de calibración manual (20/8/2026)**: manteniendo **C+Z en el título ~0,5 s** se entra en un
  overlay de calibración (instrucciones + caja sin relleno con el cursor del stick en vivo); se mueve
  el stick por todos los extremos y **C = guardar / START = cancelar**. `confirmCalibration()` deriva
  centro `(min+max)/2` + desviaciones por eje y las **persiste en NVS** (`Preferences` `jsCal`);
  `loadCalibration()` las restaura al boot. Con `useFixedCal=true` las desviaciones dejan de adaptarse
  por EMA (la adaptativa se desviaba si no mantenías el stick al centro al encender → "no responde
  bien").
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
  **DESHABILITADO (19/8/2026, `POT_DISABLED=1` en el `.ino`)**: el ADC del pot era ruidoso y al
  moverse >120 cuentas retomaba el control mientras subías con C+stick (el PWR "se reseteaba" a un
  valor menor o cero). Con el flag, la potencia se fija **solo con C + stick**; el pot no se lee
  (`readPotLevel`/`lowPass`/`potAtCycle` quedan bajo `#if !POT_DISABLED`).
- **C + stick del nunchuck → pasos de potencia (5/8/2026; rediseñado 17/8/2026)**: mientras se
  mantiene **C**, mover el stick **arriba sube / abajo baja** el `powerLevel` de forma continua
  (no pasos discretos), con rampa `PWR_STICK_RATE=0.008`/tick (~2 s de barrido completo con el
  stick a tope). `readStickYDev()` devuelve `+` arriba / `−` abajo (invertido: Y raw bajo =
  arriba); calibración adaptativa del eje Y (`stickCenterY=128` inicial, idle 40, dead zone 10).
  Con `POT_DISABLED` una vez activado (`pwrStickActive=true`) **ya no se desactiva** (sin pot que
  retome), así que ajustes sucesivos continúan desde el valor actual. Sustituye al antiguo ciclo
  por pasos `{0,25,50,75,100}%` (`powerStep`).
- **C+Z juntos (23/8/2026)**: mientras ambos están pulsados **se salta el ajuste C+stick de
  potencia** (evita el conflicto). En el título, C+Z mantenido ~0,5 s abre la calibración (ver
  "Entrada"/modo de calibración). **El paracaídas ya no usa C+Z**; ahora lo despliega el **botón
  Start** durante el vuelo (`Game::update()` consume `startPressed` en `STATE_PLAYING`).
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
   (`STICK: ROTATION`, `Z: ENGINE ON/OFF`, `C+STICK: POWER UP/DOWN`, `POT: POWER LEVEL`,
   `START: PARACHUTE (1/LEVEL)`). La línea de crédito
  `Copyright Alex Urzola 2026/Opencode` se dibuja **centrada debajo del título** (`y=40`, con
  `centerText`). El fondo es el de juego
  (estrellas + nave entrando **por la derecha** con deriva lenta a la izquierda
  (`setupTitleShip()`, `velX=-0.35`, `posX=(SCREEN_W-20)/viewScale`)).
- **Demo / attract mode (8/8/2026; niveles al azar 1..12 desde 13/8/2026)**: tras
  `DEMO_START_DELAY=13 s` en el título, `Game::startDemo()` lanza un nivel al azar (1..12;
  `DEMO_MAX_LEVEL=12`) jugado por un **autopilot** (`Game::runDemoAI()`) hacia `demoTargetX/Y`.
  **Control desacoplado (9/8/2026)**: deriva `angle=atan2(aX,aY)`, `thrust=hypot(aX,aY)/THRUST_ACCEL`
  con `aX=(desVX−velX)·0.02 − windDir·windPush` y `aY=(velY−desVY)·0.03` (tope 0.00075), `desVY`
  según fase (0.12 crucero / 0.04 cerca / 0.03 aproximación); `aX` se atenúa cerca del suelo
  (`alt<12 → ×0.6`, `alt<2.5 → ×0.05`) para aterrizar erguido; freno de ascenso si `velY<-0.01`.
  PWR rampea a `DEMO_POWER_RATE=0.4/s`. **A veces gana, a veces pierde** (~50 % "torpes",
  `demoSkill` 0–0.35 y offset hasta ±110 u → aterrizan en la ladera; win-rate ~45 % validado con
  `./demo_sim`). Al terminar muestra el resultado (`CRASH_RESET_DELAY`) y vuelve al título; `DEMO` en
  HUD bajo `PWR` en `(22,62)`; cualquier `startPressed` cancela el demo (`demo=false`).
- **Combustible (5/8/2026)**: **no se recarga entre niveles**; lo consumido queda consumido
  (`ship.fuel` se conserva en `nextLevel()`/`restartLevel()`, que antes lo reiniciaban vía
  `Ship::reset()`). El juego **NO termina al quedarse sin combustible en pleno vuelo**: se puede
  acabar el nivel (aterrizar sin motor). Al aterrizar: si `ship.fuel<=0` (tras el bonus de
  aterrizaje perfecto) → `endGame()` (`OUT OF FUEL`/`GAME OVER` y vuelta a la intro); si hay
  combustible → `nextLevel()`. Aterrizaje perfecto sigue dando +50 (con tope `FUEL_MAX`).
- Video lib: `renderer_esp32` escribe en el framebuffer de `video_get_frame_buffer_address()`.
  El render lo hace la librería (DAC → GPIO25 → RCA del TV). B/N usa luma alta (255).
- Compila validado con `arduino-cli compile --fqbn esp32:esp32:esp32`: ~525 KB flash
  (40% del app slot), RAM 109 KB (33%). **Esquema de partición `no_ota`** (ver "Flash"), app slot de 2 MB.
- Loop: `game.update()` cada 10 ms (acumulador sobre `millis()`); `game.draw(renderer)` por iteración.

## Port a VGA (paralelo, rama `vga-out`)

Segundo ESP32 dedicado que saca el juego por **VGA paralelo** (SVGA 640x480@60). El CRT (compuesto)
queda intacto en su placa. Detalle completo en `docs/PLAN_VGA.md` y cableado en `docs/hardware.md`.

| Archivo | Contenido |
|---------|-----------|
| `esp32LanderVGA.ino` | Igual que el composite (audio, nunchuck, pot, start, loop `GAME_DT`) pero con el driver VGA. `VGA_TEST_PATTERN=1` dibuja **rampa de grises + rejilla + marco** para validar grises/escalado/sync antes de pasar a `0` (juego) |
| `src/renderer_vga.h/cpp` | `RendererVGA : RendererCanvas`. Doble buffer: `game.draw()` pinta en `fbBack` (estático 76.8 KB); `flush()` hace `waitVBlank()` y `memcpy` a `fbFront`. El **framebuffer de video es heap** (`malloc` al inicio de `setup()`, antes de audio/driver — el segmento de DRAM estático no da para dos buffers de 76.8 KB, igual que el composite) |
| `src/esp32lib/` | **ESP32Lib de bitluni** (CC BY-SA 4.0) embebido como fuente (VGA/, Graphics/, I2S/, Tools/); se podaron los drivers VGA no usados. `VGA8BitDACI` (DAC mono) forkeado |
| `src/esp32lib/VGA/VGA8BitDACI.*` | **Fork con frame store externo**: `setFrameStore(store,W,H)` y `allocateFrameBuffer()` mapean las filas del driver dentro del store de 320x240 (en vez de malloc 640x480 = 300 KB, no hay PSRAM). `waitVSync()` bloquea hasta el blanking vertical. `interruptPixelLine()` escala **2x horizontal** (cada 32-bit repite el píxel en las dos muestras) cuando `mode.hRes == 2*frameStoreW` y el `interrupt()` escala 2x vertical (`y>>1` para `vDiv==1`). `init(mode, hsyncPin, vsyncPin, outputPin, voltageDivider)` |
| `src/esp32lib/I2S/I2S_ESP32.cpp` | Retocado para IDF5 del core Arduino 3.3.10: `#include "driver/dac.h"` (compat, `-iwithprefixbefore driver/deprecated` lo resuelve); `rtc_clk_apll_enable(bool)` + `rtc_clk_apll_coeff_set(odir, sdm0, sdm1, sdm2)` (firma nueva) |

- **Pines**: video **GPIO25** (DAC1) → divisor 270Ω×3 en paralelo a R/G/B del VGA (monocromo),
  HSYNC **GPIO32**, VSYNC **GPIO33**. Audio (LEDC, GPIO26) sin cambios. El board VGA es el **segundo
  ESP32**; nunchuck + pot + start se cablean igual en esta placa (el `CONTROLS_WIRED=1`).
- **Memoria**: compila con `no_ota` — 552 KB flash (42 %), RAM estática 112 KB (34 %), heap ~216 KB
  (de los que 76.8 KB van al `fbFront`). Los samples de audio se leen de flash desde el ISR, así que
  el heap queda entero para los buffers de video (misma jugada que el composite).
- Loop igual que composite: `game.update()` cada 10 ms; `game.draw()` → `flush()` espera el blanking
  (el ISR del driver nunca ve un frame a medias).

## Controles físicos decididos

| Control | Mapeo del juego | Notas |
|---------|-----------------|-------|
| Nunchuck (joystick X) | Ángulo de la nave `[-PI/2, PI/2]` → rotación `[-90°, +90°]` | I2C GPIO21/GPIO22; dead zone ±10 |
| Potenciómetro A | Nivel de potencia de motores (thrust level 0.0–1.0) | **DESHABILITADO** (`POT_DISABLED=1`); ver "Entrada" |
| Nunchuck (botón Z) | Encendido/apagado del motor (thrust = botón ? powerLevel : 0) | `NUNCHUCK_TRIGGER_Z`; alternativo C |
| Nunchuck (botón C) | Mientras se mantiene C, el stick sube/baja `powerLevel` (continuo, rampa 0.008/tick) | único fijador de PWR con `POT_DISABLED` |
| Botón (GPIO13) | Inicio / reinicio de partida | Equivale a tecla "P"; **sin autostart** (espera el botón) |

El motor se enciende/apaga con el botón Z del nunchuck (como el resorte del gatillo: soltado =
motor apagado). La potencia se fija **solo con C + stick** (desde 19/8/2026, `POT_DISABLED=1`):
el pot quedó deshabilitado porque su ADC ruidoso reseteaba el PWR a un valor menor o cero mientras
subías con C+stick.

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

**Board VGA (segundo ESP32)**: los mismos GPIO21/22/34/13/26 (nunchuck, pot, start, audio) + video
VGA por **GPIO25** (DAC1 → divisor **270Ω×3** en paralelo a R/G/B, blanco 0.717 V con 75 Ω del
monitor), HSYNC **GPIO32**, VSYNC **GPIO33**. El `100/100/220` del ejemplo de bitluni NO vale en
mono (recorte + tinte); ver `docs/hardware.md`.

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
  (one-shot) + **voz de quemado** (17/8/2026, one-shot de 4 s) y escribe el duty directo al registro
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
- RAM: **las muestras se leen desde flash (PROGMEM mapeado) directamente en el ISR** (desde
  20/8/2026). Motivo: el doble buffer de video (`fbShadow` 76.8 KB estático) dejaba el heap tan
  partido que el FB de video de 76.8 KB no cabía junto a ~116 KB de audio en RAM (crash
  `StoreProhibited`/assert de video). Como el juego **no escribe flash en runtime**, la caché de
  datos en el ISR es segura; solo el beep (8 KB) se genera en RAM. El suavizado del tono del motor
  (antes low-pass de una pasada sobre la copia) ahora es un **one-pole IIR entero por muestra en
  el ISR** (`thrustPrev += (s-thrustPrev)>>1`, alpha ~0.5).
- Disparo en el `.ino`: transición a `STATE_CRASHED` → explosión; `thrustBuild` durante
  `STATE_PLAYING` → motor; `game.windEnabled` (viento activo en el nivel y jugando) → viento;
  `game.storm.takeNewBolt()` → rayo.
- **Suavizado del sonido (12/8/2026)**: el volumen de motor y viento usa **rampa de ataque/release**
  con easing **entero** (`envEase`, `ENV_DIV=24`) en el ISR (sin FPU: punto flotante en el IRAM ISR
  del núcleo Arduino → `LoadProhibited`/reset) para que entren/salgan sin clic, y el tono del motor
  se **suaviza con un low-pass** (alpha 0.45) al copiarlo a RAM. El **viento se mantiene sutil**
  (`WIND_GAIN=140`, tope `WIND_MAX=160` sobre 256) y **proporcional a la velocidad del viento**
  (`setWind(windStrength)`). El rayo es un one-shot (crack + trueno) al formarse cada rayo.
  Validado: `test_pc` 50 checks, sketch compila 503 KB / RAM 7%, monitor serial estable (sin reset).
- Debug (serial): `debugBeep()` emite un pitido 440 Hz (0.5 s) al arrancar para confirmar el
  audio; `debugIsrCount()` imprime `[audio] isr=%u` 1×/s (~16156 ISR/s → 16 kHz reales).
- Cableado: **GPIO26 → condensador de acople en serie (1–10 µF) → RCA blanco del TV**
  (quita el DC; lógica de 3.3 V). Verificado con parlante + amplificador.

## Decisiones de arquitectura / convenciones

- Framework: **Arduino (arduino-esp32)** salvo que el usuario decida ESP-IDF.
- Port de **moonlander.seb.ly**: física y terreno del original JS, con nave hexagonal.
- Loop fijo con `millis()`, `GAME_DT=0.01`; el ritmo de video lo maneja la librería
  (`video_wait_frame()`), sin VSYNC explícito en el juego.
- Dibujado: interfaz `Renderer` (pixel/line/rect/circle/text/flush). `RendererCanvas` comparte
  las primitivas; PC y ESP32 implementan `pixel()`.
- `esp32LanderComposite/src/` y `esp32LanderVGA/src/` son **copia** de `esp32Lander/` (mismas
  fuentes); mantener en sync con `bash sync.sh` al cambiar física/dibujado.
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
  Paracaídas: `./parachute_demo <seed>` (terreno + nave con dosel en 5 estadios + rampa con
  física real, PPM en `frames/`; selftest `open=1.00 velY≈sink`).
- Compilar sketch composite: `arduino-cli compile --fqbn esp32:esp32:esp32 esp32LanderComposite/esp32LanderComposite.ino`.
- Compilar sketch VGA: `arduino-cli compile --fqbn esp32:esp32:esp32 --build-property build.partitions=no_ota esp32LanderVGA/esp32LanderVGA.ino`.
- Subir: `arduino-cli upload --fqbn esp32:esp32:esp32 --port /dev/ttyUSB0 ...` (o Arduino IDE).
- Sync PC↔sketches: `bash sync.sh` (copia las fuentes compartidas de `esp32Lander/` a
  `esp32LanderComposite/src/` y `esp32LanderVGA/src/` y verifica que queden idénticas; no toca
  los archivos propios de cada sketch: `video.*`, `renderer_esp32`, `renderer_vga`, `esp32lib/`,
  `audio*`, `nunchuck*`). Para un diff puntual: `diff esp32Lander/<f> esp32LanderComposite/src/<f>`.

## Flash / memoria

Resumen: ESP32 Dev Module flash 4 MB (QIO 80 MHz), core 3.3.10, esquema de partición `no_ota`
(2 MB app). Sketch ≈ 525 KB (40% del app slot), RAM 109 KB (33%). Layout de particiones y cómo forzar
`no_ota` al flashear (vs `arduino-cli upload` que revierte a `default`): `docs/hardware.md`.
## Proceso de trabajo / WORKLOG

Historial completo por ítem (changelog): **`docs/WORKLOG.md`**. Aquí solo quedan los
pendientes y bugs activos:

- **Pendiente de prueba en CRT (gráficos/efectos)**: nunchuck (#8). Anillos de Ganímedes (#21) y atmósfera de Titán (#20) ya rediseñados (bandas elípticas concéntricas, sin niebla en Ganímedes), validados parcialmente. Vueltas en proceso: anillos de Ganímedes + niebla de Titán (rama `twister-circ`, 22/8/2026). Torbellino de Tritón (#22): dibujo de resorte + partículas validado en CRT (21/8/2026).
- **PENDIENTE (feature)**: boca de volcán con patrón "U" aserrado (#18, ver `volcanoes.cpp` `Volcanoes::draw()`).
- **PENDIENTE (idea, audio, para luego)**: **voz de mission control** oída a través de "canal de radio" (efecto de voz distante, filtrada tipo radio FM/arrancada a KM de distancia). Sin diseño cerrado; se integraría en la sección Sonido (probablemente con la voz como un one-shot largo o muestras cortas por frase, por el estilo de los one-shots actuales: explosión/quemado/rayo). Requeriría además del pipeline ESP32. NO implementado; decidir cuando se aborde.
- **PENDIENTE (idea, arquitectura, para luego)**: **usar el otro core (0)**. Hoy Arduino-esp32 ya corre sobre FreeRTOS: `setup()/loop()` son una tarea pinneada al **core 1**, el core 0 va mayormente idle (esp_timer; WiFi sólo si se usara). El audio actual es un ISR de timer gptimer a 16 kHz que mezcla en IRAM y escribe el duty del LEDC, e interrumpe al core del juego unos microsegundos por muestra. Posible mejora legítima: mover la **mezcla de samples** (thrust+wind+explosión+voz) a una **tarea dedicada en el core 0** con ring buffer, dejando al ISR solo `pop + escribir duty`; así el core 1 no paga nada de audio y se podría subir la calidad (más canales, filtros, ~32 kHz). **Precaución**: a 16 kHz la muestra hay que entregarla cada 62.5 µs — un ISR la garantiza, una tarea normal no → ISR para el muestreo, core 0 para la mezcla pesada. **NO arregla el lag de dibujo** (p.ej. Ganímedes), que es CPU de render, no audio. NO implementado.
- **BUG PENDIENTE**: ~~los controles del HUD pestañean/se pierden en la fase de aproximación con zoom en Titán (#20)~~ → **RESUELTO (20/8/2026)**: el parpadeo/borrado parcial de minimapa, indicadores y nave en la 2ª etapa (zoom) era **tearing de framebuffer único** (el DMA de la librería aquaticus escanea el FB mientras `draw()` escribe; en zoom el frame excede la ventana de blanking). Fix: **doble buffer** en el `.ino` — `fbShadow[76800]`, `RendererESP32` pinta en el shadow, y tras `video_wait_frame()` se `memcpy(shadow→videoFB)` durante el blanking; el siguiente `draw()` pinta en el shadow durante el campo completo. El DMA solo ve frames completos (  afectaba a cualquier luna con efectos en zoom, no solo Tritón). El doble buffer rompía la RAM
  (el FB de video ya no cabía); se resolvió pasando las muestras de audio a flash (ver Sonido).
  RAM 101908 B (31%), arranque limpio verificado por serial. Pendiente re-probar en CRT.
- **BUG PENDIENTE (demo, #23)**: a veces el juego no se renderiza completo por la **izquierda** de la pantalla — queda un espacio sin pintar o sin usar, notado principalmente en el auto-demo/attract mode. Hipótesis a investigar: la librería aquaticus escanea el framebuffer por raster esta vez; posible offset de inicio de línea horizontal (back porch del CRT) o un rect/borrado que no cubre el margen izquierdo en ciertos estados. Ver WORKLOG #23.
- **TEMP**: `RING_FOG_BRIGHT=0` desactiva la niebla de las bandas de Ganímedes (decisión de diseño). `DEMO_LEVEL_FORCE=2` en el sketch ESP32 (demo juega nivel 2 = Ío como showcase de la cisterna); el PC va en `0` (nivel al azar 1..12). `START_LEVEL=1` (partida ordenada desde LUNA). `FOG_SCREEN_TOP=68` (cuadro de limpieza del HUD a la altura de MEM).
