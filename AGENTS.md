# AGENTS.md — Lunar Lander ESP32

## Objetivo

Portar el juego Lunar Lander a un **ESP32** con salida **video compuesto (AV)** hacia un
**CRT blanco y negro con entrada compuesta (video + sonido)**. Controles físicos:
**potenciómetro (ángulo) + gatillo de reóstato de pista de autos (potencia de motores)**.

Estética objetivo: arcade / retro auténtico.

## Estado actual del código (fuente del port)

- `pyLander/` — original en Python/Pygame Zero (`lunarLander.py`, `ship.py`, `terrain.py`)
  + `venv/`, `sounds/`, `fonts/`. Movido a subdirectorio para organizar. NO se usa para el ESP32.
- `cppLander/` — versión C++ del mismo juego (`final.cpp`, `ship.cpp`, `terrain.cpp`).
  Es la **base directa para portar al ESP32**: física y colisiones O(N·M) corren bien a 240 MHz.
- `esp32Lander/` — port C++ std validado en PC (ver "Port a ESP32").
- `esp32Composite/` — sketch Arduino del ESP32 (ver "Sketch ESP32").

## Port a ESP32 (ESTADO 2/8/2026)

Estructura en `esp32Lander/` (C++ std, sin dependencias de hardware):

| Archivo | Contenido |
|---------|-----------|
| `ship.h/cpp` | Nave portada de `cppLander/` (física, hitbox, colisión). Añadido `draw(Renderer&)` |
| `terrain.h/cpp` | Terreno portado de `cppLander/` (punto medio, multiplicadores). Añadido `draw(Renderer&)` |
| `game.h/cpp` | Lógica del juego: estados, física, scoring, `update()` + `draw(Renderer&)` |
| `renderer.h` | Interfaz abstracta (pixel/line/circle/text/flush). Se implementa para PC y para el ESP32 |
| `renderer_canvas.h/cpp` | Primitivas de dibujo compartidas (Bresenham, círculo, fuente 5x7) vía `pixel()` |
| `renderer_pc.h/cpp` | Renderer de validación en PC: framebuffer + PPM (extiende `RendererCanvas`) |
| `main_pc.cpp` | Demo en PC (genera snapshots PPM en `frames/`) |
| `test_pc.cpp` | Tests de validación (asserts) |

- **Entrada:** struct `Input { bool startPressed; float angle; int throttle; }`. El pot mapea
  `angle` a `[-PI, 0]`; el gatillo mapea `throttle` a `[0, 8]` (accMode). En menú `startPressed`
  equivale a la tecla "P".
- **Mundo 1400×800, pantalla 320×240**: la física y colisiones quedan en coordenadas de mundo;
  el dibujado escala (terreno con `kx=320/1400`, `ky=240/800`). La nave se dibuja a tamaño natural
  (~24 px) para verse en el CRT.
- **Constantes del port** (coinciden con `cppLander/`, NO con Python):
  empuje `xv += .025*accMode*cos`, `yv += .035*accMode*sin`, gas `-.015*accMode`;
  gravedad `yVel += 10*dt`, posición `y += yVel*dt + .5*30*dt*dt`; `dt = 0.01`.
- **Cambio vs cppLander (ambiente ESP32):** `terrain.cpp` siembra el RNG **una sola vez**
  (`static bool seeded`). En el ESP32 el sketch setea `settimeofday()` con `esp_random()`
  para que `time(NULL)` (usado como semilla) difiera en cada boot → terreno distinto cada partida.
- Validado en PC: `make && ./test_pc` → **todos los checks pasan**. Demo visual: `./main_pc`.
- Pendiente: integrar el sketch en hardware y verificar imagen/controles en el CRT.

## Mecánicas a respetar (port fiel)

- Estados: menú (1), jugando (2), game over por combustible (3). Tecla "P" en menú inicia.
- Nave: gas 750, ángulo en `[-PI, 0]`, accMode (empuje) en `[0, 8]`, vel. máxima x = ±100.
- Gravedad, combustible se consume con empuje. Aterrizaje: good (<12 vy y |vx|<25),
  hard (<25 vy y |vx|<25), crash (más rápido). Multiplicadores en el terreno (2x–5x).
- Colisión: cuerpo/patas = crash, ambos pies = posible aterrizaje.

## Controles físicos decididos

| Control | Mapeo del juego | Notas |
|---------|-----------------|-------|
| Potenciómetro A | Ángulo de la nave (`-PI` a `0`) | ADC con suavizado, dead zone en extremos |
| Gatillo (reóstato de pista de autos) | Potencia de motores | Medido y validado (ver circuito) |
| Botón (adicional) | Inicio / reinicio de partida | Equivale a tecla "P" |

El gatillo combina potencia + encendido gracias al resorte de retorno (suelto = motor apagado).
No se necesita botón separado para el motor.

## Medición del reóstato (COMPLETADA 2/8/2026)

Contexto: el gatillo de pista de autos es un reóstato de **resistencia baja** (unos pocos Ω,
porque pasaba corriente al motor del auto). **No se puede leer directo con el ADC del ESP32**
(drena demasiada corriente y la lectura sería mala). Por eso se usa divisor de voltaje.

### RESULTADO DE LA MEDICIÓN (2/8/2026)

- **Gatillo suelto (reposo):** circuito abierto (sin lectura) → motor apagado (accMode 0).
- **Primer contacto al apretar:** ~**500 Ω**.
- **Gatillo apretado al máximo:** **30 Ω** → potencia máxima.
- **Barrido 500 → 30 Ω es continuo/suave** (sin escalones discretos). Las lecturas
  "brincan" por **ruido de contacto** del cursor sobre el bobinado: se comporta así por
  el uso con corrientes de motor; se mitiga en software (suavizado) y con un condensador.
- Conclusión: es interruptor + reóstato con rango útil 500–30 Ω. El arranque (abierto→500)
  es un salto de "apagado a encendido" con dead zone natural.

#### Circuito del divisor (gatillo → ADC)

```
3.3 V ──[reóstato 1.8M→30Ω]──┬──[120 Ω]── GND
                            └──┬──[0.1 µF]── GND
                               └── ADC (GPIO34/35)
```

- Reposo (abierto) → V ≈ 0.0 V (apagado, accMode 0). Verificado con el multímetro.
- Medición final con 120 Ω (punto medio → GND): primer contacto **2.5 V**, a fondo **2.9 V**.
  Rango útil ≈ 500 cuentas ADC (de sobra para 9 niveles). Suelto = 0 V (off).
- En circuito el reóstato va de ~38 Ω (primer contacto) a ~17 Ω (a fondo): es un control
  tipo "on + acelerador" con salto natural de apagado a encendido. Se mapea en software.
- **Con 1 kΩ la ventana quedaba aplastada (~0.35 V, 2.9→3.25 V)**: el 1 kΩ "ahoga" el
  cambio de un rango de resistencia tan corto. Con 100–220 Ω la ventana es usable.
- El condensador de 0.1 µF forma un paso bajo RC (~10 µs) que filtra el ruido de contacto.
- Los "brincos" de lectura son ruido de contacto del cursor sobre el bobinado: se mitigan
  en software (suavizado, dead bands) y con el condensador. Abierto = 0 (off).
- En el código: mapear ADC → accMode 0–8 con curva de compensación (sqrt o lookup table)
  y suavizado (media móvil). Dead zone: por debajo de ~1 V → accMode 0 (off).
- Si en pruebas el gatillo se siente demasiado "todo o nada", se puede bajar la resistencia
  a ~47–56 Ω para estirar la ventana a ~0.6 V (más corriente ~50 mA) o reasignar controles.

#### Conexión del potenciómetro (ángulo → ADC)

```
3.3 V ──┬──[extremo 1]
        │
   [pot 10 kΩ]
        │
      [cursor] ──► ADC (GPIO34 o 35)
        │
   [extremo 2]
        │
 GND ───┴───
```

- Pin 1 (extremo) → 3.3 V; pin 2 (extremo) → GND; pin 3 (cursor/medio) → ADC.
- Reversible: si el ángulo sale invertido, se invierte en software o se cambian los extremos.
- **GPIO34 y 35 son solo-entrada** (sin pull-up/pull-down): ideales para ADC.
  Asignación: GPIO34 = ángulo (pot), GPIO35 = potencia (gatillo), o al revés.
- Condensador opcional de 0.1 µF del cursor a GND para limpiar ruido.
- Mapeo en software: recorrido útil del pot (con dead zones en los extremos) → `[-PI, 0]`.
  Suavizado (media móvil) igual que el gatillo.

## Salida de video (compuesta a CRT B/N)

- **Librería principal: bitluni/ESP32CompositeVideo** (Arduino IDE, I2S + DAC interno,
  **GPIO25**, cero componentes, 320×240, grises de 8 bits). La TV B/N ignora el colorburst.
- Se usa la API moderna del repo: `CompositeGraphics` (backbuffer 320×240) + `CompositeOutput`
  (NTSC/PAL, doble resolución) con tarea `compositeCore` fija al core 0 que transmite
  `graphics.sendFrameHalfResolution(&graphics.frame)`; el loop dibuja en el core 1 y hace
  swap con `graphics.end()`. NO es la API antigua `CompositeVideo::begin(RES)`.
- Los headers necesarios (`CompositeGraphics.h`, `CompositeOutput.h`, `Font.h`,
  `TriangleTree.h`) son **header-only** y están **embebidos en el sketch** (`esp32Composite/src/`),
  por lo que NO hay que instalar ninguna librería. `CompositeOutput.h` incluye además
  `soc/i2s_reg.h` (los registros I2S ya no vienen con `driver/i2s.h` en core 3.x / IDF 5).
- El sketch **compila validado** con `arduino-cli compile --fqbn esp32:esp32:esp32`
  (core 3.3.10): ~314 KB flash (23%), RAM global 7% — los framebuffers 320×240 (2×76 KB)
  se alojan en el heap en `graphics.init()`.
- Alternativa si se usa ESP-IDF: `aquaticus/esp32_composite_video_lib` (PAL/NTSC, mono, LVGL).
- Alternativa color: `ESP_8_BIT_composite` (escalera de resistencias) — **innecesaria** para B/N.

## Sketch ESP32 (`esp32Composite/`)

Sketch Arduino autónomo (funciona con Arduino IDE o `arduino-cli`). Se puede abrir
`esp32Composite/esp32Composite.ino` y subir con placa "ESP32 Dev Module" (core esp32 ≥ 3.x).

| Archivo | Contenido |
|---------|-----------|
| `esp32Composite.ino` | `setup()`/`loop()`: `CompositeGraphics`+`CompositeOutput` (NTSC 320×240, GPIO25), tarea `compositeCore` en core 0, `esp_pm_lock` (CPU máx), `settimeofday(esp_random())` para terreno distinto por boot, ADC+botón, loop fijo con `millis()` y `dt` 0.01 |
| `src/renderer_esp32.h/cpp` | `RendererESP32 : RendererCanvas`: `pixel→g.dot`, `clear→g.begin(0)`, `flush→g.end()`. Blanco = valor 100 (luma) |
| `src/CompositeGraphics.h`, `CompositeOutput.h`, `Font.h`, `TriangleTree.h` | Librería bitluni (header-only, copiada) |
| `src/ship/terrain/game/renderer_canvas/renderer/config` | Mismas fuentes que `esp32Lander/` (copias; mantener en sync con `diff`) |

- **Pines (constantes al inicio del `.ino`):** GPIO34 = pot (ángulo), GPIO35 = gatillo
  (potencia), GPIO13 = botón start (INPUT_PULLUP, flanco → `startPressed`).
- **Gatillo → throttle:** suavizado media móvil; por debajo de `TRIGGER_OFF_VOLT` (1.0 V) → 0
  (off); `TRIGGER_MIN_VOLT` (2.5 V) → 1; `TRIGGER_MAX_VOLT` (2.9 V) → 8 con curva `sqrt`.
  Constantes ajustables en el `.ino` según la medición final en circuito.
- **Pot → ángulo:** dead zone 5–95% del ADC, mapeo lineal → `[-PI, 0]`. Si sale invertido,
  invertir `a` en el `.ino` o cambiar los extremos del pot.
- **NTSC vs PAL:** `composite(CompositeOutput::NTSC, ...)` en el `.ino`; cambiar `NTSC` por
  `PAL` (50 Hz) si el TV es PAL. B/N ignora el colorburst en ambos.
- Loop: `game.update()` cada 10 ms (acumulador sobre `millis()`); `game.draw(renderer)` por
  iteración (hace `clear` + swap). El draw a mayor ritmo no rompe nada por el doble buffer.

## Sonido

- DAC2 en **GPIO26** (libre). Convertir `sounds/explosion.wav` y `sounds/rocket_thrust.wav`
  a WAV mono 8-bit / 16 kHz. Salir por I2S → GPIO26 al RCA blanco del TV, con
  **condensador de acople en serie (1–10 µF)** para quitar el DC.

## Escalado de pantalla

- Original 1400×800 → objetivo 320×240 (factor ~4.4). La física NO cambia, solo el dibujado.

## Decisiones de arquitectura / convenciones

- Framework: **Arduino (arduino-esp32)** salvo que el usuario decida ESP-IDF.
- Estructura: portar `ship` y `terrain` de `cppLander/` casi tal cual; loop fijo con `millis()`,
  `dt` fijo (0.01 s). El ritmo de video lo maneja la tarea `compositeCore` (doble buffer:
  `begin`/`end`), sin VSYNC explícito.
- Dibujado: reemplazar `gfx_*`/Pygame por la API de la librería de video (línea, círculo, texto).
- Idioma del código: inglés (coherente con `cppLander/`). Respuestas al usuario: español.
- No usar librerías no verificadas antes de consultar. No añadir comentarios al código salvo que se pidan.

## Comandos útiles

- Probar juego Python original: `python pyLander/lunarLander.py` (desde `pyLander/`, con su `venv/`).
- Compilar versión C++: `make` en `cppLander/` (usa librería `gfxnew`, no portable a ESP32).
- Validar port en PC: `make && ./test_pc` en `esp32Lander/`. Demo visual: `./main_pc` (PPM en `frames/`).
- Subir al ESP32: Arduino IDE, abrir `esp32Composite/esp32Composite.ino`, placa "ESP32 Dev Module".

## Proceso de trabajo

1. ~~Medir el reóstato y documentar resultado~~ **COMPLETADA (2/8/2026)** — ver sección de medición.
2. ~~Portar física + lógica del juego (sin hardware): validar en PC~~ **COMPLETADA (2/8/2026)**
   — ver "Port a ESP32" (`esp32Lander/`, `make && ./test_pc` OK).
3. ~~Integrar video compuesto (bitluni) → CRT (sketch `esp32Composite/` listo para compilar)~~
   **EN PROGRESO** — falta verificar imagen en el CRT real.
4. Integrar controles: potenciómetro (ángulo) y gatillo (potencia) tras confirmar el circuito.
5. Integrar sonido por GPIO26.
6. Puli r/ajustes de jugabilidad en CRT.
