---
description: Limpia AGENTS.md, verifica, commitea (o mezcla a otra rama), hace push al remoto y sube a la placa
agent: build
---

# Flujo flash (docs + verify + commit/merge + push + upload + compact)

Workflow del repo lunar lander. Acepta un argumento opcional: el **nombre del branch de
destino** (`$1`). Ejecuta los pasos en orden:

1. **Documentación (SIEMPRE, antes que nada)**: revisa `git status` y `git diff` de los
   cambios sin commitear y **limpia/actualiza `AGENTS.md`** para que refleje el estado real
   (constantes en `config.h`, pantalla de bienvenida/título, minimapa, controles, etc.) y
   elimina del mismo cualquier sección/contenido obsoleto (cosas inútiles, intentos previos
   descartados). Mantén la estructura y el idioma español existentes. Mantén `esp32Lander/`
   y `esp32LanderComposite/src/` en sync (mismas fuentes) verificando con `diff` (si algo
   difiere, cópialo para igualarlas). Actualiza también `docs/WORKLOG.md` con una entrada
   del cambio si procede.

2. **Verificación**: en `esp32Lander/` ejecuta `make clean && make` y `./test_pc`
   (todos los checks deben pasar) y `./demo_sim` si el cambio afecta al autopilot de demo.
   Compila el sketch con
   `arduino-cli compile --fqbn esp32:esp32:esp32 esp32LanderComposite/esp32LanderComposite.ino`.

3. **Commit**: haz `git add` de los archivos pertinentes (incluye `AGENTS.md` y ambos
   `game.cpp`/`config.h` si aplica) y crea un commit con mensaje corto en el estilo del
   repo (p. ej. título: "Descripción breve"). Este commit se hace SIEMPRE en el branch
   actual antes de cualquier merge.

4. **Destino del cambio** (según `$1`):
   - **Si se pasó `$1` (nombre de rama)**: mezcla los cambios de la rama actual en esa
     rama de destino. Procedimiento seguro:
     - Verifica que la rama destino existe (`git branch --list "$1"`); si no existe,
       créala desde el commit actual (`git branch "$1"`).
     - Haz `git switch "$1"` y `git merge <rama-actual> --no-ff` (o `git merge` normal) para
       incorporar el commit de la rama actual a la de destino. Asegúrate de que el merge
       quede limpio (sin conflictos; si los hay, resuélvelos).
     - El resultado debe quedar en la rama destino con todos los cambios de esta rama.
   - **Si NO se pasó `$1`**: no se mezcla nada; los cambios quedan commitados en el branch
     actual.

5. **Push al remoto**: haz `git push` de la rama donde quedaron los cambios finales (la
   rama destino si se mezcló, o la actual si no). Si la rama aún no tiene upstream en el
   remoto, usa `git push -u origin <rama>`. Si `git push` falla por no tener tracking,
   configura el upstream correspondiente y reintenta. No crees PR salvo que se pida.

6. **Subida a la placa**: la versión principal es el **ESP32-S3** (`/dev/ttyACM0`).
   Comprueba que el puerto existe (`ls /dev/ttyACM0`) y sube con:
   `arduino-cli upload --fqbn esp32:esp32:esp32s3:PSRAM=opi --port /dev/ttyACM0
   esp32LanderS3/esp32LanderS3.ino`. Confirma "Hash of data verified". Si el
   puerto no existe o falla la subida, repórtalo claramente y detente (no reintentar sin fin).
   La carpeta `esp32LanderComposite/` está descontinuada (24/8/2026).

7. **Resumen para /compact**: al terminar imprime un resumen breve (máx. 5 viñetas) de lo
   hecho y el estado actual del repo (commit, rama/s tocadas, push, archivos, resultados de
   tests y subida) para que el usuario pueda ejecutar `/compact` y continuar con contexto
   limpio.
