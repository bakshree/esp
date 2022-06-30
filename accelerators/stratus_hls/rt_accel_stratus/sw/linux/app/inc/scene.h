#ifndef CPU_SCENE_H
#define CPU_SCENE_H

// #include <vector>
// #include <random>

#include "util.h"

/*std::random_device rd;
std::mt19937 rng(rd());

std::uniform_int_distribution<std::mt19937::result_type> neg_dist(0, 10);
std::uniform_int_distribution<std::mt19937::result_type> xyz_dist(20, 50);

std::vector<Sphere> sph;
std::vector<vec3> lig;

bool is_neg() {


    return neg_dist(rng) < 5;
}

void init_lights() {
    for (int i = 0; i < 5; i++) {
        lig.push_back(is_neg() ? xyz_dist(rng()) : xyz_dist(rng()), );
    }
}

void init_spheres() {

}*/

// static const Sphere spheres[] = {
//         {{-3,   0,    -16}, 2, ivory},
//         {{-1.0, -1.5, -12}, 2, glass},
//         {{1.5,  -0.5, -18}, 3, red_rubber},
//         {{7,    5,    -18}, 4, mirror}
// };

// static const vec3 lights[] = {
//         {-20, 20, 20},
//         {30,  50, -25},
//         {30,  20, 30}
// };

static const Sphere spheres[] = {
        Sphere(vec3(-786432,   0,    -4194304), f2, ivory),
        Sphere(vec3(f1_neg, -393216, -3145728), f2, glass),
        Sphere(vec3(393216,  -131072, -4718592), 786432, red_rubber),
        Sphere(vec3(1835008,    1310720,    -4718592), 1048576, mirror)
};

static const vec3 lights[] = {
        vec3(-5242880, 5242880, 5242880),
        vec3(7864320,  13107200, -6553600),
        vec3(7864320,  5242880, 7864320)
};

#endif
