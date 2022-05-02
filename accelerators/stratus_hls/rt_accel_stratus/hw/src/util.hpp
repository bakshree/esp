#ifndef UTIL_BAK
#define UTIL_BAK

// Stratus fixed point

#include "fpdata.h"
//#include <tuple>
// #include <algorithm>
#include <cmath>
// #include <omp.h>

struct vec3 { //12 bytes
    FPDATA x = 0;
    FPDATA y = 0;
    FPDATA z = 0;

    vec3() = default; 
    // {
    //     this->x = 0;
    //     this->y = 0;
    //     this->z = 0;
    // }
    vec3(FPDATA a, FPDATA b, FPDATA c){
        this->x = a;
        this->y = b;
        this->z = c;
    }
    
    FPDATA &operator[](const int i) {
        return i == 0 ? x : (1 == i ? y : z);
    }

    const FPDATA &operator[](const int i) const {
        return i == 0 ? x : (1 == i ? y : z);
    }

    vec3 operator*(const FPDATA v) const {
        //vec3 temp = {x * v, y * v, z * v};
        return vec3(x * v, y * v, z * v);
    }

    FPDATA operator*(const vec3 &v) const {
        return x * v.x + y * v.y + z * v.z;
    }

    vec3 operator+(const vec3 &v) const {
        //vec3 temp = {x + v.x, y + v.y, z + v.z};
        return vec3(x + v.x, y + v.y, z + v.z);
    }

    vec3 operator-(const vec3 &v) const {
        // vec3 temp = {x - v.x, y - v.y, z - v.z};
        return vec3(x - v.x, y - v.y, z - v.z);
    }

    vec3 operator-() const {
        // vec3 temp = {-x, -y, -z};
        return vec3(-x, -y, -z);
    }

    [[nodiscard]] FPDATA norm() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    [[nodiscard]] vec3 normalized() const {
        return (*this) * (1.f / norm());
    }
};

vec3 cross(const vec3 v1, const vec3 v2) {
    // vec3 temp = {v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x};
    return vec3(v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x);
}

struct Material { //36 bytes
    FPDATA refraction = 1; //4 bytes
    FPDATA albedo[4] = {2, 0, 0, 0}; // 16 bytes
    vec3 diffuse = {0, 0, 0}; //12 bytes
    FPDATA specular = 0; //4bytes
    Material() = default;
    Material(FPDATA f, FPDATA al[4], vec3 diff, FPDATA spec){
        this->refraction = f;
        for(int i = 0; i<4; i++)
            this->albedo[i] = al[i];
        this->diffuse = diff;
        this->specular = spec;
    }
};

struct Sphere { // 52 bytes
    vec3 center; //12
    FPDATA radius; //4
    Material material; //36
    Sphere() = default;
    Sphere(vec3 c, FPDATA r, Material m) {
        this->center = c;
        this->radius = r;
        this->material = m;
    }
};
FPDATA ivory_al [4] = {0.9, 0.5, 0.1, 0.0};
FPDATA glass_al [4] = {0.0, 0.9, 0.1, 0.8};
FPDATA rubber_al[4] = {1.4, 0.3, 0.0, 0.0};
FPDATA mirror_al[4] = {0.0, 16.0, 0.8, 0.0};
static const Material ivory(1.0, ivory_al, vec3(0.4, 0.4, 0.3), 50.);
static const Material glass(1.5, glass_al, vec3(0.6, 0.7, 0.8), 125.);
static const Material red_rubber(1.0, rubber_al, vec3(0.3, 0.1, 0.1), 10.);
static const Material mirror(1.0, mirror_al, vec3(1.0, 1.0, 1.0), 1425.);


struct scene_int{
    bool hit;
    vec3 shadow_pt;
    vec3 N;
    Material material;
    scene_int(bool h, vec3 sp, vec3 n, Material m){
        this->hit = h;
        this->shadow_pt = sp;
        this->N = n;
        this->material = m;
    }
};


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
