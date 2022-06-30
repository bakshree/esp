// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#ifndef __RT_ACCEL_HPP__
#define __RT_ACCEL_HPP__

//#include "fpdata.hpp"
#include "rt_accel_conf_info.hpp"
#include "rt_accel_debug_info.hpp"

#include "esp_templates.hpp"

#include "rt_accel_directives.hpp"
//#include "scene.hpp"
#define FPDATA int32_t

#define FP_IL 14
#define FP_FL 18

//typedef int32_t FPDATA;

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



inline FPDATA mul(FPDATA num, FPDATA denum){
    long long temp_x = num;
    temp_x *= denum;
    temp_x >>= FP_FL;
    FPDATA res = temp_x;
    return res;
}
inline FPDATA rt_div(FPDATA num, FPDATA denum){
    long long temp_x = num;
    temp_x <<= FP_FL;
    temp_x /= denum;
    FPDATA res = temp_x;
    return res;
}

inline FPDATA rt_sqrt_parts(FPDATA n){
    // Xₙ₊₁
    int32_t x = n;

    // cₙ
    int32_t c = 0;

    //#ifdef STRATUS_HLS

    // dₙ which starts at the highest power of four <= n
    int32_t d = 1 << 30; // The second-to-top bit is set.
                         // Same as ((unsigned) INT32_MAX + 1) / 2.
    while (d > n)
        d >>= 2;

    // for dₙ … d₀
    while (d != 0) {
        if (x >= c + d) {      // if Xₘ₊₁ ≥ Yₘ then aₘ = 2ᵐ
            x -= (c + d);        // Xₘ = Xₘ₊₁ - Yₘ
            c = (c >> 1) + d;  // cₘ₋₁ = cₘ/2 + dₘ (aₘ is 2ᵐ)
        }
        else {
            c >>= 1;           // cₘ₋₁ = cₘ/2      (aₘ is 0)
        }
        d >>= 2;               // dₘ₋₁ = dₘ/4
    }
    // #else
    //  float f = fixed32_to_float(x, FP_IL);
    //  float res = std::sqrt(f);
    //  c = float_to_fixed32(res, FP_IL);
    // #endif

    return c;                  // c₋₁
}



inline FPDATA rt_sqrt(FPDATA n){
    int32_t i_part = n>>FP_FL;
    int32_t q_part = n&( (1<<FP_FL)-1 );

    int32_t i_sq = rt_sqrt_parts(i_part);
    int32_t q_sq = rt_sqrt_parts(q_part);
    FPDATA c = (i_sq<<FP_FL) + (q_sq<<(FP_FL/2));
    // // #else
    //  float f = fixed32_to_float(n, FP_IL);
    //  float res = std::sqrt(f);
    //  FPDATA c2 = float_to_fixed32(res, FP_IL);
    // // #endif
    // std::cout<<"sqrt("<<f<<") = "<<c<<" vs "<<c2<<" -- "<<fixed32_to_float(c)<<" vs "<<res<<std::endl;
    return c;
}

inline int32_t whole_of_fp(FPDATA num){
    return num>>FP_FL;
}


inline FPDATA rt_pow(FPDATA num, FPDATA times){
    // #ifdef STRATUS_HLS
    int32_t tm = whole_of_fp(times);
    FPDATA res = 1<<FP_FL;
    for(int i = 0; i<tm; i++){
        res *= num;
    }
//     #else
// float b = fixed32_to_float(num, FP_IL);
// float p = fixed32_to_float(times, FP_IL);
//     float res_f = std::pow(b, p);
//     FPDATA res = float_to_fixed32(res_f, FP_IL);
//     #endif

    return res;
}

// inline FPDATA rt_sqrt(FPDATA n){
//     // Xₙ₊₁
//     int32_t x = n;

//     // cₙ
//     int32_t c = 0;

//     // dₙ which starts at the highest power of four <= n
//     int32_t d = 1 << 30; // The second-to-top bit is set.
//                          // Same as ((unsigned) INT32_MAX + 1) / 2.
//     while (d > n)
//         d >>= 2;

//     // for dₙ … d₀
//     while (d != 0) {
//         if (x >= c + d) {      // if Xₘ₊₁ ≥ Yₘ then aₘ = 2ᵐ
//             x -= c + d;        // Xₘ = Xₘ₊₁ - Yₘ
//             c = (c >> 1) + d;  // cₘ₋₁ = cₘ/2 + dₘ (aₘ is 2ᵐ)
//         }
//         else {
//             c >>= 1;           // cₘ₋₁ = cₘ/2      (aₘ is 0)
//         }
//         d >>= 2;               // dₘ₋₁ = dₘ/4
//     }
//     return c;                  // c₋₁
// }

// inline int32_t whole_of_fp(FPDATA num){
//     return num>>FP_FL;
// }


// inline FPDATA rt_pow(FPDATA num, FPDATA times){
//     int32_t tm = whole_of_fp(times);
//     FPDATA res = 1<<FP_FL;
//     for(int i = 0; i<tm; i++){
//         res *= num;
//     }
//     return res;
//}

const FPDATA f1 = 262144;
const FPDATA f2 = 524288;
const FPDATA f1_neg = -262144;
    
// #include <omp.h>


struct vec3 { //12 bytes
    FPDATA x ;//= 0;
    FPDATA y ;//= 0;
    FPDATA z ;//= 0;

    vec3()// = default; 
    {
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
        long long temp_x = x;
        long long temp_y = y;
        long long temp_z = z;
        temp_x *= v;
        temp_y *= v;
        temp_z *= v;
        temp_x >>= FP_FL;
        temp_y >>= FP_FL;
        temp_z >>= FP_FL;
        return vec3(temp_x, temp_y, temp_z);
    }

    FPDATA operator*(const vec3 &v) const {
        long long temp_x = x;
        long long temp_y = y;
        long long temp_z = z;
        temp_x *= v.x;
        temp_y *= v.y;
        temp_z *= v.z;
        temp_x >>= FP_FL;
        temp_y >>= FP_FL;
        temp_z >>= FP_FL;
        //return x * v.x + y * v.y + z * v.z;
        return temp_x + temp_y + temp_z;
    }

    vec3 operator/(const vec3 &v) const {
        long long temp_x = x;
        long long temp_y = y;
        long long temp_z = z;
        temp_x <<= FP_FL;
        temp_y <<= FP_FL;
        temp_z <<= FP_FL;
        temp_x /= v.x;
        temp_y /= v.y;
        temp_z /= v.z;
        //return x * v.x + y * v.y + z * v.z;
        return vec3(temp_x, temp_y, temp_z);
    }

    vec3 operator/(const FPDATA &v) const {
        long long temp_x = x;
        long long temp_y = y;
        long long temp_z = z;
        temp_x <<= FP_FL;
        temp_y <<= FP_FL;
        temp_z <<= FP_FL;
        temp_x /= v;
        temp_y /= v;
        temp_z /= v;
        //return x * v.x + y * v.y + z * v.z;
        return vec3(temp_x, temp_y, temp_z);
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

    vec3 operator=(const vec3 &v) const {
        // vec3 temp = {x - v.x, y - v.y, z - v.z};
        return vec3(v.x, v.y, v.z);
    }

    FPDATA norm() const {
        //return rt_sqrt(x * x + y * y + z * z);
        return rt_sqrt((*this)*(*this));
    }
    // [[nodiscard]] float norm(float x, float y, float z) const {
    //     return std::sqrt(x * x + y * y + z * z);
    // }

    vec3 normalized() const {
        //vec norm_res = this->norm();

        return (*this) / norm();
    }

    // [[nodiscard]] vec3 normalized(float x, float y, float z) const {
    //     float n = norm(x, y, z);
    //     //return (*this) * (1.f / norm());
    //     return vec3(float_to_fixed32(x/n), float_to_fixed32(y/n), float_to_fixed32(z/n));
    //     //return (*this) * (1.f / norm());
    // }
};
// class vec3 { //12 bytes
//     public:
//     FPDATA x;
//     FPDATA y;
//     FPDATA z;

//     vec3(){
//         this->x = 0;
//         this->y = 0;
//         this->z = 0;
//     }
//     vec3(FPDATA a, FPDATA b, FPDATA c){
//         this->x = a;
//         this->y = b;
//         this->z = c;
//     }
    
//     FPDATA &operator[](const int i) {
//         return i == 0 ? x : (1 == i ? y : z);
//     }

//     const FPDATA &operator[](const int i) const {
//         return i == 0 ? x : (1 == i ? y : z);
//     }

//     vec3 operator*(const FPDATA v) const {
//         //vec3 temp = {x * v, y * v, z * v};
//         return vec3(x * v, y * v, z * v);
//     }

//     FPDATA operator*(const vec3 &v) const {
//         return x * v.x + y * v.y + z * v.z;
//     }

//     vec3 operator+(const vec3 &v) const {
//         //vec3 temp = {x + v.x, y + v.y, z + v.z};
//         return vec3(x + v.x, y + v.y, z + v.z);
//     }

//     vec3 operator-(const vec3 &v) const {
//         // vec3 temp = {x - v.x, y - v.y, z - v.z};
//         return vec3(x - v.x, y - v.y, z - v.z);
//     }


//     vec3 operator=(const vec3 &v) const {
//         // vec3 temp = {x - v.x, y - v.y, z - v.z};
//         return vec3(v.x, v.y, v.z);
//     }

//     vec3 operator-() const {
//         // vec3 temp = {-x, -y, -z};
//         return vec3(-x, -y, -z);
//     }

//     FPDATA norm() const {
//         return rt_sqrt(x * x + y * y + z * z);
//     }

//     vec3 normalized() const {
//         return (*this) * (1.f / norm());
//     }
// };

// inline vec3 rt_cross(const vec3 v1, const vec3 v2) {
//     // vec3 temp = {v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x};
//     return vec3(v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x);
// }

class Material { //36 bytes
    public:
        FPDATA refraction; //4 bytes
        FPDATA albedo[4]; // 16 bytes
        vec3 diffuse; //12 bytes
        FPDATA specular; //4bytes
        Material() {

            this->refraction = f1;
            this->albedo[0] = f2;
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
        Material(const FPDATA f, const FPDATA al[4], vec3 diff, FPDATA spec){
            this->refraction = f;
            for(int i = 0; i<4; i++)
                this->albedo[i] = al[i];
            this->diffuse = diff;
            this->specular = spec;
        }
};

class Sphere { // 52 bytes
public:
    vec3 center; //12
    FPDATA radius; //4
    Material material; //36
    Sphere(){
//nobody
    }
    Sphere(vec3 c, FPDATA r, Material m) {
        center = c;
        radius = r;
        material = m;
    }
};
static const FPDATA ivory_al [4] = {235929, 131072, 26214, 0};
static const FPDATA glass_al [4] = {0, 235929, 26214, 209715};
static const FPDATA rubber_al[4] = {367001, 78643, 0, 0};
static const FPDATA mirror_al[4] = {0, 4194304, 209715, 0};
static const Material ivory(f1, ivory_al, vec3(104857, 104857, 78643), 13107200);
static const Material glass(393216, glass_al, vec3(157286, 183500, 209715), 32768000);
static const Material red_rubber(f1, rubber_al, vec3(78643, 26214, 26214), 2621440);
static const Material mirror(f1, mirror_al, vec3(f1, f1, f1), 373555200);


class scene_int{
    public:
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
        // HLS_MAP_plm(plm_out_pong, PLM_OUT_NAME);
        HLS_MAP_plm(plm_out_ping, PLM_OUT_NAME);
        // HLS_MAP_plm(plm_in_pong, PLM_IN_NAME);
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
    // sc_dt::sc_int<DATA_WIDTH> plm_in_pong[PLM_IN_WORD];
    sc_dt::sc_int<DATA_WIDTH> plm_out_ping[PLM_OUT_WORD];
    // sc_dt::sc_int<DATA_WIDTH> plm_out_pong[PLM_OUT_WORD];

};


#endif /* __RT_ACCEL_HPP__ */
