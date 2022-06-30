// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0
#ifndef __RT_ACCEL_FUNCTIONS_HPP__
#define __RT_ACCEL_FUNCTIONS_HPP__
#include "rt_accel.hpp"

// Optional application-specific helper functions


inline vec3 reflect(const vec3 &I, const vec3 &N) {
    return I - N * f2 * (I * N);
}

inline vec3 refract(const vec3 &I, const vec3 &N, const FPDATA eta_t, const FPDATA eta_i = f1) {
    // FPDATA cosi = -1*max(f1_neg, min(f1, I * N));
    // if (cosi < 0){
    //     // return refract(I, -N, eta_i, eta_t);
    //     N = (-N);
    //     cosi = -1*max(f1_neg, min(f1, I * N));
    // }
    // FPDATA eta = eta_i / eta_t;
    // FPDATA k = f1 - eta * eta * (1f - cosi * cosi);
    // return k < 0 ? vec3(f1, 0, 0) : I * eta + N * (eta * cosi - rt_sqrt(k));

    FPDATA cosi = -1*max(f1_neg, min(f1, I * N));
    if (cosi < 0){
        // return refract(I, -N, eta_i, eta_t);
        N = (-N);
        cosi = -1*max(f1_neg, min(f1, I * N));
    }
    // FPDATA eta = eta_i / eta_t;
    // FPDATA k = 1 - eta * eta * (1 - cosi * cosi);
    // return k < 0 ? vec3(1, 0, 0) : I * eta + N * (eta * cosi - std::sqrt(k));
    FPDATA eta = rt_div(eta_i , eta_t);
    FPDATA k = f1 - mul(eta , mul(eta, (f1 - mul(cosi,cosi))));
    return k < 0 ? vec3(f1, 0, 0) : I * eta + N * (mul(eta , cosi) - rt_sqrt(k));
}

// std::tuple<bool, FPDATA> ray_sphere_intersect(const vec3 &orig, const vec3 &dir, const Sphere &s) {
    
//         vec3 L = s.center - orig;
//         FPDATA tca = L * dir;
//         FPDATA d2 = L * L - tca * tca;
//         if (d2 > s.radius * s.radius) return std::tuple<bool, FPDATA>{false, 0.0f};
//         FPDATA thc = std::sqrt(s.radius * s.radius - d2);
//         FPDATA t0 = tca - thc, t1 = tca + thc;
//         if (t0 > 262) return std::tuple<bool, FPDATA>{true, t0};
//         if (t1 > 262) return std::tuple<bool, FPDATA>{true, t1};
//         return std::tuple<bool, FPDATA>{false, 0};
// }

inline scene_int scene_intersect(const vec3 &orig, const vec3 &dir) {
    vec3 pt, N;
    Material material;

    FPDATA nearest_dist = INT32_MAX;
    // if (std::abs(dir.y) > 262) {
    //     FPDATA d = -(orig.y + 1048576) / dir.y;
    //     vec3 p = orig + dir * d;
    //     if (d > 262 && d < nearest_dist && std::abs(p.x) < 2621440 && p.z < -2621440 && p.z > -7864320) {
    //         nearest_dist = d;
    //         pt = p;
    //         N = vec3(0, 262144, 0);
    //         material.diffuse = (int(131072 * pt.x + 262144000) + int(131072 * pt.z)) & 1 ? vec3(78643, 78643, 78643) : vec3(78643, 52428, 26214);
    //     }
    // }

    // // for (const Sphere &s: spheres) { // intersect the ray with all spheres
    // for (int sp_count = 0; sp_count<4; sp_count++) { // intersect the ray with all spheres
    //     // auto val = ray_sphere_intersect(orig, dir, s);
    //     // auto intersection = std::get<0>(val);
    //     // auto d = std::get<1>(val);
    //     //ray_sphere_intersect(orig, dir, s);
    //     const Sphere &s = spheres[sp_count];
    //     bool intersection = false;
    //     FPDATA d = 0;
    //     vec3 L = s.center - orig;
    //     FPDATA tca = L * dir;
    //     FPDATA d2 = L * L - tca * tca;
    //     // if (d2 > s.radius * s.radius){
    //     //     //return std::tuple<bool, FPDATA>{false, 0.0f};
    //     //     intersection = false;
    //     //     d = 0;
    //     // } 
    //     // else{
    //     FPDATA sp_sq = s.radius * s.radius;
    //     if (d2 <= sp_sq){
    //         FPDATA thc = rt_sqrt(sp_sq - d2);
    //         FPDATA t0 = tca - thc, t1 = tca + thc;
    //         if (t0 > 262) {
    //             //return std::tuple<bool, FPDATA>{true, t0};
    //             intersection = true;
    //             d = t0;
    //         }
    //         else if (t1 > 262) {
    //             // return std::tuple<bool, FPDATA>{true, t1};
    //             intersection = true;
    //             d = t1;
    //         }
    //         //return std::tuple<bool, FPDATA>{false, 0};
    //     }
    //     //ray_sphere_intersect ends

    //     if (!intersection || d > nearest_dist) continue;
    //     nearest_dist = d;
    //     pt = orig + dir * nearest_dist;
    //     N = (pt - s.center).normalized();
    //     material = s.material;
    // }

    if (std::abs(dir.y) > 262) {
        FPDATA d = -1*rt_div((orig.y + 1048576) , dir.y);
        vec3 p = orig + dir * d;
        if (d > 262 && d < nearest_dist && std::abs(p.x) < 2621440 && p.z < -2621440 && p.z > -7864320) {
            nearest_dist = d;
            pt = p;
            N = vec3(0, 262144, 0);

            material.diffuse = (((mul(131072 , pt.x) + 262144000)>>FP_FL) + (mul(131072, pt.z)>>FP_FL)) & 1 ? vec3(78643, 78643, 78643) : vec3(78643, 52428, 26214);
            //material.diffuse = (int(131072 * pt.x + 262144000) + int(131072 * pt.z)) & 1 ? vec3(78643, 78643, 78643) : vec3(78643, 52428, 26214);
        }
    }

    // for (const Sphere &s: spheres) { // intersect the ray with all spheres
    for (int sp_count = 0; sp_count<4; sp_count++) { // intersect the ray with all spheres
        const Sphere &s = spheres[sp_count];
        bool intersection = false;
        FPDATA d = 0;

        vec3 L = s.center - orig;
        FPDATA tca = L * dir;
        FPDATA d2 = L * L - mul(tca, tca);
        FPDATA sp_sq = mul(s.radius , s.radius);
        if (d2 <= sp_sq){
            FPDATA thc = rt_sqrt(sp_sq - d2);
            FPDATA t0 = tca - thc, t1 = tca + thc;
            if (t0 > 262) {
                //return std::tuple<bool, FPDATA>{true, t0};
                intersection = true;
                d = t0;
            }
            else if (t1 > 262) {
                // return std::tuple<bool, FPDATA>{true, t1};
                intersection = true;
                d = t1;
            }
            //return std::tuple<bool, FPDATA>{false, 0};
        }
        //ray_sphere_intersect ends

        if (!intersection || d > nearest_dist) continue;
        nearest_dist = d;
        pt = orig + dir * nearest_dist;
        // N = (pt - s.center).normalized();
        N = (pt - s.center).normalized();
        material = s.material;
    }

    bool is_near = nearest_dist < 262144000;
    return scene_int(is_near, pt, N, material);
}
// vec3 cast_ray2(const vec3 &orig, const vec3 &dir, const int depth = 2) {

//     scene_int val = scene_intersect(orig, dir);
//     // auto hit = std::get<0>(val);
//     // auto point = std::get<1>(val);
//     // auto N = std::get<2>(val);
//     // auto material = std::get<3>(val);

//     // if (depth > 2 || !val.hit)
//     //     return vec3(052428, 0.7, 0.8);

//     vec3 reflect_dir = reflect(dir, val.N).normalized();
//     vec3 refract_dir = refract(dir, val.N, val.material.refraction).normalized();
//     vec3 reflect_color = vec3(52428, 183500, 209715);
//     vec3 refract_color = vec3(52428, 183500, 209715);

//     FPDATA diffuse_light_intensity = 0, specular_light_intensity = 0;
//     //for (const vec3 &light: lights) { // checking if the point lies in the shadow of the light
//     for (int l_cnt = 0; l_cnt <3; l_cnt++) { // checking if the point lies in the shadow of the light
//         const vec3 &light = lights[l_cnt];
//         vec3 light_dir = (light - val.shadow_pt).normalized();
//         scene_int val2 = scene_intersect(val.shadow_pt, light_dir);

//         // auto hit = std::get<0>(val);
//         // auto shadow_pt = std::get<1>(val);
//         // auto trashnrm = std::get<2>(val);
//         // auto trashmat = std::get<3>(val);

//         if (val2.hit && (val2.shadow_pt - val.shadow_pt).norm() < (light - val.shadow_pt).norm()) continue;
//         diffuse_light_intensity += max(0, light_dir * val.N);
//         specular_light_intensity += pow(max(0, -reflect(-light_dir, val.N) * dir), val.material.specular);
//     }
//     return val.material.diffuse * diffuse_light_intensity * val.material.albedo[0] +
//            vec3(f1, f1, f1) * specular_light_intensity * val.material.albedo[1] + reflect_color * val.material.albedo[2] +
//            refract_color * val.material.albedo[3];
// }

// vec3 cast_ray1(const vec3 &orig, const vec3 &dir, const int depth = 1) {

//     scene_int val = scene_intersect(orig, dir);
//     // auto hit = std::get<0>(val);
//     // auto point = std::get<1>(val);
//     // auto N = std::get<2>(val);
//     // auto material = std::get<3>(val);

//     // if (depth > 2 || !val.hit)
//     //     return vec3(052428, 0.7, 0.8);

//     vec3 reflect_dir = reflect(dir, val.N).normalized();
//     vec3 refract_dir = refract(dir, val.N, val.material.refraction).normalized();
//     vec3 reflect_color = cast_ray2(val.shadow_pt, reflect_dir);
//     vec3 refract_color = cast_ray2(val.shadow_pt, refract_dir);

//     FPDATA diffuse_light_intensity = 0, specular_light_intensity = 0;
//     //for (const vec3 &light: lights) { // checking if the point lies in the shadow of the light
//     for (int l_cnt = 0; l_cnt <3; l_cnt++) { // checking if the point lies in the shadow of the light
//         const vec3 &light = lights[l_cnt];
//         vec3 light_dir = (light - val.shadow_pt).normalized();
//         scene_int val2 = scene_intersect(val.shadow_pt, light_dir);

//         // auto hit = std::get<0>(val);
//         // auto shadow_pt = std::get<1>(val);
//         // auto trashnrm = std::get<2>(val);
//         // auto trashmat = std::get<3>(val);

//         if (val2.hit && (val2.shadow_pt - val.shadow_pt).norm() < (light - val.shadow_pt).norm()) continue;
//         diffuse_light_intensity += max(0, light_dir * val.N);
//         specular_light_intensity += pow(max(0, -reflect(-light_dir, val.N) * dir), val.material.specular);
//     }
//     return val.material.diffuse * diffuse_light_intensity * val.material.albedo[0] +
//            vec3(f1, f1, f1) * specular_light_intensity * val.material.albedo[1] + reflect_color * val.material.albedo[2] +
//            refract_color * val.material.albedo[3];
// }

vec3 cast_ray(const vec3 &orig, const vec3 &dir, const int depth = 0) {

    scene_int val = scene_intersect(orig, dir);
    ///////////
    ///////////

    // vec3 reflect_dir = reflect(dir, val.N).normalized();
    // vec3 refract_dir = refract(dir, val.N, val.material.refraction).normalized();
    // if (depth > 2 || !val.hit)
    //     return vec3(052428, 0.7, 0.8);
    if(!val.hit) return vec3(52428, 183500, 209715);
    vec3 reflect_color = vec3(52428, 183500, 209715); //cast_ray1(val.shadow_pt, reflect_dir);
    vec3 refract_color = vec3(52428, 183500, 209715); //cast_ray1(val.shadow_pt, refract_dir);

    FPDATA diffuse_light_intensity = 0, specular_light_intensity = 0; //PREVIOUSLY 0
    for (int l_cnt = 0; l_cnt <3; l_cnt++) { // checking if the point lies in the shadow of the light
        const vec3 &light = lights[l_cnt];
        vec3 light_dir = (light - val.shadow_pt).normalized();
        scene_int val2 = scene_intersect(val.shadow_pt, light_dir);
        if (val2.hit && (val2.shadow_pt - val.shadow_pt).norm() < (light - val.shadow_pt).norm()) continue;
        diffuse_light_intensity += max(0, light_dir * val.N);
        specular_light_intensity += rt_pow(max(0, -reflect(-light_dir, val.N) * dir), val.material.specular);
    }
   return val.material.diffuse * mul(diffuse_light_intensity , val.material.albedo[0]) +
           vec3(f1, f1, f1) * mul(specular_light_intensity , val.material.albedo[1]) + reflect_color * val.material.albedo[2] +
           refract_color * val.material.albedo[3];
}




#endif
