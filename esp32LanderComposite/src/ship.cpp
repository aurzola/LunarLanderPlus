#include <cmath>
#include "ship.h"
#include "config.h"
#include "renderer.h"

Ship::Ship()
    : xpos(0), ypos(0), xVel(0), yVel(0), ang(0), accMode(0), gas(750)
{
}

Ship::Ship(double x, double y, double xv, double yv)
    : xpos(x), ypos(y), xVel(xv), yVel(yv), ang(0), accMode(0), gas(750)
{
}

double Ship::getXpos() { return xpos; }
double Ship::getYpos() { return ypos; }
void Ship::setPos(double x, double y) { xpos = x; ypos = y; }
double Ship::getXvel() { return xVel; }
double Ship::getYvel() { return yVel; }
void Ship::setVel(double xv, double yv) { xVel = xv; yVel = yv; }
double Ship::getGas() { return gas; }
void Ship::setGas(double g) { gas = (g > 0) ? g : 0; }

void Ship::rotate(double newAng)
{
    if (newAng > 0) ang = 0;
    else if (newAng < -PI) ang = -PI;
    else ang = newAng;
}

void Ship::setAng(double angle) { ang = angle; }
double Ship::getAng() { return ang; }
void Ship::setAccMode(int mode) { accMode = mode; }
int Ship::getAccMode() { return accMode; }

void Ship::accelerate(double &xv, double &yv)
{
    xv = xv + .04 * accMode * cos(ang);
    yv = yv + .05 * accMode * sin(ang);
    gas = gas - .02 * accMode;
    if (gas < 0) gas = 0;
}

void Ship::accelerateChange(int modifier)
{
    accMode += modifier;
    if (accMode < 0 || accMode > 8) accMode -= modifier;
    if (gas <= 0) accMode = 0;
}

void Ship::draw(Renderer &r)
{
    double kx = SCREEN_W / WORLD_W;
    double ky = SCREEN_H / WORLD_H;
    double sx = xpos * kx;
    double sy = ypos * ky;

    r.line(sx - 6. * cos(ang + PI / 6.), sy - 6. * sin(ang + PI / 6.),
           sx - 12. * cos(ang + PI / 6.), sy - 12. * sin(ang + PI / 6.));
    r.line(sx - 6. * cos(ang - PI / 6.), sy - 6. * sin(ang - PI / 6.),
           sx - 12. * cos(ang - PI / 6.), sy - 12. * sin(ang - PI / 6.));

    if (accMode > 0) {
        r.line(sx - 6. * cos(ang + PI / 6.), sy - 6. * sin(ang + PI / 6.),
               sx - 6. * cos(ang) - accMode * 2. * cos(ang),
               sy - 6. * sin(ang) - accMode * 2. * sin(ang));
        r.line(sx - 6. * cos(ang - PI / 6.), sy - 6. * sin(ang - PI / 6.),
               sx - 6. * cos(ang) - accMode * 2. * cos(ang),
               sy - 6. * sin(ang) - accMode * 2. * sin(ang));
    }
    r.circle(sx, sy, 6);
}

int Ship::collision(const std::vector<int> &xt, const std::vector<int> &yt)
{
    double kx = SCREEN_W / WORLD_W;
    double ky = SCREEN_H / WORLD_H;
    double sy = ypos * ky;
    const double LAND_TOL = 5.0;
    bool foot1Touch = false;
    bool foot2Touch = false;

    for (int i = 0; i < (int)xt.size(); i++) {
        int tx = (int)roundf(xt[i] * kx);
        int ty = (int)roundf(yt[i] * ky);
        for (int k = 0; k < (int)foot1XPoints.size(); k++) {
            if (foot1XPoints[k] > tx - 2 && foot1XPoints[k] < tx + 2) {
                if (foot1YPoints[k] >= ty - LAND_TOL && foot1YPoints[k] <= ty + 2) foot1Touch = true;
            }
            if (foot2XPoints[k] > tx - 2 && foot2XPoints[k] < tx + 2) {
                if (foot2YPoints[k] >= ty - LAND_TOL && foot2YPoints[k] <= ty + 2) foot2Touch = true;
            }
            if (foot1Touch && foot2Touch) return 2;
        }
    }

    for (int i = 0; i < (int)xt.size(); i++) {
        int tx = (int)roundf(xt[i] * kx);
        int ty = (int)roundf(yt[i] * ky);
        for (int k = 0; k < (int)leg1XPoints.size(); k++) {
            if (leg1XPoints[k] > tx - 2 && leg1XPoints[k] < tx + 2) {
                if (leg1YPoints[k] >= ty) return 1;
            }
            if (leg2XPoints[k] > tx - 2 && leg2XPoints[k] < tx + 2) {
                if (leg2YPoints[k] >= ty) return 1;
            }
        }
    }

    for (int i = 0; i < (int)xt.size(); i++) {
        int tx = (int)roundf(xt[i] * kx);
        int ty = (int)roundf(yt[i] * ky);
        for (int j = 0; j < (int)bodyXPoints.size(); j++) {
            if (bodyXPoints[j] > tx - 2 && bodyXPoints[j] < tx + 2) {
                if (bodyYPoints[j] > sy && bodyYPoints[j] > ty) return 1;
            }
        }
    }

    return 0;
}

void Ship::hitbox()
{
    double kx = SCREEN_W / WORLD_W;
    double ky = SCREEN_H / WORLD_H;
    double sx = xpos * kx;
    double sy = ypos * ky;

    bodyXPoints.clear();
    bodyYPoints.clear();
    leg1XPoints.clear();
    leg1YPoints.clear();
    leg2XPoints.clear();
    leg2YPoints.clear();
    foot1XPoints.clear();
    foot1YPoints.clear();
    foot2XPoints.clear();
    foot2YPoints.clear();

    for (float i = 0; i < 2. * PI; i += .25) {
        bodyXPoints.push_back((int)roundf(sx - 6. * cos(i)));
        bodyYPoints.push_back((int)roundf(sy - 6. * sin(i)));
    }

    for (float i = 6.; i < 11.; i += 1) {
        leg1XPoints.push_back((int)roundf(sx - i * cos(ang + PI / 6.)));
        leg1YPoints.push_back((int)roundf(sy - i * sin(ang + PI / 6.)));
        leg2XPoints.push_back((int)roundf(sx - i * cos(ang - PI / 6.)));
        leg2YPoints.push_back((int)roundf(sy - i * sin(ang - PI / 6.)));
    }

    for (float i = 11.; i <= 12.; i += 1) {
        foot1XPoints.push_back((int)roundf(sx - i * cos(ang + PI / 6.)));
        foot1YPoints.push_back((int)roundf(sy - i * sin(ang + PI / 6.)));
        foot2XPoints.push_back((int)roundf(sx - i * cos(ang - PI / 6.)));
        foot2YPoints.push_back((int)roundf(sy - i * sin(ang - PI / 6.)));
    }
}
