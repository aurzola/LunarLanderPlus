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
- `esp32LanderS3/` — **VERSIÓN PRINCIPAL para CRT B/N (24/8/2026)**: sketch Arduino del ESP32-S3, video compuesto por LCD_CAM+GDMA (ver "Port a ESP32-S3").
- `esp32LanderComposite/` — **DESCONTINUADA (24/8/2026)**: reemplazada por la S3 (se ve y rinde mejor); se conserva como archivo histórico, ya no recibe sync ni uploads (ver "Sketch ESP32 clásico — DESCONTINUADO").
- `esp32LanderVGA/` — port **VGA paralelo** a un segundo ESP32 (ver "Port a VGA").

## Port a ESP32 (ESTADO 4/8/2026)

El juego portado es **moonlander.seb.ly** (JS): física, terreno fijo y nave hexagonal.
Estructura en `esp32Lander/` (C++ std, sin dependencias de hardware):

| Archivo | Contenido |
|---------|-----------|
| `ship.h/cpp` | Nave hexagonal (6 shapes: cuerpo, cabina, patas, toberas). Física, rotación suave, `draw(Renderer&, viewX, viewY, viewScale)`. Explosión al chocar. **Relleno sólido (10/8/2026)**: shapes cerrados (0=ascenso, 1=descenso) se rellenan con `fillPolygon` en gris 140/100 y contorno blanco encima; ventana con `rectShade` 160 + borde. **Polvo de impacto (rama `crash-dust`)**: al estrellarse `initGroundParticles()` levanta `GROUND_PARTICLES_MAX=40` partículas de regolito desde la línea de contacto (distribución 40/40/20 de tamaños punto/`+`/roca 3×3, brillo propio 0.7–1.0), siguen la velocidad del impacto (×2.5 en explosión de combustible), arquean con `GROUND_PARTICLE_GRAV=0.012` **escalada por la gravedad de la luna** (`·gravity/GRAVITY`, 27/8/2026: antes constante 0.018; en Encélado/Tritón el polvo flota ~30 % más) y se desvanecen en `GROUND_PARTICLE_LIFE=70` ticks (ver `Ship::updateExplosion()`). Velocidad de lanzamiento **igualada al rango de los trozos de la nave** (0.08–0.35 u/tick, 27/8/2026: jitter horizontal ±0.35, empuje vertical 0.08–0.33; antes ±1.2 / 0.15–0.9). **Paracaídas (23/8/2026)**: campos `chute`/`chuteOpen`, física de frenado hacia `PARACHUTE_SINK` y dibujo del dosel (ver sección "Paracaídas"). **Disolución por ácido (rama `acid-rain`)**: `dissolve()` desprende las 6 partes secuencialmente (~0.35 s, 5-28 ticks) con deriva en X e Y individual; la nave conserva su inercia (`posX/Y += velX/Y`) mientras las piezas se separan |
| `terrain.h/cpp` | Terreno fijo (154 puntos, S=1.35, OY=130), zonas de aterrizaje con multiplicadores y `labelX` (label único por zona), estrellas, colisión línea-segmento. **Cráter de choque (rama `crash-dust`)**: `setCrater(x, halfW)`/`clearCrater()` guardan un tramo del mundo; `draw()` **recorta la polilínea** en ese tramo (segmento entero dentro se omite, parciales se cortan por interpolación) dejando un **hueco abierto del ancho de la nave** (`CRATER_HALF_W=3.5`, ~7 u) en el punto de impacto — sin relleno ni borde, solo indica que ahí hubo un choque. `Game` lo activa en crash duro y smash de torbellino y lo limpia en `newGame`/`restartLevel`/`nextLevel`/`startDemo`. **Teselado horizontal / wrap (28/8/2026)**: `draw(... bool wrap=true)`, `drawStarField()` y `drawLabels()` teselan el terreno por franjas `k·tileWidth` (loop desde `floor(wxMin/tileWidth)` a `floor(wxMax/tileWidth)`), cerrando la franja finita `[0, tileWidth]` sobre el mundo circular de la cámara en vista normal — elimina las bandas/franjas en blanco de los bordes de pantalla (la cámara ve ~933 u > los ~900 del mundo); `draw()` con `wrap=false` se reserva para el horneado del bgLayer (un solo tile). Compatible con cráteres/rupturas (afectan un solo tile) |
| `game.h/cpp` | Estados, zoom, scoring, `update()` + `draw(Renderer&)` |
| `renderer.h` | Interfaz abstracta (pixel/line/rect/circle/text/flush). `rectShade(x,y,w,h,b)` y `fillPolygon(xs,ys,n,b)` para relleno de polígonos con gris real (10/8/2026) |
| `renderer_canvas.h/cpp` | Primitivas compartidas (Bresenham con caso explícito dx=0/dy=0, círculo, rect, fuente 5x7) vía `pixel()`. `rectShade`: rectángulo sólido con `pixelShade`. `fillPolygon`: scanline fill para polígonos convexos (intersecciones por fila + líneas horizontales sombreadas) |
| `bglayer.h/cpp` | **bgLayer — terreno pre-horneado por nivel (24/8/2026, rama `s3-port`)**: `BgLayer` (buffer vía hook `bgSetAllocator()`; default malloc → falla elegante sin PSRAM), `LayerPainter : RendererCanvas` que pinta al buffer crudo (su `line()` usa barrido vertical por columna para cobertura tras el downsampling ×3), `Renderer::drawLayer` virtual (no-op default; nativa en PC y S3). `Game::bakeBg()` hornea el terreno a transform identidad; re-horneado por `Terrain::revision()` (contador en init/generate/cráteres/rupturas). Estrellas y labels dinámicos (`drawStarField`/`drawLabels`, nuevos métodos públicos de Terrain). Solo vista normal; zoom sigue vectorial. En S3 activo con ps_malloc (A/B en placa: fps neutrales ~58.6, costo en efectos no terreno); composite/VGA nunca activa. Ver WORKLOG #54 |
| `renderer_pc.h/cpp` | Renderer de validación en PC: framebuffer + PPM (extiende `RendererCanvas`). `drawLayer` con **wrap horizontal por módulo** (no clamp, 28/8/2026) para la teselación del mundo |
| `main_pc.cpp` | Demo en PC (genera snapshots PPM en `frames/`) |
| `test_pc.cpp` | Tests de validación (asserts) |
| `storm.h/cpp` | **Tormenta eléctrica (solo visual, 11/8/2026)**: rayos (polilínea con jitter + glow `pixelShade`) de brillo moderado y breves, destello único con `fade`. Sin nubes ni flash/lavado de pantalla (se quitaron el 12/8/2026 por efecto estroboscópico en CRT). `reset(level)`, `update(dt, terrain)`, `drawSky` (no-op)/`drawBolts`. Sin física todavía |
| `storm_demo.cpp` | Prueba de visualización en PC: terreno + nave estática (sin física) + tormenta → PPM en `frames/` |
| `moons.h` | **Lunares de nivel (14/8/2026; gravedad 15/8/2026; géiseres 16/8/2026; volcanes 16/8/2026; lluvia ácida 26/8/2026; terremotos 12/8/2026; **orden de dificultad 25/8/2026, rama `demo-random-progression`**): tabla `MoonInfo {name, gravity}` con 8 lunas (LUNA, IO, EUROPA, GANYMEDES, CALLISTO, TITAN, ENCELADUS, TRITON) y `moonIndex(level)`/`moonName(level)`/`moonGravity(level)`/`moonHasGeysers(level)`/`moonHasVolcanoes(level)`/`moonHasRings(level)`/`moonHasTwister(level)`/`moonHasAcidRain(level)`/`moonHasQuakes(level)`. **`MOON_DIFFICULTY_ORDER` (25/8/2026)**: el juego normal avanza en dificultad por ciclo de 8 — `LUNA → EUROPA → CALLISTO → ENCELADUS → TITAN → GANYMEDES → TRITON → IO` (`moonIndex` devuelve el índice de tabla del slot; `moonSlotOfIndex` es la inversa, usada por `wormholeJump`). Efectos ambientales: géiseres (Encélado), volcanes (Ío), niebla (Titán), anillos de roca (Ganímedes), torbellino (Tritón), lluvia ácida (Europa), terremotos (Callisto). `moonEffectFree()` deja solo LUNA como anfitrión del agujero de gusano (Callisto ahora tiene terremotos). **PENDIENTE (19/8/2026)**: personalidad/efectos propios por luna (dificultad, cielo, título) |
| `geysers.h/cpp` | **Géiseres de Encélado (16/8/2026, rama `moon-flavor`)**: activos en niveles de Encélado (`moonHasGeysers(level)` → `moonIndex==6`, i.e. nivel 7, 15, 23…). `reset(level, terrain)` coloca `GEYSER_VENTS=5` respiraderos en las bases de las laderas junto a los pads (`GEYSER_VENT_OFFSET=14 u`); erupcionan cíclicamente (burst `GEYSER_BURST=3 s` + pausa `GEYSER_GAP_MIN..MAX=3..9 s`, desincronizados) emitiendo partículas en arco y una **columna cónica** con doblez en S (alto `GEYSER_SPOUT_H=40 u`). **Física térmica**: `inPlume(x,y)` detecta el chorro (distancia horizontal ≤ `GEYSER_RADIUS=8`, entre suelo y `GEYSER_PLUME_H=40`); `Game::update()` aplica `ship.velY -= GEYSER_PUSH=0.00025 · (ship.gravity/GRAVITY)` (~50 % de la gravedad local, **escalada por luna desde 16/9/2026**: en Encélado 0.70× el push fijo anulaba 71 % de la gravedad y el autopilot del demo podía quedar flotando; ahora siempre cancela ~50 %). API tests: `ventCount()`/`ventX(i)` |
| `geyser_demo.cpp` | Prueba de visualización en PC: terreno generado + géiseres → PPM en `frames/` (selftest `active` + `maxAlive>0`) |
| `volcanoes.h/cpp` | **Volcanes de Ío (16/8/2026, rama `moon-flavor`; validado en CRT 17/8/2026)**: activos en niveles de Ío (`moonHasVolcanoes(level)` → `moonIndex==1`, i.e. nivel 2, 10, 18, 26…). `reset(level, terrain)` escanea el terreno (muestreo cada 6 u, desnivel `VOLCANO_MIN_DROP=14 u` sobre `VOLCANO_FLOW_LEN=60 u`, sin pisar zonas landable) y coloca `VOLCANO_VENTS=4` volcanes en laderas descendentes. Flujo de lava (línea brillante 1 px sobre la superficie, pulso `0.85+0.15·sin(t·2+phase)`) + erupciones casi continuas (destello radial + partículas en arco, desvanecidas 220→40). **Mecánica de lava**: `computeLava()`/`clampFlows()` calculan los **rangos reales** que cubren la plataforma (`LavaRange {x1,x2}`, franja segura `VOLCANO_SAFE_STRIP=8 u` central; el pad nunca queda 100 % cubierto); `landOnLava(x1,x2)`. **Cualquier colisión sobre lava quema** → final `"YOU BURNED" / "LAVA DESTROYED THE SHIP"`. API tests: `active()`, `volcanoCount()`, `particlesAlive()`, `lavaRangeCount/X1/X2`, `landOnLava` |
| `volcano_demo.cpp` | Prueba de visualización en PC: terreno generado + volcanes → PPM en `frames/` (selftest `active` + `maxAlive>0`; nivel por defecto 2 = Ío) |
| `atmosphere.h/cpp` | **Atmósfera de Titán (19/8/2026; rediseño 22/8/2026)**: activa en niveles de Titán (`moonHasTitan(level)`, `moonIndex==5`, i.e. nivel 5, 13, 21…). `reset(level)` + `update(dt)` + `drawSky`. **Niebla (rediseño 22/8/2026)**: adopta el mismo estilo de Ganímedes/rings — `FOG_BAND_COUNT=3` **bandas elípticas concéntricas** (`centerAt(i,x)` = arco `sqrt(1-t²)` + deriva vertical viva) con **degradado gaussiano por LUT de 256 entradas** (creado una vez, no expf por píxel) y `FOG_BRIGHT=44`. **`hidesShip(x,y)`** oculta la nave al cruzar cada banda. Halo sobre la silueta + clamp `FOG_SCREEN_TOP` para no invadir el HUD. **Física**: arrastre `velX *= ATMOS_DRAG=0.9992` + corriente descendente `velY += ATMOS_DOWN=0.00008`. Viento reactivado; tormenta apagada (`storm.setEnabled(false)`). **Draw order (26/8/2026)**: `drawSky` se dibuja **después del terreno** y antes de la nave, así las bandas de niebla se superponen al terreno (antes el terreno las tapaba). Ver WORKLOG #20 |
| `titan_demo.cpp` | Prueba de visualización en PC: terreno generado + atmósfera → PPM en `frames/` (nivel por defecto 6 = Titán) |
| `rings.h/cpp` | **Anillos de roca de Ganímedes (rediseño 22/8/2026; banda única + gradiente 9/8/2026)**: activos en niveles de Ganímedes (`moonHasRings(level)`, `moonIndex==3`, i.e. nivel 4, 12, 20, 28…). **Una sola banda** (`RING_COUNT=1`, `RING_CY=420`, `RING_DRIFT=-5`) con **gradiente de densidad**: mitad superior = 30 rocas pequeñas decorativas (radio 1.2–3.8, huecas, 4–8 vértices, sin colisión) en `[-RING_Y_JITTER, 0]`; mitad inferior = 24 rocas grandes peligrosas (radio 5.0–13.0, relleno sólido `fillPolygon` a 170 + contorno blanco encima, 4–8 vértices, colisionan con `RING_ROCK_HIT·size + RING_SHIP_RADIUS`) en `[0, +RING_Y_JITTER]`. Dispersión vertical `RING_Y_JITTER=38` (banda apretada). `RING_GAP_MIN=34` garantiza paso. `RING_ROCK_HIT=0.55`, `RING_SHIP_RADIUS=6.5`. Pequeñas con X aleatorio (máxima entropía), peligrosas en grilla con jitter ±38%. Elipse `RING_ELLIPSE_CX=400/RAD=520`, arco `CURVE_A=55`. `draw(r,t,vx,vy,vs)` sin frame-skip (1 sola banda, ~54 rocas). **Zoom en Ganímedes (9/8/2026)**: doble trigger — al entrar en la banda (`posY` en `[bandTop, bandBot]`) → zoom-in (2x), y también al bajar cerca del suelo (`alt < APPROACH_ALT=200`) → zoom-in de acercamiento final. `RING_FOG_BRIGHT=0` (niebla desactivada). API: `rocksInRing()`/`rockVisible(t,i,x,y)`/`rockDanger(i)`/`hitsShip(...)`/`centerBandY(t,x)`. Colisión con rocas grandes → `"YOU CRASHED" / "STRUCK BY ORBITAL DEBRIS"`. Ver WORKLOG #21, #31 |
| `rings_demo.cpp` | Prueba de visualización en PC: terreno generado + anillos → PPM en `frames/` (selftest `active` + `rocksVisible>0`; nivel por defecto 4 = Ganímedes) |
| `twister.h/cpp` | **Torbellino de nitrógeno de Tritón (rama `twister-circ`; física v5 + dibujo de resorte/cola 21/8/2026)**: activo en niveles de Tritón (`moonHasTwister(level)`, `moonIndex==7`, i.e. nivel 8, 16, 24…). `reset(level, terrain)` coloca un vórtice que **deambula** por el mundo (`TWISTER_DRIFT_SPEED=8 u/s`, rebota en `[40,760]`) con `strength` aleatoria y giro `swirl` ±1. **Física v5 (física real, no se toca en dibujo)**: `apply(ship,terrain,stickDeg)` se hookea en `Game::update()`; al cruzar `TWISTER_RADIUS=150` captura la nave **cabalga la pared del embudo cónico** (`coneR` = radio del cono a la altura h: `TWISTER_BASE_HALF` abajo → `TWISTER_TOP_HALF` arriba) con **zig-zag en onda triangular** `triWave(swirlAngle_)` que se **cierra al descender** (`posX = cx + dir·amp·tri`, `amp` con ease `TWISTER_CAPTURE_RAMP`), gira con `TWISTER_SPIRAL_RATE=200 °/s` y desciende `velY = TWISTER_DESCENT=45·strength`. **Giro continuo**: `rotation = wobble(t) ±60°` + joystick con autoridad reducida (cap ±80). **Escape físico por profundidad** (`depth=1−h/HEIGHT`): empuje radial sostenido > `escThr` y velocidad saliente > `escVel` durante `TWISTER_ESCAPE_TICKS` → la nave sale **lanzada** (`TWISTER_FLING`+`TWISTER_SPIN_KICK`) y el grip queda off hasta salir del radio. **Cualquier contacto con el suelo estando `captured()`** → final `"YOU CRASHED" / "TWISTER SMASHED THE SHIP"`. La tormenta y el viento se apagan en Tritón. **Dibujo (21/8/2026, diseño propio nuevo, física intacta)**: en `draw(r,terrain,viewX,viewY,viewScale,zoomedIn)` un **resorte / cola de cerdo en espiral** que **arranca fino en el suelo y se ensancha hacia arriba** (`r = 4+30·tt`) — **2 espirales** en vista normal y **3** en zoom-in (`zoomedIn`, además con **5 vueltas** vs 7 para separarlas). Cada espiral es una **hélice discontinua**: segmentos rotos por gaps pseudo-aleatorios (`prand`, determinista por frame con `phase_`), jitter radial, trazo fino + una línea tenue al lado (2/3 brillo), y **brillo que desvanece al fondo de la bobina** (`front = 0.5+0.5·cos(ang)`) para dar giro 3D. **Partículas (21/8/2026)**: motas brillantes que **viajan descendiendo por las espirales** (wrapping con `t_·speed`), más numerosas, rápidas y como **blobs con estela** en zoom (30, blob 3-4 px + estela) vs puntos finos en normal (12). Labio superior tenue, motas de giro y falda de polvo pequeña abajo. API tests: `active()`/`coreX()`/`coreY(terrain)`/`strength()`/`captured()`/`justEscaped()`. `draw` recibe `zoomedIn`. **Fix flag stale (28/8/2026)**: `twisterCrash` ahora se resetea también en `newGame`/`nextLevel`/`startDemo` (faltaba; un crash de torbellino en Tritón dejaba el flag y un crash posterior en otra luna mostraba `TWISTER SMASHED THE SHIP`). Ver WORKLOG #22 |
| `twister_demo.cpp` | Prueba de visualización en PC: terreno generado + torbellino → PPM en `frames/` (selftest `active`; la nave entra en el radio y es succionada; nivel por defecto 8 = Tritón) |
| `wormhole.h/cpp` | **Agujero de gusano en el cielo (rama `event-horizon`; 2/9/2026 visual, hookup en Game 2026; física en dos zonas 12/9/2026)**: `reset(cx,cy)` activa un vórtice espiral (disco de acreción) que **se traga la nave**. Máquina de fases `WH_IDLE → WH_EMERGING (1.2 s, la espiral gira desde un punto) → WH_ACTIVE (persistente) → WH_SWALLOW (destello) → WH_DYING (0.8 s) → WH_IDLE`. **Dibujo** (`draw(r,viewX,viewY,viewScale)`): 3 brazos espirales **logarítmicos** (`r = OUTER·(CORE/OUTER)^tt`, de `WORMHOLE_OUTER_R=200` a `WORMHOLE_CORE_R=16` u) aplastados en Y (`SQUASH=0.5`) para leer como **elipse** (no círculo), para leer como disco inclinado, polilíneas con jitter `prand` y brillo que **aumenta hacia el núcleo** (`b≈45+185·tt^1.5`); **núcleo oscuro** (`fillPolygon` brillo 0 a 1.4×coreR) que se traga estrellas y las vueltas internas; **anillo fotónico** (`circle` blanco + halo tenue) dejando el interior vacío; ~20 partículas que cabalgan los brazos y migran al núcleo. **Física en dos zonas** (`apply(Ship&)` → bool): fuera de `WORMHOLE_CAPTURE_R=100` (= mitad del radio de acción) el campo es un **empuje radial puro** `a = WORMHOLE_PULL_MAX·(1−d/GRAB_R)` (`PULL_MAX=0.0028` ≈ 1.55× empuje máx) y la nave **puede escapar** acelerando en dirección contraria al centro (colisiones normales, retorna `false`); al cruzar `CAPTURE_R` la nave queda **capturada sin escape posible**: el agujero la hace girar en **vórtice guionado** (como el `pullShip` antiguo) — órbita en espiral hacia el núcleo (`rr = SWALLOW_R + (captureRad−SWALLOW_R)·(1−p)`, `ang = startAng + spin·p·VORTEX_TURNS·TAU`, `VORTEX_T=2.5 s`, 3 vueltas), **se encoge** (`scale` →25 %) y **apunta la nariz al núcleo** (`setTargetRotation(atan2(−cos,sin))` aplanado a ±90°); retorna `true` siempre (el `Game` **salta `checkCollisions()`** durante el vórtice). `d < SWALLOW_R=30` → tragado inmediato. **Hookup en `Game`**: `spawnWormhole(bool force)` en `newGame`/`nextLevel` (nivel ≥ `WORMHOLE_START_LEVEL=2`, chance `WORMHOLE_CHANCE_PERCENT=25`) **solo en lunas sin efecto ambiental** (`moonEffectFree()`, LUNA/CALLISTO) y en cualquier posición del cielo (`cx` aleatorio, `cy∈[150,280]` con clamp `WORMHOLE_SKY_CLEAR` sobre el terreno); `isolateForWormhole()` apaga los demás efectos y el tanque mientras esté presente. Mientras `captured()` el `Game` **congela el zoom** (para no pisar el `ship.scale` que se encoge) y **corta el thrust** del jugador. Al `swallowed()` en `STATE_PLAYING` → **`wormholeJump()`**: luna aleatoria distinta (`level=9+nidx`, `nidx≠moonIndex`), terreno nuevo, respawn entre cielo y terreno **sin intro de nivel** (`introTimer=0`; el nivel arranca ya jugando) con **fade-in de la nave** (`ship.scale` 0→1.5 durante `WORMHOLE_WARP_IN_T=1.2 s`; la rampa corre en el update normal tras `updateView()`, que congela el zoom mientras `warpInT>0` para que `setZoom` no pise `ship.scale`), **fuel conservado**. **Banner de teletransporte (12/8/2026)**: al aparecer en la luna destino se muestra 4 s (`WORMHOLE_RECYCLED_T`, `config.h`) el texto `CONGRATULATIONS,` / `YOU'VE BEEN RECYCLED!` (mayúsculas, dos líneas, centrado en `(90,102)`, **mismo estilo plano que los mensajes de aterrizaje/crash** — `r.text`, sin negrita/fade); `recycledTimer` se arma en `wormholeJump()`, decrece en `update()` y se resetea en `newGame`/`restartLevel`/`startDemo` (getter `recycledBanner()`; test en `test_pc.cpp`). También aparece en el demo (el autopilot también teletransporta). **En el attract demo (10/9/2026; secuencia completa 12/9/2026; **showcase OFF 16/9/2026** `DEMO_WORMHOLE_FIRST=false`): mientras el flag está activo, el **primer nivel del demo SIEMPRE** abre el showcase (`DEMO_WORMHOLE_FIRST`; `startDemo` re-tira el nivel hasta que pueda alojarlo — excluye LUNA nivel 1 y Tritón 8) con wormhole en **cualquier punto aleatorio del cielo** (mismas reglas de colocación que una partida real; `setupDemoTarget()` reposiciona la nave junto al hueco, fuera de la zona de no-retorno pero dentro de la de empuje, y la lanza hacia él → tragada garantizada) y **ningún otro efecto** (viento/tormenta/géiseres/volcanes/atmósfera/anillos/torbellino apagados con `setEnabled(false)`); al `swallowed()` el demo **también hace el teletransporte completo** (`wormholeJump()` + `setupDemoTarget()` re-apunta el autopilot): la nave reaparece en otra luna **sin intro de nivel**, con fade-in mientras ya se juega, y el demo sigue volando ahí hasta aterrizar/estrellarse y solo entonces vuelve al título (`endDemoToTitle()` en `STATE_LANDED`/`STATE_CRASHED`). El `setupDemoTarget()` re-elegido tras un teleport ya no se usa en demo. **Con `DEMO_WORMHOLE_FIRST=false` la demo nunca abre el wormhole en attract** (la rama demo de `spawnWormhole` exige `force=true`), así que cada ciclo muestra una luna/efecto al azar sin wormhole. Draw del wormhole **encima de la nave** (el núcleo oscuro la oculta al tragarse). API tests: `active()`, `captured()`, `phase()`, `swallowed()`, `coreX()`, `coreY()`. **PENDIENTE (audio)**: one-shot de warp en `wormholeJump()` — el teleport ya funciona sin él |
| `acidrain.h/cpp` | **Lluvia ácida de Europa (rama `acid-rain`; 26/8/2026)**: activa en niveles de Europa (`moonHasAcidRain(level)`, `moonIndex==2`, i.e. nivel 3, 11, 19…). `reset(level, terrain)` coloca `ACID_RAIN_CELLS=3` celdas de tormenta que derivan (`ACID_CELL_DRIFT=12 u/s`, rebote en `[ACID_CELL_RADIUS=90, worldW−RADIUS]`). Lluvia visual: `ACID_RAIN_STREAKS=26` rayas oblicuas por celda (`ACID_STREAK_SLANT=0.35`, `ACID_STREAK_LEN=7`, caída `ACID_FALL_SPEED=45, ciclo `ACID_STREAK_CYCLE=750`), splashes en el terreno, vapor (`drawSizzle`) sobre la nave. **Streaks recortadas contra el terreno** (no caen debajo). **Mecánica de corrosión**: `inRain(x,y)` (distancia horizontal ≤ `ACID_CELL_RADIUS`), `corrode()` suma `ACID_RAIN_CORRODE=0.002`/tick, `dry()` resta `ACID_DRY_RATE=0.0004`/tick; al llegar a 100 % → **disolución de la nave**: `ship.dissolve()` desprende las 6 partes secuencialmente (~0.35 s, umbrales 5-28 ticks) con velocidades de deriva individuales en X e Y, la nave mantiene su inercia mientras se desintegra. Mensaje `"ACID RAIN CORRODED THE SHIP"` (una línea centrada, sin "YOU CRASHED"). HUD `ACID nn` (número fijo, palabra `ACID` parpadea ≥ 90 %; sin `%`, con espacio entre etiqueta y número) en `(250,72)` (`warnY`→82). Tanto `ACID` como `WIND` muestran glitch al caer un rayo. `isolateForWormhole()` apaga la lluvia mientras haya agujero de gusano; `moonEffectFree()` excluye Europa (ya no es host de wormhole). API: `active()`, `cellCount()`, `cellX(i)`, `inRain`, `meterGet`, `setMeter` |
| `wormhole_demo.cpp` | Prueba de visualización en PC: terreno generado + agujero en el cielo (x 260–560, y 150–200) + nave que siente el empuje radial y, al cruzar la mitad del radio, es capturada y espiralada en vórtice hacia el núcleo → PPM en `frames/` (selftest: fase `EMERGING`→`ACTIVE`, `captured()` y `swallowed()` al final) |
| `tanker.h/cpp` | **Nave cisterna aérea con repostaje en vuelo (ronda 5b refine 3: mini-juego de docking 8/8/2026)**: aparece desde niveles ≥1 (`TANKER_START_LEVEL=1`) con `TANKER_CHANCE_PERCENT` (100 %, determinista, 28/8/2026) **y solo si el combustible está bajo** (`fuel < FUEL_MAX·TANKER_FUEL_FRACTION=0.5`); bypass por `force` reinstalado (28/8/2026) solo para el showcase del demo (`DEMO_TANKER_FIRST`, eliminado 26/8/2026: el repostaje ya no es parte del showcase del demo). Aeronave **zeppelin**: globo elargado `lineShade`, góndola, aletas, faro, motor. Flota a `TANKER_HOVER_ALT=420 u` (por encima de `APPROACH_EXIT_ALT=350`: flotar en la zona muerta [200,350] hacía parpadear el zoom entre el bgLayer horneado y el vector) con deriva ±40 u @ 9 u/s y bob ±3 u. En Titán `baseY=TANKER_TITAN_Y=250` (10/9/2026: **entre las dos bandas de niebla**, la 1ª en ~205 y la 2ª en ~343+, más cerca de la 1ª; antes `=85`, arriba del todo); excluida de Ganímedes (`moonHasRings`). Docking **probe-and-drogue** con **una manguera** (`TANKER_HOSE_LEN=20 u`) y cesta inferior (`drogueX/Y = bodyX+sway / portY+HOSE_LEN+sway`, sway ±2 u @ 1.2 rad/s). **Mini-juego de mantenimiento**: tras engancharse hay que mantener el probe dentro de `TANKER_DOCK_TOL_X=20`/`TANKER_DOCK_TOL_Y=14` (con velocidad ≤ 0.14 vertical / 0.20 horizontal) durante `TANKER_DOCK_LOCK_TIME=0.3 s` para que fluya el combustible; salir de `TANKER_DOCK_BREAK_TOL_X=28`/`TANKER_DOCK_BREAK_TOL_Y=22` durante `TANKER_DOCK_BREAK_TIME=1.5 s` rompe el acople (`breakAway`). El joystick controla el offset horizontal del probe (`ship.velX` → nudge, muelle de centrado suave); el motor (Z) desengancha. Repostaje incremental `TANKER_REFUEL_RATE=200/s` hasta `FUEL_MAX`. **Auto-desconexión al llenar el tanque (fix01, 2026)**: al llegar `ship.fuel == FUEL_MAX` la cisterna suelta el probe con un leve empujón (`velX=-0.03`, `velY=0.04`, `posY+=2.0`) y **se queda en estación** (sin `leaving`/`done`) para permitir un re-dock posterior; arranca un cooldown `TANKER_REDOCK_COOLDOWN=2.0 s` durante el cual el probe no se vuelve a asentar (`targeted()`/`checkDock()` devuelven false) para que el módulo recién expulsado no chasquee de vuelta a la cesta (sigue solapando el drogue al soltarse). `breakAway` (motor) también arma el cooldown. **Restricciones de spawn restauradas (fix01, 2026)**: la cisterna solo aparece con `fuel < 50 %`, excluida de Ganímedes (`moonHasRings`) y con `TANKER_CHANCE_PERCENT=100 %` (determinista, 28/8/2026); el bypass por `force` quedó solo para el showcase del demo (`DEMO_TANKER_FIRST`, off por defecto). **Guarda anti-bucle del autopilot de demo (fix01, 2026; corregida 28/8/2026)**: la demo no debe quedar pegada re-acoplándose a la cisterna tras llenar el tanque. El bucle real tenía una **causa física además de la del AI**: aunque `runDemoAI()` saliera del modo tanque (`demoTankerPhase=-1`) al llegar `fuel>=FUEL_MAX`, la nave quedaba **flotando a la altura del drogue** (el auto-desenganche solo da un empujoncito `velX=-0.03`/`velY=0.04`), y el **latch físico `checkDock()`/`beginDock()` de `Game::checkCollisions()`** (que NO depende de `demoTankerPhase`) re-acoplaba la nave en cuanto caducaba el cooldown de re-dock de 2 s — bucle sin fin. Fix completo (28/8/2026): (1) `setupDemoTarget()` solo apunta a la cisterna con déficit real (`ship.fuel < FUEL_MAX·TANKER_FUEL_FRACTION`, gate igual al spawn), para no volver a apuntar con el tanque solo "no lleno"; (2) `runDemoAI()` arranca con una guarda en el top (antes de `targeted()/docked()`, porque tras el auto-desenganche ambas son false y el bloque cisterna no se ejecuta): si `demoTankerPhase>=0 && fuel>=FUEL_MAX` → `demoTankerPhase=-1` + `setupDemoTarget()`; (3) **crítico — en el auto-desenganche del demo `tanker.leaving = true`** (la cisterna se va a `done`), porque solo así `targeted()` pasa a false y el latch físico `checkDock()` ya no puede re-acoplar aunque la nave siga junto al drogue. El jugador humano conserva la estación (`sin leaving/done`) para re-dock. Verificado con harness de acoplamientos: 0 multi-docks en 500 semillas (`demo_sim` sin colgar, win-rate ~30 %). Colisión con el casco = destrucción mutua. **Visual**: cesta triangular en vista general / tronco de cono invertido en PiP; probe con varilla fina + **flecha sólida triangular** (sin círculo brillante en punta); **anillo de estado** en el PiP (verde/amarillo/rojo). **Oculto en aproximación final**: no se dibuja cuando `ship.altitude < APPROACH_ALT` (se está aterrizando, no repostando).   **Visual plutónico (22/8/2026)** (rama `fuel-tanker`): globo elipsoide relleno por filas con `lineShade` (brillo 120→160) + contorno 255 + arco de resalte superior (200) + **franja oscura a media altura** (3 filas, brillo 60→80) que reemplaza a la antigua línea central resaltada; **góndola-cabina aerodinámica** con contorno `\___|` (nariz diagonal tocando el casco, panza plana, popa vertical), **rellena** como el globo (trapezoide con degradado) y **sin cables** de soporte (antes parecía colgando). **Escala unificada (25/8/2026; rediseñada 28/8/2026, `test_scale`)**: la cisterna se dibuja con el **mismo factor que la nave** (`drawScale = ship.scale·viewScale`, `Tanker::drawScaleFor`) — no hay boost de dibujo; se lee más grande solo porque su **geometría base es mayor** (balloon ~13 u vs ~10 u de la nave, factor `TANKER_GEOM=1.5` aplicado solo al cuerpo/cesta/manguera). El **tamaño intrínseco** es un solo knob: `TANKER_SIZE=1.2` multiplica **a la vez** render (body, gondola, hose, drogue) y física/interface (hitbox `hullHalfW`/`balloonHalfH`, `portY`/`hullHalfH`, `hoseLen`, tolerancias de dock `dockTolX/Y`, caja de trigger `dockZoneX/Y` y plataforma `platformW` vía accessors escalados en `tanker.h`; los clamps de control del funnel `ctrlX/ctrlY` derivan de `dockTol`). El probe de repostaje (`TANKER_NOZZLE_LEN`) es de la nave y NO escala. **`restartLevel()` (28/8/2026)**: al repetir un nivel se re-rolla la cisterna (`tanker.reset(level, terrain, ship.fuel)`) — con el fuel conservado (<50 %) el tanque vuelve a aparecer. Demo AI corrige offset durante dock. Ver WORKLOG #29 |
| `parachute_demo.cpp` | Prueba de visualización en PC (23/8/2026): terreno generado + nave con **paracaídas** en 5 estadios de inflado + rampa con física real (brake→sink) → PPM en `frames/` (selftest `open=1.00 velY≈sink`) |
| `acidrain_demo.cpp` | Prueba de visualización en PC (26/8/2026): terreno generado (nivel 3 = Europa) + celdas de lluvia ácida + nave escaneando el mundo → PPM en `frames/` (selftest `active + maxMeter>0.3 + inRain>0 + outRain>0`) |
| `quake.h/cpp` | **Terremotos de Callisto (rama `quake`; 12/8/2026; golpes repetidos y ruptura de superficie 14/9/2026)**: activos en niveles de Callisto (`moonHasQuakes(level)`, `moonIndex==4`, i.e. nivel 3, 11, 19…). `reset(level, terrain, ship)` fija un temporizador aleatorio de 8–20 s; **el epicentro sigue a la nave** (`pickStrikeTarget()` = `ship.posX` ± `QUAKE_STRIKE_JITTER=12 u`, clamp al mundo): por eso un temblor puede ocurrir tanto con la nave alta en el cielo como **cerca de la superficie**, donde destruye el suelo/plataforma que está pisando. Fase RUMBLING (~1 s): screen-shake (`QUAKE_SHAKE_MAX=4 px`, offset determinista añadido a `viewX/viewY` en `Game::draw()` y restaurado antes del HUD), polvo (14 partículas con cruz de 3 px), y **grieta pulsante** (`prand`). Al terminar la cuenta atrás: **`terrain.ruptureSurface(targetX, QUAKE_SURFACE_HALF_W=12)`** abomba un tramo de 24 u de la superficie (joroba de pico `QUAKE_LIFT=22 u`); si la ventana **toca una plataforma de aterrizaje, la plataforma entera se destruye** (`ruptureZone`: segmentos inclinados `landable=false`, `multiplier=1`, `labelX=-1` → desaparecen las luces de aproximación, el label "Nx"/"p" y el punto del minimapa) y **ya no se puede aterrizar ahí** (`checkLanding()` vuelve 1). **Se re-arma** (`QUAKE_REARM_TIME=1.5 s` + nuevo timer 15–30 s) para golpear varias veces por nivel, aunque **no dispara fuera de `STATE_PLAYING`** (`canStrike`). `isRupturedAt` (quake) cubre la última ventana; `Terrain::isRupturedAt()` es **persistente** (zonas rotas + `ruptureRanges_`, limpiado en `init()`/`generate()`) y es el que usa `Game::checkCollisions()`. `justStruck()` un frame. `justRumbled()` otro flanco (entrada a RUMBLING) para el sonido. Crash sobre terreno/plataforma rota → `quakeCrash=true` → mensaje `"YOU CRASHED" / "THE GROUND GAVE WAY"`. HUD `SEISMIC` parpadeante en (250,72) durante RUMBLING. Demo y autopilot normales (si el azar cae en Callisto, los temblores siguen a la nave y casi siempre rompen su pad → showcase del efecto). `isolateForWormhole()` apaga el quake (defensivo; Callisto nunca alberga wormhole). API tests: `active/phase/justStruck/shake/rupturedZone/strikeX/isRupturedAt`. Ver WORKLOG #49 |
| `quake_demo.cpp` | Prueba de visualización en PC: terreno generado + quake en 2 fases — (A) nave **alta en el cielo**, el temblor la sigue y abomba la superficie abierta; (B) nave **baja sobre un pad intacto**, el siguiente golpe **destruye el pad** (label a -1, `landable=false`) → PPM en `frames/`. Selftest `active + isRupturedAt(strikeX) + zoneBroken + !landable + labelX<0`. Nivel por defecto 3 = Callisto |

### Mundo y pantalla

- Mundo 800×600, pantalla 320×240. Física y colisiones en coordenadas de mundo; el dibujado escala.
- Vista normal `viewScale = SCREEN_H/700`. Nave `scale = 1.0` (vista normal) / `0.32` (zoom).
- **Culling de viewport (16/9/2026, rama `quake`)**: los efectos ambientales solo se
  actualizan/dibujan mientras su ancla cae en el rectángulo de mundo visible (con margen de
  alcance), calculado a partir de `viewX/viewY/viewScale` (`effectVisible(wx,wy,margin)`,
  `xInView(wx,margin)`, `bandVisible(wy,margin)`, `atmosphereInView()` en `game.h/cpp`). En
  zoom la ventana visible son ~187 u de ancho, así que un efecto lejos de la vista se salta
  entero (el wormhole ~900 powf + miles de `pixelShade` por frame cuando ni siquiera se ve).
  Aplicado a: wormhole (update si visible **o** si puede alcanzar la nave `< WORMHOLE_GRAB_R`
  o capturado/tragado; draw si visible), atmósfera de Titán (update/draw por bandas), géiseres
  (solo dibuja si un vent + alcance del chorro está en X), volcanes (`countInView`), anillos
  (banda vertical `RING_CY` ± curva+jitter), torbellino (ancla ± `TWISTER_HEIGHT`). La física
  (drag/arrastre, colisiones) NO se culla: los hooks corren directo en `Game`. Test en
  `test_pc.cpp` (`testViewportCull`, renderer contador `CountRenderer`).

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
  **Veredicto fijo al tocar (1/9/2026)**: el resultado se decide **una sola vez en el touchdown**
  (`landPerfect = ship.velY < LAND_PERFECT_VY` al entrar en `result==2`). El mensaje en `STATE_LANDED`
  usa `landPerfect` y ya no
  re-evalúa `ship.velY`, que seguía cambiando durante el mensaje (gravedad y frenado del
  paracaídas hacían que un aterrizaje con vela a sink 0.09 mostrara `PERFECT LANDING` sin haber
  dado el +50). Ahora: perfecto → `CONGRATULATIONS / PERFECT LANDING`; aterrizaje seguro no
  perfecto → `GOOD LANDING`. **El +50 de fuel se entrega al INICIO del siguiente nivel**
  (2/9/2026): el touchdown guarda `landFuelBonus=50` y el `resetTimer` de la transición
  (`STATE_LANDED`) lo suma a `ship.fuel` (tope `FUEL_MAX`) justo antes de `nextLevel()`, así la
  recompensa se ve en el contador `FUEL` del nivel siguiente (300 → 350) donde el jugador ya está
  relajado viendo el HUD, no en el aterrizaje donde tiene el ojo en el landing spot. El bonus de
  score (`50×mult`) sí se da en el touchdown. Aterrizar con fuel 0 en perfecto sigue salvando la
  partida (el +50 se aplica antes del chequeo `ship.fuel<=0` → `endGame`).
- `checkLanding()` usa la **zona completa** (segmentos `landable` contiguos) en vez de un solo
  segmento: en terreno clásico (nivel 1) las plataformas varían de ancho con el multiplicador
  (5x→3, 4x→4, 2x→5 segmentos; ~13.5/20/38 u) vs caja de la nave ~9.6 u en aterrizaje (escala 0.48
  en zoom 5x; antes el segmento plano único medía 6.8 → crash por desbordar el borde con
  rot/vy válidos). En terreno procedural (nivel ≥ 2) también varían (5x→4, 4x→5, 2x→6 segmentos,
  ~20–42 u) y en Ganímedes son 2 segmentos más anchos (6/7/8) porque se aterriza con zoom 2×
  (caja ~24 u); el pad más estrecho siempre queda holgado sobre la caja.
- Zoom: **un solo umbral de aproximación** (`APPROACH_ALT=200`, salida con histéresis
  `APPROACH_EXIT_ALT=350`; sustituyen a `ZOOM_IN_ALT`/`ZOOM_OUT_ALT`, rama `tanker-docking`
  25/8/2026): zoom-in 5× (`viewScale = SCREEN_H/700*5`) cuando `alt < APPROACH_ALT`, zoom-out
  cuando `alt > APPROACH_EXIT_ALT`. El mismo umbral gobierna el minimapa y la cisterna (ver
  abajo), así ya no hay sistemas que peleen entre sí.   **Cámara en zoom**: sigue a la nave **centrada en X y al 50 % desde arriba**
  (`APPROACH_CAM_FRAC=0.50`, `config.h`; `updateView()` fija `viewX/viewY` cada frame),
  dejando la mitad inferior para el terreno y la zona de aterrizaje (bajado del 33 % el
  25/8/2026: a 33 % la nave quedaba pegada al minimapa y el medio inferior se desperdiciaba).
  **Zoom de dock zone (cisterna)**: dentro de la caja `TANKER_DOCK_ZONE_X/Y` (histéresis
  +30/+20 u vía `tankerZooming`) la vista va a macro-zoom centrado en la **nave**. La cisterna
  flota a 420 u (sobre `APPROACH_EXIT_ALT`), así al desacoplar el zoom de altitud (zoom-out a
  `alt > 350`) la saca solo de la vista; ambos zooms nunca pelean. **Altitud de zoom
  independiente de escala (fix 27/8/2026)**: `ship.altitude` se mide desde `ship.bottom =
  posY + 14·ship.scale`, y `ship.scale` cambia con el zoom (1.5 normal / 0.48 en 5×) → al
  hacer zoom la altitud saltaba ~14 u y el zoom **oscilaba** alrededor del umbral (bucle
  zoom↔scale↔altitud; se veía el terreno dibujado doble, zoom-out y zoom-in). El umbral usa
   ahora `approachAlt` medido desde `ship.posY` (centro de la nave, independiente de escala), y
   se eliminó el force-zoom-out por `tanker.leaving` que peleaba contra el zoom-in por altitud.
   **Altitud de aproximación por elevación local del terreno (28/8/2026, rama `fix2`)**: el zoom
   de aproximación se mide hacia el **terreno interpolado bajo la nave** (`Terrain::yAt(ship.posX)`
   en `Game::updateApproachAlt()`, método público extraído de `updateView()` + getter
   `approachAltGet()`; muros verticales `yAt` los omite), no un datum absoluto: al volar hacia un
   pico alto el suelo sube bajo la nave y el hueco se encoge → el zoom-in se dispara aunque la
   altitud absoluta supere `APPROACH_ALT` (aproximación montañosa: la referencia es el suelo
   alcanzable, no la cima del cielo). Antes usaba el `y1` del segmento bajo la nave (sin
   interpolar), que en laderas largas/picos medía mal. Test `testApproachElevation` en `test_pc`.
   **Blend con la altitud absoluta sobre el plano de aterrizaje (28/8/2026, rama `fix2`)**: el
   zoom-out por pozo se arregla combinando el hueco relativo con la altura general —
   `approachAlt = min(relAlt, absAlt)` en `updateApproachAlt()`; `relAlt` es la elevación local,
   `absAlt = Terrain::padFloorY() − posY − 14` con `padFloorY()` = el **pad más profundo del nivel**
   (máximo `zone.baseY`, nuevo método de Terrain). Sobre un pozo profundo junto a un pad, `yAt`
   devuelve el fondo del pozo y `relAlt` solo superaría los 350 u de salida en pleno descenso → el
   zoom volvía a zoom-out justo al aterrizar (incontrolable). Con el blend: zoom-in si `rel<200` **o**
   `abs<200`; zoom-out solo si **ambas** `>350` (la nave debe subir de verdad). `absAlt` se clampa a
   ≥0 (descenso a un cráter bajo el plano también se queda en zoom). Los umbrales y la histéresis
   no cambian; Ganímedes usa el mismo valor.
   **`approachAlt` compartido también por el minimapa (fix01, 2026)**: `approachAlt` pasó a ser un
   miembro de `Game` (calculado en `updateView()`), y el minimapa de aproximación (`inApproach` en
   `Game::draw()`) usa la misma medida en vez de `ship.altitude` — con `ship.altitude` (basada en
   `ship.bottom`, que salta ~14 u al entrar el zoom) el minimapa **desaparecía de forma
   intermitente** durante el descenso en la 2ª fase; ahora zoom y minimapa se encienden/apagan con
   el mismo umbral y la misma base de medición.
  **En Ganímedes (`moonHasRings`)**: zoom de banda al entrar al anillo de rocas (`posY` en
  `[bandCY±jitter±30]`) + zoom de altitud al bajar (`alt < APPROACH_ALT`); `setZoom(true, 2.0f)`
  (2× en vez de 5×). Histeresis de ±30 u en entrada / ±60 u en salida de la banda para evitar
  flickering.
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
  HUD: `WIND nn>` (o `<`) en `(250,62)`, debajo de `G` (con viento, el aviso
  `REFUELING`/`DOCKING` de la cisterna pasa a `(250,72)`; sin viento queda en `(250,62)`; ver Sketch
  ESP32); glifos `<` y `>` añadidos a la fuente 5x7.
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
- **Indicadores de aterrizaje (7/8/2026)** (detectados por `labelX >= 0`, único por zona):
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
- Zonas de aterrizaje: índices `{34, 63, 106, 133}` con multiplicadores `{4, 5, 5, 2}`. **Ancho
  variable con el multiplicador (1/9/2026)**: 5x→3, 4x→4, 2x→5 segmentos en clásico (~13.5/20/38 u);
  procedural 5x→4, 4x→5, 2x→6 (y +2 en Ganímedes por el zoom 2×). El pad de mayor puntaje es el más
  estrecho pero siempre deja holgura sobre la caja de la nave (9.6 u normal / 24 u Ganímedes).
  `checkLanding()` trata cada grupo de segmentos `landable` contiguos como una plataforma entera.
- `labelX` se setea solo en el primer segmento de cada zona → el label "Nx" se dibuja una sola vez.
- **Muros verticales de las plataformas (8/8/2026)**: al aplanar la zona (`init()` solo aplanaba
  los segmentos `idx..idx+3`) el punto `idx+4` conservaba su `y` original → quedaba un salto de
  altura en el mismo `x` que **no se dibujaba** (muro invisible en la vista).
  Fix: `Terrain::draw()` dibuja un **conector vertical** cuando dos
  segmentos consecutivos comparten `x2==x1` y difieren en `y`. `generate()` no tenía el bug
  (aplana el punto de frontera `zoneStart..zoneStart+4`).
- **Niveles procedurales (5/8/2026)**: `Terrain::generate(level)` para nivel ≥ 2. Nivel 1 = terreno
  clásico (`init()`). Generación: random walk con deriva acotada (±40) + colinas sinusoidales
  (  `freq`/`phase` por nivel) + 2 pasadas de suavizado; 150 puntos, ancho ~900. 4 zonas planas
  (multiplicadores `{4,5,5,2}`), anchos **inversos al multiplicador** (1/9/2026): 5x→4, 4x→5,
  2x→6 segmentos (~20/30/36 u; +2 en Ganímedes). Dificultad: amplitud
  del random walk `4+level` (tope 12). Semilla `srand(esp_random())` en `setup()` del `.ino`.

## Sketch ESP32 clásico (`esp32LanderComposite/`) — DESCONTINUADO 24/8/2026

> **DESCONTINUADO**: la versión principal para el CRT B/N es ahora el port a **ESP32-S3** (`esp32LanderS3/`), que se ve y rinde mejor (driver propio LCD_CAM+GDMA, ~58.6 fps en demo). Esta sección se conserva como referencia histórica; la carpeta NO recibe más sync (`sync.sh` ya no la toca) ni uploads. Para reflashearla habría que re-sincronizar sus fuentes manualmente.

## Detalle histórico del sketch compuesto

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
  `game.windEnabled` (viento aleatorio por nivel ≥ 4). Con viento el aviso
  `REFUELING`/`DOCKING` de la cisterna pasa a `(250,72)`; sin viento queda en `(250,62)`.
  **`SC`/`VW`/`TK` de debug (TEMP)**: `SC` (`ship.scale`), `VW` (`viewScale`) y `TK`
  (`Tanker::drawScaleFor`) en `(250,92)`/`(250,102)`/`(250,112)` bajo `#if SHOW_DEBUG_SCALES` —
  herramienta para validar el escalado/zoom en CRT; apagado por defecto.
  Aviso parpadeante `LOW FUEL` (o `OUT OF FUEL`) alineado con los indicadores de la derecha.
  **Etiquetas `VX`/`VY` con flashing (12/8/2026, sustituye al aviso `TOO FAST`)**: mientras se
  juega (`STATE_PLAYING` y fuera de la intro), la etiqueta `VY` parpadea (`counter%50>=30`, se omite
  el texto) cuando `velY > LAND_HARD_VY` y la etiqueta `VX` hace lo mismo cuando
  `|velX| > LAND_HARD_VX` — la velocidad no permitiría aterrizar con seguridad. No hay banner
  `TOO FAST`.
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
  nivel, trama cruzada + tramas de sombreado con `pixel`). **Rellenos sólidos (10/8/2026)**: `fillPolygon` en etapa de descenso 100, ascenso 140 y panel central 100; ventana y cajas laterales con `rectShade` 160/120, todos con contorno blanco encima. Controles en letras pequeñas a la derecha
   en x=170
   (`STICK: ROTATION`, `Z: ENGINE ON/OFF`, `C+STICK: POWER UP/DOWN`, `POT: POWER LEVEL`,
   `START: PARACHUTE (1/LEVEL)`). La línea de crédito
  `Copyright Alex Urzola 2026/Opencode` se dibuja **centrada debajo del título** (`y=40`, con
  `centerText`). El fondo es el de juego
  (estrellas + nave entrando **por la derecha** con deriva lenta a la izquierda
  (`setupTitleShip()`, `velX=-0.35`, `posX=(SCREEN_W-20)/viewScale`)).
- **Demo / attract mode (8/8/2026; niveles al azar 1..12 desde 13/8/2026; al azar en CADA ciclo
  desde 25/8/2026, rama `demo-random-progression`)**: tras `DEMO_START_DELAY=13 s` en el título,
  `Game::startDemo()` lanza un nivel al azar (1..12; `DEMO_MAX_LEVEL=12`) jugado por un
  **autopilot** (`Game::runDemoAI()`) hacia `demoTargetX/Y`. Cada ciclo re-tira el nivel al azar
  (cualquier luna/efecto; `DEMO_LEVEL_FIRST=0`). **Spawn aleatorio completo (27/8/2026)**: la nave
  aparece en posición completamente aleatoria — X∈[50,850] y Y∈[80,350] (`DEMO_SPAWN_X_MIN/MAX`,
  `DEMO_SPAWN_Y_MIN/MAX`); la cámara se centra al spawnear (`viewX`/`viewY` ajustados al centro
  de pantalla). **Fuel y hull acumulativos en demo (26/8/2026)**:
  `ship.fuel` y `hullIntegrity` se preservan entre ciclos del demo; solo se resetean a
  `FUEL_MAX` / `100` cuando `ship.fuel` llega a 0 durante el vuelo (reset automático a
  `FUEL_MAX`). El fuel se guarda/restaura tanto en `startDemo()` como en `endDemoToTitle()`
  (el fix de `setupTitleShip` que borraba `ship.fuel` vía `ship.reset()`). La cisterna aparece
  solo cuando `fuel < 50 %` (`TANKER_CHANCE_PERCENT=100`, determinista); con `DEMO_TANKER_FIRST`
  el primer ciclo del demo hace el showcase (fuerza la cisterna con fuel bajo); `srand(esp_random())`
  re-seeda en cada ciclo en ESP32 para variedad. `DEMO_FORCE_TANKER_CRASH=0` (el showcase de choque
  quedó desactivado). **Sensores ignorados en demo**: `readInputs()` del sketch
  no lee nunchuck/pot mientras `game.demo` (solo el botón start, que cancela el demo) — leerlos
  cada loop pisaba la rampa lenta de ángulo del autopilot y la nave subía en vez de volar.
  **Control desacoplado (9/8/2026)**: deriva `angle=atan2(aX,aY)`, `thrust=hypot(aX,aY)/THRUST_ACCEL`
  con `aX=(desVX−velX)·0.02 − windDir·windPush` y `aY=(velY−desVY)·0.03` (tope 0.00075), `desVY`
  según fase (0.12 crucero / 0.04 cerca / 0.03 aproximación); `aX` se atenúa cerca del suelo
  (`alt<12 → ×0.6`, `alt<2.5 → ×0.05`) para aterrizar erguido; freno de ascenso si `velY<-0.01`.
  PWR rampea a `DEMO_POWER_RATE=0.4/s`. **Oscilación de thrust (27/8/2026)**: `sinf(counter·0.04)·0.06`
  añadido al thrust en los 3 modos del autopilot (crucero, altitude-hold, tanker) para que el
  power/VY varíe orgánicamente en vez de quedarse constante. **A veces gana, a veces pierde**
  (~50 % "torpes", `demoSkill` 0–0.35 y offset hasta ±110 u → aterrizan en la ladera;
  win-rate ~45 % validado con `./demo_sim`). Al terminar muestra el resultado (`CRASH_RESET_DELAY`)
  y vuelve al título; `DEMO` en HUD bajo `PWR` en `(22,62)`; cualquier `startPressed` cancela el
  demo (`demo=false`).
- **Combustible (5/8/2026)**: **no se recarga entre niveles**; lo consumido queda consumido
  (`ship.fuel` se conserva en `nextLevel()`/`restartLevel()`, que antes lo reiniciaban vía
  `Ship::reset()`). El juego **NO termina al quedarse sin combustible en pleno vuelo**: se puede
  acabar el nivel (aterrizar sin motor). Al aterrizar: si `ship.fuel<=0` (tras el bonus de
  aterrizaje perfecto, que se aplica al pasar de nivel) → `endGame()` (`OUT OF FUEL`/`GAME OVER` y
  vuelta a la intro); si hay combustible → `nextLevel()`. Aterrizaje perfecto sigue dando +50 (con
  tope `FUEL_MAX`), visible en el `FUEL` del nivel siguiente.
- Video lib: `renderer_esp32` escribe en el framebuffer de `video_get_frame_buffer_address()`.
  El render lo hace la librería (DAC → GPIO25 → RCA del TV). B/N usa luma alta (255).
- Compila validado con `arduino-cli compile --fqbn esp32:esp32:esp32`: ~525 KB flash
  (40% del app slot), RAM 109 KB (33%). **Esquema de partición `no_ota`** (ver "Flash"), app slot de 2 MB.
- Loop: `game.update()` cada 10 ms (acumulador sobre `millis()`); `game.draw(renderer)` por iteración.

## Port a ESP32-S3 (`esp32LanderS3/`) — VERSIÓN PRINCIPAL para CRT B/N (24/8/2026)

Port a la placa **ESP32-S3** (8 MB PSRAM octal) con video compuesto NTSC por **driver directo
LCD_CAM + anillo GDMA auto-enlazado** (sin esp_lcd, sin costuras entre campos). FQBN:
`esp32:esp32:esp32s3:PSRAM=opi`, puerto `/dev/ttyACM0`. Fuentes del juego en
`esp32LanderS3/src/` (copias de `esp32Lander/`; bglayer ya integrado en `sync.sh`). Estado:
**Fase 0 (video) ✓** (artefacto diagonal superior eliminado), **Fase 1 (juego corriendo) ✓**
(título + demo attract validados en CRT), **controles ✓** (nunchuck SDA=GPIO21/SCL=GPIO9,
start GPIO13, pot GPIO8), **audio ✓** (GPIO18; motor/explosión probados en juego),
**Fase 2/3 bgLayer ✓** (terreno pre-horneado en PSRAM, activo; A/B fps neutrales ~58.6).
Detalles técnicos y quirks LEDC en WORKLOG #53/#54.

| Archivo | Contenido |
|---------|-----------|
| `esp32LanderS3.ino` | setup/loop igual que composite (pm lock, nunchuck, audio, acumulador GAME_DT); flags temporales `AUDIO_BRINGUP`/`AUDIO_TEST_TONE` |
| `src/video_s3.h/cpp` | Driver de video propio: registros LCD_CAM + GDMA ring (26 descs × 3930 B, EOF→notificación a loopTask), `composeStaticLines()` precomputa lo no visible una vez |
| `src/renderer_s3.h/cpp` | `RendererS3 : RendererCanvas` sobre el framebuffer de video; `drawLayer` nativa (muestreo nearest fixed-point 16.16, con **wrap horizontal por módulo** no clamp, 28/8/2026, idéntica a la de PC) |
| `src/bglayer.h/cpp` + core actualizado | Copias de `esp32Lander/` vía sync (bglayer en la lista SHARED); el `.ino` llama `bgSetAllocator(ps_malloc)` al arranque → layer activo en PSRAM; print one-shot `[bg] world layer active`; `[perf]` con `drawAvg`/`drawMax`. A/B en placa: fps neutrales (~58.6 demo), se deja activo por si crecen los efectos |
| resto de `src/` | Copias del core del juego + `nunchuck.*` (SCL=9) + `audio.*` (pin 18, sin DAC, LEDC_USE_APB_CLK) |

- **Quirks LEDC S3 (audio)**: reloj por defecto es XTAL 40 MHz → hay que llamar
  `ledcSetClockSource(LEDC_USE_APB_CLK)` antes de attach para 312.5 kHz @ 8-bit; no existe
  `LEDC_HIGH_SPEED_MODE`; **el duty solo se adopta pulsando AMBOS** `conf1.duty_start=1` Y
  `conf0.low_speed_update=1` tras escribir el registro (cada uno solo = silencio, `duty_rd=0`).
- Compila: ~641 KB flash (48%), RAM estática ~209 KB.

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
| Nunchuck (joystick X) | Ángulo de la nave `[-PI/2, PI/2]` → rotación `[-90°, +90°]` | I2C SDA=21/SCL=9 (S3) · 21/22 (clásico); dead zone ±10 |
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
**`docs/hardware.md`** (lo consulta el `hardware` agent).

**Pinado de la versión principal (`esp32LanderS3/`, ESP32-S3)**:

| Señal | GPIO |
|-------|------|
| I2C nunchuck SDA / SCL | GPIO21 / GPIO9 |
| Pot (nivel de potencia) | GPIO8 |
| Botón start | GPIO13 |
| Video compuesto (bus LCD_CAM D0–D7) | GPIO4, 5, 6, 7, 15, 16, 40, 41 |
| Audio (LEDC PWM) | GPIO18 |

**Pinado histórico del sketch clásico** (`esp32LanderComposite/`, DESCONTINUADO 24/8/2026 —
la tabla y el detalle quedan en `docs/hardware.md`):

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

**Principal: driver propio LCD_CAM+GDMA en el ESP32-S3** (`esp32LanderS3/src/video_s3.*`):
NTSC 320×240 B/N sin costuras entre campos, ~58.6 fps en demo; bus de datos
GPIO4/5/6/7/15/16/40/41. Ver "Port a ESP32-S3".

Histórico (composite clásico, descontinuado): librería aquaticus
`esp32_composite_video_lib` (GPL) embebida como `src/video.h/c`:
`video_graphics(NTSC_320x240, FB_FORMAT_GREY_8BPP)`, DAC GPIO25 → RCA, B/N luma 255.
El renderer escribe en `video_get_frame_buffer_address()`; `video_wait_frame()` sincroniza.
Cableado y detalle del framebuffer: `docs/hardware.md`.

## Sonido (COMPLETADA 4/8/2026)

- **Vía: PWM por LEDC + timer ISR**. En la S3 (principal) sale por **GPIO18** con
  `ledcSetClockSource(LEDC_USE_APB_CLK)` y el handshake dual de duty (quirks en WORKLOG #53).
  Histórico composite: GPIO26 (NO I2S). Motivo del LEDC: la librería de video
  (aquaticus) usa I2S0 + DAC1 (GPIO25) y `dac_i2s_enable()` fuerza DAC2 (GPIO26) a modo DMA,
  así que GPIO26 no estaba realmente libre para I2S. Solución: `dac_output_disable(DAC_CHANNEL_2)`
  libera la almohadilla y se usa **LEDC (canal 0, HS mode) como PWM portador a 312.5 kHz
  (resolución 8-bit = máx)**.
- `src/audio.h/cpp`: timer gptimer a **16 kHz** con ISR (`IRAM_ATTR`) que mezcla
  `THRUST_SOUND` (loop) + `WIND_SOUND` (loop) + `EXPLOSION_SOUND` (one-shot) + `LIGHTNING_SOUND`
  (one-shot) + `QUAKE_SOUND` (one-shot, 14/9/2026) + **voz de quemado** (17/8/2026, one-shot de 4 s)
  y escribe el duty directo al registro
  `LEDC.channel_group[0].channel[0].duty.duty` (`val<<4`) + handshake `duty_start`. API:
  `Audio::begin()`, `Audio::setThrust(0..1)`, `Audio::setWind(0..1)`, `Audio::playExplosion()`,
  `Audio::playBurn()`, `Audio::playLightning()`, `Audio::playQuake()`.
- Datos: `src/audio_data.h` generado (PROGMEM) desde `sounds/rocket_thrust.wav` (32 k, loop),
  `sounds/explosion.wav` (27.4 k, one-shot), `sounds/wind.wav` (32 k, loop),
  `sounds/lightning.wav` (19.2 k, one-shot) y `sounds/quake.wav` (41.6 k, one-shot, 2.6 s)
  — mono 8-bit / 16 kHz. Generados por
  `sounds/gen_storm_sounds.py` (viento, rayo y terremoto; los reales de motor/explosión no se tocan) y
  combinados por `sounds/convert_wav.py`. El terremoto: retumbar grave lowpass que crece en
  crescendo durante el aviso (coincide con `QUAKE_RUMBLE_TIME`=1.0 s) y aterriza en un boom/trueno
  seco justo en el golpe, con cola de réplica; se dispara con `Audio::playQuake()` desde el `.ino`
  cuando `game.quake.justRumbled()` (flanco de entrada a fase RUMBLING, sample completo 2.6 s).
  **`sounds/mission_control_radio.py` (10/8/2026)**: efecto offline de "radio de mission
  control Apollo" para clips de voz (wav/mp3 → 8-bit mono 16 kHz): banda de voz 250–3200 Hz
  (FFT con bordes suaves), overdrive `tanh`, siseo limitado en banda con compuerta (squelch)
  que sigue la envolvente de la voz; opciones `--noise`, `--drive`, `--echo` (eco de
  retransmisión tierra-luna, ~2.55 s). Sin tonos Quindar (se quitaron a pedido). Salida lista
  para `convert_wav.py`. Validado con voz sintética (formato, squelch 0.04↔0.46 RMS).
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
  `game.storm.takeNewBolt()` → rayo; `game.quake.justRumbled()` (entrada a RUMBLING) → terremoto.
- **Suavizado del sonido (12/8/2026)**: el volumen de motor y viento usa **rampa de ataque/release**
  con easing **entero** (`envEase`, `ENV_DIV=24`) en el ISR (sin FPU: punto flotante en el IRAM ISR
  del núcleo Arduino → `LoadProhibited`/reset) para que entren/salgan sin clic, y el tono del motor
  se **suaviza con un low-pass** (alpha 0.45) al copiarlo a RAM. El **viento se mantiene sutil**
  (`WIND_GAIN=140`, tope `WIND_MAX=160` sobre 256) y **proporcional a la velocidad del viento**
  (`setWind(windStrength)`). El rayo es un one-shot (crack + trueno) al formarse cada rayo.
  Validado: `test_pc` 50 checks, sketch compila 503 KB / RAM 7%, monitor serial estable (sin reset).
- Debug (serial): `debugIsrCount()` imprime `[audio] isr=%u` 1×/s (~16156 ISR/s → 16 kHz reales).
  El pitido de arranque (`debugBeep`) se eliminó (27/8/2026).
- Cableado: **GPIO26 → condensador de acople en serie (1–10 µF) → RCA blanco del TV**
  (quita el DC; lógica de 3.3 V). Verificado con parlante + amplificador.

## Decisiones de arquitectura / convenciones

- Framework: **Arduino (arduino-esp32)** salvo que el usuario decida ESP-IDF.
- Port de **moonlander.seb.ly**: física y terreno del original JS, con nave hexagonal.
- Loop fijo con `millis()`, `GAME_DT=0.01`; el ritmo de video lo maneja la librería
  (`video_wait_frame()`), sin VSYNC explícito en el juego.
- Dibujado: interfaz `Renderer` (pixel/line/rect/circle/text/flush). `RendererCanvas` comparte
  las primitivas; PC y ESP32 implementan `pixel()`.
- `esp32LanderS3/src/` y `esp32LanderVGA/src/` son **copia** de `esp32Lander/` (mismas
  fuentes); mantener en sync con `bash sync.sh` al cambiar física/dibujado.
  `esp32LanderComposite/src/` está **descontinuada** (24/8/2026) y ya no recibe sync.
- Idioma del código: inglés (coherente con el port). Respuestas al usuario: español.
- No usar librerías no verificadas antes de consultar. No añadir comentarios al código salvo que se pidan.
- **El agente NO hace commit ni push salvo que el usuario lo pida explícitamente.** Los cambios
  quedan en el working tree; solo se commitea cuando el usuario lo ordena en el chat o al ejecutar
  el comando `/flash` (que hace el commit como parte de su flujo documentado en
  `.opencode/commands/flash.md`; "hacer flash" ≠ "que el agente commitee por su cuenta").
  Subir a la placa (upload) sí está permitido para probar en CRT sin commitear.
- **Todo trabajo de implementación termina SIEMPRE subiendo a la placa (upload)**, sin que el
  usuario tenga que pedirlo: el paso final de cualquier tarea es `sync.sh` → compilar → upload a
  la **versión principal CRT B/N: `esp32LanderS3/`** (FQBN `esp32:esp32:esp32s3:PSRAM=opi`,
  puerto `/dev/ttyACM0`). No preguntar "¿lo subo?" — subir directamente al terminar. Solo se
  omite (y se avisa) si no hay placa conectada, el build falla o el usuario pidió explícitamente
  no subir. El sketch VGA se sube únicamente cuando el cambio toque `esp32LanderVGA/` y el
  usuario lo indique. La carpeta `esp32LanderComposite/` está descontinuada: NO se sube.

## Comandos útiles

- Validar port en PC: `make && ./test_pc` en `esp32Lander/` (todos los checks pasan).
  Demo visual: `./main_pc` (PPM en `frames/`). Win-rate del autopilot de demo:
  `./demo_sim <seeds>`; render de un demo: `./demo_render <seed>` (PPM en `frames/`).
  Tormenta: `./storm_demo <seed> <level>` (terreno + nave estática + rayos, sin física,
  PPM en `frames/`; selftest `bolts>0` + `maxAlive>0`).
  Paracaídas: `./parachute_demo <seed>` (terreno + nave con dosel en 5 estadios + rampa con
  física real, PPM en `frames/`; selftest `open=1.00 velY≈sink`).
  Agujero de gusano: `./wormhole_demo <seed> <level>` (terreno + espiral en el cielo que se traga
  la nave, PPM en `frames/`; selftest fases `EMERGING`→`PULLING`→`SWALLOW` + `swallowed()`).
  Lluvia ácida: `./acidrain_demo <seed> <level>` (terreno + celdas que corroen la nave;
  selftest `active + maxMeter>0.3 + inRain>0 + outRain>0`).
  Terremotos: `./quake_demo <seed> <level>` (terreno + quake sobre Callisto, PPM en `frames/`;
  selftest `active + isRupturedAt(strikeX) + zoneBroken + !landable + labelX<0`).
- Compilar sketch S3 (principal): `arduino-cli compile --fqbn esp32:esp32:esp32s3:PSRAM=opi esp32LanderS3/esp32LanderS3.ino`.
- Subir S3: `arduino-cli upload --fqbn esp32:esp32:esp32s3:PSRAM=opi --port /dev/ttyACM0 esp32LanderS3/esp32LanderS3.ino`.
- Compilar sketch VGA: `arduino-cli compile --fqbn esp32:esp32:esp32 --build-property build.partitions=no_ota esp32LanderVGA/esp32LanderVGA.ino`.
- Sketch composite (LEGACY, descontinuado 24/8/2026): `arduino-cli compile --fqbn esp32:esp32:esp32 esp32LanderComposite/esp32LanderComposite.ino`.
- Sync PC↔sketches: `bash sync.sh` (copia las fuentes compartidas de `esp32Lander/` a
  `esp32LanderS3/src/` y `esp32LanderVGA/src/` y verifica que queden idénticas; no toca
  los archivos propios de cada sketch: `video_s3.*`, `renderer_s3`, `renderer_vga`, `esp32lib/`,
  `audio*`, `nunchuck*`). La carpeta `esp32LanderComposite/` ya no recibe sync. Para un diff
  puntual: `diff esp32Lander/<f> esp32LanderS3/src/<f>`.

## Flash / memoria

Principal (S3): flash ~643 KB (49 % del app slot de 1.3 MB), RAM estática ~211 KB (64 %),
FQBN `esp32:esp32:esp32s3:PSRAM=opi`. El bgLayer vive en PSRAM (~638 KB).

Histórico composite: ESP32 Dev Module flash 4 MB (QIO 80 MHz), core 3.3.10, esquema de partición `no_ota`
(2 MB app). Sketch ≈ 525 KB (40% del app slot), RAM 109 KB (33%). Layout de particiones y cómo forzar
`no_ota` al flashear (vs `arduino-cli upload` que revierte a `default`): `docs/hardware.md`.
## Proceso de trabajo / WORKLOG

Historial completo por ítem (changelog): **`docs/WORKLOG.md`**. Aquí solo quedan los
pendientes y bugs activos:

- **Pendiente de prueba en CRT (gráficos/efectos)**: nunchuck (#8). Anillos de Ganímedes (#21) y atmósfera de Titán (#20) ya rediseñados (bandas elípticas concéntricas, sin niebla en Ganímedes), validados parcialmente. Vueltas en proceso: anillos de Ganímedes + niebla de Titán (rama `twister-circ`, 22/8/2026). Torbellino de Tritón (#22): dibujo de resorte + partículas validado en CRT (21/8/2026).
- **PENDIENTE (feature)**: boca de volcán con patrón "U" aserrado (#18, ver `volcanoes.cpp` `Volcanoes::draw()`).
- **PENDIENTE (idea, audio, para luego)**: **voz de mission control** oída a través de "canal de radio" (efecto de voz distante, filtrada tipo radio FM/arrancada a KM de distancia). Sin diseño cerrado; se integraría en la sección Sonido (probablemente con la voz como un one-shot largo o muestras cortas por frase, por el estilo de los one-shots actuales: explosión/quemado/rayo). Requeriría además del pipeline ESP32. **El efecto offline ya existe** (`sounds/mission_control_radio.py`, 10/8/2026): convierte cualquier voz wav/mp3 al carácter de radio Apollo (banda 250–3200 Hz + overdrive + squelch). Falta: generar las frases, integrar a `convert_wav.py` y al mezclador del sketch.
- **PENDIENTE (idea, arquitectura, para luego)**: **usar el otro core (0)**. Hoy Arduino-esp32 ya corre sobre FreeRTOS: `setup()/loop()` son una tarea pinneada al **core 1**, el core 0 va mayormente idle (esp_timer; WiFi sólo si se usara). El audio actual es un ISR de timer gptimer a 16 kHz que mezcla en IRAM y escribe el duty del LEDC, e interrumpe al core del juego unos microsegundos por muestra. Posible mejora legítima: mover la **mezcla de samples** (thrust+wind+explosión+voz) a una **tarea dedicada en el core 0** con ring buffer, dejando al ISR solo `pop + escribir duty`; así el core 1 no paga nada de audio y se podría subir la calidad (más canales, filtros, ~32 kHz). **Precaución**: a 16 kHz la muestra hay que entregarla cada 62.5 µs — un ISR la garantiza, una tarea normal no → ISR para el muestreo, core 0 para la mezcla pesada. **NO arregla el lag de dibujo** (p.ej. Ganímedes), que es CPU de render, no audio. NO implementado.
- **PENDIENTE (idea, port a consolas retro, para luego)**: **portar a Wii** (y, casi gratis, GameCube) usando devkitPPC+libogc: el core es C++ std puro con `Renderer` abstracto → solo hay que escribir un `Renderer` de framebuffer + entrada nunchuck (mando nativo de Wii, coincide con los controles actuales) + audio. El ranking completo de consolas disponibles (NES/SNES/Mega Drive/Wii/GameCube/PS2/Xbox 360) y la decisión de aplazarlo está en WORKLOG #38. **Decisión**: diferido hasta pulir la versión ESP32 actual. NO implementado.
- **BUG PENDIENTE**: ~~los controles del HUD pestañean/se pierden en la fase de aproximación con zoom en Titán (#20)~~ → **RESUELTO (20/8/2026)**: el parpadeo/borrado parcial de minimapa, indicadores y nave en la 2ª etapa (zoom) era **tearing de framebuffer único** (el DMA de la librería aquaticus escanea el FB mientras `draw()` escribe; en zoom el frame excede la ventana de blanking). Fix: **doble buffer** en el `.ino` — `fbShadow[76800]`, `RendererESP32` pinta en el shadow, y tras `video_wait_frame()` se `memcpy(shadow→videoFB)` durante el blanking; el siguiente `draw()` pinta en el shadow durante el campo completo. El DMA solo ve frames completos (  afectaba a cualquier luna con efectos en zoom, no solo Tritón). El doble buffer rompía la RAM
  (el FB de video ya no cabía); se resolvió pasando las muestras de audio a flash (ver Sonido).
  RAM 101908 B (31%), arranque limpio verificado por serial. Pendiente re-probar en CRT.
- **BUG PENDIENTE (demo, #23)**: a veces el juego no se renderiza completo por la **izquierda** de la pantalla — queda un espacio sin pintar o sin usar, notado principalmente en el auto-demo/attract mode. Hipótesis a investigar: la librería aquaticus escanea el framebuffer por raster esta vez; posible offset de inicio de línea horizontal (back porch del CRT) o un rect/borrado que no cubre el margen izquierdo en ciertos estados. Ver WORKLOG #23.
- **TEMP**: `RING_FOG_BRIGHT=0` desactiva la niebla de las bandas de Ganímedes (decisión de diseño). `DEMO_LEVEL_FORCE=0` en PC y sketch (demo elige nivel al azar 1..12). **`DEMO_LEVEL_FIRST=0` (25/8/2026, rama `demo-random-progression`)**: la demo elige nivel al azar 1..12 en CADA ciclo (cualquier luna/efecto); el antiguo ciclo fijo en Luna n.º 1 quedó revertido. **`DEMO_WORMHOLE_FIRST=false` (16/9/2026: la demo ya NO fuerza el showcase de wormhole; cada ciclo elige nivel al azar 1..12, cualquier luna/efecto)**. **`DEMO_SPAWN_X_MIN/MAX` (50/850), `DEMO_SPAWN_Y_MIN/MAX` (80/350)**: la nave de la demo aparece siempre en una posición completamente aleatoria — X e Y al azar dentro de esa banda, con la cámara centrada al spawnear. `FOG_SCREEN_TOP=68` (cuadro de limpieza del HUD a la altura de MEM). **`SC`/`VW`/`TK` (TEMP, `test_scale`)**: etiquetas de debug de `ship.scale`, `viewScale` y `Tanker::drawScaleFor` en `(250,92)`/`(250,102)`/`(250,112)` bajo `#if SHOW_DEBUG_SCALES` — apagado por defecto (`#define SHOW_DEBUG_SCALES 0`); activar solo para validar el escalado/zoom en CRT.
