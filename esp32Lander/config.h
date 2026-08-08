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
const int DEMO_LEVEL_FORCE = 0;  // el demo elige nivel al azar 1..DEMO_MAX_LEVEL
const int START_LEVEL = 1;  // el juego comienza por el nivel 1 (LUNA)
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
// Titan fog now uses the same soft style as Ganymede: concentric elliptical
// bands with a gentle gaussian falloff (via LUT). They still hide the ship
// while crossing. Each band: base world-y center + concentric arc + drift.
const int FOG_BAND_COUNT = 3;
const float FOG_BAND_HALF = 34.0f;     // band half thickness (u)
const float FOG_BAND_START = 205.0f;   // first band center (world-y)
const float FOG_BAND_GAP_MIN = 70.0f;
const float FOG_BRIGHT = 44.0f;        // fog luma (a bit stronger than before)
// Concentric ellipse the fog follows (shared center with the moon, like rings).
const float FOG_ELLIPSE_CX = 400.0f;
const float FOG_ELLIPSE_RAD = 520.0f;
const float FOG_CURVE_A = 45.0f;
// Alive so the blind zones can't be memorized: each band drifts vertically.
const float FOG_DRIFT_A = 18.0f;
const float FOG_DRIFT_SPEED_MIN = 0.12f;
const float FOG_DRIFT_SPEED_MAX = 0.20f;
// Fog and terrain halo are never drawn above this screen row so they can't
// reach the HUD / minimap / warnings (LOW FUEL=72, TOO FAST=82 with wind) and
// make them flicker during the approach phase. 100 clears them all.
const int FOG_SCREEN_TOP = 68;

// Ganymede debris bands (franjas de roca): the moon's debris is rendered as
// two layers of rock floating above the terrain silhouette, like Jupiter's
// rings seen edge-on. Each band hugs the terrain at a fixed height; rocks are
// irregular polygons of various sizes drifting along the band so the gaps
// shift, and the ship weaves down through both without touching one. The
// higher band is denser (harder), the lower sparser, but both keep a minimum
// passable gap (RING_GAP_MIN) so it is never impossible.
// Ganymede debris bands (franjas de roca): the moon's debris is rendered as a
// few bands of hollow rock polygons hugging the terrain silhouette (the same
// "band" concept as Titan's fog sheets, but made of rocks). Each band sits at
// a fixed height above the terrain and holds danger rocks drawn only as
// outlines (hollow), of varying irregular shape and size. The ship weaves down
// through both without touching a rock. Higher band is denser (harder), the
// lower sparser, but both keep a guaranteed passable gap (RING_GAP_MIN).
const int RING_COUNT = 2;
const float RING_CY_HIGH = 360.0f;   // upper band: crossed first in the normal
                                     // (zoom-out) approach, higher in the sky
const float RING_CY_LOW = 560.0f;    // lower band: concentric ellipse, lower
                                     // (crossed in zoom-in), kept above terrain
const int RING_SMALL_HIGH = 24;      // decorative small rocks (no collision)
const int RING_SMALL_LOW = 30;
const int RING_DANGER_HIGH = 8;      // big dangerous rocks (collide)
const int RING_DANGER_LOW = 16;      // more, so the lower band is harder
const float RING_DRIFT_LOW = 4.0f;     // horizontal drift (world u/s)
const float RING_DRIFT_HIGH = -6.0f;   // opposite direction for the upper band
const float RING_SMALL_MIN_R = 1.2f;  // small rock radius range (never collide)
const float RING_SMALL_MAX_R = 3.0f;
const float RING_DANGER_MIN_R = 7.0f; // big rock radius range (collides)
const float RING_DANGER_MAX_R = 13.0f;
const float RING_Y_JITTER = 45.0f;     // vertical scatter: rocks spread up/down,
                                       // not a single row
const float RING_SPIN_MAX = 0.6f;      // rock rotation speed (rad/s)
const float RING_GAP_MIN = 22.0f;      // gap between danger rocks (u); grouped so
                                       // they are tighter but still passable
const float RING_SHIP_RADIUS = 8.0f;   // ship collision circle radius
const float RING_ROCK_HIT = 0.7f;      // danger collision radius = RING_ROCK_HIT*size
// Concentric elliptical arc the rings follow (like real rings around a moon):
// a smooth bow peaking over the moon's center, no sharp edges. Both rings
// share the same ellipse center (concentric), only their ring radius differs.
const float RING_ELLIPSE_CX = 400.0f;  // shared ellipse center x (world)
const float RING_ELLIPSE_RAD = 520.0f; // semi-major axis spanning the world
const float RING_CURVE_A = 55.0f;      // vertical bow amplitude at the center (u)
// Denser debris fog behind the rocks so they stand out on the band (Ganymede).
// Brightness is higher than Titan's fog (FOG_BRIGHT=32) for extra contrast.
const float RING_FOG_HALF = 34.0f;     // band half thickness (u)
const int RING_FOG_BRIGHT = 0; // TEMP TEST: niebla desactivada (ver solo rocas)

// Triton nitrogen twister: a wandering vortex that sucks the ship toward its
// base (on the ground). While captured the ship is held ON the funnel wall (a
// cone that tapers to a thin point at the ground) and spirals down it, while
// the nose tumbles continuously (270-360+ degrees). Touching the ground inside
// the vortex smashes the ship. The escape is physical: if the outward radial
// thrust overcomes the (strength-scaled) pull and the ship already moves
// outward, it is violently flung out along the tangent and must regain heading.
// Escape chance is inversely proportional to strength because the pull grows
// with it.
const float TWISTER_RADIUS = 150.0f;        // influence radius (world units)
const float TWISTER_HEIGHT = 240.0f;        // funnel top above ground
const float TWISTER_HOLD_GAIN = 0.06f;      // tangential grip gain while fighting (per tick)
const float TWISTER_HOLD_RAMP = 20.0f;      // ticks to ramp the grip to full (no snap)
const float TWISTER_ORBIT_SPEED = 0.010f;   // tangential target speed per unit radius (u/tick/u)
const float TWISTER_ORBIT_MAX = 0.35f;      // tangential speed cap (u/tick)
const float TWISTER_CAPTURE_RAMP = 40.0f;   // ticks to ease the ship onto the funnel wall
const float TWISTER_SPIRAL_RATE = 200.0f;   // zig-zag advance while captured (deg/s)
const float TWISTER_DESCENT = 45.0f;        // vertical descent while captured (u/s)
const float TWISTER_WOBBLE_RANGE = 60.0f;   // self-rock of the nose while captured (deg, x strength)
const float TWISTER_WOBBLE_RATE = 3.5f;     // rad/s of the wobble oscillation (x strength)
const float TWISTER_WOBBLE_MAX = 80.0f;     // hard cap on captured rotation (never reaches +/-90)
const float TWISTER_STICK_GAIN = 0.5f;      // joystick authority while captured (of the full angle)
const float TWISTER_ESCAPE_THRUST = 0.0011f;// radial-thrust at the top that counts as "fighting"
const float TWISTER_ESCAPE_DEPTH_THRUST = 1.0f; // extra escape thrust needed per unit depth
const float TWISTER_ESCAPE_VEL = 0.06f;     // outward radial speed at the top that breaks free
const float TWISTER_ESCAPE_DEPTH_VEL = 1.5f;    // extra escape velocity needed per unit depth
const float TWISTER_ESCAPE_TICKS = 25;      // sustained ticks above escape velocity to break free
const float TWISTER_FLING = 0.35f;          // outward velocity added on escape
const float TWISTER_ESCAPE_MARGIN = 0.15f;  // thrust must exceed pull by this factor
const float TWISTER_ESCAPE_COOLDOWN = 1.5f; // grace period after an escape (s)
const float TWISTER_STRENGTH_MIN = 0.5f;    // random per level
const float TWISTER_STRENGTH_MAX = 1.4f;
const float TWISTER_DRIFT_SPEED = 8.0f;     // horizontal wander (world u/s)
const float TWISTER_SPIN_KICK = 25.0f;      // deg yank on escape (x strength)
// Twister cone geometry (physics + visual tapering to a thin ground tip).
const float TWISTER_BASE_HALF = 6.0f;        // half width at the ground tip (u) [ORIGINAL]
const float TWISTER_TOP_HALF = 34.0f;        // half width at the funnel top (u)
const float TWISTER_EDGE_POKE = 0.12f;      // max overshoot beyond the cone wall while captured
const float TWISTER_SWAY_AMP = 7.0f;        // funnel sway (u)
const float TWISTER_SWAY_SPEED = 1.3f;      // rad/s

// Fuel tanker: an aerial tanker ship hovering over flat terrain. It trails two
// refueling hoses (probe-and-drogue): the ship must fly the module's refuel
// probe into the drogue basket at the end of a hose and STAY connected, because
// fuel flows incrementally (longer connection = more fuel). Touching the
// mothership hull destroys both ships. It only appears when fuel is running low
// (< half), so the rendezvous is reserved for when it actually matters.
const int TANKER_START_LEVEL = 2;
const int TANKER_FORCE_LEVEL1 = 1; // TEMP CRT test: force tanker in level 1
const int TANKER_CHANCE_PERCENT = 70;
const float TANKER_HOVER_ALT = 340.0f;   // hover so the dock altitude clears the minimap (ZOOM_IN_ALT=200)
const float TANKER_TITAN_Y = 85.0f;      // Titan: fixed world-y, clear of the fog
const float TANKER_FUEL_FRACTION = 0.5f; // spawn only when fuel < FUEL_MAX * this
const float TANKER_HULL_W = 22.0f;
const float TANKER_HULL_H = 6.0f;
const float TANKER_PLATFORM_W = 24.0f;
const float TANKER_DRIFT_SPEED = 9.0f;
const float TANKER_DRIFT_RANGE = 40.0f;
const float TANKER_BOB_AMP = 3.0f;
const float TANKER_BOB_SPEED = 0.9f;
const float TANKER_DOCK_TOL_X = 8.0f;
const float TANKER_DOCK_TOL_Y = 5.0f;
const float TANKER_DOCK_ZONE_X = 90.0f;
const float TANKER_DOCK_ZONE_Y = 45.0f;
const float TANKER_HOSE_LEN = 20.0f;         // drogue hangs this far from the hull
const float TANKER_APPROACH_X = 55.0f;       // demo pre-position: left of the drogue
const float TANKER_DROGUE_RIM = 3.0f;        // basket mouth half-width (world u)
const float TANKER_DROGUE_DEPTH = 3.5f;      // cone depth from target to mouth (world u)
const float TANKER_DROGUE_BACK_R = 1.6f;     // filled target radius at the basket back
// Picture-in-picture docking window: magnified contact point (basket + probe).
const int PIP_SIZE = 76;                     // window size in px
const float PIP_SCALE = SCREEN_H / 700.0f * 16.0f; // px per world unit inside the PiP
const float TANKER_DROGUE_SWAY = 3.0f;       // drogue sway amplitude (world u)
const float TANKER_DROGUE_SWAY_SPEED = 1.6f; // rad/s, independent of the bob phase
const float TANKER_REFUEL_RATE = 200.0f;     // fuel units per second while connected
const float TANKER_HULL_MARGIN = 3.0f;       // ship touching the hull destroys both
const float TANKER_LEAVE_SPEED = 1.4f;
const float TANKER_LEAVE_DIST = 90.0f;
const float TANKER_NOZZLE_LEN = 8.0f;    // module refuel probe offset from ship center

// Docking mini-game: hold the probe in the drogue for 1 second before fuel
// starts to flow, then keep it aligned while the tanker drifts/bobs/sways.
// Going outside the larger break tolerance for this long breaks the link.
const float TANKER_DOCK_LOCK_TIME = 1.0f;
const float TANKER_DOCK_BREAK_TIME = 0.4f;
const float TANKER_DOCK_BREAK_TOL_X = 12.0f;
const float TANKER_DOCK_BREAK_TOL_Y = 8.0f;

#endif
