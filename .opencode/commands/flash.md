---
description: Commit, actualiza AGENTS.md, sube a la placa y deja resumen para /compact
agent: build
---

# Flujo flash (commit + docs + upload + compact)

Workflow del repo lunar lander. Ejecuta los pasos en orden:

1. **Documentación**: revisa `git status` y `git diff` de los cambios sin commitear y
   actualiza `AGENTS.md` para que refleje el estado real (constantes en `config.h`,
   pantalla de bienvenida/título, minimapa, controles, etc.). Mantén la estructura y el
   idioma español existentes. Mantén `esp32Lander/` y `esp32LanderComposite/src/` en
   sync (mismas fuentes) verificando con `diff`.

2. **Verificación**: en `esp32Lander/` ejecuta `make clean && make` y `./test_pc`
   (todos los checks deben pasar) y `./demo_sim` si el cambio afecta al autopilot de
   demo. Compila el sketch con
   `arduino-cli compile --fqbn esp32:esp32:esp32 esp32LanderComposite/esp32LanderComposite.ino`.

3. **Commit**: haz `git add` de los archivos pertinentes (incluye `AGENTS.md` y ambos
   `game.cpp`/`config.h` si aplica) y crea un commit con mensaje corto en el estilo del
   repo (p. ej. título: "Descripción breve"). No hagas push ni crees PR a menos que se pida.

4. **Subida a la placa**: comprueba que el puerto existe (`ls /dev/ttyUSB0`) y sube con
   `arduino-cli upload --fqbn esp32:esp32:esp32 --port /dev/ttyUSB0
   esp32LanderComposite/esp32LanderComposite.ino`. Confirma "Hash of data verified". Si el
   puerto no existe o falla la subida, repórtalo claramente y detente (no reintentar sin fin).

5. **Resumen para /compact**: al terminar imprime un resumen breve (máx. 5 viñetas) de lo
   hecho y el estado actual del repo (commit, archivos tocados, resultados de tests y
   subida) para que el usuario pueda ejecutar `/compact` y continuar con contexto limpio.
