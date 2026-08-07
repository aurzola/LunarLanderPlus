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
     **Rediseño niebla (22/8/2026, rama twister-circ)**: la niebla adopta el mismo estilo de las
     bandas de Ganímedes — bandas elípticas concéntricas (`centerAt(i,x)` = arco `sqrt(1-t²)` con
     centro compartido `FOG_ELLIPSE_CX/RAD` + deriva vertical viva `FOG_DRIFT_A`) con degradado
     gaussiano por LUT de 256 entradas y `FOG_BRIGHT=44`; se quitan `FOG_WAVE_*`/`centerY`/`halfAt`.
     `FOG_SCREEN_TOP` baja de 100 a **68** (a la altura de la etiqueta MEM) para que el cuadro de
     limpieza del HUD sea más corto y no borre efectos; ok que DEMO quede tocada. `hidesShip()` usa el
     mismo `centerAt`. Validado en PC (`test_pc` ALL PASSED + `titan_demo`, nave oculta ~266 frames).
 21. **Anillos de roca de Ganímedes (6/8/2026; rediseño mayor 22/8/2026, rama `twister-circ`)**: en
     niveles de Ganímedes (`moonHasRings(level)`, `moonIndex==3` → nivel 4, 12, 20, 28…) el módulo
     `Rings` (`rings.h/cpp`, PC + composite) dibuja **2 bandas elípticas concéntricas** de rocas que
     siguen la curvatura de la luna (ya no son anillos orbitando un centro). `bandY` usa el arco
     `sqrt(1-t²)` con centro compartido `RING_ELLIPSE_CX/RAD` → forma suave, sin sinusoides ni bordes
     afilados. Banda alta `RING_CY_HIGH=360` (se cruza en la primera aproximación zoom-out) y banda
     baja `RING_CY_LOW=560` (se cruza en zoom-in). Cada banda mezcla:
     - Rocas pequeñas decorativas (`RING_SMALL_LOW/HIGH`, radio `RING_SMALL_MIN_R..MAX_R=1.2..3`),
       huecas (solo contorno), con dispersión vertical `RING_Y_JITTER=45` → NO colisionan.
     - Rocas grandes peligrosas (`RING_DANGER_MIN_R..MAX_R=7..13`, huecas + relleno suave `fillDanger`
       con dithering `pixelShade 170`), colisionan con `RING_ROCK_HIT·size + RING_SHIP_RADIUS` en
       `hitsShip` (solo iterando las danger).
     Los huecos se garantizan con `RING_GAP_MIN` (agrupan las danger apretadas pero pasables).
     `update` deriva en x con wrap y rota los polígonos. Golpear una roca grande → final "YOU CRASHED" /
     "STRUCK BY ORBITAL DEBRIS" (texto: centrado en zoom-out, debajo de la banda en zoom-in).
     NIEBLA de las bandas descartada (`RING_FOG_BRIGHT=0`) tras pruebas — Ganímedes queda sin niebla,
     solo rocas. Rendimiento (22/8/2026): el `drawFog` usaba `expf()` por píxel (~2 bandas × 320 col ×
     ~116 px ≈ 74k/frame en zoom) → reemplazado por una LUT gaussiana de 256 entradas creada una sola
     vez (indexación, sin expf por píxel). Fue el lag de este nivel. API tests: `active()`,
     `ringCount()`, `rocksInRing(i)`, `rockVisible(t,i,k,x,y)`, `rockDanger(i,k)`,
     `hitsShip(t,sx,sy,shipR)`, `lowerBandY(t,x)`. Validado en PC: `test_pc` ALL PASSED + `rings_demo`
     (PPM en `frames/`, dos bandas verticales separadas). TEMP al final: `DEMO_LEVEL_FORCE=6` (Titán),
     `START_LEVEL=4` (Ganímedes). Sync a `esp32LanderComposite/src/`. **Pendiente de prueba en CRT.**
22. **Torbellino de nitrógeno de Tritón (6/8/2026, rama `moon-flavor`)**: en niveles de Tritón
    (`moonHasTwister(level)`, `moonIndex==7` → nivel 8, 16, 24…) el módulo `Twister`
    (`twister.h/cpp`, PC + composite) crea un **vórtice de nitrógeno** que **deambula** por el mundo
    (`TWISTER_DRIFT_SPEED=8 u/s`, rebota en `[40,760]`, misma cota que `Terrain`), con `strength`
    aleatoria por nivel (`TWISTER_STRENGTH_MIN=0.5..MAX=1.4`) y sentido de giro `swirl` ±1. **Física
    de vórtice** (`apply(ship,terrain)`, hookeado en `Game::update()` justo tras `ship.update()`, en
    el mismo punto que el empuje de géiseres/atmósfera): si la nave está a `dist < TWISTER_RADIUS=150`
    del eje, se le aplica **succión radial hacia la base** (`TWISTER_PULL=0.0011·strength·prox`, con
    `prox=1−dist/radius`), **remolino tangencial** (`TWISTER_SPIN=0.9` sobre la velocidad lineal) y
    **hundimiento** (`TWISTER_SINK=0.7`) → la nave entra en espiral hacia la base. Mientras está
    **`captured()`**, cualquier contacto con el suelo la **destruye** → final "YOU CRASHED" /
    **"TWISTER SMASHED THE SHIP"** (nuevo `twisterCrash`, análogo a `lavaBurn`/`ringHit`). **Escape
    físico y sin dados**: si el **empuje radial** de la nave (componente del thrust a lo largo del
    vector saliente) supera la succión × `TWISTER_ESCAPE_MARGIN=0.15` **y** ya hay velocidad radial
    saliente, sale **lanzada** conservando la velocidad tangencial acumulada y recibiendo
    `TWISTER_FLING=0.25·strength` de impulso exterior + tirón de morro `TWISTER_SPIN_KICK=25°·swirl`
    (el jugador debe recuperar el rumbo; cooldown `TWISTER_ESCAPE_COOLDOWN=1.5 s`). La probabilidad de
    escape es **inversamente proporcional a la fuerza** porque el pull escala con `strength`: cuanto
    más fuerte el twister, más empuje radial se exige. **Dibujo** (pixelShade, sin fuentes extra):
    embudo cónico oscilante (base half `TWISTER_BASE_HALF=6` → top `TWISTER_TOP_HALF=34`, sway
    senoidal `TWISTER_SWAY_AMP=7` que crece hacia arriba + micro-oscilación), bandas horizontales de
    polvo en espiral dentro del embudo, remolino de polvo en la base y `TWISTER_ORBIT_COUNT=8`
    partículas orbitando la columna (hacen visible el giro). Integrado en `Game` junto a
    geysers/volcanos/atmósfera/anillos: `reset` en constructor/newGame/restartLevel/nextLevel/startDemo
    (y en el bucle de regeneración del demo), `update` tras `rings`, hook físico tras `ship.update()`,
    `draw` tras `rings.draw` antes de la nave, colisión en `checkCollisions` (tras lava, antes del
    terrain). API para tests: `active()`, `coreX()`, `coreY(terrain)`, `strength()`, `captured()`,
    `justEscaped()`. Validado en PC: `test_pc` **886 checks ALL PASSED** (nuevos `testTwister`:
    activación por nivel de Tritón, fuerza dentro de rango, captura y hundimiento de una nave parada
    dentro del radio, escape con empuje exterior, `Game::nextLevel()` → nivel 8 activo),
    `twister_demo <seed> 8` (PPM en `frames/`, selftest `active`; la nave entra en el radio y es
    succionada → `smashed=1` en los seeds probados). El demo pasó a fijarse en Tritón
     (`DEMO_LEVEL_FORCE=8`, TEMP de twister). Sync completado a `esp32LanderComposite/src/` (config/game.h/game.cpp/moons.h/twister.\*);
     `FOG_SCREEN_TOP=78` del composite conservado; sketch compila 506414 B (38%), RAM 7%.
     **Pendiente de prueba en CRT.**

    **Rediseño de la física del vórtice (19/8/2026)**: en CRT la nave **pasaba a través del twister sin
    verse afectada** (las aceleraciones `TWISTER_PULL/SPIN` eran demasiado débiles frente a la inercia).
    Nueva física en `Twister::apply()` (v2→v3 tras pruebas en PC): **grip tangencial** inyectado en la
    velocidad (órbita circular `TWISTER_ORBIT_SPEED=0.010 u/tick/u`, tope `TWISTER_ORBIT_MAX=0.35`,
    ganancia `TWISTER_HOLD_GAIN=0.06` rampeada en `TWISTER_HOLD_RAMP=20` ticks con `holdT_`) + **agarre
    radial** que reela hacia dentro (`TWISTER_RADIAL_INFLOW=0.30·strength` como deriva objetivo). La
    nave capturada **orbita y espirala al núcleo** (validado con `/tmp/tw_escape.cpp`: cruce a
    velocidad máxima 0.35 atrapado sin salir del radio; espiral d≈150→d≈12). **Escape por pelea**:
    solo si el empuje radial `> TWISTER_ESCAPE_THRUST=0.0011·strength` (se desactiva la inyección
    radial) **y** la velocidad radial saliente supera `TWISTER_ESCAPE_VEL=0.06` durante
    `TWISTER_ESCAPE_TICKS=25` ticks → la nave sale lanzada (`TWISTER_FLING=0.35·strength` +
    `TWISTER_SPIN_KICK=25°`) y el grip queda off hasta salir del radio (`escapeCooldown_=(dist+40)/flingV`).
    El requisito de **velocidad radial real** (no solo apuntar y apretar, que escapaba en 0.2 s) hace
    que el escape sea una pelea de ~2.5 s a fondo; al 40% no se escapa. **Tambaleo**: ruido en la
    **posición** (el de velocidad se acumulaba en aceleración y dominaba la órbita), `TWISTER_WOBBLE
    =0.15·strength·prox` + vaivén de morro `TWISTER_HEADING_KICK=2.0·strength·prox·sin`.
    `START_LEVEL=8` (TEMP, primer nivel jugable = Tritón) + `DEMO_LEVEL_FORCE=8` (TEMP).
    **Bug detectado en CRT**: los rayos de la tormenta (activa también en Tritón) causaban el efecto
    estroboscópico, cortaban el thrust (`setThrust(0)` durante `stormHitTimer`) y ponían `PWR ####`
    aleatorio en el HUD (el "POW se resetea"). Se apaga la tormenta en niveles de Tritón
    (`if (moonHasTitan(level) || moonHasTwister(level)) storm.setEnabled(false);` en los 5 sitios),
    igual que Titán — el torbellino es el clima propio de Tritón. `test_pc` **885 checks ALL PASSED**
    (test de escape actualizado a `TWISTER_ESCAPE_TICKS`); sync re-hecha (config/game/twister.\*),
    sketch compila 506766 B (38%). **Subido a placa 19/8/2026; pendiente re-probar en CRT.**

    **`POT_DISABLED=1` (19/8/2026, misma sesión de CRT)**: en la prueba, al subir el PWR con C+stick
    el nivel **se reseteaba a un valor menor o cero** de repente. Causa: el ADC del pot (GPIO34) es
    ruidoso y, al superar las 120 cuentas respecto a `potAtCycle`, disparaba el "last-used wins"
    (`pwrStickActive=false` → `powerLevel=potLevel`). El pot quedó **deshabilitado por flag** en el
    `.ino` (`readPotLevel`/`lowPass`/`smoothPot`/`potAtCycle` bajo `#if !POT_DISABLED`); la potencia
    ahora se fija **solo con C + stick** y, una vez activado `pwrStickActive`, ya no se desactiva.
    Sketch compila 501162 B (38%). **Pendiente de prueba en CRT.**

    **Twister ↔ viento excluyentes (20/8/2026)**: en CRT el usuario reportó que el minimapa, los
    indicadores y la nave **parpadean y se borran parcialmente**, y sospechaba del dibujo del viento
    (que además competía con el torbellino en el mismo nivel). Se añade `!moonHasTwister(level)` al
    cálculo de `windEnabled` en los 3 sitios (`newGame()`, `nextLevel()`, `startDemo()`, líneas ~74/
    115/156): en niveles de Tritón **no hay viento** (ni streaks ni polvo; `spawnWind`/`spawnDust`
    ya salen antes con `windEnabled=false`). Como el twister solo existe en Tritón, la exclusión
    queda en ambas direcciones. `test_pc` **889 checks ALL PASSED**; sync de `game.cpp` al composite;
    sketch compila 501250 B (38%). **Subido a placa 20/8/2026; pendiente re-probar en CRT**
    (verificar si el parpadeo/borrado persiste sin viento; si persiste, el causante es otro — el
    viento se dibuja antes que nave/indicadores/minimapa en `Game::draw`).

    **Twister física v4 + dibujo de embudo cónico (20/8/2026, misma sesión)**: en la re-proba en CRT
    el parpadeo **persistió sin viento** → nuevo sospechoso: el **dibujo del twister** en la 2ª
    etapa. Además se pidió (a) que la nave capturada quede **dentro de los límites del dibujo** del
    torbellino, tambaleándose/girando 270-360° y **descendiendo en espiral** siguiendo el vórtice, y
    (b) que el torbellino sea **más fino en la punta** que toca el suelo. Se rediseña la captura en
    `twister.cpp` `apply()`: la nave cabalga la **pared del embudo cónico** (dist→`coneR` con ease
    `TWISTER_CAPTURE_RAMP=40` ticks, `capOff_` = offset de captura, `dir` ±1), **weave horizontal**
    `posX = cx + dir·amp·cos(swirlAngle_)` con `swirlAngle_` auto-acumulado a
    `TWISTER_SPIRAL_RATE=90·strength °/s` y **descenso** `velY = TWISTER_DESCENT=45·strength u/s`
    (positivo hacia abajo; fix del bug de signo que invertía la altura — antes `velY` negativo
    cancelaba el hundimiento). Rotación = **`tumbleDeg_` acumulador** (`TWISTER_TUMBLE_RATE=110·strength
    °/s` → 270-360°+ en el descenso) + jitter seno `TWISTER_TUMBLE_JITTER=35·strength·sin(t·0.9+phase)`
    como **off-set no acumulativo** (antes el jitter se acumulaba en `rotation` → -3952°). El escape
    físico (pelear `>TWISTER_ESCAPE_THRUST·strength` + velocidad radial `>TWISTER_ESCAPE_VEL` durante
    `TWISTER_ESCAPE_TICKS` → fling + cooldown) no cambia. `draw()` reescrito: **embudo cónico de
    `TWISTER_TIP_HALF=2` (punta fina) en el suelo hasta `TWISTER_RADIUS` arriba**, bandas de polvo en
    espiral (`TWISTER_BAND_STEP=4`), bordes de pared brillantes (`pixelShade 210`), nube base y
    debris orbitando en la pared. Constantes viejas eliminadas de `config.h` (`TWISTER_RADIAL_INFLOW`,
    `TWISTER_SINK`, `TWISTER_WOBBLE/WOBBLE_FREQ`, `TWISTER_HEADING_KICK`, `TWISTER_BASE_HALF`,
    `TWISTER_TOP_HALF`). Validado con harness `/tmp/tw_spiral.cpp`: 3 trials → xoff ondea entre las
    paredes, h desciende 120→36/4, rotación acumula 204°/-275°/-451°, `captured=1` (el "ascenso" del
    trial 2 es el embudo siguiendo el terreno ascendente por el que deriva — la nave sigue dentro del
    cono dibujado). `test_pc` **889 checks ALL PASSED** (el check del tumble ahora usa 60 ticks: en 30
    el jitter de ±35° cancela el tumble aún pequeño; a 60 el acumulador domina). Sync composite
    (config/twister.\*); sketch compila 501582 B (38%). **PWR inicial al 50%** (`powerLevel=0.5f` en
    el `.ino`, antes 0). **Subido a placa 20/8/2026; pendiente re-probar en CRT** (parpadeo del
    twister, nave dentro del vórtice en espiral con tumble 270-360°, punta fina, PWR 50%).

    **Twister: vuelta al embudo tornado (20/8/2026)**: en CRT el embudo cónico v4 (punta 2 → radio
    150) se veía **como una pirámide**, "horrible e irreal". Se restaura la forma de tornado
    (`TWISTER_BASE_HALF=2.5` en la punta que toca el suelo → `TWISTER_TOP_HALF=34` arriba, sustituyen
    a `TWISTER_TIP_HALF`), con **la misma geometría en la física y el dibujo**: `coneR` de
    `apply()` usa `BASE_HALF→TOP_HALF` (no `→TWISTER_RADIUS`), así la nave capturada cabalga la
    pared del tornado y queda dentro del dibujo. `test_pc` 889 OK.

    **Parpadeo: causa raíz encontrada = tearing de framebuffer único (20/8/2026)**: el parpadeo/
    borrado parcial de minimapa, indicadores y nave **persistía sin viento y con cualquier dibujo
    del twister**, sobre todo en la 2ª etapa (zoom). Diagnóstico: la librería aquaticus usa **un
    único framebuffer** leído por DMA mientras `draw()` escribe; `video_wait_frame()` espera el fin
    del campo visible y `game.draw()` corre en la ventana de blanking (~2 ms). En zoom (×5) con
    efectos pesados el dibujo excede esa ventana y el DMA escanea contenido **a medio dibujar** →
    tear/flicker. **Fix (doble buffer)**: `fbShadow[76800]` en el `.ino`; `RendererESP32` pinta en
    el shadow y, justo tras `video_wait_frame()`, `memcpy(shadow → videoFB)` durante el blanking;
    el siguiente `game.draw()` pinta en el shadow durante el campo completo (~16 ms). El DMA solo
    ve frames completos → sin tearing en ninguna luna. RAM 101908 B (31%). **Subido a placa
    20/8/2026; pendiente re-probar en CRT.**

    **RAM: muestras de audio pasan a flash (20/8/2026) — el doble buffer ya arranca en placa**:
    el `fbShadow[76800]` estático (76.8 KB) dejó el heap tan partido que el FB de video de 76.8 KB
    ya **no cabía** (crash `StoreProhibited` en `memcpy_P` con destino NULL / `assert
    setup_video_signal video.c:253` "Failed to allocate 76800 bytes"; `largest_free_block` ~59 KB).
    Un pool único de audio de 118589 B tampoco entraba tras el FB de video. **Solución final**:
    `Audio::begin()` ya no copia las muestras a RAM — `thrustBuf/explBuf/windBuf/boltBuf` apuntan
    directo a los arrays PROGMEM (flash mapeado, `0x3f4xxxxx`) y el ISR (IRAM) las lee desde ahí
    (el juego no escribe flash en runtime → la caché de datos en el ISR es segura). Solo el beep
    (8 KB) se genera en RAM. El suavizado del tono del motor (antes low-pass de una pasada sobre la
    copia en RAM) ahora es un **one-pole IIR entero por muestra en el ISR** (`thrustPrev += (s-
    thrustPrev)>>1`, alpha ~0.5). Orden de inicio: `Audio::begin()` (8 KB) → `video_graphics()`
    (FB 76.8 KB cabe en la región grande del heap, free 227736→147744). **Verificado en placa por
    serial**: arranque limpio (sin FATAL/assert/reset), `VIDEO[dac] tx_start ok`, ISR de audio a
    ~16 kHz leyendo de flash (delta ~33042 cuentas / 2 s), nunchuck OK, `pwr=50`. Se quitaron los
    prints de debug temporales (`[audio] mem`, `[audio] flash buffers`, `[audio] buffers`,
    `[mem] after video`). Sketch 501422 B (38%), RAM globales 101908 B (31%). **Pendiente re-probar
    en CRT** (parpadeo con el doble buffer, forma de tornado del twister, PWR 50%).

    **Rama `twister-circ` + física v5 del twister (20/8/2026)**: el usuario comparó en CRT las dos
    versiones (`moon-flavor` tornado con bordes brillantes 210 vs `twister-circ` sin `pixelShade`) y
    **ambas se veían idénticas** y ninguna como la original (que no tenía bordes brillantes). Causa
    raíz: el borde brillante lo pintaba el **gradiente de las bandas** `b = 45+165·depth` (210 en la
    pared), no los `pixelShade`; y `TWISTER_BASE_HALF` 2.5 vs 6 son ~1 px vs 3 px en pantalla. Los
    PPM del 6/8 en `esp32Lander/frames/` resultaron ser del demo base (estrellas+terreno+nave), no
    del twister, así que el visual original no se pudo recuperar exacto. Se reconstruye en
    `twister-circ` — ver más abajo el **dibujo de líneas horizontales continuas** (el "embudo en V
    con densidad 118 interior `65+45·depth`/contorno 118" se descartó en CRT porque en los extremos
    se veían "lineitas o puntos" y el original "jugaba solo con líneas horizontales continuas").
    Además **física v5** pedida por el usuario:
    - **Rotación tambaleante ±60°** (`TWISTER_WOBBLE_RANGE=60`, oscilación seno, no más giro
      acumulado 270-360°): `rotation = wobble + stickDeg·TWISTER_STICK_GAIN(0.5)`, **hard cap
      `TWISTER_WOBBLE_MAX=80`** → el joystick nunca llega a ±90° dentro del vórtice. `apply()`
      recibe `stickDeg` (nuevo parámetro con default 0; `game.cpp` pasa `input.angle·180/π`).
    - **Espiral hacia abajo con escape justo fuera del cono**: `target = coneR·(1+TWISTER_EDGE_POKE
      (0.12)·sin(swirlAngle·2+phase))` → la nave asoma ~12 % fuera de la pared y vuelve, siempre
      dentro de `TWISTER_RADIUS`. **Zig-zag que cierra al caer**: el barrido pasa de `cos` a
      **onda triangular** `triWave(swirlAngle_)` (`posX = cx + dir·amp·tri`), `TWISTER_SPIRAL_RATE` de
      90 → **200 °/s** (varias reversiones) y, como el cono se estrecha al bajar, el zig-zag se cierra
      (span ~±60 → ~±12, `/tmp/tw_zz.cpp`). `s.velX = posX − weavePrevX_` (nuevo miembro, clamp
      `±TWISTER_ORBIT_MAX`).
    - **Escape por POW con dificultad por profundidad**: el umbral de "pelea" y la velocidad radial
      ahora escalan con `depth = 1 − h/TWISTER_HEIGHT` (0 arriba → 1 en el suelo):
      `escThr = 0.0011·(1+1.0·depth)`, `escVel = 0.06·(1+1.5·depth)`. Arriba escapa con ~60-85 %
      de potencia bien alineada; abajo (h≲85 u) es imposible. Se quitó `strength_` del umbral de
      escape (antes lo multiplicaba → dependía del azar). Validado con harness `/tmp/tw_v5.cpp`:
      wobble en [−60, 60] exacto, `rotAt90=0`, `poked=1`, escape arriba 5/5 trials (cualquier
      strength), escape abajo 0/5. `test_pc` **890 checks ALL PASSED** (test de escape movido a
      poca profundidad con rumbo radial calculado `atan2(dx,−dy)`; el check del tumble ahora mide
      el máximo durante el vuelo y verifica `<85`). Sync composite OK; sketch 501662 B (38%), RAM
      101908 B (31%). **Subido a placa 20/8/2026; pendiente re-probar en CRT** (embudo densidad
      118, wobble ±60 con joystick limitado, escape por POW).

    **Twister: líneas horizontales continuas (20/8/2026)**: en CRT el vaso de la "densidad 118"
    (interior `65+45·depth` + contorno 118) se veía con **"lineitas o puntos en los extremos"** y no
    como el original, que "jugaba solo con líneas horizontales continuas". La causa: los dos
    `pixelShade(cxx±half, sy, 118)` de contorno (formaban líneas de puntos en cada borde) y el
    `yoff = sin(...)·0.5` vertical que punteaba cada banda. Fix en `twister.cpp` `draw()`: se quitan
    los dos `pixelShade` de contorno y el `yoff`; cada fila es ahora una **línea horizontal continua**
    en `−half..half` con brillo `TWISTER_BAND_BRIGHT=120 + TWISTER_BAND_SWIRL=30·sin(rot+xx·0.25)`
    tope `TWISTER_BAND_MAX=160`, **sin ningún punto/borde en los extremos** (el largo de la línea,
    `2·half`, crece con el cono: corta abajo → larga arriba). `TWISTER_BAND_STEP=4→2.5` (más denso).
    Verificado en render ASCII: cono limpio de líneas contiguas sin puntos aislados. `test_pc` 890.
    Sync composite; sketch 511334 B (39%), RAM 101940 B (31%). Subido a placa.

    **Calibración de joystick en el juego (20/8/2026, `.ino`)**: el usuario reportó que el stick "no
    responde bien" — la calibración adaptativa fija el centro con 40 lecturas al boot (si no lo tenías
    al centro al encender, todo queda torcido) y las desviaciones derivan con EMA. Se añade un **modo
    de calibración visual**: manteniendo **C+Z a la vez en el título ~0.5 s** se entra en un overlay
    (`drawCalibration`) con instrucciones, una **caja sin relleno** (4 `line`) que mapea el rango del
    stick 0–255 y un **cursor** (rect 5×5) que muestra la posición en vivo. Se mueve el stick por
    todos los extremos (se captura `calMinX/MaxX/MinY/MaxY`); **C = guardar, START = cancelar**.
    `confirmCalibration()` deriva centro `(min+max)/2` y desviaciones por eje (mín 40), activa
    `useFixedCal=true` y persiste en **NVS** (`Preferences` `jsCal`: set/cx/cy/ld/rd/ud/dd);
    `loadCalibration()` en `setup()` los restaura al boot (imprime `[jsCal] loaded ...`). Con
    `useFixedCal` las desviaciones dejaron de adaptarse por EMA en `readStickAngle`/`readStickYDev`.
    Mientras se calibra, `loop()` no llama a `game.update()` (congela el fondo). Bug inicial: el
    overlay usaba `r.rect(0,0,320,240)` que **rellenaba** la pantalla de blanco (texto blanco sobre
    blanco → "pantalla en blanco"); se cambió a `r.clear()` (fondo negro) y la caja a solo borde.
    Sketch 511398 B (39%), RAM 101940 B (31%). Subido a placa; pendiente probar en CRT la
    calibración y el zig-zag del twister.

    **Rediseño del dibujo del twister como resorte/cola de cerdo (21/8/2026)**: el usuario no
    quedó conforme con el embudo tornado previo y pidió "olvidar toda indicación anterior sobre la
    forma" e implementar a partir de cero un **resorte helicoidal fino que se estrecha hacia el
    suelo** (cola de cerdo), **discontinuo** (líneas rotas/gaps) e **irregular** (jitter radial +
    brillos variables). **Física intacta**: solo se reescribió `Twister::draw()` (`apply()` y el
    resto del módulo no se tocaron). Iteraciones: (1) columna de hebras helicoidales hueca cerró
    como "no me gusta"; (2) resorte con 4 hilos rotos quedó "al revés" (ancho en el suelo, no en
    arriba); (3) corregir el volteo + **2 espirales en paralelo** → "mucho mejor". Se añadió luego
    una **tercera espiral solo en zoom-in** (cambio de firmas: `Twister::draw(...)` gana el
    parámetro `bool zoomedIn`; `game.cpp` pasa `zoomedIn`) — en PC el demo no la muestra porque
    dibuja con `zoomedIn=false`, solo se ve en CRT. Fix de visibilidad en zoom (el torbellino se
    escala ×5 y los puntos de 1px no se notaban): en zoom las partículas pasan a **blobs de 3–4 px
    con estela** (30, más rápidas `0.55` vs `0.35`) vs puntos finos en normal (12). Para que las 3
    espirales se distingan en zoom se reduce el nº de **vueltas a 5** (vs 7 en normal) y el trazo
    gana una línea tenue al lado (2/3 de brillo). Detalle del dibujo final: resorte que **arranca
    fino en el suelo y se ensancha hacia arriba** (`r = 4+30·tt`), cada espiral es una hélice
    discontinua con gaps pseudo-aleatorios deterministas por frame (`prand(seed+phase_)`), jitter
    radial, y brillo que desvanece con `front = 0.5+0.5·cos(ang)` (fondo de la bobina más tenue);
    labio superior tenue, motas de giro y falda de polvo en la base. Validado: `make && ./test_pc`
    **890 ALL CHECKS PASSED**; `./twister_demo 1 8` OK. Sync a `esp32LanderComposite/src/`.
    Sketch 511398 → 512802 B (39%), RAM 101940 B (31%). Subido a placa; aprobado en CRT.
