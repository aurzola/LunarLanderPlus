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

const float DEMO_START_DELAY = 5.0f;
const int DEMO_MAX_LEVEL = 12;
const int DEMO_LEVEL_FORCE = 6;   // TEMP: demo fija nivel Titán (revertir a 0 tras CRT)
const int START_LEVEL = 1;
const float DEMO_POWER_RATE = 0.4f;

const int WIND_START_LEVEL = 4;
const int WIND_CHANCE_PERCENT = 50;
const float WIND_MIN = 0.35f;
const float WIND_ACCEL = 0.0004f;
const int WIND_STREAK_COUNT = 18;
const float WIND_STREAK_MIN = 20.0f;
const float WIND_STREAK_MAX = 60.0f;
const float WIND_STREAK_SPEED = 45.0f;
const float WIND_ALT_MAX = 250.0f;
const float WIND_FORK_ALT = 0.5f;
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

const int STORM_START_LEVEL = 3;
const int STORM_CHANCE_PERCENT = 50;
const float STORM_BOLT_MIN = 4.0f;
const float STORM_BOLT_MAX = 8.0f;
const float STORM_BOLT_LIFE = 0.22f;
const float STORM_BOLT_FADE = 0.35f;
const int STORM_BOLT_SEGMENTS = 12;
const float STORM_BOLT_JITTER = 48.0f;
const float STORM_HIT_RADIUS = 70.0f;
const float STORM_HIT_FUEL = 60.0f;
const float STORM_CONTROL_LOSS = 1.5f;

const int GEYSER_VENTS = 5;
const float GEYSER_BURST = 3.0f;
const float GEYSER_GAP_MIN = 3.0f;
const float GEYSER_GAP_MAX = 9.0f;
const int GEYSER_PARTS_PER_TICK = 1;
const float GEYSER_PART_LIFE = 2.5f;
const float GEYSER_PART_SPEED = 24.0f;
const float GEYSER_PART_GRAV = 7.0f;
const float GEYSER_PART_SPREAD = 0.6f;
const float GEYSER_SPOUT_H = 40.0f;
const float GEYSER_FLASH = 0.25f;
const int GEYSER_MAX_PARTS = 250;
const float GEYSER_VENT_OFFSET = 14.0f;
const float GEYSER_RADIUS = 8.0f;
const float GEYSER_PLUME_H = 40.0f;
const float GEYSER_PUSH = 0.00025f;

const int VOLCANO_VENTS = 4;
const float VOLCANO_BURST = 9.0f;
const float VOLCANO_GAP_MIN = 0.5f;
const float VOLCANO_GAP_MAX = 1.5f;
const float VOLCANO_FLOW_LEN = 60.0f;
const float VOLCANO_FLOW_STEP = 4.0f;
const float VOLCANO_ERUPT_SPEED = 26.0f;
const float VOLCANO_PART_GRAV = 10.0f;
const float VOLCANO_PART_LIFE = 1.3f;
const float VOLCANO_FLASH = 0.25f;
const int VOLCANO_MAX_PARTS = 200;
const float VOLCANO_MIN_DROP = 14.0f;
const float VOLCANO_PEAK_R = 20.0f;
const float VOLCANO_SAFE_STRIP = 8.0f;
const int VOLCANO_MAX_VISIBLE = 3;

// Titan (thick methane atmosphere): extra horizontal drag and a gentle
// downdraft per tick, plus fog bands that hide the ship while crossing them.
const float ATMOS_DRAG = 0.9992f;
const float ATMOS_DOWN = 0.00008f;
const int FOG_BAND_COUNT = 3;
const float FOG_BAND_HALF_MIN = 35.0f;
const float FOG_BAND_HALF_MAX = 50.0f;
const float FOG_BAND_START = 185.0f;
const float FOG_BAND_GAP_MIN = 70.0f;
const float FOG_BRIGHT = 32.0f;
// Fog is alive so the blind zones can't be memorized: each band drifts
// vertically and its edges undulate (variable thickness across x and time).
const float FOG_DRIFT_A = 20.0f;
const float FOG_DRIFT_SPEED_MIN = 0.12f;
const float FOG_DRIFT_SPEED_MAX = 0.20f;
const float FOG_WAVE_A = 0.35f;
const float FOG_WAVE_K = 0.02f;
const float FOG_WAVE_SPEED = 0.15f;
// Fog and terrain halo are never drawn above this screen row so they can't
// reach the HUD / minimap / warnings (LOW FUEL=72, TOO FAST=82 with wind) and
// make them flicker during the approach phase. 100 clears them all.
const int FOG_SCREEN_TOP = 100;

// Ganymede debris rings: two concentric rings of orbiting rock the ship must
// weave through while descending. The inner ring turns slowly, the outer one
// fast (opposite direction), so the gaps are never static. Rocks only exist
// (and collide) above the terrain silhouette - the far side is hidden by the
// moon itself.
const int RING_COUNT = 2;
const float RING_CX = 400.0f;
const float RING_CY = 260.0f;
const float RING_RADIUS_INNER = 130.0f;
const float RING_RADIUS_OUTER = 215.0f;
const int RING_ROCKS_INNER = 22;
const int RING_ROCKS_OUTER = 32;
const float RING_SPEED_INNER = 0.06f;   // slow ring (rad/s)
const float RING_SPEED_OUTER = -0.30f;  // fast ring, opposite direction
const float RING_ROCK_RADIUS = 6.0f;    // world radius of one rock (collision)
const float RING_SHIP_RADIUS = 12.0f;   // ship collision circle radius
const int RING_ROCK_DRAW_MIN = 1;       // min screen radius in px

#endif
