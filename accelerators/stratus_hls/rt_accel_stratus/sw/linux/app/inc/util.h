#ifndef UTIL
#define UTIL

//#include <tuple>
// #include <algorithm>
#include <iostream>
#include <cmath>
// #include <omp.h>
#include "fixed_point.h"
//Bakshree
#define FPDATA int32_t

#define rt_plus(x,y) ((x)+(y))

typedef int int32_t;
//typedef long long int64_t;

static const FPDATA f1 = float_to_fixed32(1.f, FP_IL); //262144;
static const FPDATA f2 = float_to_fixed32(2.f, FP_IL); //524288;
static const FPDATA f1_neg = float_to_fixed32(-1.f, FP_IL); //-262144;
    




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

inline FPDATA cap(long long a){
    return (a>INT32_MAX)? INT32_MAX: a;
}

inline FPDATA mul(FPDATA num, FPDATA denum){
    long long temp_x = num;
    temp_x *= denum;
    temp_x >>= FP_FL;
    FPDATA res = cap(temp_x);
    return res;
}
inline FPDATA rt_div(FPDATA num, FPDATA denum){
    // std::cout<<"Inside rt_div num:"<<num<<" denum:"<<denum<<std::endl;
    long long temp_x = num;
    temp_x <<= FP_FL;
    temp_x /= denum;
    FPDATA res = temp_x;
    // std::cout<<"res:"<<res<<std::endl;
    return res;
}

inline FPDATA rt_sqrt_newton(FPDATA n){
    if(n==0) return 0;
    if(n==f1) return f1;
    FPDATA x = n;
 
    // The closed guess will be stored in the root
    FPDATA root;
    //l: 
    // To count the number of iterations
    int count = 0;
 
    while (1) {
        count++;
 
        // std::cout<<"Inside rt_newton loop:"<<count<<" root:"<<root<<std::endl;
        // std::cout<<"Inside rt_newton n:"<<n<<" x:"<<x<<std::endl;
        // Calculate more closed x
        root =  (x + rt_div(n , x));
        root >>=1;
        // std::cout<<"abs(root - x):"<<std::abs(root - x)<<std::endl;
        // Check for closeness
        if (std::abs(root - x) < 26 || count == 5)
            break;
 
        // std::cout<<"Inside rt_newton loop ####:"<<count<<std::endl;
        // Update root
        x = root;
    }
 
    return root;
}
inline FPDATA rt_sqrt_parts(FPDATA n){
    // Xₙ₊₁
    int32_t x = n;

    // cₙ
    int32_t c = 0;

    // #if 0

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
    return c;                  // c₋₁
}


inline FPDATA rt_sqrt(FPDATA n){
    int32_t i_part = n>>FP_FL;
    int32_t q_part = n&( (1<<FP_FL)-1 );

    int32_t i_sq = rt_sqrt_parts(i_part);
    int32_t q_sq = rt_sqrt_parts(q_part);
    FPDATA c = (i_sq<<FP_FL) + (q_sq<<(FP_FL/2));
    // #else
     float f = fixed32_to_float(n, FP_IL);
     float res = std::sqrt(f);
     FPDATA c2 = float_to_fixed32(res, FP_IL);
    // #endif
    return c2;
}


inline int32_t whole_of_fp(FPDATA num){
    return num>>FP_FL;
}


inline FPDATA rt_pow(FPDATA num, FPDATA times){
    #if 0
    int32_t tm = whole_of_fp(times);
    FPDATA res = 1<<FP_FL;
    for(int i = 0; i<tm; i++){
        res *= num;
    }
    #else
float b = fixed32_to_float(num, FP_IL);
float p = fixed32_to_float(times, FP_IL);
    float res_f = std::pow(b, p);
    FPDATA res = float_to_fixed32(res_f, FP_IL);
    #endif

    return res;
}



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
        // long long temp_x = x;
        // long long temp_y = y;
        // long long temp_z = z;
        // temp_x *= v;
        // temp_y *= v;
        // temp_z *= v;
        // temp_x >>= FP_FL;
        // temp_y >>= FP_FL;
        // temp_z >>= FP_FL;
        // return vec3(temp_x, temp_y, temp_z);
        return vec3(mul(x, v), mul(y, v), mul(z,v));
    }

    FPDATA operator*(const vec3 &v) const {
        // long long temp_x = x;
        // long long temp_y = y;
        // long long temp_z = z;
        // temp_x *= v.x;
        // temp_y *= v.y;
        // temp_z *= v.z;
        // temp_x >>= FP_FL;
        // temp_y >>= FP_FL;
        // temp_z >>= FP_FL;
        // //return x * v.x + y * v.y + z * v.z;
        // return temp_x + temp_y + temp_z;
        return (mul(x, v.x)+mul(y, v.y)+mul(z,v.z));
    }

    vec3 operator/(const vec3 &v) const {
        // long long temp_x = x;
        // long long temp_y = y;
        // long long temp_z = z;
        // temp_x <<= FP_FL;
        // temp_y <<= FP_FL;
        // temp_z <<= FP_FL;
        // temp_x /= v.x;
        // temp_y /= v.y;
        // temp_z /= v.z;
        // //return x * v.x + y * v.y + z * v.z;
        // return vec3(temp_x, temp_y, temp_z);
        return vec3(rt_div(x, v.x), rt_div(y, v.y), rt_div(z,v.z));
    }

    vec3 operator/(const FPDATA &v) const {
        // long long temp_x = x;
        // long long temp_y = y;
        // long long temp_z = z;
        // temp_x <<= FP_FL;
        // temp_y <<= FP_FL;
        // temp_z <<= FP_FL;
        // temp_x /= v;
        // temp_y /= v;
        // temp_z /= v;
        // return vec3(temp_x, temp_y, temp_z);
        // //return x / v.x + y / v.y + z / v.z;
        return vec3(rt_div(x, v), rt_div(y, v), rt_div(z,v));
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
        //return rt_sqrt(x * x + y * y + z * z);
        return rt_sqrt((*this)*(*this));
    }
    [[nodiscard]] float norm(float x, float y, float z) const {
        return std::sqrt(x * x + y * y + z * z);
    }

    [[nodiscard]] vec3 normalized() const {
        //vec norm_res = this->norm();

        return (*this) / norm();
    }

    [[nodiscard]] vec3 normalized(float x, float y, float z) const {
        float n = norm(x, y, z);
        //return (*this) * (1.f / norm());
        return vec3(float_to_fixed32(x/n), float_to_fixed32(y/n), float_to_fixed32(z/n));
        //return (*this) * (1.f / norm());
    }
};

vec3 cross(const vec3 v1, const vec3 v2) {
    // vec3 temp = {v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x};
    return vec3(mul(v1.y , v2.z ) - mul(v1.z, v2.y) , mul(v1.z, v2.x) - mul(v1.x, v2.z), mul(v1.x, v2.y) - mul(v1.y , v2.x));
    //     long long temp_x1 = v1.y; //v1.y * v2.z
    //     temp_x1 *= v2.z;
    //     temp_x1 >>= FP_FL;
    //     long long temp_y1 = v1.z; //v1.z * v2.x
    //     temp_y1 *= v2.x;
    //     temp_y1 >>= FP_FL;
    //     long long temp_z1 = v1.x; //v1.x * v2.y
    //     temp_z1 *= v2.y;
    //     temp_z1 >>= FP_FL;

    //     long long temp_x2 = v1.z; //v1.z * v2.y
    //     temp_x2 *= v2.y;
    //     temp_x2 >>= FP_FL;
    //     long long temp_y2 = v1.x; //v1.x * v2.z
    //     temp_y2 *= v2.z;
    //     temp_y2 >>= FP_FL;
    //     long long temp_z2 = v1.y; //v1.y * v2.x
    //     temp_z2 *= v2.x;
    //     temp_z2 >>= FP_FL;

    //     temp_x1 -= temp_x2;
    //     temp_y1 -= temp_y2;
    //     temp_z1 -= temp_z2;

    // //return vec3(v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x);
    //     return vec3(temp_x1, temp_y1, temp_z1);
}

struct Material { //36 bytes
    FPDATA refraction = 1; //4 bytes
    FPDATA albedo[4] = {f2, 0, 0, 0}; // 16 bytes
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

    Material(const FPDATA f, const FPDATA al[4], vec3 diff, FPDATA spec){
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
// FPDATA ivory_al [4] = {0.9, 0.5, 0.1, 0.0};
// FPDATA glass_al [4] = {0.0, 0.9, 0.1, 0.8};
// FPDATA rubber_al[4] = {1.4, 0.3, 0.0, 0.0};
// FPDATA mirror_al[4] = {0.0, 16.0, 0.8, 0.0};
// static const Material ivory(1.0, ivory_al, vec3(0.4, 0.4, 0.3), 50.);
// static const Material glass(1.5, glass_al, vec3(0.6, 0.7, 0.8), 125.);
// static const Material red_rubber(1.0, rubber_al, vec3(0.3, 0.1, 0.1), 10.);
// static const Material mirror(1.0, mirror_al, vec3(1.0, 1.0, 1.0), 1425.);

static const FPDATA ivory_al [4] = {235929, 131072, 26214, 0};
static const FPDATA glass_al [4] = {0, 235929, 26214, 209715};
static const FPDATA rubber_al[4] = {367001, 78643, 0, 0};
static const FPDATA mirror_al[4] = {0, 4194304, 209715, 0};
static const Material ivory(f1, ivory_al, vec3(104857, 104857, 78643), 13107200);
static const Material glass(393216, glass_al, vec3(157286, 183500, 209715), 32768000);
static const Material red_rubber(f1, rubber_al, vec3(78643, 26214, 26214), 2621440);
static const Material mirror(f1, mirror_al, vec3(f1, f1, f1), 373555200);

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

// namespace mats{

// constexpr Material ivory = {1.0, {0.9, 0.5, 0.1, 0.0}, {0.4, 0.4, 0.3}, 50.};
// constexpr Material glass = {1.5, {0.0, 0.9, 0.1, 0.8}, {0.6, 0.7, 0.8}, 125.};
// constexpr Material red_rubber = {1.0, {1.4, 0.3, 0.0, 0.0}, {0.3, 0.1, 0.1}, 10.};
// constexpr Material mirror = {1.0, {0.0, 16.0, 0.8, 0.0}, {1.0, 1.0, 1.0}, 1425.};
// };
#endif
