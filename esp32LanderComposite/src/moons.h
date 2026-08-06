#ifndef MOONS_H
#define MOONS_H

struct MoonInfo {
    const char *name;
};

static const MoonInfo MOONS[] = {
    { "LUNA" },
    { "IO" },
    { "EUROPA" },
    { "GANYMEDES" },
    { "CALLISTO" },
    { "TITAN" },
    { "ENCELADUS" },
    { "TRITON" },
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

#endif
