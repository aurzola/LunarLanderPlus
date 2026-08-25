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
    (deja libres `WIND`=62 y `REFUELING/DOCKING`=72 con viento) y **recorta también el halo del terreno**
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
23. **BUG PENDIENTE — render incompleto por la izquierda en el demo (23/8/2026, reportado en CRT)**: a
    veces el juego no se renderiza completo por el **margen izquierdo** de la pantalla, dejando una
    franja sin pintar o sin usar. Notado principalmente en el auto-demo / attract mode (aunque puede
    no ser exclusivo del demo). El bug convive con el doble buffer (el `memcpy(shadow→videoFB)` en el
    blanking copia el frame completo, así que no es tearing del DMA vertical). Hipótesis a investigar:
    (a) offset de inicio de línea horizontal (back porch del CRT / sincronización de la librería
    aquaticus) que desplaza el área visible; (b) algún `clear`/`rect` que no cubre el margen izquierdo
    en ciertos estados de cámara (el demo usa vista normal `viewX=0`); (c) deriva/desbordamiento en el
    render de efectos (niebla/rocas/anillos) que no pinta la primera columna. Referencia:
    `Game::draw()` (clear → storm.drawSky → atmosphere.drawSky → terreno → nave → minimapa → HUD) en
    `esp32Lander/game.cpp`. **Sin arreglo todavía; ajeno a los anillos de Ganímedes / niebla de Titán
    que ya están validados.**
 24. **Nave cisterna aérea con repostaje en vuelo (30/8/2026 → rediseño 7/9/2026, ronda 2 8/9/2026):** nave
     cisterna (nave espacial: fuselaje alargado, aletas, cabina, faro, brillo de motor) que aparece en
     niveles ≥2 (70 %) y **flota** a `TANKER_HOVER_ALT=140 u` sobre terreno plano con **deriva horizontal**
     (rebote en ±40 u) + **balanceo vertical** sinusoidal. El jugador debe **acoplar en vuelo** (docking
     aéreo): `checkDock()` alinea la trompa de la nave con el puerto inferior (`portY = bodyY + HULL_H/2`,
     tolerancias `TOL_X=8`/`TOL_Y=5`, `|velY|<0.09`, `|velX|<0.14`). **Zoom de docking (ronda 2)**: 
     `updateView()` fuerza zoom-in centrado entre nave y cisterna cuando `tanker.targeted()` y la nave está
     en la zona `DOCK_ZONE_X=90`/`DOCK_ZONE_Y=45`; al salir, lógica normal de altitud. Dock 0.5 s (nave
     enganchada siguiendo la deriva, línea de conexión + gotas), recarga a `FUEL_MAX` y se suelta **sin
     rebote** (`velX=velY=0`, ronda 2) para continuar el curso; la cisterna vuela arriba-derecha hasta salir.
     Si la nave embiste el casco, empuje suave hacia abajo. En demo forzado (`DEMO_LEVEL_FORCE>0`) el
     autopilot sube, acopla **30/30** en simulación (todos con zoom) y tras el refill re-apunta al pad
     **más cercano** con **fase de crucero** (`demoHoldAltitude`: mantiene altitud hasta llegar a la X del
     pad y descende solo encima → aterriza 21/30 tras el refill; antes 5/30 con descenso directo). El
     rediseño reemplaza al camión terrestre con plataforma elevada original (el docking no puede ser
     aterrizar sobre un vehículo inmóvil).
     Archivos: `tanker.h/cpp` en `esp32Lander/` y `esp32LanderComposite/src/`. API tests: `testTanker`
     (916 ALL CHECKS PASSED). Sketch ESP32: 520522 B (39%), RAM 109204 B (33%).
     TODO: sonido de repostaje (LEDC), minimapa con marcador de tanker, ajustar `TANKER_CHANCE_PERCENT`.
 25. **Nave cisterna aérea, ronda 3 (7/9/2026, rama tanker-r3):** rediseño de la aparición y el docking.
     - **Aparición por combustible (petición del usuario)**: la cisterna **solo aparece cuando el nivel
       de combustible está bajo** (`fuel < FUEL_MAX · TANKER_FUEL_FRACTION`, `TANKER_FUEL_FRACTION=0.5`),
       para que el rendezvous solo ocurra cuando importa. `Tanker::reset()` recibe `float fuel`; el modo
       demo/attract (`force=true`) ignora el chequeo. `TANKER_CHANCE_PERCENT` se mantiene (70 %) como
       segunda condición.
     - **Altura**: `TANKER_HOVER_ALT` 140→**300 u** (fuera del alcance de los efectos de superficie).
       En Titán (`moonHasTitan`) `baseY = TANKER_TITAN_Y = 85` fijo, por encima del techo de la niebla
       (~108). **Excluida de Ganímedes** (`moonHasRings`, nivel 4): la banda alta de anillos de roca
       (`RING_CY_HIGH=360`) la destruiría; el torbellino de Tritón no alcanza (vuela alto).
     - **Dos puertos de repostaje (ronda 3)**: boquilla **inferior** (`bodyX, portY`) y boquilla de
       **nariz** (`nosePortX() = bodyX − HULL_W/2`, `nosePortY() = bodyY`). `checkDock()` elige puerto
       según la rotación del módulo: `rotation <= TANKER_NOSE_ANGLE` (−45°) → nariz; si no → inferior.
       Alinea el **centro de la boquilla del módulo** (`posX + NOZZLE_LEN·scale·sin(rad)`,
       `posY − NOZZLE_LEN·scale·cos(rad)`, `TANKER_NOZZLE_LEN=8`) con el puerto objetivo (tolerancias
       `TOL_X=8`/`TOL_Y=5`, `|velY|<0.09`, `|velX|<0.14`). `beginDock()` engancha a la boquilla
       correspondiente y fija la rotación (inferior: pos bajo el puerto, rot 0; nariz: colgando de la
       nariz, rot −90) siguiendo deriva/bob.
     - **Visual (ronda 3)**: boquilla del módulo dibujada en escena solo durante la maniobra
       (`tanker.active && zoomedIn && dentro de la zona`, sin lava/niebla): línea de `5·scale` a
       `NOZZLE_LEN·scale` a lo largo de `(sin,−cos)` + círculo brillante. Anillo de boquilla de nariz
       en la cisterna. HUD `DOCKING` parpadeante al entrar en la zona (`tanker.targeted()`), antes de
       `REFUELING` durante el dock. El bloque docked **ya no lee `input.angle`** (la rotación la fija
       el enganche).
     - Validación: `test_pc` **928 ALL CHECKS PASSED** (testTanker reescrito: full tank no spawnea,
       forzado ignora combustible, Ganímedes excluida incluso forzada, Titán `baseY==TANKER_TITAN_Y`,
       dock inferior con rot 0, dock frontal con rot −90, no-dock con rot 0 en la nariz, velocidad
       excesiva rechazada, drift/bob, refill, secuencia leaving→done). Simulación 30 seeds con
       `DEMO_LEVEL_FORCE=2`: **30/30 docks, 21/30 aterrizajes post-refill** (mismo ratio que la ronda 2).
       Sketch ESP32: 521442 B (39%), RAM 109212 B (33%). **Subido a la placa** (`/dev/ttyUSB0` apareció
       después; upload ronda 3 completado y verificado).
     - Archivos: `config.h` (constantes TANKER), `tanker.h/cpp`, `game.cpp` (4 call sites de `reset`,
       HUD `DOCKING`, boquilla del módulo), `test_pc.cpp`, `tanker_demo.cpp` (frames de dock/leaving en
       `frames/frame_0000.ppm` y `_0001.ppm`).
  26. **Nave cisterna aérea, ronda 4 — probe-and-drogue (8/8/2026, rama tanker-r4):** la mecánica pasa a
      **probe-and-drogue** (dos mangueras con cesta) con **repostaje incremental** y **colisión letal**
      contra el casco de la nodriza. Petición del usuario: más tiempo conectado = más combustible.
     - **Dos hoses con drogue**: la cisterna extiende `TANKER_HOSE_LEN=20 u` de manguera bajo la panza
       (hacia abajo) y por la nariz (hacia la izquierda), cada una terminada en **cesta drogue** con
       sway sinusoidal independiente (`TANKER_DROGUE_SWAY=3 u`, `TANKER_DROGUE_SWAY_SPEED=1.6 rad/s`,
       `droguePhase`). `drogueX(0)=bodyX+sway`, `drogueY(0)=portY+HOSE_LEN+sway`; `drogueX(1)=
       nosePortX()−HOSE_LEN+sway`, `drogueY(1)=bodyY+sway`. El módulo introduce su **probe** (la boquilla
       `NOZZLE_LEN=8` ya dibujada) en la cesta.
     - **Colisión contra el casco (nodo central)**: `hitsHull(x,y)` (caja `HULL_W/2+3` × `HULL_H/2+3`)
       → **se destruyen AMBAS naves**: `tankerCrash=true`, `tanker.destroy()`, `ship.crash()`, final
       `"BOTH DESTROYED" / "COLLIDED WITH THE TANKER"` (`tankerCrashGet()`). `destroy()` deja
       `active=false, done=true`; la colisión se comprueba antes de `checkLanding`.
     - **Refuel incremental**: `TANKER_REFUEL_RATE=200 fuel/s` mientras hay conexión (se quitó
       `TANKER_REFUEL_TIME`). En `update()` docked, `fuel += RATE·dt` hasta `FUEL_MAX` → `leaving=true`.
       **Podar antes** (`breakAway(Ship&)`, nuevo): suelta el probe con pequeño impulso de separación y
       **la cisterna se queda en estación** para reconectar; el fuel acumulado se conserva (el HUD lo
       refleja: `game.cpp` hace `fuel=ship.fuel` cada frame). En el bloque docked, `input.thrust>0` (no
       demo) dispara `breakAway`.
     - **Demo AI de 2 fases** (`demoTankerPhase`): la nave no puede decelerar lo bastante para frenar en
       la cesta (empuje limitado) y cruzaba la banda del casco (`y∈[384,402]`) en la bajada → crash
       total en sim. Fix: fase 0 desciende en una **pre-posición `TANKER_APPROACH_X=55 u` a la izquierda
       del drogue** (la bajada nunca cruza la banda del casco), cambia a fase 1 (|Δx|<8, |Δy|<12) y
       **desliza en horizontal a la altitud del drogue** (por debajo del casco) hasta clavar el probe.
     - Validación: `test_pc` **947 ALL CHECKS PASSED** (testTanker: dock solo contra drogue, cesta sway,
       refill incremental —60 frames no llenan—, `breakAway` conserva fuel y deja la cisterna en
       estación, reconexión, `hitsHull` letal + `destroy` la desactiva, dock nariz con rot −90). Sim 60
       seeds `DEMO_LEVEL_FORCE=2`: **60/60 docks, 47/60 aterrizajes post-refill, 0 choques contra el
       casco**. Sketch ESP32: 523430 B (39%), RAM 109220 B (33%). **Upload ronda 4 a `/dev/ttyUSB0`
       completado y verificado.**
     - Archivos: `config.h` (HOSE_LEN/APPROACH_X/DROGUE_SWAY/SWAY_SPEED/REFUEL_RATE/HULL_MARGIN, sin
       REFUEL_TIME), `tanker.h/cpp` (`drogueX/Y`, `breakAway`, `hitsHull`, `destroy`), `game.cpp/h`
       (`tankerCrash`, `demoTankerPhase`, demo AI 2 fases, HUD REFUELING), `test_pc.cpp`,
       `tanker_demo.cpp`. `esp32LanderComposite/src/` sincronizado (`diff` limpio, `DEMO_LEVEL_FORCE=2`).
  27. **Nave cisterna aérea, ronda 5 — cesta proporcional + PiP de docking (8/8/2026, rama tanker-r5):**
      la cesta drogue deja de ser un rectángulo 6×6 que "parece que atrapará toda la nave" y pasa a un
      **cono truncado hueco proporcional** (boca ~6 u, algo menor que la nave ~6.4 u), y el zoom forzado
      a pantalla completa del acople se sustituye por una **ventana picture-in-picture** de contacto.
     - **Cesta proporcional**: `drawDrogue` (ahora pública estática de `Tanker`, compartida con el PiP)
       dibuja un cono truncado HUECO cuyo fondo es un **objetivo relleno** (`fillTarget`, círculo sólido
       `TANKER_DROGUE_BACK_R=1.6 u` + outline) = el punto físico de alineación (`drogueX/drogueY`).
       Boca `TANKER_DROGUE_RIM=3 u` (≈6 u de ancho) < nave ~6.4 u → atrapa solo la punta del probe;
       profundidad `TANKER_DROGUE_DEPTH=3.5 u` + anillo intermedio. kind 0 = boca abajo, kind 1 = boca
       izquierda (nariz). La **física no cambia**: `checkDock` sigue alineando el centro de la boquilla
       con `drogueX/drogueY` (TOL_X=8/TOL_Y=5).
     - **Mangueras como mangueras**: `drawHose` sustituye la línea recta por una **polilínea de 5
       segmentos** con doblez sinusoidal perpendicular (`1.4·s·sin(t·π)`) y endpoints fijos → leen como
       manguera flexible, no como rayo. Se usa para ambos hoses y para el tramo del dock.
     - **PiP de docking (en vez del zoom forzado)**: `updateView()` ya no fuerza zoom en la zona de
       dock; el viewport queda en vista normal con ambas naves + mangueras, y `drawDockingPiP` pinta en
       una ventana de esquina (recuadro blanco, abajo-derecha, `PIP_SIZE=76`, `PIP_MARGIN=8`) el punto
       medio entre cesta y punta del probe magnificado a `PIP_SCALE=SCREEN_H/700·16` px/u. Si la pareja
       está más lejos de lo que la ventana muestra, hace **zoom-out de ajuste** (`sc=fitHalf/dist`, suelo
       `PIP_SCALE/3`) para que nunca quede una caja negra vacía; a distancia de alineación vuelve a
       escala máxima. `port` se elige igual que `checkDock` (rotación ≤ −45° → nariz). Crosshair de 6 px
       sobre el objetivo. Se muestra mientras `tanker.active && !done && (docked || en zona)`.
     - **Probe rediseñado**: `drawProbe` (estática en `game.cpp`) dibuja una **varilla fina de 2
       carriles** desde la panza/nariz hasta la boquilla + **punta de diamante** (triángulos superior e
       inferior + base). Se dibuja en escena durante la maniobra (`tankerDockShow`, **sin exigir
       `zoomedIn`**) y también dentro del PiP.
     - **Clip del renderer**: `Renderer` gana `setClip/clearClip` (puras virtuales); `RendererCanvas` los
       implementa (`clipOn`/`clipX..H`, `clipTest`, `px`/`pxShade`) y TODAS las primitivas (`line`,
       `rect`, `circle`, `text`, `textScaled`) dibujan vía `px`/`pxShade` → el recorte funciona también
       en `RendererESP32` (que solo hereda). El PiP pinta fondo negro opaco + cesta + probe + crosshair
       dentro del clip y el marco blanco fuera.
     - **Fix encontrado durante la validación visual**: la ventana se centraba en `pipX` (esquina) en vez
       de `pipX+P/2` → la composición quedaba desplazada medio marco a la izquierda. Y `pwy` usaba `-`
       con `uy` ya negado → el punto medio se desplazaba 16 u y la cesta salía de la ventana. Verificado
       con composiciones estáticas (acercamiento inferior alineado / desalineado, nariz rot −45°, lejos
       con zoom-out) y con el dock real (probe enchufado en la cesta, boom colgando por la boca).
     - Validación: `test_pc` **947 ALL CHECKS PASSED**; sim 60 seeds `DEMO_LEVEL_FORCE=2`:
       **60/60 docks, 47/60 aterrizajes post-refill, 0 choques** (la física no cambió). Sketch ESP32:
       524974 B (40%), RAM 109220 B (33%). Sin upload (pendiente probar en CRT).
     - Archivos: `config.h` (DROGUE_RIM/DEPTH/BACK_R, PIP_SIZE/PIP_MARGIN/PIP_SCALE), `renderer.h` y
       `renderer_canvas.h/cpp` (clip), `tanker.h/cpp` (`drawDrogue` estática, `drawHose`), `game.h/cpp`
       (`drawDockingPiP`, `drawProbe`, `updateView` sin zoom de dock), `.gitignore` (tanker_demo).
       `esp32LanderComposite/src/` sincronizado (`diff` limpio, `DEMO_LEVEL_FORCE=2`).
  28. **Nave cisterna aérea, ronda 5b — manguera única + zoom macro + PiP arriba-centro (8/8/2026, rama tanker-r5b):**
      - **Vuelta al zoom macro de dock (9/8)**: el usuario pidió recuperar el zoom-in forzado de la aproximación
        (que ronda 5 había sustituido por el vista general) y dejar el PiP **solo** como vista de precisión fina del
        probe & drogue. `updateView()` ahora **fuerza `setZoom(true)`** en la zona de dock (`tanker.active &&
        !tanker.done && |dx|<TANKER_DOCK_ZONE_X=90 && |dy|<TANKER_DOCK_ZONE_Y=45` vs `portY`) y centra el viewport
        en el **punto medio nave↔cisterna** (`midX/midY` = `(ship+body)/2`, `viewX/viewY` = mitad de pantalla −
        mid·viewScale), con `return` antes de la lógica de altitud. Fuera de la zona → lógica normal por altitud
        (`ZOOM_IN_ALT=200`/`ZOOM_OUT_ALT=350`). Anteriormente `updateView` hacía `if (tankerZone) { if (zoomedIn)
        setZoom(false); }` (los dos ships + manguera en general view y el punto de contacto solo en el PiP).
      - **Minimapa suprimido en la zona de dock (punto 3 confirmado por el usuario)**: el minimapa es útil en la
        fase de acercamiento (baja altitud) y el docking/refuel se hace a mayor altura → sin conflicto; en la zona
        de dock se **salta el minimapa** (`if (zoomedIn && !dockZone)`), dejando el hueco arriba-centro al PiP.
        El PiP ya vivía arriba-centro (`pipX=(SCREEN_W−PIP_SIZE)/2`, `pipY=22`); comentarios ajustados.
      - **Manguera única (1/8 → pedido «la izquierda quitala, déjemos una sola»)**: se eliminó la drogue de
        **nariz (puerto 1)** — `nosePortX()/nosePortY()`, `dockPort` y `TANKER_NOSE_ANGLE` borrados. Solo queda la
        **cesta inferior** (`drogueX()/drogueY()` sin argumento = `bodyX+sway` / `portY+HOSE_LEN+sway`). `checkDock`
        siempre apunta a la cesta inferior (alinea el probe con `drogueX()/drogueY()`, rotación correcta al acoplar
        desde abajo); `Tanker::update` docked solo cuelga la nave erguida (rot 0°) bajo la cesta; `breakAway` solo
        empuje vertical; `Tanker::draw` dibuja **una** manguera (`drawHose`) + **una** cesta (`drawDrogue`, sin `kind`),
        con los drops de fuel solo verticales; `drawDockingPiP` sin selección de puerto. Se quitó `PIP_MARGIN` (sin uso).
      - **Probe & drogue con mejor calidad visual**: `drawProbe` — varilla de 2 carriles con **carril trasero dim
        `lineShade` 150** + **centro dim 110**, **collar** en la base (línea perpendicular `1.05·scale·sc`), y punta de
        **diamante relleno** (secciones transversales por filas, `steps=ceil(tipF)`, `hw=tipW·(1−t)`) + outline + ápice
        brillante. `drawDrogue`/`fillTarget` — **bullseye**: disco relleno dim 150 + **core brillante** (`rect`) +
        outline; el cono con **ribs** dim (`lineShade` 120) + **lip** (2 líneas en la boca) + **barbs** en los extremos
        del rim. **`lineShade` (ronda 5b)**: nuevo primitivo puro `Renderer` (`lineShade(x0,y0,x1,y1,b)`) implementado
        en `RendererCanvas` como Bresenham con `pxShade` (respeta clip).
      - **Altitud de la cisterna (ajuste posterior)**: `TANKER_HOVER_ALT` 300→**340 u** para que la altitud de la nave
        en el dock (~340 −(HULL_H/2+HOSE_LEN+sway+probe·scale) ≈ **307 u**) quede holgadamente **por encima del umbral
        del minimapa** (`ZOOM_IN_ALT=200`) y por debajo de `ZOOM_OUT_ALT=350`. Así durante **toda** la maniobra la nave
        está fuera del rango donde el minimapa empieza a mostrarse y nunca choca con la ventana PiP.
      - Validación: `test_pc` **942 ALL CHECKS PASSED** (se quitaron ~5 checks del puerto 1; test de nariz sustituido
        por un negativo de rotación 90° que desvía el probe en Y). Sim 60 seeds `DEMO_LEVEL_FORCE=2` a 340 u:
        **60/60 docks, 0 choques**, aterrizajes post-refill 45–47/60 (ruido de semilla; ronda 4 era 47/60). Sketch ESP32:
        524978 B (40%), RAM 109212 B (33%). **Upload a `/dev/ttyUSB0` completado y verificado (hash + RTS).**
      - Archivos: `config.h` (HOVER_ALT=340, sin NOSE_ANGLE/PIP_MARGIN), `renderer.h` + `renderer_canvas.h/cpp`
        (`lineShade`), `tanker.h/cpp` (manguera/cesta única, sin dockPort/nosePort, drawDrogue sin kind),
        `game.cpp` (updateView zoom macro, minimap gate, drawProbe/drawDockingPiP), `test_pc.cpp`, `tanker_demo.cpp`.
        `esp32LanderComposite/src/` sincronizado (`diff` limpio salvo `DEMO_LEVEL_FORCE=2`).
   29. **Nave cisterna aérea, ronda 5b (refine 3) — mini-juego de docking + cisterna grande en zoom out (8/8/2026):**
       Petición del usuario: convertir el acoplamiento/repostaje en un **mini-juego de mantenimiento** durante toda la
       conexión, con probe visible sin círculo brillante en la punta y cisterna más grande solo en zoom out (hitbox sin
       cambios).
      - **Hold de 1 s + mini-juego de corrección**: `Tanker` añade `dockOffsetX/Y`, `dockLockTimer`, `fuelFlowing` y
        `dockBreakTimer`. `beginDock(vx,vy)` engancha con offset inicial suave. `update()` docked convierte la entrada
        del joystick (`ship.velX`) en **empujón horizontal** del probe dentro de la cesta; un **muelle de centrado**
        suave devuelve el probe al punto de alineación con stick neutro. Hay que mantener el probe dentro de
        `TANKER_DOCK_TOL_X=8`/`TANKER_DOCK_TOL_Y=5` durante `TANKER_DOCK_LOCK_TIME=1.0 s` para que empiece a fluir el
        combustible; si se sale de `TANKER_DOCK_BREAK_TOL_X=12`/`TANKER_DOCK_BREAK_TOL_Y=8` durante
        `TANKER_DOCK_BREAK_TIME=0.4 s`, `breakAway()` libera el probe. El combustible sigue fluyendo incrementalmente
        (`TANKER_REFUEL_RATE=200/s`) mientras se mantenga alineado; al llenarse la cisterna se va.
      - **Probe rediseñado**: `drawProbe` ahora dibuja una **flecha sólida triangular** en la punta (sin círculo
        brillante), con la base brillante en el punto de contacto físico (`TANKER_NOZZLE_LEN`). El vástago sigue siendo
        una varilla fina de dos carriles.
      - **Feedback visual en el PiP**: `drawDockingPiP` añade un **anillo de estado** alrededor de la cesta (verde =
        repostando, amarillo = alineando/enganchado, rojo = desalineado) y una pequeña cruz que marca la posición real de
        la punta del probe respecto al asiento.
      - **Cisterna más grande solo en zoom out**: `Tanker::draw()` usa `drawScale = viewScale*1.6f` para el globo del
        zeppelin cuando `viewScale < 1.0f`; en zoom-in forzado se conserva la escala actual. La hitbox física
        (`hitsHull`) y el punto de enganche (`portY`, `drogueX/Y`) **no cambian**.
      - **Demo AI adaptado**: durante `tanker.docked` el autopilot corrige el offset horizontal con el stick para mantener
        el probe centrado y evitar rupturas. El breakaway por motor solo afecta al jugador (`!demo`).
      - **HUD**: mientras está enganchado pero aún no fluye combustible muestra `DOCKING` parpadeante; una vez fluye,
        `REFUELING` sólido.
      - Validación: `test_pc` **949 ALL CHECKS PASSED** (nuevos checks: hold de 1 s sin fuel, ruptura tras 0.4 s fuera de
        tolerancia, fuel fluye tras lock). `tanker_demo 1`: dock frame 0, refill completo frame 595. `demo_sim 60 seeds`
        con `DEMO_LEVEL_FORCE=0`: **47% win rate** (sin cambios; el tanker no forzado no afecta la mayoría de demos).
        Sketch ESP32: **525086 B (40%)**, RAM **109236 B (33%)**. **Upload a `/dev/ttyUSB0` verificado.**
       - Archivos: `config.h` (`TANKER_DOCK_LOCK_TIME/BREAK_TIME/BREAK_TOL_*`), `tanker.h/cpp` (`dockOffsetX/Y`,
         `fuelFlowing`, hold/break logic, escala visual condicional), `game.cpp` (nudge desde `input.angle`, feedback PiP,
         probe flecha sólida, demo AI docked, HUD DOCKING/REFUELING), `test_pc.cpp`. `esp32LanderComposite/src/`
         sincronizado (`DEMO_LEVEL_FORCE=2`).
   29b. **Visual plutónico de la cisterna (22/8/2026)** — globo y góndola rediseñados:
       - **Globo**: elipsoide relleno por filas con `lineShade` (brillo 120→160, más claro al centro) + contorno 255 en
         los bordes + arco de resalte superior (200) + **franja oscura a media altura** (3 filas, brillo 60→80) que
         reemplaza a la antigua línea central resaltada (que se leía como un corte).
       - **Góndola**: cabina aerodinámica con contorno `\___|` — nariz en diagonal que toca el casco (`gNoseTopX,gy`),
         panza plana (`___`) y popa vertical (`|`) pegada al mismo `gy` que el borde inferior del globo, sin arista de
         techo separada. **Rellenada** como el globo (trapezoide con degradado `lineShade` 150→120 descendiendo,
         izquierda inclinada de `gNoseTopX` a `gNoseBotX`), conservando ventana/luz 230. **Se quitaron los 2 cables de
         soporte** que la hacían parecer colgando (ahora es una cabina pegada al casco).
       - Validación: `test_pc` **950 ALL CHECKS PASSED**, sketch ESP32 **576038 B (43%)** / RAM 109260 B (33%).
         `tanker.cpp` sincronizado en `esp32LanderComposite/src/`; **upload a `/dev/ttyUSB0` verificado**.

(End of file - total 689 lines)
30. **Paracaídas dirigible one-shot por nivel (23/8/2026)** — ver AGENTS.md "Paracaídas":
    - **Diseño (confirmado con el usuario)**: desplegable una sola vez por nivel con **C+Z** en
      vuelo (edge trigger; en el título C+Z sigue abriendo la calibración). Motor permitido
      mientras está abierto (variante B) → un toque de motor hace el flare para aterrizar
      perfecto. Sin motor = aterrizaje hard (sin bonus de fuel). Apertura **ignorada bajo
      `PARACHUTE_MIN_ALT=80`** con aviso `TOO LOW` parpadeante (el dosel no abriría a tiempo).
    - **Física** (`Ship::update()`): el dosel se infla `PARACHUTE_OPEN_TIME=0.5 s`; frena la caída
      hacia `PARACHUTE_SINK=0.09` **solo si cae más rápido** (con `-gravity` dentro del brake para
      que la velocidad terminal sea exactamente el sink, no sink+gravity/rate). El flare del motor
      puede bajar de sink (aterrizaje perfecto). **Dirigible**: el stick ya no rota, **desvía
      lateralmente** `velX += sin(angle)*PARACHUTE_STEER=0.0012` (con `chuteOpen` como rampa);
      `Game` auto-nivela la rotación a 0° con la vela; el viento actúa como **vela ×2**
      (`PARACHUTE_WIND_GAIN=2.0`), tope horizontal `PARACHUTE_DRIFT_MAX=0.30`. Un rayo en modo vela
      "revuelve" el steering en vez de la rotación.
    - **Visual** (`Ship::draw()`): paquete plegado 4×2 px sobre el casco mientras disponible;
      desplegado = **cúpula** (arco elíptico con vértice centrado, corregido: la 1ª versión lo
      dibujaba invertido con los picos en los bordes) + **borde festoneado** (4 chevrons) + 5
      líneas de suspensión (`lineShade` 120) + **relleno de tela** por filas con `shadedLine`
      (brillo 30→15, media elipse `sqrt(1-t²)`). Todo escala desde el top del casco (dy=-5) con
      `chuteOpen` en coordenadas locales del ship. `crash()`/`reset()` limpian el chute.
    - **HUD**: `CHUTE` sólido en `(22,220)` mientras disponible, parpadeante desplegado,
      `TOO LOW` parpadeante 1.2 s al rechazar apertura. Línea de título
      `C+Z: PARACHUTE (1/LEVEL)`.
    - **Demo**: el autopilot no despliega el chute en v1 (herramienta solo del jugador).
    - Validación: `test_pc` **963 ALL CHECKS PASSED** (8 checks nuevos: brake→sink, flare por
      debajo de sink, tope horizontal, viento ×2, reset limpia, deploy alto, one-shot, rechazo
      bajo + timer). `parachute_demo 1`: open=1.00 velY=0.090 sink=0.090. Sketch ESP32:
      **575790 B (43%)** / RAM 110540 B (33%). `esp32LanderComposite/src/` sincronizado
      (config.h, ship.h/cpp, game.h/cpp); `.ino` añade `lastBothPressed` (edge C+Z) y salta el
      ajuste de potencia mientras ambos están pulsados. Pendiente prueba en CRT.

31. **Port a VGA paralelo (23/8/2026, rama `vga-out`)** — segundo ESP32 con salida SVGA 640x480@60
    monocromo, CRT intacto. Plan: `docs/PLAN_VGA.md`.
    - **Decisión (con el usuario)**: dos placas dedicadas (composite ↔ VGA), driver bitluni
      `VGA8BitDACI` embebido como fuente, `MODE640x480` con escalado 2x software del canvas
      320x240, patrón de prueba primero.
    - **Estructura**: `esp32LanderVGA/` = copia del composite; se quitaron `video.h/c` y
      `renderer_esp32.*`; `.ino` reescrito (loop idéntico, `static Game`, `VGA_TEST_PATTERN`).
    - **`src/renderer_vga.{h,cpp}`**: `RendererVGA : RendererCanvas` con doble buffer —
      `game.draw()` pinta en `fbBack` (estático 76.8 KB); `flush()` = `waitVBlank()` +
      `memcpy` a `fbFront`. `fbFront` es **heap** (el DRAM estático no da para dos buffers de
      76.8 KB; patrón del composite).
    - **`src/esp32lib/`**: ESP32Lib embebido (VGA/, Graphics/, I2S/, Tools/; podado de drivers
      no usados). **Fork de `VGA8BitDACI`**: `setFrameStore(store,W,H)` +
      `allocateFrameBuffer()` mapean las filas del driver dentro del store 320x240 (sin malloc
      de 300 KB, no hay PSRAM); `interruptPixelLine()` escala 2x horizontal (repite el byte en
      las dos muestras del 32-bit) y el `interrupt()` 2x vertical (`y>>1`). `waitVSync()`
      bloquea hasta el blanking vertical.
    - **`I2S_ESP32.cpp` retocado para IDF5** (core Arduino 3.3.10): `#include "driver/dac.h"`
      (compat vía `-iwithprefixbefore driver/deprecated`) y `rtc_clk_apll_*` con firma nueva.
    - **Pines**: GPIO25 (DAC1) → divisor **270Ω×3** en paralelo a R/G/B (verificado por el agente
      hardware: el `100/100/220` de bitluni es para 2 DACs y en mono recortaría a 1.414 V, spec
      0.7 V, con tinte; 270 Ω da 0.717 V neutro); HSYNC GPIO32, VSYNC GPIO33 directos (TTL);
      audio/nunchuck/pot/start igual que el composite (el `.ino` lleva `CONTROLS_WIRED=1`).
    - **Compila**: `--fqbn esp32:esp32:esp32 --build-property build.partitions=no_ota` →
      **552546 B (42%)** flash, RAM estática **111940 B (34%)**, heap ~215 KB (76.8 KB al `fbFront`).
    - **`sync.sh`** (raíz): copia las fuentes compartidas de `esp32Lander/` a ambos sketches y
      verifica con `cmp` que queden idénticas (no toca `video.*`, renderers, `esp32lib/`, audio,
      nunchuck). Ejecutado: ambos árboles `OK`.
    - **Pendiente en hardware**: cablear el segundo ESP32 + conector DE-15, flashear con
      `VGA_TEST_PATTERN=1`, verificar rampa de grises/rejilla/marco, y pasar a `0` (juego).
     - Docs actualizados: `AGENTS.md` (sección "Port a VGA" + pinado board VGA + comandos),
       `docs/hardware.md` (sección VGA), `docs/PLAN_VGA.md` (estado → implementado).
  32. **Ganímedes: altura de banda baja + frame-skip (9/8/2026)**: la segunda banda de rocas
      (`RING_CY_LOW`) estaba a 560 (prácticamente al nivel del terreno) → subida a **710** para que
      cruzar la banda coincida con el inicio del zoom-in (`alt<200`, ZOOM_IN_ALT). Banda baja queda a
      Y=655–710 (centro–bordes), 55–210 u sobre el terreno. **Frame-skip**: la banda baja (46 rocas)
      se dibuja cada 2 frames (`mutable frameCtr_` en `Rings`, banda 0 siempre dibuja); física y
      colisiones siguen cada frame. Reduce ~50% el coste de dibujo de la banda más densa.
      Verificación: `test_pc` 963 OK, `rings_demo` OK, sketch compila 576 KB (43%).
  33. **Relleno sólido de polígonos con grises reales (10/8/2026, rama `polygon-fill`)**: la versión
      VGA se veía "plana" porque todo se dibujaba solo con líneas blancas. Solución con grises reales
      del DAC (sin dithering, sin patrones):
      - **Nuevas primitivas**: `rectShade(x,y,w,h,brightness)` y `fillPolygon(xs,ys,n,brightness)`
        (scanline fill para polígonos convexos) en `Renderer` / `RendererCanvas`.
      - **Nave**: shapes cerrados (0=ascenso gris 140, 1=descenso gris 100) rellenos con
        `fillPolygon`; ventana con `rectShade` 160. Contornos blancos encima.
      - **Terreno**: columnas verticales con `lineShade` 80 bajo cada píxel de la superficie
        (masa sólida gris oscuro bajo la silueta).
      - **Título**: módulo Apollo Eagle relleno (descenso 100, ascenso 140, panel 100, ventana
        160, cajas laterales 120).
      - **No afecta al CRT en negativo**: el phosphor ya difumina; el fill añade un tono base
        sutil que da más cuerpo. Código compartido en `RendererCanvas`.
      Tests PC: 960 OK. Sketch VGA: 583 KB (44 %), subido a placa (`Hash of data verified`).
  34. **Relleno sólido de las rocas de Ganímedes (10/8/2026, rama `polygon-fill`)**: las rocas grandes
      peligrosas del anillo (`Rings::fillDanger`) se rellenaban con **dither** (`pixelShade` 170 en
      1 de cada 4 px, patrón `(x+y)&2`), heredado de la época de líneas blancas. Se sustituye por
      **relleno sólido** `fillPolygon` a brillo 170 + contorno blanco encima (`tracePoly` después del
      relleno para que el borde quede siempre visible), mismo estilo que la nave (#33). Las rocas
      pequeñas decorativas siguen huecas. `fillDanger` queda en dos líneas (se elimina el scanline
      local; lo hace `RendererCanvas::fillPolygon`). Sync a ambos sketches (`sync.sh` OK).
      Verificación: `test_pc` 951 OK, `rings_demo` OK (PPM; max_run de 170 = 7 px → relleno sólido,
      con dither sería ~1). Docs: `AGENTS.md` actualizado.
  35. **Terreno procedural: zonas de aterrizaje más anchas + fix de caja de la nave en returns
      tempranos (10/8/2026, rama `polygon-fill`)**: 
      - `Terrain::generate()`: cada zona landable aplanaba 5 puntos (`zoneStart..zoneStart+4`, 4
        segmentos → ~19–31 u). Ahora aplanan **7 puntos** (`zoneStart..zoneStart+6`, 6 segmentos →
        ~28–46 u, típico 30–40; medido con un tool de ancho de zonas sobre 5 semillas × niveles
        2/7/12). `zoneStart` pasa de `(NP-12)*j/4` a `(NP-20)*j/4` para que las zonas no pisen la
        frontera de wrap. Aterrizar es más indulgente en niveles ≥ 2 (caja de la nave 6.4).
      - `Game::update()`: los dos `return` tempranos del `STATE_PLAYING` (intro de nivel y el
        path de choque con anillo antes de `checkCollisions()`) **recalculan `ship.left/right/
        bottom`** para que `checkLanding`/`altitude` usen la caja fresca (antes quedaba la de un
        tick anterior → colisiones/altura potencialmente erróneas durante la intro).
      - Texto de crash del anillo bajado para no pisar la banda: zoom-out 108/120 → **116/128**,
        zoom-in offset +18 → **+36**.
      Verificación: `test_pc` 951 OK, `rings_demo` OK, demo_sim (win-rate estable). Docs:
      `AGENTS.md` actualizado (plataformas ~28–46 u).
  36. **Choque: polvo de regolito + cráter en el terreno (25/8/2026, rama `crash-dust`)**:
      - **Polvo de impacto** (`Ship`): al estrellarse `initGroundParticles()` levanta 40
        partículas desde la línea de contacto de las patas (`posY + 14·scale`), con distribución
        40/40/20 de tamaños punto / `+` de 5 px / roca 3×3 rellena, brillo propio 0.7–1.0, que
        siguen la velocidad del impacto (`velX·0.4 + jitter`, `velY` ascendente con la caída del
        ship; ×2.5 en explosión de combustible), arquean con `GROUND_PARTICLE_GRAV=0.018` y se
        desvanecen en `GROUND_PARTICLE_LIFE=70` ticks (fade 200→0 según `life`). Se dibujan en
        `Ship::draw()` cuando `exploding`; se limpian en `reset()`. Config: `GROUND_PARTICLES_MAX`,
        `GROUND_PARTICLE_LIFE`, `GROUND_PARTICLE_GRAV`.
      - **Cráter de choque** (`Terrain`): `setCrater(x, halfW)` guarda el punto de impacto y
        `draw()` **recorta la polilínea** en ese tramo: segmento entero dentro del cráter se omite,
        parciales se cortan por interpolación (helper `interp`), dejando un **hueco abierto** de
        `CRATER_HALF_W=3.5` u (~7 u = ancho de la nave) sin relleno ni borde — solo indica que ahí
        hubo un choque. `Game` lo activa en crash duro (`result==1`) y smash del torbellino y lo
        limpia (`terrain.clearCrater()`) en `newGame`/`restartLevel`/`nextLevel`/`startDemo`.
      - **Config de demo/partida**: `DEMO_LEVEL_FORCE=0` (demo elige nivel al azar 1..12, ya no el
        showcase forzado) y `START_LEVEL=1` (partida ordenada desde LUNA). Sincronizado a ambos
        sketches con `sync.sh`.
      - **Limpieza de AGENTS.md**: se eliminaron secciones obsoletas — el **minimapa 96×49**
        (arriba-centro 112,22) que ya no se dibuja en el código, el relleno del suelo por columnas
        `lineShade` 80 (eliminado en `a285373` "Remove terrain fill") y la flechita de minimapa de
        los indicadores de aterrizaje; se actualizó el TEMP de demo (`DEMO_LEVEL_FORCE`).
      Verificación: `make && ./test_pc` 951 OK, render PC del cráter (rotura visible en la
      polilínea, comprobada por diff de PPM), `sync.sh` OK, sketch compila 579 KB (44 %).
  37. **Escala unificada de la cisterna (26/8/2026, rama `fuel-tanker`)**: hasta ahora el globo
      se dibujaba con `drawScale = viewScale·(1.6 si zoom-out)` pero los accesorios (góndola,
      aletas, faro, motor, manguera, cesta y gotas de combustible) usaban `viewScale` a secas —
      en zoom-out la cisterna parecía desproporcionada (globo grande, accesorios diminutos) y en
      zoom-in quedaba pequeña. Fix: un **factor global `TANKER_DRAW_SCALE=1.25`** aplicado a
      `drawScale` para TODO el dibujo (`drawScale = viewScale·(1.6 si zoom-out)·1.25`), manteniendo
      los accesorios proporcionales al globo en ambas vistas. Las posiciones en mundo (física de
      docking, `portY`, `drogue`) NO cambian: solo crece el dibujo. Verificado con bench de render
      PC (bbox del tanque completo: zoom-out 16→20 px y zoom-in 50→63 px de ancho = ×1.25 exacto;
      píxeles >90: 55→106 y 644→1073). `config.h` + `tanker.cpp` + AGENTS.md; `make && ./test_pc`
       951 OK, `sync.sh` OK, sketch compila 579 KB (44 %).
   38. **IDEA PENDIENTE — Port a consolas retro (27/8/2026, diferido)**: se evaluó portar el juego a
       las consolas disponibles del usuario (NES, SNES, Mega Drive, Wii, GameCube, PS2, Xbox 360).
       El core es C++ std puro con `Renderer` abstracto → portar = "escribir un `Renderer` + entrada
       + audio". **Ranking por facilidad**: (1) **Wii** — 729 MHz/88 MB, framebuffer 1:1
       (devkitPPC+libogc), y el **nunchuck es el mando nativo** (joystick=ángulo, Z=motor, C=potencia,
       Start=paracaídas) → coincide con los controles actuales; sin hardmod (LetterBomb+HC vía SD).
       (2) **Xbox 360** — el más potente (3.2 GHz/512 MB) pero requiere consola **RGH-moddeada**.
       (3) **GameCube** — mismo toolchain que Wii (devkitPPC/libogc), sin hardmod (SD Media Launcher/
       Swiss), sin nunchuck (remapeo a pad GC); ~90% del trabajo de Wii. (4) **PS2** — PS2SDK,
       framebuffer, FreeHDBoot; fricción GS/DMA + remapeo a DualShock. (5) **Mega Drive** — SGDK pero
       **sin framebuffer** → reescritura de render a tiles/sprites. (6) **SNES** — devkitSNES, PPU/
       modo 7. (7) **NES** — no realista (6502, 1.79 MHz, 2 KB RAM). **Decisión**: se aplaza; primero
        se pule la versión ESP32 actual (hay muchas mejoras pendientes). Si se retoma → **Wii primero**
        (camino Wii→GC casi regalado). NO implementado.
   39. **Polvo de choque: velocidad de lanzamiento igualada a los trozos + gravedad lunar (27/8/2026)**: el
       usuario reportó que las partículas de la explosión contra el terreno salían mucho más rápido que
       los trozos de la nave. Comparación real (mismas unidades): trozos 0.05–0.30 u/tick vs partículas
       0.15–1.20 u/tick (jitter horizontal ±1.2). Además la gravedad de las partículas era una constante
       fija `GROUND_PARTICLE_GRAV=0.018` (36× la de la nave), sin escalar por la luna. Fix:
       - `initGroundParticles()`: jitter horizontal `(rand()%2400)-1200` → `(rand()%700)-350` (±0.35);
         empuje vertical `(rand()%750)+150` → `(rand()%250)+80` (0.08–0.33). Las partículas quedan en el
         mismo rango que los trozos (0.08–0.35 u/tick).
       - `updateExplosion()`: `p.velY += GROUND_PARTICLE_GRAV * (gravity/GRAVITY)` — el arco se escala por
         la gravedad de la luna (la nave ya la propaga en `game.cpp`). Encélado/Tritón flotan ~30 % más.
       - `GROUND_PARTICLE_GRAV` 0.018 → **0.012** (arco más suave; con el escalado, Io ≈ 0.0132 ≈ antes).
        Verificación: `make && ./test_pc` 951 OK, `sync.sh` OK, sketch compila 579 KB (44 %). Pendiente
        re-probar en CRT.
   40. **Pads de ancho variable por puntaje (1/9/2026)**: el usuario pidió que los landing spots de mayor
       puntaje sean **más estrechos (difíciles)** que los de menor puntaje, pero nunca imposibles.
       Antes todas las zonas aplanaban el mismo número de segmentos (4 en clásico, 6 en procedural).
       Fix en `terrain.cpp`:
       - **Clásico (`init()`, nivel 1 = Luna)**: 5x→3, 4x→4, 2x→5 segmentos. Medido: pads de
         13.5 / 18.9 (5x), 20.2 (4x) y 37.8 u (2x). El más angosto (13.5 u) supera la caja de la nave
         en aterrizaje (~9.6 u = `±10·ship.scale` con scale 0.48 en zoom 5×) con ~4 u de holgura.
       - **Procedural (`generate(level)`)**: 5x→4, 4x→5, 2x→6 segmentos (20–42 u; el 5x más angosto
         20.2 u vs caja 9.6). En **Ganímedes** (`moonHasRings`) cada pad suma **2 segmentos** (6/7/8)
         porque se aterriza en zoom 2× con caja ~24 u (`setZoom(true, 2.0f)` → scale 1.2) — los pads
         quedan 35–54 u, siempre por encima de la caja.
       - `zoneCenterX`, labels, multiplicadores y `chuteZone` se recalculan con el ancho real de cada
         zona (antes asumían 4/6 líneas fijas). El label "Nx" queda centrado en el pad ya estrechado.
       - Sin cambios en `checkLanding()` (usa la zona `landable` contigua real).
        Verificación: `make && ./test_pc` 957 OK, medición propia de anchos por seed (gradiente
        5x < 4x < 2x en clásico y procedural, y 6/7/8 en Ganímedes), `./demo_sim` win-rate 45 % → 33 %
        (60 seeds; los pads altos ahora cuestan más, esperado), `sync.sh` OK, compila composite 579 KB
        (44 %) y VGA 585 KB (44 %).
   41. **Veredicto de aterrizaje fijo al tocar (1/9/2026)**: el usuario reportó que a veces decía
       `PERFECT LANDING` pero no daba el +50 de gasolina. Causa real: en `STATE_LANDED` la física
       **sigue corriendo** (`game.cpp` `ship.update()` durante el mensaje). El bonus se decidía una vez
       en el touchdown, pero el mensaje **re-evaluaba `ship.velY` cada frame** (condición
       `ship.velY < LAND_PERFECT_VY` en el draw). Con el **paracaídas desplegado** (sink 0.09 ≥ 0.075,
       → aterrizaje hard sin +50), el frenado de la vela seguía bajando `velY` durante el mensaje y a
       los ~0.3 s cruzaba 0.075 → la pantalla mostraba `CONGRATULATIONS / PERFECT LANDING` aunque el
       bonus nunca se había dado. Fix:
       - Nuevo campo privado `landPerfect` (`game.h`) + getter `landPerfectGet()`. Se fija **una sola
         vez** al entrar en `result==2`: `landPerfect = ship.velY < LAND_PERFECT_VY;` y el bonus (+50
         fuel, `50×mult`) usa ese mismo valor.
       - El mensaje de `STATE_LANDED` usa `landPerfect`: perfecto → `CONGRATULATIONS / PERFECT
         LANDING`; aterrizaje seguro no perfecto → **`GOOD LANDING`** (se quitaron `HARD LANDING` /
         `HOPELESSLY MAROONED`, que además eran engañosos: un aterrizaje suave con vela a sink 0.09
         decía "marooned" sin más fuel). Ahora "PERFECT LANDING" **solo sale si se dio el +50**.
       - Nuevo test `testLandingBonus()`: tocar el pad clásico (mult 4) con `velY=0.05` → `STATE_LANDED`
          + fuel 900→950 + `landPerfect`; con `velY=0.10` → `STATE_LANDED` + fuel sin cambio + `!landPerfect`.
        Verificación: `make && ./test_pc` 963 OK, `sync.sh` OK, compila composite 579 KB (44 %) y
        VGA 585 KB (44 %).
        **Ajuste (2/9/2026)**: el usuario pidió ver la recompensa: "no veo la recompensa, tengo el ojo
        en el landing spot". El +50 de fuel ya no se suma en el touchdown (donde la vista está en la
        plataforma, no en el HUD): el touchdown guarda `landFuelBonus = landPerfect ? 50 : 0` y el
        `resetTimer` de la transición `STATE_LANDED` lo aplica a `ship.fuel` (tope `FUEL_MAX`) justo
        antes de `nextLevel()`. Así aterrizas con 300 y ves **350** en el contador `FUEL` del nivel
        siguiente, ya relajado. El bonus de score (`50×mult`) sigue en el touchdown. Aterrizar perfecto
        con fuel 0 sigue salvando la partida (el +50 se aplica antes del chequeo `ship.fuel<=0`).
        Test actualizado: `testLandingBonus()` ahora verifica fuel 900 en el touchdown y 950 tras pasar
        de nivel (loop de `update()` hasta salir de `STATE_LANDED`); el pad se busca sobre el terreno
        actual (clásico o procedural) con 4 segmentos `landable` contiguos. Verificación: `make &&
        ./test_pc` **967 OK**, `sync.sh` OK, compila composite 579 KB (44 %) y VGA 585 KB (44 %).

    42. **Wormhole en el cielo (efecto visual, rama `event-horizon`, 2/9/2026)**: el usuario pidió un
        "black hole en forma de espiral que se trague la nave y la haga aparecer en otra luna al azar".
        Decisión: validar **solo el visual** primero (fase teleport queda pendiente). El usuario eligió:
        nombre `wormhole`, posición **en el cielo**, nave con **coreografía guionada**.
        - `wormhole.h/cpp` (clase `Wormhole`, C++ std): máquina de fases
          `WH_IDLE → WH_EMERGING (1.2 s) → WH_PULLING (3 s) → WH_SWALLOW (0.15 s) → WH_DYING (0.8 s) → WH_IDLE`.
        - `reset(cx,cy)` (sin hookup en `Game`); `update(dt)` anima los brazos
          (`rot_ += spin·WORMHOLE_SPIN·dt`); `pullShip(Ship&)` solo en `PULLING` guiona la órbita:
          espiral logarítmica `rr = CORE + (R0−CORE)·(1−p)` con `θ = A0 + spin·p·3·TAU`, `scale` 1.0→0.25
          y nariz apuntando al núcleo; `swallowed()` al entrar en `SWALLOW`.
        - **Dibujo** (`draw(r,viewX,viewY,viewScale)`): 3 brazos espirales logarítmicos
          (`r = OUTER·(CORE/OUTER)^tt`, `WORMHOLE_OUTER_R=120`/`CORE_R=8` u) aplastados en Y
          (`SQUASH=0.65`) → disco de acreción inclinado; polilíneas `shadeSeg` con jitter `prand`
          determinista y gaps; brillo creciente hacia el núcleo (`45+185·tt^1.5`); **núcleo oscuro**
          (`fillPolygon` brillo 0 a 1.4×coreR) que se traga estrellas y las vueltas internas; **anillo
          fotónico** (`circle` 255 + halo punteado tenue) dejando el interior vacío; ~20 partículas que
          cabalgan los brazos y derivan hacia el núcleo (`tt += rot·k`). Fades por fase
          (`EMERGING` spin-up, `SWALLOW` destello breve, `DYING` apagado).
        - Ajuste de validación: en PC el núcleo no leía como hueco (glow central + vueltas internas
          brillantes + disco pequeño). Fix: núcleo oscuro más grande (1.4×) + quitar glow central +
          `CORE_R=8`/`OUTER_R=120`. Perfil de brillo por anillos verificado por script sobre los PPM
          (vacío en r=0-1, anillo brillante r=2-4, espiral hasta r≈35 px).
        - `wormhole_demo.cpp`: args `<seed> <level>`, agujero en (x 260-560, y 150-200) mundo, nave
          arranca a un lado; selftest fases `EMERGING`→`PULLING`→`SWALLOW` + `swallowed()`. PPM a
          `frames/` cada 5 frames (~115).
        - `Makefile`: `wormhole.cpp` en `SRC` + target `wormhole_demo` (en `all`/`clean`).
          `sync.sh`: `wormhole.cpp/h` añadidos a `SHARED`.
        - `test_pc.cpp`: nuevo `testWormhole()` (reset activo, fase `PULLING` tras EMERGING, la nave se
          acerca al núcleo, `SWALLOW`+`swallowed()`, vuelve a `IDLE`).
        - Verificación: `./test_pc` **978 OK**, `./wormhole_demo 1 1` / `2 5` OK, storm/twister demos
          OK, `sync.sh` OK, compila composite 579 KB (44 %) y VGA 585 KB (44 %).
    43. **Wormhole hookeado en `Game` — fase teleport (fecha actual)**: se cierra el PENDIENTE de la
        entrada 42. `Game` dispara el agujero por nivel y al tragar la nave la teletransporta a otra luna.
        - **Spawn (`Game::spawnWormhole(bool force)`)** en `newGame`/`nextLevel`: nivel ≥
          `WORMHOLE_START_LEVEL=2` y `rand()%100 < WORMHOLE_CHANCE_PERCENT=25`; **excluido de Tritón**
          (`moonHasTwister`). Posición fija en el cielo: `cx ∈ [200,550]` mundo, `cy = WORMHOLE_SKY_Y=110`.
        - **Showcase en el attract demo (ver ítem 43)**: en modo demo el spawn es una decisión
          explícita — solo el primer nivel del demo (`DEMO_WORMHOLE_FIRST`) abre el wormhole y lo
          coloca **sobre el spawn** (`cx = ship.posX+40`) para que la tragada + teleport se vea siempre;
        - **Captura selectiva (`Wormhole::pullShip` ahora devuelve `bool`)**: solo captura si la nave
          está dentro del alcance de la espiral (`d ≤ WORMHOLE_GRAB_R=120`) **y** volando alta
          (`posY ≤ cy + WORMHOLE_GRAB_BELOW=80`); si nunca entra en alcance, al expirar `PULLING` el
          agujero se desvanece (`DYING`) **sin** `swallowed()` (antes tragaba sí o sí). Al capturar se
          reinicia `t_` para que la órbita de 3 s completa se vea siempre.
        - **Teleport (`Game::wormholeJump()`)**: al `swallowed()` en `STATE_PLAYING` →
          `level = 9 + nidx` con `nidx = rand()%8 != moonIndex(level)` (otra luna, banda de dificultad
          media) y `nextLevel()` → terreno nuevo, intro, **fuel conservado**. En demo (ítem 43) →
          `wormholeJump()` + `setupDemoTarget()`: el autopilot **continúa** volando en la luna destino
          (antes `endDemoToTitle()`).
        - **Física**: `wormhole.pullShip()` se llama tras el twister; si devuelve `true` (capturada)
          se **salta `checkCollisions()`** (la nave está siendo arrastrada al cielo, no al suelo).
          El paracaídas no despliega mientras `wormhole.captured()`.
        - **Dibujo**: `wormhole.draw()` tras `storm.drawBolts` → el **núcleo oscuro (brillo 0) pinta
          encima de la nave** ya encogida en el `SWALLOW` → la desaparición es orgánica, sin corte.
        - Config nueva: `WORMHOLE_START_LEVEL`, `WORMHOLE_CHANCE_PERCENT`, `WORMHOLE_SKY_Y`,
          `WORMHOLE_GRAB_R`, `WORMHOLE_GRAB_BELOW` (config.h).
        - `test_pc.cpp`: `testWormhole()` extendido (navaja lejos → se desvanece sin tragar; en alcance
          pero baja → no captura; **integración en `Game`**: captura → teleport a luna distinta con
          fuel intacto). Verificación: `./test_pc` **987 OK**, `wormhole_demo` OK, `demo_sim` OK
          (con el showcase del ítem 43), `sync.sh` OK, compila composite 585 KB (44 %) y VGA
          590 KB (45 %).
        - **PENDIENTE (audio)**: one-shot de warp al `wormholeJump()` (estilo explosión/rayo) —
          sonido aún no generado; el teleport ya funciona sin él.

    43. **Wormhole en el attract demo (10/9/2026, rama `event-horizon`)**: el usuario pidió que el
        agujero de gusano **se muestre también en la demo** ("ponlo como primer nivel de la demo"),
        manteniendo el `demo_sim` determinista (la física del wormhole es determinista, así que la
        demostración es reproducible seed a seed). Hoy no requiere tocar el sketch VGA.
        - `config.h`: `DEMO_WORMHOLE_FIRST = true` — el primer nivel del demo abre el showcase.
        - `Game::startDemo()`: tras `ship.reset()` → `spawnWormhole(DEMO_WORMHOLE_FIRST)` +
          `setupDemoTarget()`. El bloque de selección de target (tanker/lava/pad) de `startDemo` se
          extrajo a `Game::setupDemoTarget()` para poder re-elegir target tras el teleport.
        - `Game::spawnWormhole(bool force=false)`: en modo demo `!force` → no spawn (los niveles
          posteriores del demo y el destino del teleport quedan **sin** wormhole); `force=true` →
          coloca el agujero **sobre el spawn** (`cx = ship.posX+40`, `cy = WORMHOLE_SKY_Y`) para que la
          captura + órbita de 3 s + tragada se vean siempre. Tritón (nivel 8) y LUNA (nivel 1) quedan
          fuera por diseño (`moonHasTwister` / `level < WORMHOLE_START_LEVEL`).
        - `Game::update()`: al `swallowed()` en demo → `demoHoldAltitude=false; wormholeJump();
          setupDemoTarget();` — el autopilot **sigue** en la luna destino hasta aterrizar/estrellarse
          y volver al título (antes `endDemoToTitle()`). `wormholeJump()` → `nextLevel()` → la intro
          del nivel destino cubre la transición.
        - Verificación: `./test_pc` 987 OK; probe ad-hoc (8 seeds) → wormhole `active`, fase
          `EMERGING`→`PULLING`, `sawPulling`, y `endLevel` en 9..16 distinto del inicial (teleport);
          `demo_sim 8` sin timeouts (2/8 WIN) con niveles finales 9..16; `sync.sh` OK; compila
          composite 585 KB (44 %).



    44. **Cisterna: regla de fuel incondicional + posición en Titán; wormhole más bajo/grande/
        elíptico y showcase garantizado en la demo (10/9/2026)**. Ajustes pedidos probando en CRT:
        - **Cisterna solo con fuel < 50 % (siempre)**: `Tanker::reset` quita el bypass
          `!force && fuel > FUEL_MAX·TANKER_FUEL_FRACTION` → ahora el fuel gate se aplica también
          con `force=true` (el demo/attract ya no muestra la cisterna con el tanque lleno). El
          `force` solo salta nivel-1 y chance. `test_pc` actualizado (`forcedSpawn` con fuel 0.4×,
          nuevo `fullForced` con FUEL_MAX → nunca activa).
        - **Cisterna en Titán entre las dos bandas de niebla**: `TANKER_TITAN_Y` de 85 → **250**
          (banda 1ª centrada en ~205, 2ª en ~343+, 3ª bajo el terreno; la cisterna queda en el hueco
          libre, más cerca de la 1ª banda). Antes `=85` ("arriba del todo").
        - **Wormhole más bajo y más grande**: `WORMHOLE_SKY_Y` 110 → **220**, `WORMHOLE_OUTER_R`
          120 → **200**, `WORMHOLE_CORE_R` 8 → **16**, `WORMHOLE_GRAB_R` → **200** (el disco se
          asienta sobre el terreno y es claramente visible). Muestreo del espiral `M` 160 → 300
          (radio mayor).
        - **Espiral elíptico, no circular**: `SQUASH` 0.65 → **0.5** y el **anillo fotónico** ya no
          se dibuja con `circle()` sino como **elipse** (48 puntos con el mismo squash); el núcleo y
          las partículas ya iban aplastados. Todo el disco lee como elipse inclinada.
        - **Showcase garantizado en la demo**: `startDemo` con `DEMO_WORMHOLE_FIRST` **re-tira el
          nivel** (`while level < WORMHOLE_START_LEVEL || moonHasTwister(level)`) → el primer nivel
          del demo siempre puede alojar el wormhole (excluye LUNA nivel 1 y Tritón 8). Cuando el
          showcase está activo se apagan **todos** los demás efectos: viento (`windEnabled=false`),
          tormenta, géiseres, volcanes, atmósfera, anillos y torbellino (`setEnabled(false)`, nuevo
          método inline en cada módulo); la cisterna no aparece (fuel lleno). Solo el wormhole en un
          nivel limpio (terreno + estrellas + pads).
        - Demo: al `swallowed()` en demo → **`endDemoToTitle()`** (vuelve al título; una partida
          real sí hace `wormholeJump()`). Antes el demo seguía volando en la luna destino.
        - Verificación: `./test_pc` **990 OK** (incluye `testDemo` con `sawWormhole` — el demo
          termina en el trago, que es un outcome válido), `demo_sim` sin timeouts (todos LOSE ~10 s:
          el showcase traga siempre, esperado), `sync.sh` OK, compila composite 585 KB (44 %),
          subido a la placa.

45. **Wormhole: vórtice continuo al capturar + escala normal en el showcase (12/8/2026)**.
    Reportado en CRT: al aparecer el agujero la nave se veía más pequeña y, al cruzar el punto de
    no retorno, **brincaba** a otro punto de la elipse.
    - **Nave pequeña en el showcase (demo)**: `setupDemoTarget()` hacía `ship.reset(sx, cy)`, y
      `Ship::reset()` deja `scale = 1.0`; la escala de vista normal es **1.5** (`setZoom(false)`),
      así que la nave del showcase volaba a 2/3 de tamaño mientras el agujero estaba presente.
      Fix: `ship.scale = 1.5f` tras el reset del showcase (AGENTS.md). Verificado en PC:
      `shipScale=1.50` antes y después de la intro del demo.
    - **Salto al capturar**: `startAng_ = atan2(dy, dx)` (ángulo nave→núcleo) con
      `posX = cx + cos(ang)·rr` colocaba a la nave en el **punto simétrico** de la elipse (una
      nave 70 u a la izquierda aterrizaba 140 u a la derecha). Fix: `captureRad_`/`startAng_` se
      calculan en el sistema **aplastado por `SQUASH`** (`rdx = posX−cx`, `rdy = posY−cy`,
      `captureRad = hypot(rdx, rdy/SQUASH)`, `startAng = atan2(rdy/SQUASH, rdx)`) → en `p=0` la
      espiral pasa **exactamente** por la posición de la nave (X e Y continuos, sin blend).
    - `test_pc.cpp`: test de continuidad del vórtice (horizontal y vertical) que **detecta el bug**
      (FAIL `|posX−340| < 20` con el código viejo; PASS con el fix). Verificación: `./test_pc`
      **1033 OK**, `wormhole_demo`/demo 25/25 seeds tragan + teletransportan, `demo_sim 40`
      winRate 22% (sin regresión), compila composite + VGA, subido a la placa.

46. **HUD: se quita el aviso `TOO FAST`; las etiquetas `VX`/`VY` hacen flashing (12/8/2026)**.
    El usuario pidió eliminar el banner `TOO FAST` y, en su lugar, que las etiquetas `VX`/`VY`
    parpadeen según corresponda.
    - `game.cpp`: se elimina el bloque `TOO FAST` y la variable `fastY`. Las etiquetas `VX`/`VY`
      (en `(250,32)`/`(250,42)`) ahora parpadean (`ship.counter % 50 >= 30`, se omite el texto)
      mientras se juega (`STATE_PLAYING` y `introTimer <= 0`) cuando `|velX| > LAND_HARD_VX`
      (parpadea `VX`) o `velY > LAND_HARD_VY` (parpadea `VY`). `warnY` del aviso
      `REFUELING`/`DOCKING` sigue desplazándose con WIND (62→72).
    - Docs: `AGENTS.md` (sección HUD), `config.h` comentario de `FOG_SCREEN_TOP`,
      `docs/WORKLOG.md` ítem 201. Verificación: `./test_pc` **1033 OK**, compila composite + VGA,
      subido a la placa.
47. **Wormhole: banner "YOU'VE BEEN RECYCLED" al aparecer en la otra luna (12/8/2026)**.
    Tras el teletransporte, al materializarse la nave en la luna destino se muestra durante 4 s
    el texto `CONGRATULATIONS,` / `YOU'VE BEEN RECYCLED!` (mayúsculas, dos líneas, centrado).
    - `game.cpp`: nuevo campo `recycledTimer` (0 por defecto). `wormholeJump()` lo arma con
      `WORMHOLE_RECYCLED_T` (4 s, `config.h`); `update()` lo decrementa cada tick (clamp ≥ 0) y
      `draw()` lo pinta centrado en `(90,102)` con `textScaled` escala 1, con **fade-out** en los
      últimos 0.8 s (`brightness = 255·t/0.8`). Se resetea a 0 en `newGame`/`restartLevel`/
      `startDemo`. El banner también aparece en el demo/attract (el autopilot también teletransporta).
    - `game.h`: getter `recycledBanner()`.
    - `test_pc.cpp`: el test de integración del wormhole verifica que el banner arranca en
      `(0, WORMHOLE_RECYCLED_T]` tras el jump y baja a 0 (con el wormhole deshabilitado cada
      iteración, porque en la luna nueva re-aparece y re-traga a la nave re-armando el banner).
      Verificación: `./test_pc` **1036 OK**, render PPM verificado (dos líneas centradas legibles),
      `sync.sh` OK, compila composite, subido a la placa.
48. **Lluvia ácida de Europa (rama `acid-rain`, 26/8/2026)**. EUROPA (moonIndex==2, niveles 3/11/19…)
    recibe su efecto ambiental propio: celdas de tormenta que corroen la nave.
    - **config.h**: `ACID_RAIN_CELLS=3`, `ACID_CELL_RADIUS=90`, `ACID_CELL_DRIFT=12`, `ACID_RAIN_CORRODE=0.002`,
      `ACID_DRY_RATE=0.0004`, `ACID_CELL_TOP=-50`, `ACID_RAIN_STREAKS=26`, `ACID_STREAK_LEN=7`,
      `ACID_STREAK_SLANT=0.35`, `ACID_FALL_SPEED=45`, `ACID_STREAK_CYCLE=750`.
    - **moons.h**: `moonHasAcidRain(level)` (moonIndex==2), `moonEffectFree()` ahora excluye Europa
      (solo LUNA y CALLISTO albergan wormhole).
    - **acidrain.h/cpp**: 3 celdas con deriva y rebote, streaks oblicuas, splashes en terreno, puffs de vapor.
      `prand()` determinista (no rand() en draw). API: `reset/active/setEnabled/update/draw/drawSizzle/
      inRain/corrode/dry/meterGet/cellCount/cellX`.
    - **game.h**: miembro público `AcidRain acidrain`, flag `bool acidBurn` + getter `acidBurnGet()`.
    - **game.cpp**: hooks en los 4 sitios de reset (newGame/nextLevel/restartLevel/startDemo con
      `showcase`), `isolateForWormhole()` apaga la lluvia, `update()` con `setEnabled(false)` guard
      en `AcidRain` mientras `wormhole.active()`. Física en STATE_PLAYING: `corrode()`/`dry()` según
      `inRain`, crash al 100% (`acidBurn=true`, pérdida de fuel, `STATE_CRASHED` + return).
      Draw: `acidrain.draw()` después de twister, `drawSizzle()` tras el ship. Mensaje
      `"YOU CRASHED" / "ACID RAIN CORRODED THE SHIP"`. HUD `ACID nn%` en (250,72) con warnY→82.
    - **renderer_canvas.cpp**: glifo `%` añadido a la fuente 5×7 para el HUD.
    - **acidrain_demo.cpp + Makefile + sync.sh + .gitignore**: demo Escan eando el mundo, selftest
      `active + maxMeter>0.3 + inRain>0 + outRain>0`.
    - **test_pc.cpp**: tests de luna (EUROPA sí, otras no), `moonEffectFree(3)==false`, directo
      de AcidRain (inRain, corrode/dry, clamp a 100), integración con Game (meter→100% → crash acidBurn).
    - **AGENTS.md**: fila `acidrain.h/cpp` en la tabla, `moons.h` actualizado con lluvia ácida,
      `acidrain_demo.cpp` añadido a la tabla y comandos.
 49. **Terremotos de Ío (rama `quake`, 12/8/2026)**. IO (moonIndex==1, niveles 2/10/18…)
     ya tenía volcanes; ahora también sufre **terremotos tectónicos**: a los 8–20 s de empezar
     el nivel hay un temblor de aviso (~1 s de screen-shake + polvo + grieta pulsante sobre la
     plataforma objetivo) y luego **la plataforma más cercana a la nave se abomba**: los
     segmentos planos del pad se inclinan en una joroba (pico `QUAKE_LIFT=22 u`) que ya no es
     aterrizable y **permanece rota para el resto del nivel**. Aterrizar ahí = crash con el
     mensaje `"YOU CRASHED" / "THE GROUND GAVE WAY"`.
     - **config.h**: `QUAKE_START_TIME_MIN=8`, `QUAKE_START_TIME_MAX=20`, `QUAKE_RUMBLE_TIME=1`,
       `QUAKE_SHAKE_MAX=4` px, `QUAKE_LIFT=22`, `QUAKE_DUST_COUNT=14`, `QUAKE_DUST_RANGE=14`,
       `QUAKE_DUST_HEIGHT=18`, `QUAKE_DUST_LIFE=50`.
     - **moons.h**: `moonHasQuakes(level)` (moonIndex==1); `moonEffectFree()` también excluye a
       Ío (redundante con volcanes, por coherencia).
     - **terrain.h/cpp**: **registro de zonas** `zones_` (startIdx/segCount/labelX/baseY/broken)
       poblado por `init()` y `generate()`; API `zoneCount/zoneStart/zoneSegCount/zoneLabelX/
       zoneBroken`. `ruptureZone(zone)` inclina los segmentos del pad con `y=baseY+QUAKE_LIFT·
       sin(k·π/n)` (cada uno queda no plano → `checkLanding` vuelve 1), pone `landable=false`,
       `multiplier=1`, `labelX=-1` (las luces de aproximación, el label "Nx" y los puntos del
       minimapa desaparecen solos). `isZoneRupturedAt(x)`. Los conectores verticales ya existentes
       de `Terrain::draw()` cierran los bordes de la joroba.
     - **quake.h/cpp**: máquina de fases `IDLE→RUMBLING→BROKEN`. `findNearestZone()` elige el pad
       más cercano a `ship.posX` (saltando los ya rotos). Polvo (`Dust` con vida propia, cruz 3 px),
       grieta pulsante determinista (`prand`, sin rand() en draw), `justStruck()` un frame, `shake()`
       decae (×0.88) tras el golpe. API tests: `active/phase/justStruck/shake/rupturedZone/
       isRupturedAt`.
     - **game.h**: miembro `Quake quake`, flag `bool quakeCrash` + getter `quakeCrashGet()`.
     - **game.cpp**: `quake.reset()` en newGame/nextLevel/startDemo (con `setEnabled(false)` en
       showcase), `isolateForWormhole()` lo apaga (defensivo; Ío nunca alberga wormhole),
       `quake.update(dt, terrain, ship)` en la cadena de efectos. **Screen-shake** en `draw()`:
       offset aleatorio determinista añadido a `viewX/viewY` del bloque de mundo y restaurado
       antes del HUD (todo el mundo vibra, el HUD no). Crash en `checkCollisions()` si
       `quake.isRupturedAt(ship.posX)` → `quakeCrash=true` → mensaje `"THE GROUND GAVE WAY"`.
       HUD `SEISMIC` parpadeante en `(250,72)` solo durante RUMBLING.
     - **quake_demo.cpp + Makefile + sync.sh**: demo que encuadra la vista en el pad objetivo,
       render PPM en las 3 fases (intacto, RUMBLING con grieta/polvo, BROKEN con la joroba);
       selftest `active + phase==BROKEN + zoneBroken + !landable`. `quake.cpp/h` añadidos a
       `SRC` y a `SHARED` de `sync.sh`.
     - **test_pc.cpp**: luna (IO sí, LUNA/EUROPA no), `zoneCount==4`, transición a BROKEN,
       segmentos del pad `!landable`, `labelX==-1`, `checkLanding` en la zona rota → 1 y en otra →
       2 (solo la más cercana se rompe), `isRupturedAt` dentro/fuera, integración con Game
       (crash sobre la zona rota → `quakeCrashGet()` + `STATE_CRASHED`).
     - Verificación: `./test_pc` **1098 OK** (+34), `./quake_demo` pasa (joroba visible: superficie
       plana y~131 → perfil irregular y 126–139), compila composite 597 KB (45 %) y VGA 603 KB
       (46 %), subido a la placa CRT (boot limpio, audio ~16 kHz).
    - **Refinamientos (27/8/2026)**: la nave se **disuelve** en vez de explotar — `ship.dissolve()` 
      desprende las 6 partes secuencialmente (patas→toberas→cabina→cuerpo, 0.35 s) con
      velocidades bajas en X e Y (0.12-0.65 u/tick), conservando la inercia que traía. Mensaje
      `"ACID RAIN CORRODED THE SHIP"` en una sola línea centrada. HUD: solo la palabra `ACID`
      parpadea (≥90%), número fijo, sin `%`, con espacio. Tanto `ACID` como `WIND` glitchean
      al caer rayo. Las streaks de lluvia se recortan contra el terreno. Glifo `%` en la
      fuente 5×7 del renderer.
    - **Refinamientos (14/9/2026, golpes repetidos + ruptura de superficie)**: los terremotos
      dejaron de ser un golpe único a la plataforma más cercana. Ahora **el epicentro sigue a la
      nave** (el `ship.posX` ± `QUAKE_STRIKE_JITTER=12 u`, clamp al mundo), así un temblor puede
      ocurrir tanto con la nave alta en el cielo como **cerca de la superficie**, justo donde
      está volando. El efecto físico es **destruir parte de la superficie**: `Terrain::
      ruptureSurface(cx, QUAKE_SURFACE_HALF_W=12)` abomba un tramo de 24 u (joroba de pico
      `QUAKE_LIFT`), y si la ventana toca una plataforma de aterrizaje, **la plataforma entera
      se destruye** (`ruptureZone` → `landable=false`, `multiplier=1`, `labelX=-1`: desaparecen
      las luces de aproximación, el label "Nx"/"p" y el punto del minimapa; `checkLanding()`
      vuelve 1 → no se puede aterrizar ahí). El quake **se re-arma** (`QUAKE_REARM_TIME=1.5 s`
      + timer 15–30 s) para golpear varias veces por nivel, pero **no dispara fuera de
      `STATE_PLAYING`** (`update(dt,t,s,canStrike)`; el shake/polvo sí decaen en los mensajes
      de crash/landing). `Terrain::isRupturedAt(x)` es **persistente** (zonas rotas +
      `ruptureRanges_`, limpiado en `init()`/`generate()`) y es el que usa `checkCollisions()`;
      `Quake::isRupturedAt` cubre solo la última ventana (para tests). API nueva: `strikeX()`,
      `Terrain::zoneOverlapping(x1,x2)`. `quake_demo` ahora tiene 2 fases: (A) nave alta →
      superficie abombada; (B) nave baja sobre un pad intacto → el golpe lo destruye. En el
      primer demo de Ío los temblores siguen a la nave y casi siempre rompen su pad → showcase
      del efecto. Verificado: `./test_pc` **1108 OK**, `./quake_demo` (fases A y B), `sync.sh`
      OK.
    - **TEMP (14/9/2026)**: la partida arranca en Ío (`START_LEVEL=2`, revertir a 1) y la demo
      SIEMPRE abre su nivel en Ío/terremoto (`DEMO_QUAKE_FIRST=true` en config.h, revertir a
      false). Además la nave de la demo aparece siempre a **altitud aleatoria** en la banda
      `DEMO_SPAWN_Y_MIN..MAX` (100–260 u) en vez del fijo 150 (`game.cpp startDemo()`: spawn
      `ship.reset(110, sy)`), para que cada run del attract se vea distinto.
    - **Sonido del terremoto (14/9/2026)**: `sounds/gen_storm_sounds.py` genera `quake.wav`
      (41.6 k, 2.6 s): retumbar grave lowpass (banda ~90–120 Hz + senos sub de 36/54 Hz) que
      **crece en crescendo durante los 1.0 s de aviso** (`QUAKE_RUMBLE_TIME`, env `0.25+0.75·a²`
      con pulso tectónico 1.7 Hz) y aterriza en un **boom/trueno seco de 48 Hz** con transitorio
      ruidoso justo en el golpe, seguido de cola de réplica. `convert_wav.py` lo emite como
      `QUAKE_SOUND` en `audio_data.h`. `audio.cpp`: buffer `quakeBuf` + `quakePos` (one-shot como
      el rayo), mezclado en el ISR; `Audio::playQuake()`. El `.ino` lo dispara con
      `game.quake.justRumbled()` — nuevo **flanco de entrada a RUMBLING** en `quake.h/cpp` (se
      pone al pasar IDLE→RUMBLING, se limpia al inicio de `update()` como `justStruck_`), así el
      crescendo del sample acompaña el aviso visual y el boom coincide con la ruptura. Flash:
      composite 640998 B (48 %) / VGA 646150 B (49 %). Verificado: `./test_pc` **1104 OK**,
      `sync.sh` OK, subido a CRT.
50. **Culling de viewport para los efectos ambientales (16/9/2026, rama `quake`)**.
    - **Motivo**: en zoom (5×) la ventana visible son solo ~187 u de ancho de un mundo de 800+,
      pero todos los efectos se actualizaban y dibujaban cada frame sin importar dónde estuviera
      la cámara. El wormhole es el caso extremo: ~900 `powf` + miles de `pixelShade` por frame
      (espiral, núcleo, partículas) aun cuando está completamente fuera de pantalla.
    - **Mecánica**: `Game` calcula el rectángulo de mundo visible a partir de `viewX/viewY/
      viewScale` y expone 4 helpers (`game.h/cpp`): `effectVisible(wx,wy,margin)` (punto + margen
      de alcance), `xInView(wx,margin)` (solo horizontal, para cosas sobre el terreno),
      `bandVisible(wy,margin)` (solo vertical, para bandas que cruzan todo el ancho) y
      `atmosphereInView()` (bandas de niebla de Titán).
    - **Aplicado en `Game::update()`**: atmósfera solo si `atmosphereInView()`; wormhole solo si
      está visible **o** puede alcanzar a la nave (`< WORMHOLE_GRAB_R`) **o** está capturado/
      tragado (el Game espera a las fases swallow/dying para saltar de luna, así que esas
      secuencias siguen corriendo aunque el hueco salga de vista).
    - **Aplicado en `Game::draw()`**: wormhole (`effectVisible(WORMHOLE_OUTER_R)`), atmósfera
      (`atmosphereInView()`), géiseres (algún vent + alcance del chorro en X), volcanes
      (`volcanoes.countInView`), anillos (banda `RING_CY` ± curva+jitter), torbellino (ancla ±
      `TWISTER_HEIGHT`).
    - **La física NO se culla**: arrastre de atmósfera, corrientes, colisiones de anillos, etc.
      corren directo en `Game::update()`/`checkCollisions()` siempre. El culling es solo para el
      update visual y el draw (el coste caro).
    - **`rings.cpp`**: `drawFog` vuelve al inicio si `RING_FOG_BRIGHT==0` (la niebla desactivada
      ya no recorre la banda entera escribiendo brillo 0).
    - **Tests**: `testViewportCull` en `test_pc.cpp` con un `CountRenderer` que cuenta todas las
      llamadas primitivas (incluidas fuera de pantalla): parquea la nave en zoom y comprueba (a)
      que un hueco `EMERGING` fuera de vista **no avanza** mientras que uno visible sí pasa a
      `ACTIVE`, y (b) que el draw fuera de pantalla no genera píxeles extra del wormhole
      (`on.px > off.px`).
    - **Demo (16/9/2026)**: `DEMO_QUAKE_FIRST` se elimina (ya no hay showcase de Ío) y se
      sustituye por `DEMO_LEVEL_FIRST=7` (config.h): el **primer ciclo** de la demo abre en
      Encélado/géiseres (`demoFirstLevelPending` consumido en `startDemo()`); los ciclos
      siguientes re-tiran nivel al azar 1..12. `DEMO_WORMHOLE_FIRST=false` (16/9/2026): la demo
      ya NO fuerza el showcase de wormhole — cada ciclo muestra una luna/efecto al azar y el
      wormhole nunca abre en attract (la rama demo de `spawnWormhole` exige `force=true`).
    - Verificado: `./test_pc` **1123 OK** (incluye `testViewportCull`), `sync.sh` OK (composite y
      VGA idénticos), composite 641466 B (48 %), subido a CRT.
51. **Géiseres de Encélado: push escalado por gravedad + debug de escala en HUD (16/9/2026,
    rama `demo-enceladus-scale-hud`)**.
    - **Bug "hover forever" del demo en Encélado**: en la columna del géiser el push fijo
      `GEYSER_PUSH=0.00025` anulaba el **71 %** de la gravedad local (Encélado = 0.70×), dejando
      caída neta residual de 0.00010 (~29 %); el autopilot del demo, que corta empuje con
      `velY < -0.01`, podía quedar flotando a `VY=-2` (velY −0.01) quemando combustible para
      siempre. **Fix**: `Game::update()` aplica `ship.velY -= GEYSER_PUSH · (ship.gravity/GRAVITY)`
      → la columna cancela **siempre ~50 % de la gravedad local** (en Encélado queda caída neta
      0.000175, la nave nunca flota).
    - **Detección en `demo_sim.cpp`**: rastrea el "hovering" (|velY| < 0.02, `alt > 30`, empuje
      > 0.05) por seed, imprime los peores casos y cuenta `stuck` en el resumen
      (`=== wins=.. losses=.. timeouts=.. stuck=..`).
    - **Test nuevo en `test_pc.cpp`** (`testGeysers`): verifica la fórmula neta en Encélado
      (0.0005·0.70 − 0.00025·0.70 = 0.000175) y que la caída neta queda > 0.000150.
    - **Debug `SCL`/`VWS` en el HUD (TEMP)**: mientras la demo está activa se dibuja
      `SCL <ship.scale>` y `VWS <viewScale>` en `(250,92)`/`(250,102)` para validar en CRT el
      escalado/zoom (nave y vista) durante la rama; quitar al cerrar.
    - **Demo TEMP**: `DEMO_LEVEL_FIRST=7` ahora abre SIEMPRE en Encélado/géiseres **cada ciclo**
      (antes solo el primer ciclo y luego al azar 1..12); `demoFirstLevelPending` queda sin
      consumir. REVERTIR al primer-ciclo-fijo.
    - Verificado: `./test_pc` OK, `sync.sh` OK (esp32Lander == composite == VGA), subido a CRT.
52. **PENDIENTE (hardware) — Amplificador tentativo para Video Monitor Monocromo (16/8/2026)**:
    módulo XPT8871 (clase AB/D, mono, 3 W @ 4 Ω @ 5 V, entrada ~15 kΩ) conectado a la salida de
    audio (GPIO26 → cap 1–10 µF → módulo → parlante). **NUNCA SONÓ.** Hipótesis abiertas:
    módulo defectuoso/soldadura fría, falta el circuito amplificador previo (filtro RC de 2 polos
    para el portador PWM de 312.5 kHz que el XPT8871 deja pasar por su BW ~2.5 MHz → intermodula
    y suena apagado), alimentación insuficiente (dar 5 V externos), o que se necesite otro
    amplificador. Detalle completo, esquema del filtro y checklist de pruebas en
    `docs/hardware.md` (sección "Amplificador tentativo para Video Monitor Monocromo").
    No toca lógica de juego → no aparece en `AGENTS.md`.
53. **Migración del juego a ESP32-S3 (rama `s3-port`; 24/8/2026, en progreso)**: port completo
    a la placa S3 (8 MB PSRAM octal) con video compuesto por **driver directo LCD_CAM + anillo
    GDMA auto-enlazado** (sin esp_lcd): 26 descriptores × 3930 B sobre un fieldBuf de 102180 B
    (26 campos NTSC), pclk exacto 80/13 MHz vía PLL_F160M ÷26 (`lcd_clk_sel=3`,
    `clkm_div_num=25`, `lcd_clk_equ_sysclk=1`), ruteo de pines con
    `esp_rom_gpio_connect_out_signal(LCD_DATA_OUT0_IDX+i)`, EOF del último descriptor → ISR →
    notificación a `loopTask` y `video_wait_frame()` = `ulTaskNotifyTake` + `composeField()`
    (~2160 µs ≪ 16.6 ms). Esto eliminó el artefacto diagonal superior del esp_lcd (costura DMA
    entre transferencias). Estado: **Fase 0 (video) ✓**, **Fase 1 (juego corriendo) ✓**
    (título + demo attract validados en CRT), **controles (nunchuck SCL=9) ✓**,
    **audio GPIO18 ✓** (motor/explosión probados en juego). Pendiente Fases 2–4:
    sprites/bgLayer pre-renderizados en PSRAM + optimización.
    - **Quirks LEDC del S3 descubiertos (audio)**: (1) el core usa reloj **XTAL 40 MHz por
      defecto** para LEDC en S3 → 312.5 kHz @ 8-bit imposible ("div_param=0"); fix:
      `ledcSetClockSource(LEDC_USE_APB_CLK)` antes de `ledcAttachChannel`. (2) No existe
      `LEDC_HIGH_SPEED_MODE` (usar `LEDC_LOW_SPEED_MODE`). (3) **El duty solo se adopta si se
      pulsan AMBOS bits tras escribir el registro**: `conf1.duty_start=1` Y
      `conf0.low_speed_update=1` — cada uno solo deja `duty_rd=0` y silencio total (pin a 0 V).
    - Notas de hardware: audio pasa por amplificador externo (el pitido de prueba 1 kHz suena
      fuerte); dos episodios de "no hay video" resultaron ser contactos flojos del cableado, no
      software (firmware seguía componiendo campos según `[perf]`). Sketch de prueba de audio:
      `/tmp/opencode/audioTest/audioTest.ino`. Debug de registros LEDC disponible vía
      `Audio::debugRegs()` (imprime conf0/conf1/duty/duty_rd/timer).
54. **Fase 2/3 — bgLayer: terreno pre-horneado por nivel + blit escalado (24/8/2026,
    rama `s3-port`)**: nueva capa de fondo en espacio mundo (`esp32Lander/bglayer.{h,cpp}`,
    compartida a los 3 targets vía `sync.sh`) que se hornea UNA vez por nivel y se blitea
    cada frame con muestreo nearest fixed-point 16.16.
    - **Diseño**: `BgLayer` (buffer asignado vía hook `bgSetAllocator()` — `ps_malloc` en S3,
      `malloc` default que falla con elegancia en boards sin PSRAM → camino vectorial intacto),
      `LayerPainter : RendererCanvas` que escribe al buffer crudo, y
      `Renderer::drawLayer(layer,lw,lh,offX,offY,scale)` virtual (no-op default; impl nativa en
      `RendererPC` y `RendererS3`, idénticas). `Game::bakeBg()` pinta el terreno con transform
      identidad (`terrain.draw(p,0,0,1,0,false,false)`); el re-horneado se dispara comparando
      `Terrain::revision()` (contador ++ en init/generate/setCrater/clearCrater/ruptureZone/
      ruptureSurface) contra `bgBakedRev` — cero puntos de invalidación manual. El layer solo se
      usa en vista normal (`|viewScale - SCREEN_H/700| < 0.0005`); zoom sigue vectorial. Estrellas
      y labels NO se hornean: las estrellas de 1 px desaparecerían con el muestreo ×3 (punto
      a punto se salta 2 de cada 3 mundiales) → `Terrain::drawStarField()`/`drawLabels()` nuevos
      métodos públicos, dibujados dinámicos encima del blit (~120 rects, baratos).
    - **`LayerPainter::line()` sobrescrito** con barrido vertical por columna (`vspan`) en vez de
      Bresenham: garantiza cobertura continua tras la reducción ×3 en tramos empinados (el muestreo
      salta columnas mundiales enteras). El alto del layer sale de `max(y2)` de las líneas del
      terreno (¡el terreno vive en y≈600-703, no en WORLD_H=600! — primer intento horneaba vacío).
    - **Validación PC**: `test_pc.cpp` nuevo `testBgLayer()` (17 checks: raster del painter, blit
      identidad/offset/escala, fallo de alloc con fallback legacy, smoke de 120 frames) → 1142
      checks totales. Harness externo `bgdiff`: misma escena con layer vs legacy forzado → ratio
      de tinta 0.96, solo 1% de píxeles fuera de tolerancia ±2 px. Lección de método: un smoke
      "pasó" con el layer VACÍO porque newGame sin ticks de update no genera terreno procedural —
      los harness deben bombea updates antes de draw (y `bgActive()` getter público para probar
      que el camino realmente corre).
    - **Resultado en S3 (A/B real en placa)**: con layer activo (`[bg] world layer active`,
      ps_malloc) vs legacy forzado (sin allocator): **fps idénticos ~58.6 en demo**
      (293 campos/5 s) y `drawAvg` ≈ 8.5–9.8 ms en ambos → el terreno vectorial NO era cuello de
      botella en S3 (el blit cuesta lo mismo que 154 Bresenham); el costo está en efectos
      (géiseres/tormenta/HUD). El título sigue en ~38 fps (arte Apollo vectorial, 7.9 ms).
      **Decisión**: dejar el layer ACTIVO en S3 (costo nulo, útil si crecen los efectos);
      no perfilar más hoy. El pico de ~56 ms una vez por transición de nivel existe en AMBOS
      builds (no es el horneado).
    - **Instrumentación**: `[perf]` ahora imprime `drawAvg`/`drawMax` (micros() alrededor de
      `game.draw()`); print one-shot `[bg] world layer active`. En composite/VGA el código entra
      por sync pero nunca activa (malloc de ~638 KB falla en DRAM) — compilan limpios ambos
      (composite 643 KB/33%, VGA 648 KB/33%). Pendiente CRT: mirada del usuario al juego normal
      en S3 para confirmar visual idéntico.
55. **DESCONTINUADO el sketch composite clásico; el ESP32-S3 es la versión principal para
    CRT B/N (24/8/2026)**: tras validar en CRT que la S3 se ve y rinde mejor (driver propio
    LCD_CAM+GDMA sin costuras entre campos, ~58.6 fps en demo vs ~38 del título en composite,
    bgLayer activo en PSRAM), se oficializa el cambio:
    - `esp32LanderComposite/` queda como **archivo histórico**: ya no recibe sync
      (`sync.sh` targets ahora `esp32LanderS3/src` + `esp32LanderVGA/src`) ni uploads; para
      reflashearla habría que re-sincronizar sus fuentes manualmente.
    - La regla de trabajo pasa a ser `sync.sh` → compilar → upload a `esp32LanderS3/`
      (FQBN `esp32:esp32:esp32s3:PSRAM=opi`, puerto `/dev/ttyACM0`).
    - Docs actualizadas: AGENTS.md (estado del código, sección composite marcada histórica,
      pinado S3 al frente con bus LCD_CAM D0–D7 GPIO4/5/6/7/15/16/40/41 + audio 18 +
      nunchuck SDA21/SCL9 + pot 8 + start 13), docs/hardware.md (banner de estado + pinout S3),
      comandos útiles (compile/upload S3 como principal).
56. **Cisterna: refino de docking + showcase del demo; umbral de aproximación unificado
    (rama `tanker-docking`, 25/8/2026)**. Conjunto de mejoras que cierran la rama:
    - **Docking más fácil**: `TANKER_DOCK_TOL_X/Y` 12/8→20/14, límites de velocidad relajados
      (vy ±0.14 / vx 0.20), `TANKER_DOCK_BREAK_TOL_X/Y` 18/14→28/22, `TANKER_DOCK_LOCK_TIME`
      0.4→0.3 s, `TANKER_CONE_GUIDE` 12→20, sway del drogue 3.0/1.6→2.0/1.2.
    - **Tanker forzosa en nivel 1 y en demo**: `TANKER_START_LEVEL=1`, `force` salta la regla de
      combustible (`TANKER_FUEL_FRACTION`); la demo arranca al 30 % de fuel y SIEMPRE abre en
      Luna (`DEMO_LEVEL_FIRST=1`) → el attract muestra el repostaje completo (vuela al drogue,
      dockea, reposta, crucero-desciende al pad). `DEMO_FORCE_TANKER_CRASH=0` (showcase de choque
      desactivado). `demoHoldAltitude` ahora desciende (`desVY ∈ [−0.12, 0.08]`);
      `demoTargetY = terrain.yAt(pad)` tras repostar.
    - **Bug demo "sube y sube" (fijado)**: `readInputs()` del `.ino` pisaba `input.angle` cada
      loop deshaciendo la rampa del autopilot → early-return en modo demo (solo el botón start).
    - **Escala visual del zeppelin unificada**: `drawScale = ship.scale·viewScale·
      TANKER_DRAW_SCALE` (`TANKER_DRAW_SCALE=1.5`) en todas las vistas (antes ×1.6 solo en
      zoom-out); helper `Tanker::drawScaleFor()` + label `TK` de debug en el HUD.
    - **Umbral de aproximación único `APPROACH_ALT=200`** (salida `APPROACH_EXIT_ALT=350`,
      sustituyen a `ZOOM_IN_ALT`/`ZOOM_OUT_ALT`): gobierna zoom-in, minimapa (`altitude <
      APPROACH_ALT && !dockZone`), ocultación de la cisterna y dibujado del terreno. Cámara de
      zoom centrada en la nave al 50 % desde arriba (`APPROACH_CAM_FRAC=0.50`).
    - **Fix oscilación de zoom (fijado)**: `ship.altitude` se medía desde `bottom = posY +
      14·ship.scale` y `ship.scale` cambia con el zoom (1.5/0.48) → la altitud saltaba ~14 u al
      flipar el zoom y el zoom oscilaba frame a frame (visto como doble dibujado del mundo).
      El umbral usa ahora `approachAlt` medido desde `ship.posY` (independiente de escala);
      se eliminó el force-zoom-out por `tanker.leaving`. Validado: 1 solo flip de zoom en 8000
      frames × 8 seeds (antes 311).
    - **Otros**: `vspan` del bgLayer engrosado 3 px por ambos ejes (sobrevive el downsampling ×3
      en laderas), cobertura de lava variable por pad (0.4–0.8), `setZoom(false)` al resetear la
      nave, tests ampliados (slope test del bgLayer + test tanker con force fuel). `test_pc`
      1144/1144, `demo_sim` sin cambios de win-rate.

57. **Demo con niveles al azar de nuevo + progresión de dificultad del juego normal
    (rama `demo-random-progression`, 25/8/2026)**.
    - **Demo vuelve a elegir nivel al azar en CADA ciclo**: `DEMO_LEVEL_FIRST=1` → `0` (config.h),
      así el attract mode ya no abre siempre en Luna/nivel 1 con la cisterna; cada ciclo muestra
      cualquier luna 1..`DEMO_MAX_LEVEL`. Se elimina el campo muerto `demoFirstLevelPending`
      (game.h/game.cpp). La demo sigue forzando la cisterna (`force=true`) y arrancando al 30 % de
      combustible, así el repostaje puede aparecer en cualquier luna donde el tanque esté permitido
      (excluido de Ganímedes por los anillos, y de las lunas con agujero de gusano vía
      `isolateForWormhole`).
    - **Juego normal: dificultad progresiva por ciclo de 8 niveles**. Nueva tabla
      `MOON_DIFFICULTY_ORDER` en `moons.h`: mapea cada slot del ciclo `(level-1)%8` a una luna,
      caminando de las calmas a las hostiles:
      `LUNA → EUROPA (lluvia ácida) → CALLISTO (tranquila, host de wormhole) → ENCELADUS
      (géiseres) → TITAN (niebla) → GANYMEDES (anillos) → TRITON (torbellino) → IO (volcanes +
      terremotos, gravedad más pesada)`. `moonIndex()` devuelve ahora el índice de tabla del slot
      (antes identidad); las predicciones `moonHas*`, `moonGravity`, `moonName` y `moonEffectFree`
      siguen funcionando porque comparan contra el índice de la tabla. La dificultad del terreno
      procedural (amplitud `4+level`, tope 12) ya escalaba sola y no se toca.
    - **Fix `wormholeJump()` con el orden nuevo**: el teletransporte elegía `level = 8+nidx` con
      `nidx` como índice de tabla, lo que ya no garantiza aterrizar en OTRA luna con un orden no
      identidad. Ahora usa `moonSlotOfIndex(nidx)` (inversa del orden) para saltar al slot cuya
      luna es `nidx`.
    - **Tests y demos al nuevo mapeo**: `test_pc` actualizado (todas las constantes de nivel por
      luna: EUROPA 3→2, CALLISTO 5→3, ENCELADUS 7→4, TITAN 6→5, GANYMEDES 4→6, TRITON 8→7, IO 2→8;
      `moonEffectFree`, `testMoon` documenta la rampa y comprueba `moonGravity(8)==1.10`). Defaults
      de los demos visuales de PC actualizados (volcano/quake 2→8, geyser 7→4, rings 4→6, titan
      6→5, twister 8→7, acidrain 3→2). `test_pc` 1066/1066, demos PC con selftest OK, sketch S3
      compila (645 KB / 49 %, RAM 211 KB).
