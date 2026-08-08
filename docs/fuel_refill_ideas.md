# Ideas para repostar FUEL — Lunar Lander ESP32

> **Contexto actual (config.h, game.cpp, ship.cpp):**
> - `FUEL_MAX = 1000`. El combustible **no se recarga entre niveles** (se conserva en `restartLevel()` / `nextLevel()`).
> - Consumo: `FUEL_PER_THRUST = 0.2` por tick mientras el motor está activo.
> - Pérdidas: `-60` en strike de tormenta (`STORM_HIT_FUEL`), `-200..400` en crash/ring/twister.
> - Ganancia única: `+50` en aterrizaje perfecto.
> - Game over: al tocar el suelo con `fuel <= 0` → `endGame()` (`OUT OF FUEL`).

## Principios orientadores
- **Estética arcade/retro** (CRT B/N, 320×240, luma 0–255). Sin color.
- **Tensión permanente**: recargar no es "gratis"; implica riesgo, tiempo o elección.
- **Visual legible**: la mecánica se lee en la pantalla (luz, sprite, HUD) sin depender de color.
- **Simple de implementar** en el port de `esp32Lander/` y el sketch ESP32.

---

## 1. Nave cisterna "tanker" (prioridad alta sobre la mesa)

**Concepto (versión sencilla — dock como aterrizaje):**
- Una nave cisterna se genera en tierra, en una zona *no landable* (o en una landable secundaria). Se dibuja como un rectángulo grueso / cilindro con luces parpadeantes (líneas horizontals dobles, un "T" en la tapa, luces de borde que parpadean).
- Si la nave jugadora toca el **techo** de la cisterna (rectángulo inferior de contacto), se produce un *dock*:
  - Se recarga combustible al **100 %** (`ship.fuel = FUEL_MAX`).
  - Se reproduce un sonido de encaje ("click" / chasquido).
  - La cisterna se "echa a volar" suavemente (sube un poco + derecha/izquierda) y sale de la escena; el jugador continúa volando.
  - Bonus de puntuación pequeño (`+50`) por uso.
- La cisterna es **inmóvil mientras no se dokea**; si el jugador se estrella contra ella lateralmente sin alinearse, cuenta como choque normal (no se recarga).

**Variante arriesgada (versión "trompa"):**
- El dock se hace por el **techo** de la cisterna con la nariz de la nave; mientraspermanece conectado, recarga progresivamente (`fuel += k·t`).
- Mientras está conectada, la nave **oscila** (lerp de rotación ±5°) y cualquier input de rotación la desconecta; al soltarse la cisterna se aleja.
- Visual: línea fina (1 px) entre la nariz de la nave y el techo de la cisterna que se vuelve más brillante con el tiempo; HUD muestra `REFUELING xx%`.
- Física: fuerzo de unión suave, `velY` de la nave se acerca a 0 mientras está conectada; si se pasa el tiempo, el jugador cae al suelo.

**Alternativas de spawn de la cisterna:**
- En niveles pares: zona intermedia del mapa, sobre una colina.
- En niveles con *géiseres* (Encélado) o *volcanes* (Ío): la cisterna aterriza *a propósito* cerca del peligro (tensión + y - riesgo de lava).
- En nivel de *anillos* (Ganímedes): la cisterna navega entre bandas de roca (visor de paso estrecho).

---

## 2. Zonas de repostaje en la pista (zonas "depot")

**Visual:**
- Una o dos de las cuatro zonas `landable` llevan un **borde punteado doble** o luces de aproximación alternadas (`(k + counter/25) & 1` invertado respecto al resto) en vez del `2×2` parpadeante habitual.
- La etiqueta `Nx` se acompaña de una **U invertida** (símbolo bomba) encima.

**Gameplay:**
- Aterrizaje perfecto en zona *depot*: `+250` fuel + `×2` puntuación.
- Aterrizaje *hard* (no perfecto) en zona *depot*: `+150` fuel, puntuación normal.
- Aterrizaje perfecto en zona normal: `+50` (como hoy) *sin* bonus fuel.
- La zona *depot* es **más estrecha** (19 u) que las normales (25–31 u), para mantener riesgo.

---

## 3. Recogida de objetos flotantes

**Tipos por luna:**
- **Encélado (géiseres):** canoas de hidrógeno — burbujas translúcidas (círculo hueco + brillo interno que crece). `+50` cada una.
- **Ío (volcanes):** lingotes de lava — rectángulos rotos con borde interno brillante. `+75`, pero si caen al suelo se deslucen (recoger rápido).
- **Titán (niebla):** fragmentos de cardón metano — diamantes diminutos que aparecen en las bandas de niebla. `+30`.
- **Luna base (cualquier nivel):** escombros de nave — triángulos rotos. `+25`.

**Mecánica:**
- Aparecen **3–5** objetos visibles de golpe, en coordenadas de mundo.
- Al tocar uno, desaparece con un *pop* (partícula radial) y se regenera otro en otro punto.
- HUD muestra un contador: `MODULES 2/4` o `xx/150` (fuel coleccionado / objetivo).
- El jugador decide: ¿recorrer el mapa por fuel o aterrizar directamente?

---

## 4. Recompensas por eficiencia / estilo

**Concepto:**
- Si el nivel termina con `fuel > 0.7 * FUEL_MAX`, se otorga un **bónus de eficiencia**: `+100–250` fuel extra para el siguiente nivel (acumulable hasta un techo de `+FUEL_MAX`).
- Si se realiza un aterrizaje perfecto **sin usar el motor en los últimos 80 u** de altura (brinco "de gravedad"), +150 fuel.
- Visual de HUD: `SAVED FUEL 180` al terminar el nivel.

---

## 5. Riesgo / recompensa con elementos ambientales

| Luna / evento | Mecánica de recarga | Riesgo |
|---|---|---|
| **Tormenta (STORM)** | Dejar que un rayo te pique → `+100` fuel, pero `-90°` de rotación temporal y `stormHitTimer`. | Control perdido 1.5 s. |
| **Volcán (Ío)** | Pasar bajo la columna de lava (no tocar) → `+flux` fuel por "calor residual". | Lava sube al tocar. |
| **Géyser (Encélado)** | Activar un géyser (entrar en pluma) → empuje ascendente *y* `+50` fuel por "refrigeración". | Si te llevas el plomo, te lanza al cielo. |
| **Torbellino (Tritón)** | Salir del vórtice bajo presión (escape exitoso) → `+80` fuel de "compresión de escape". | Si te atrapa de nuevo, -150. |

---

## 6. Recuperación post-crash / segunda oportunidad

**Concepto:**
- Si `fuel > 0` al estrellar, al reiniciar el nivel (`restartLevel`) el jugador paga un **costo de rescate**:
  - `fuel = fuel * 0.5 + 100` (penalización parcial).
  - Visual: pantalla parpadea y el HUD muestra `SALVAGE: FUEL x0.5`.
- Si `fuel == 0`, sigue el game over (`OUT OF FUEL`).
- Tres salvamentos seguidos: la nave se convierte en escombros (game over forzoso).

---

## 7. Power-ups temporales y HUD

- **"Turbo"**: pick-up temporal que duplica el rendimiento del motor (thrustBuild sin coste extra) por 3 s.
- **"Ghost"**: 1 s de inmunidad a daño — útil para pasar entre anillos/rocas.
- **HUD dinámico**: los picks aparecen como iconos en la barra inferior (`[T]` turbo, `[G]` ghost, `[F+]` tanque). En B/N se usan símbolos geométricos (triángulo = turbo, cuadrado = ghost, barra vertical partida = fuel).

---

## Prioridad sugerida para la primera iteración

1. **Nave cisterna (versión sencilla).**
2. **Zonas depot (doble línea punteada + bonus).**
3. **Power-ups temporales (turbo).**

Las ideas 4, 5 y 6 son accesorios elegantes que se pueden apilar. La mecánica de la **cisterna dock** es visualmente fuerte y físicamente simple: reutiliza `checkLanding()` con un rectángulo de “techo” y `fuel = FUEL_MAX`, luego despega la cisterna.

