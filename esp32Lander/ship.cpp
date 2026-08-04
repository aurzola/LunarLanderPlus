#include <cmath>
#include <cstring>
#include "ship.h"
#include "config.h"
#include "renderer.h"

Ship::Ship()
    : posX(0), posY(0), velX(0), velY(0),
      rotation(-90), targetRotation(-90),
      thrustBuild(0), fuel(FUEL_MAX), scale(1.0f),
      altitude(0), active(true), exploding(false), counter(0)
{
    defineShapes();
    memset(shapePosX, 0, sizeof(shapePosX));
    memset(shapePosY, 0, sizeof(shapePosY));
}

void Ship::defineShapes()
{
    // hexagonal body
    shapes[0].dx[0] = -2.6f; shapes[0].dy[0] = -5.0f;
    shapes[0].dx[1] =  2.6f; shapes[0].dy[1] = -5.0f;
    shapes[0].dx[2] =  5.0f; shapes[0].dy[2] = -2.6f;
    shapes[0].dx[3] =  5.0f; shapes[0].dy[3] =  2.6f;
    shapes[0].dx[4] =  2.6f; shapes[0].dy[4] =  5.0f;
    shapes[0].dx[5] = -2.6f; shapes[0].dy[5] =  5.0f;
    shapes[0].dx[6] = -5.0f; shapes[0].dy[6] =  2.6f;
    shapes[0].dx[7] = -5.0f; shapes[0].dy[7] = -2.6f;
    shapes[0].count = 8;
    shapes[0].closed = true;
    shapes[0].velX = 1.0f; shapes[0].velY = -2.5f;

    // cockpit rect (right side window)
    shapes[1].dx[0] = 0.5f; shapes[1].dy[0] = -3.5f;
    shapes[1].dx[1] = 3.0f; shapes[1].dy[1] = -3.5f;
    shapes[1].dx[2] = 3.0f; shapes[1].dy[2] = -1.0f;
    shapes[1].dx[3] = 0.5f; shapes[1].dy[3] = -1.0f;
    shapes[1].count = 4;
    shapes[1].closed = true;
    shapes[1].velX = 2.0f; shapes[1].velY = -1.5f;

    // left leg
    shapes[2].dx[0] = -2.5f; shapes[2].dy[0] = 5.0f;
    shapes[2].dx[1] = -5.0f; shapes[2].dy[1] = 10.0f;
    shapes[2].dx[2] = -7.0f; shapes[2].dy[2] = 10.0f;
    shapes[2].dx[3] = -3.5f; shapes[2].dy[3] = 10.0f;
    shapes[2].count = 4;
    shapes[2].closed = false;
    shapes[2].velX = 0.0f; shapes[2].velY = -3.0f;

    // right leg
    shapes[3].dx[0] = 2.5f;  shapes[3].dy[0] = 5.0f;
    shapes[3].dx[1] = 5.0f;  shapes[3].dy[1] = 10.0f;
    shapes[3].dx[2] = 7.0f;  shapes[3].dy[2] = 10.0f;
    shapes[3].dx[3] = 3.5f;  shapes[3].dy[3] = 10.0f;
    shapes[3].count = 4;
    shapes[3].closed = false;
    shapes[3].velX = 3.0f; shapes[3].velY = -1.0f;

    // left thruster nozzle
    shapes[4].dx[0] = -1.5f; shapes[4].dy[0] = 5.0f;
    shapes[4].dx[1] = -3.0f; shapes[4].dy[1] = 9.0f;
    shapes[4].dx[2] = -2.5f; shapes[4].dy[2] = 10.0f;
    shapes[4].count = 3;
    shapes[4].closed = false;
    shapes[4].velX = 1.0f; shapes[4].velY = -1.0f;

    // right thruster nozzle
    shapes[5].dx[0] = 1.5f;  shapes[5].dy[0] = 5.0f;
    shapes[5].dx[1] = 3.0f;  shapes[5].dy[1] = 9.0f;
    shapes[5].dx[2] = 2.5f;  shapes[5].dy[2] = 10.0f;
    shapes[5].count = 3;
    shapes[5].closed = false;
    shapes[5].velX = 2.5f; shapes[5].velY = -1.0f;
}

void Ship::reset(float x, float y)
{
    posX = x;
    posY = y;
    velX = 0.415f;
    velY = 0;
    rotation = 0;
    targetRotation = 0;
    thrustBuild = 0;
    fuel = FUEL_MAX;
    scale = 1.0f;
    active = true;
    exploding = false;
    counter = 0;
    for (int i = 0; i < 6; i++) {
        shapePosX[i] = 0;
        shapePosY[i] = 0;
    }
}

void Ship::setTargetRotation(float deg)
{
    if (deg < ROTATION_MIN_DEG) deg = ROTATION_MIN_DEG;
    if (deg > ROTATION_MAX_DEG) deg = ROTATION_MAX_DEG;
    targetRotation = deg;
}

void Ship::setThrust(float power)
{
    if (power < 0) power = 0;
    if (power > 1) power = 1;
    thrustBuild += (power - thrustBuild) * 0.4f;
    if (thrustBuild < 0.01f) thrustBuild = 0;
}

void Ship::update()
{
    counter++;

    rotation += (targetRotation - rotation) * ROTATION_LERP;
    if (fabsf(rotation - targetRotation) < 0.1f) rotation = targetRotation;

    if (exploding) {
        updateExplosion();
        return;
    }

    if (!active) return;

    if (fuel <= 0) {
        thrustBuild = 0;
    }

    if (thrustBuild > 0) {
        float rad = rotation * PI / 180.0f;
        velX += THRUST_ACCEL * thrustBuild * sinf(rad);
        velY -= THRUST_ACCEL * thrustBuild * cosf(rad);
        fuel -= FUEL_PER_THRUST * thrustBuild;
    }

    posX += velX;
    posY += velY;
    velX *= DRAG;
    velY += GRAVITY;

    if (velY > TOP_SPEED) velY = TOP_SPEED;
    else if (velY < -TOP_SPEED) velY = -TOP_SPEED;
    if (velX > TOP_SPEED) velX = TOP_SPEED;
    else if (velX < -TOP_SPEED) velX = -TOP_SPEED;

    left = posX - 10.0f * scale;
    right = posX + 10.0f * scale;
    bottom = posY + 14.0f * scale;
    top = posY - 5.0f * scale;

    if (fuel < 0) fuel = 0;
}

void Ship::draw(Renderer &r, float viewX, float viewY, float viewScale)
{
    float sx = posX * viewScale + viewX;
    float sy = posY * viewScale + viewY;
    float rad = rotation * PI / 180.0f;
    float cs = cosf(rad);
    float sn = sinf(rad);
    float sc = scale * viewScale;

    for (int s = 0; s < 6; s++) {
        const ShipShape &sh = shapes[s];
        float ox = shapePosX[s] * viewScale;
        float oy = shapePosY[s] * viewScale;

        for (int i = 0; i < sh.count; i++) {
            float x1 = sx + (sh.dx[i] * cs - sh.dy[i] * sn) * sc + ox;
            float y1 = sy + (sh.dx[i] * sn + sh.dy[i] * cs) * sc + oy;
            int ni = (i + 1) % sh.count;
            if (!sh.closed && i == sh.count - 1) continue;
            float x2 = sx + (sh.dx[ni] * cs - sh.dy[ni] * sn) * sc + ox;
            float y2 = sy + (sh.dx[ni] * sn + sh.dy[ni] * cs) * sc + oy;
            r.line(x1, y1, x2, y2);
        }
    }

    if (thrustBuild > 0 && active) {
        float flicker = ((counter >> 1) % 3) * 0.2f + 1.0f;
        float flameLen = thrustBuild * 20.0f * flicker;
        float fx1 = sx + (-1.5f * cs - 5.0f * sn) * sc;
        float fy1 = sy + (-1.5f * sn + 5.0f * cs) * sc;
        float fx2 = sx + (0 * cs - (5.0f + flameLen) * sn) * sc;
        float fy2 = sy + (0 * sn + (5.0f + flameLen) * cs) * sc;
        float fx3 = sx + (1.5f * cs - 5.0f * sn) * sc;
        float fy3 = sy + (1.5f * sn + 5.0f * cs) * sc;
        r.line(fx1, fy1, fx2, fy2);
        r.line(fx2, fy2, fx3, fy3);
    }
}

void Ship::crash()
{
    rotation = 0;
    targetRotation = 0;
    active = false;
    exploding = true;
    thrustBuild = 0;
}

void Ship::land()
{
    active = false;
    thrustBuild = 0;
}

void Ship::updateExplosion()
{
    for (int i = 0; i < 6; i++) {
        shapePosX[i] += shapes[i].velX * 0.1f;
        shapePosY[i] += shapes[i].velY * 0.1f;
    }
}
