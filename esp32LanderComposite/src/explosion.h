#ifndef EXPLOSION_H
#define EXPLOSION_H

#include <stdint.h>
#include <cmath>
#include "renderer.h"

// Configuración de rendimiento de la animación
#define MAX_PARTICLES 60
#define MAX_DEBRIS 8
#define DURATION_FRAMES 90

// Estructuras de datos con punto fijo (16.16: posición *256, velocidad *256 con signo)
struct ExplosionParticle {
    int32_t x_fp;
    int32_t y_fp;
    int16_t vx_fp;
    int16_t vy_fp;
    bool activa;
};

struct ExplosionDebris {
    int32_t x_fp;
    int32_t y_fp;
    int16_t vx_fp;
    int16_t vy_fp;
    int16_t longitud;
    int16_t angulo;
    int16_t v_rot;
    bool activo;
};

class ExplosionManager {
private:
    ExplosionParticle particles[MAX_PARTICLES];
    ExplosionDebris debris[MAX_DEBRIS];
    int cx, cy;
    int current_frame;
    bool activa;

    // Tabla Look-Up de Seno (escala 0-256) en 64 pasos (360 grados), sin float
    const int16_t sinLUT[64] = {
        0, 25, 50, 74, 98, 120, 142, 162, 180, 197, 212, 225, 236, 244, 250, 253,
        254, 253, 250, 244, 236, 225, 212, 197, 180, 162, 142, 120, 98, 74, 50, 25,
        0, -25, -50, -74, -98, -120, -142, -162, -180, -197, -212, -225, -236, -244, -250, -253,
        -254, -253, -250, -244, -236, -225, -212, -197, -180, -162, -142, -120, -98, -74, -50, -25
    };

    int16_t getSin(int angle) { return sinLUT[angle & 63]; }
    int16_t getCos(int angle) { return sinLUT[(angle + 16) & 63]; } // cos(x) = sin(x + 90)

    // Disco relleno con brillo (nuestro Renderer::circle es de contorno sin brillo)
    void fillCircle(Renderer &r, int x, int y, int radio, int brillo) {
        if (radio <= 0) { r.pixelShade((float)x, (float)y, brillo); return; }
        for (int yy = -radio; yy <= radio; yy++) {
            int hw = (int)(sqrtf((float)(radio * radio - yy * yy)));
            for (int xx = -hw; xx <= hw; xx++)
                r.pixelShade((float)(x + xx), (float)(y + yy), brillo);
        }
    }

public:
    ExplosionManager() : cx(0), cy(0), current_frame(0), activa(false) {}

    void inicializar(int x, int y) {
        cx = x;
        cy = y;
        current_frame = 0;
        activa = true;

        for (int i = 0; i < MAX_PARTICLES; i++) {
            particles[i].x_fp = x << 8;
            particles[i].y_fp = y << 8;
            int angulo = (i * 64) / MAX_PARTICLES;
            int velocidad_base = 150 + (i % 3) * 40;
            particles[i].vx_fp = (int16_t)((getCos(angulo) * velocidad_base) >> 8);
            particles[i].vy_fp = (int16_t)((getSin(angulo) * velocidad_base) >> 8);
            particles[i].activa = true;
        }

        unsigned int seed = (unsigned int)(x ^ y);
        for (int i = 0; i < MAX_DEBRIS; i++) {
            seed = seed * 1103515245 + 12345;
            debris[i].x_fp = x << 8;
            debris[i].y_fp = y << 8;
            int angulo = (int)(seed % 64);
            int vel = 60 + (int)(seed % 50);
            debris[i].vx_fp = (int16_t)((getCos(angulo) * vel) >> 8);
            debris[i].vy_fp = (int16_t)((getSin(angulo) * vel) >> 8);
            debris[i].longitud = (int16_t)(6 + (seed % 8));
            debris[i].angulo = (int16_t)(seed % 64);
            debris[i].v_rot = (int16_t)(1 + (seed % 3));
            if (seed & 1) debris[i].v_rot = -(int16_t)debris[i].v_rot;
            debris[i].activo = true;
        }
    }

    void actualizarYRenderizar(Renderer &r) {
        if (!activa) return;

        current_frame++;
        if (current_frame >= DURATION_FRAMES) {
            activa = false;
            return;
        }

        uint8_t brillo = (uint8_t)(255 - ((current_frame * 255) / DURATION_FRAMES));

        // FASE 1: destello inicial ultra-brillante (primeros instantes)
        if (current_frame <= 3) {
            fillCircle(r, cx, cy, current_frame * 14, 255);
            return;
        }

        // FASE 2: partículas espaciales en expansión esférica
        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (!particles[i].activa) continue;
            particles[i].x_fp += particles[i].vx_fp;
            particles[i].y_fp += particles[i].vy_fp;
            int px = particles[i].x_fp >> 8;
            int py = particles[i].y_fp >> 8;
            if (px >= 0 && px < 320 && py >= 0 && py < 240) {
                r.pixelShade((float)px, (float)py, brillo);
            } else {
                particles[i].activa = false;
            }
        }

        // FASE 3: escombros vectoriales de la estructura (zeppelin roto)
        for (int i = 0; i < MAX_DEBRIS; i++) {
            if (!debris[i].activo) continue;
            debris[i].x_fp += debris[i].vx_fp;
            debris[i].y_fp += debris[i].vy_fp;
            debris[i].angulo = (int16_t)((debris[i].angulo + debris[i].v_rot) & 63);

            int dx = debris[i].x_fp >> 8;
            int dy = debris[i].y_fp >> 8;
            int mitad_l = debris[i].longitud >> 1;
            int x_offset = (getCos(debris[i].angulo) * mitad_l) >> 8;
            int y_offset = (getSin(debris[i].angulo) * mitad_l) >> 8;
            int x1 = dx - x_offset, y1 = dy - y_offset;
            int x2 = dx + x_offset, y2 = dy + y_offset;
            r.lineShade((float)x1, (float)y1, (float)x2, (float)y2, brillo);

            if (dx < -20 || dx > 340 || dy < -20 || dy > 260) debris[i].activo = false;
        }
    }

    bool estaActiva() const { return activa; }
    int frame() const { return current_frame; }
};

#endif
