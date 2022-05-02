// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#ifndef __RT_ACCEL_HPP__
#define __RT_ACCEL_HPP__

#include "rt_accel_conf_info.hpp"
#include "rt_accel_debug_info.hpp"

#include "esp_templates.hpp"

#include "rt_accel_directives.hpp"
// #include <random>

#define __round_mask(x, y) ((y)-1)
#define round_up(x, y) ((((x)-1) | __round_mask(x, y))+1)
/* <<--defines-->> */
#define DATA_WIDTH 32
#define DMA_SIZE SIZE_WORD
#define PLM_OUT_WORD 4800
#define PLM_IN_WORD 4800
#include <tuple>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cmath>
//#include <omp.h>

// Stratus fixed point

#include "fpdata.h"

//typedef cynw_fixed<FPDATA_WL, FPDATA_IL, SC_RND> FPDATA;
//util.h
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

//scene.h

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
