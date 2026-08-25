#ifndef MOONS_H
#define MOONS_H

struct MoonInfo {
    const char *name;
    float gravity;
};

static const MoonInfo MOONS[] = {
    { "LUNA", 1.00f },
    { "IO", 1.10f },
    { "EUROPA", 0.85f },
    { "GANYMEDES", 0.95f },
    { "CALLISTO", 0.90f },
    { "TITAN", 0.90f },
    { "ENCELADUS", 0.70f },
    { "TRITON", 0.75f },
};

static const int MOON_COUNT = (int)(sizeof(MOONS) / sizeof(MOONS[0]));

// Gameplay order of the 8-level cycle, walking from calm moons to hostile
// ones so the normal game ramps up in difficulty:
//   1 LUNA      (grav 1.00, no hazard) - baseline
//   2 EUROPA    (0.85, acid rain)      - first soft hazard
//   3 CALLISTO  (0.90, calm)           - breather, wormhole host
//   4 ENCELADUS (0.70, geysers)        - floaty gravity + plume push
//   5 TITAN     (0.90, fog/atmosphere) - low visibility, drag
//   6 GANYMEDES (0.95, debris rings)   - rock collisions, zoom weaving
//   7 TRITON    (0.75, twister)        - vortex capture
//   8 IO        (1.10, lava + quakes)  - finale, heaviest gravity
static const int MOON_DIFFICULTY_ORDER[MOON_COUNT] = {0, 2, 4, 6, 5, 3, 7, 1};

static inline int moonIndex(int level)
{
    int s = (level - 1) % MOON_COUNT;
    if (s < 0) s += MOON_COUNT;
    return MOON_DIFFICULTY_ORDER[s];
}

// Inverse map: the slot within the cycle that plays moon table index idx.
static inline int moonSlotOfIndex(int idx)
{
    for (int s = 0; s < MOON_COUNT; s++)
        if (MOON_DIFFICULTY_ORDER[s] == idx) return s;
    return 0;
}

static inline const char *moonName(int level)
{
    return MOONS[moonIndex(level)].name;
}

static inline float moonGravity(int level)
{
    return MOONS[moonIndex(level)].gravity;
}

static inline bool moonHasGeysers(int level)
{
    return moonIndex(level) == 6; // ENCELADUS
}

static inline bool moonHasVolcanoes(int level)
{
    return moonIndex(level) == 1; // IO
}

static inline bool moonHasTitan(int level)
{
    return moonIndex(level) == 5; // TITAN
}

static inline bool moonHasRings(int level)
{
    return moonIndex(level) == 3; // GANYMEDES (debris rings)
}

static inline bool moonHasTwister(int level)
{
    return moonIndex(level) == 7; // TRITON (nitrogen twister)
}

static inline bool moonHasAcidRain(int level)
{
    return moonIndex(level) == 2; // EUROPA (acid rain)
}

static inline bool moonHasQuakes(int level)
{
    return moonIndex(level) == 1; // IO (tectonic quakes)
}

// Moons with no ambient effect of their own (no geysers, volcanoes, fog,
// rings, twister, acid rain or quakes): the only ones that can host a sky
// wormhole, because the wormhole never combines with any other effect.
static inline bool moonEffectFree(int level)
{
    return !moonHasGeysers(level) && !moonHasVolcanoes(level) &&
           !moonHasTitan(level) && !moonHasRings(level) && !moonHasTwister(level) &&
           !moonHasAcidRain(level) && !moonHasQuakes(level);
}

#endif
