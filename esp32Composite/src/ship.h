#ifndef SHIP_H
#define SHIP_H

#include <vector>

class Renderer;

class Ship {
public:
    Ship();
    Ship(double x, double y, double xv, double yv);

    double getXpos();
    double getYpos();
    void setPos(double x, double y);

    double getXvel();
    double getYvel();
    void setVel(double xv, double yv);

    double getGas();
    void setGas(double g);

    void rotate(double newAng);
    void setAng(double angle);
    double getAng();

    void setAccMode(int mode);
    int getAccMode();

    void accelerate(double &xv, double &yv);
    void accelerateChange(int modifier);

    void draw(Renderer &r);
    int collision(const std::vector<int> &xt, const std::vector<int> &yt);
    void hitbox();

private:
    double xpos;
    double ypos;
    double xVel;
    double yVel;
    double ang;
    int accMode;
    double gas;

    std::vector<int> foot1XPoints;
    std::vector<int> foot1YPoints;
    std::vector<int> foot2XPoints;
    std::vector<int> foot2YPoints;
    std::vector<int> leg1XPoints;
    std::vector<int> leg1YPoints;
    std::vector<int> leg2XPoints;
    std::vector<int> leg2YPoints;
    std::vector<int> bodyXPoints;
    std::vector<int> bodyYPoints;
};

#endif
