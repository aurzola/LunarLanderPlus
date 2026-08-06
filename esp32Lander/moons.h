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

static inline int moonIndex(int level)
{
    int i = (level - 1) % MOON_COUNT;
    if (i < 0) i += MOON_COUNT;
    return i;
}

static inline const char *moonName(int level)
{
    return MOONS[moonIndex(level)].name;
}

static inline float moonGravity(int level)
{
    return MOONS[moonIndex(level)].gravity;
}

#endif
