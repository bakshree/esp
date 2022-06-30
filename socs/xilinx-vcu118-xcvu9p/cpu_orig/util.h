#ifndef UTIL
#define UTIL

#include <tuple>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <omp.h>

struct vec3 { //12 bytes
    float x = 0;
    float y = 0;
    float z = 0;

    vec3() = default; 
    // {
    //     this->x = 0;
    //     this->y = 0;
    //     this->z = 0;
    // }
    vec3(float a, float b, float c){
        this->x = a;
        this->y = b;
        this->z = c;
    }
    
    float &operator[](const int i) {
        return i == 0 ? x : (1 == i ? y : z);
    }

    const float &operator[](const int i) const {
        return i == 0 ? x : (1 == i ? y : z);
    }

    vec3 operator*(const float v) const {
        //vec3 temp = {x * v, y * v, z * v};
        return vec3(x * v, y * v, z * v);
    }

    float operator*(const vec3 &v) const {
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

    [[nodiscard]] float norm() const {
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
    float refraction = 1; //4 bytes
    float albedo[4] = {2, 0, 0, 0}; // 16 bytes
    vec3 diffuse = {0, 0, 0}; //12 bytes
    float specular = 0; //4bytes
    Material() = default;
    Material(float f, float al[4], vec3 diff, float spec){
        this->refraction = f;
        for(int i = 0; i<4; i++)
            this->albedo[i] = al[i];
        this->diffuse = diff;
        this->specular = spec;
    }
};

struct Sphere { // 52 bytes
    vec3 center; //12
    float radius; //4
    Material material; //36
    Sphere() = default;
    Sphere(vec3 c, float r, Material m) {
        this->center = c;
        this->radius = r;
        this->material = m;
    }
};
float ivory_al [4] = {0.9, 0.5, 0.1, 0.0};
float glass_al [4] = {0.0, 0.9, 0.1, 0.8};
float rubber_al[4] = {1.4, 0.3, 0.0, 0.0};
float mirror_al[4] = {0.0, 16.0, 0.8, 0.0};
static const Material ivory(1.0, ivory_al, vec3(0.4, 0.4, 0.3), 50.);
static const Material glass(1.5, glass_al, vec3(0.6, 0.7, 0.8), 125.);
static const Material red_rubber(1.0, rubber_al, vec3(0.3, 0.1, 0.1), 10.);
static const Material mirror(1.0, mirror_al, vec3(1.0, 1.0, 1.0), 1425.);

// namespace mats{

// constexpr Material ivory = {1.0, {0.9, 0.5, 0.1, 0.0}, {0.4, 0.4, 0.3}, 50.};
// constexpr Material glass = {1.5, {0.0, 0.9, 0.1, 0.8}, {0.6, 0.7, 0.8}, 125.};
// constexpr Material red_rubber = {1.0, {1.4, 0.3, 0.0, 0.0}, {0.3, 0.1, 0.1}, 10.};
// constexpr Material mirror = {1.0, {0.0, 16.0, 0.8, 0.0}, {1.0, 1.0, 1.0}, 1425.};
// };
#endif
