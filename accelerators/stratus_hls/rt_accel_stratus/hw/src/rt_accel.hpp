// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#ifndef __RT_ACCEL_HPP__
#define __RT_ACCEL_HPP__

#include "fpdata.hpp"
#include "rt_accel_conf_info.hpp"
#include "rt_accel_debug_info.hpp"

#include "esp_templates.hpp"

#include "rt_accel_directives.hpp"
//#include "scene.hpp"

#define __round_mask(x, y) ((y)-1)
#define round_up(x, y) ((((x)-1) | __round_mask(x, y))+1)
/* <<--defines-->> */
#define DATA_WIDTH 32
#define DMA_SIZE SIZE_WORD
#define PLM_OUT_WORD 4800
#define PLM_IN_WORD 4800

///////////
// Stratus fixed point

//#include <tuple>
// #include <algorithm>
#include <cmath>


const FPDATA f1 = 1;
const FPDATA f2 = 2;
const FPDATA f1_neg = -1;
    
// #include <omp.h>

struct vec3 { //12 bytes
    FPDATA x;
    FPDATA y;
    FPDATA z;

    vec3(){
        this->x = 0;
        this->y = 0;
        this->z = 0;
    }
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

    FPDATA norm() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    vec3 normalized() const {
        return (*this) * (1.f / norm());
    }
};

vec3 cross(const vec3 v1, const vec3 v2) {
    // vec3 temp = {v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x};
    return vec3(v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x);
}

struct Material { //36 bytes
    FPDATA refraction; //4 bytes
    FPDATA albedo[4]; // 16 bytes
    vec3 diffuse; //12 bytes
    FPDATA specular; //4bytes
    Material() {

        this->refraction = 1;
        this->albedo[0] = 2;
        for(int i = 1; i<4; i++)
            this->albedo[i] = 0;
        this->diffuse.x = 0;
        this->diffuse.y = 0;
        this->diffuse.z = 0;
        this->specular = 0;
    }
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
    Sphere(){
//nobody
    }
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
        Sphere(vec3(-3,   0,    -16), 2, ivory),
        Sphere(vec3(-1.0, -1.5, -12), 2, glass),
        Sphere(vec3(1.5,  -0.5, -18), 3, red_rubber),
        Sphere(vec3(7,    5,    -18), 4, mirror)
};

static const vec3 lights[] = {
        vec3(-20, 20, 20),
        vec3(30,  50, -25),
        vec3(30,  20, 30)
};

inline FPDATA min(FPDATA a, FPDATA b){
    if (a < b)
	return a;
    else
	return b;
}

inline FPDATA max(FPDATA a, FPDATA b){
    if (a > b)
	return a;
    else
	return b;
}


///////////
class rt_accel : public esp_accelerator_3P<DMA_WIDTH>
{
public:
    // Constructor
    SC_HAS_PROCESS(rt_accel);
    rt_accel(const sc_module_name& name)
    : esp_accelerator_3P<DMA_WIDTH>(name)
        , cfg("config")
    {
        // Signal binding
        cfg.bind_with(*this);

        // Map arrays to memories
        /* <<--plm-bind-->> */
        HLS_MAP_plm(plm_out_pong, PLM_OUT_NAME);
        HLS_MAP_plm(plm_out_ping, PLM_OUT_NAME);
        HLS_MAP_plm(plm_in_pong, PLM_IN_NAME);
        HLS_MAP_plm(plm_in_ping, PLM_IN_NAME);
    }

    // Processes

    // Load the input data
    void load_input();

    // Computation
    void compute_kernel();

    // Store the output data
    void store_output();

    // Configure rt_accel
    esp_config_proc cfg;

    // Functions

    // Private local memories
    sc_dt::sc_int<DATA_WIDTH> plm_in_ping[PLM_IN_WORD];
    sc_dt::sc_int<DATA_WIDTH> plm_in_pong[PLM_IN_WORD];
    sc_dt::sc_int<DATA_WIDTH> plm_out_ping[PLM_OUT_WORD];
    sc_dt::sc_int<DATA_WIDTH> plm_out_pong[PLM_OUT_WORD];

};


#endif /* __RT_ACCEL_HPP__ */
