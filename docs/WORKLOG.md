# WORKLOG — Lunar Lander ESP32

Historial/bitácora del desarrollo (changelog). Contenido movido desde `AGENTS.md` para
dejar el contexto del agente principal liviano. Aquí vive la historia completa por ítem;
`AGENTS.md` solo conserva los pendientes activos.

---

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
21. **Anillos de roca de Ganímedes (6/8/2026, rama `moon-flavor`)**: en niveles
    de Ganímedes (`moonHasRings(level)`, `moonIndex==3` → nivel 4, 12, 20, 28…) el módulo `Rings`
    (`rings.h/cpp`, PC + composite) dibuja **dos anillos concéntricos de rocas** orbitando el centro
    `(RING_CX=400, RING_CY=260)`: el anillo **interior lento** (`RING_SPEED_INNER=0.06 rad/s`,
    `RING_ROCKS_INNER=22`, radio `RING_RADIUS_INNER=130`) y el **exterior rápido en sentido contrario**
    (`RING_SPEED_OUTER=-0.30`, `RING_ROCKS_OUTER=32`, radio `RING_RADIUS_OUTER=215`) → **los huecos
    nunca son estáticos**, esa es la dificultad: esquivar las rocas para que no te golpeen durante el
    descenso. **Solo la cara visible del anillo existe**: cada roca se dibuja/colisiona únicamente si
    queda **por encima de la silueta del terreno** (`rockVisible` filtra `y < terrainYAt(x)`), de modo
    que la cara lejana queda oculta por la propia luna (las rocas nunca aparecen "flotando" sobre la
    superficie). Dibujo: disco relleno (`pixelShade` 255 centro / 160 borde), radio de pantalla
    `RING_ROCK_RADIUS·viewScale` (mín 1 px). **Colisión** círculo-círculo
    (`RING_ROCK_RADIUS=6` + `RING_SHIP_RADIUS=12`) en `checkCollisions()`: golpear una roca en vuelo
    **destruye la nave** → final "YOU CRASHED" / **"STRUCK BY ORBITAL DEBRIS"** (nuevo `ringHit`,
    análogo a `lavaBurn`). Integrado en `Game` junto a geysers/volcanos/atmósfera: `reset` en
    constructor/newGame/nextLevel/startDemo (y en el bucle de regeneración del demo), `update` tras
    `atmosphere`, `draw` tras `drawWind` antes de la nave, colisión en `checkCollisions` (antes del
    terrain). API para tests: `active()`, `ringCount()`, `rocksInRing(i)`, `rockVisible(t,i,k,x,y)`,
    `hitsShip(t,sx,sy,shipR)`. Validado en PC: `test_pc` **865 checks ALL PASSED** (nuevos
    `testRings`: activación por nivel de Ganímedes, geometría de radios/anillos, colisión al
    posicionar la nave sobre una roca, no-colisón lejos, las posiciones cambian con el tiempo,
    `Game::nextLevel()` → nivel 4 activo), `rings_demo 1 4` (PPM en `frames/`, selftest `active` +
    `rocksVisible>0`; análisis del framebuffer confirma dos arcos concéntricos a radio 130 y 215 solo
    sobre la silueta). `DEMO_LEVEL_FORCE=6` (TEMP de Titán) inalterado. Sync completado a
    `esp32LanderComposite/src/` (config/game.h/game.cpp/moons.h/rings.\*); `FOG_SCREEN_TOP=78` del
    composite conservado. **Pendiente de prueba en CRT.**
