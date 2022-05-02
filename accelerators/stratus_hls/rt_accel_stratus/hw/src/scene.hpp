#ifndef CPU_SCENE_H
#define CPU_SCENE_H

#include "util.h"


static const Sphere spheres[] = {
        {{-3,   0,    -16}, 2, ivory},
        {{-1.0, -1.5, -12}, 2, glass},
        {{1.5,  -0.5, -18}, 3, red_rubber},
        {{7,    5,    -18}, 4, mirror}
};

static const vec3 lights[] = {
        {-20, 20, 20},
        {30,  50, -25},
        {30,  20, 30}
};

#endif
