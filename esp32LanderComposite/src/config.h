#ifndef CONFIG_H
#define CONFIG_H

const float WORLD_W = 800.0f;
const float WORLD_H = 600.0f;
const float SCREEN_W = 320.0f;
const float SCREEN_H = 240.0f;
const float GAME_DT = 0.01f;
#ifndef PI
const float PI = 3.14159265f;
#endif

const float GRAVITY = 0.0005f;
const float THRUST_ACCEL = 0.0018f;
const float DRAG = 0.9997f;
const float TOP_SPEED = 0.35f;
const float FUEL_MAX = 1000.0f;
const float FUEL_PER_THRUST = 0.2f;
const float FUEL_CRASH_LOSS = 250.0f;

const float ZOOM_IN_ALT = 200.0f;
const float ZOOM_OUT_ALT = 350.0f;
const float ZOOM_FACTOR = 2.5f;

const float ROTATION_STEP = 15.0f;
const float ROTATION_LERP = 0.3f;
const int ROTATION_MIN_DEG = -90;
const int ROTATION_MAX_DEG = 90;

const float LAND_PERFECT_VY = 0.075f;
const float LAND_HARD_VY = 0.15f;
const float LAND_HARD_VX = 0.15f;
const float LAND_MAX_ROTATION = 5.0f;

const float CRASH_RESET_DELAY = 4.0f;
const float GAMEOVER_RESET_DELAY = 5.0f;

const float DEMO_START_DELAY = 13.0f;
const int DEMO_MAX_LEVEL = 4;
const int DEMO_LEVEL_FORCE = 4;
const float DEMO_POWER_RATE = 0.4f;

const int WIND_START_LEVEL = 1;
const float WIND_MIN = 0.35f;
const float WIND_ACCEL = 0.0004f;
const int WIND_STREAK_COUNT = 18;
const float WIND_STREAK_MIN = 20.0f;
const float WIND_STREAK_MAX = 60.0f;
const float WIND_STREAK_SPEED = 45.0f;
const float WIND_ALT_MAX = 250.0f;
const int WIND_STREAK_MIN_VISIBLE = 3;

const int DUST_COUNT = 24;
const float DUST_SPEED = 0.7f;
const float DUST_LIFE = 5.0f;
const float DUST_RANGE = 120.0f;
const float DUST_NEAR_RANGE = 150.0f;

const float LEVEL_INTRO_TIME = 2.4f;
const float INTRO_FADE_IN = 0.35f;
const float INTRO_FADE_OUT = 0.9f;

const int MAX_STARS = 60;

#endif
