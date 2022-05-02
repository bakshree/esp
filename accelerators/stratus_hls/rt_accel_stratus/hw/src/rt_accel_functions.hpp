// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#include "rt_accel.hpp"

// Optional application-specific helper functions

vec3 reflect(const vec3 &I, const vec3 &N) {
    return I - N * f2 * (I * N);
}

vec3 refract(const vec3 &I, const vec3 &N, const FPDATA eta_t, const FPDATA eta_i = 1.f) {
    FPDATA cosi = -max(f1_neg, min(f1, I * N));
    if (cosi < 0)
        return refract(I, -N, eta_i, eta_t);
    FPDATA eta = eta_i / eta_t;
    FPDATA k = 1 - eta * eta * (1 - cosi * cosi);
    return k < 0 ? vec3(1, 0, 0) : I * eta + N * (eta * cosi - std::sqrt(k));
}

// std::tuple<bool, FPDATA> ray_sphere_intersect(const vec3 &orig, const vec3 &dir, const Sphere &s) {
    
//         vec3 L = s.center - orig;
//         FPDATA tca = L * dir;
//         FPDATA d2 = L * L - tca * tca;
//         if (d2 > s.radius * s.radius) return std::tuple<bool, FPDATA>{false, 0.0f};
//         FPDATA thc = std::sqrt(s.radius * s.radius - d2);
//         FPDATA t0 = tca - thc, t1 = tca + thc;
//         if (t0 > .001) return std::tuple<bool, FPDATA>{true, t0};
//         if (t1 > .001) return std::tuple<bool, FPDATA>{true, t1};
//         return std::tuple<bool, FPDATA>{false, 0};
// }

scene_int scene_intersect(const vec3 &orig, const vec3 &dir) {
    vec3 pt, N;
    Material material;

    FPDATA nearest_dist = 1e10;
    if (std::abs(dir.y) > .001) {
        FPDATA d = -(orig.y + 4) / dir.y;
        vec3 p = orig + dir * d;
        if (d > .001 && d < nearest_dist && std::abs(p.x) < 10 && p.z < -10 && p.z > -30) {
            nearest_dist = d;
            pt = p;
            N = vec3(0, 1, 0);
            material.diffuse = (int(.5 * pt.x + 1000) + int(.5 * pt.z)) & 1 ? vec3(.3, .3, .3) : vec3(.3, .2, .1);
        }
    }

    //for (const Sphere &s: spheres) { // intersect the ray with all spheres
    for (int sp_count = 0; sp_count<4; sp_count++) { // intersect the ray with all spheres
        // auto val = ray_sphere_intersect(orig, dir, s);
        // auto intersection = std::get<0>(val);
        // auto d = std::get<1>(val);
        //ray_sphere_intersect(orig, dir, s);
        const Sphere &s = spheres[sp_count];
        bool intersection;
        FPDATA d;
        vec3 L = s.center - orig;
        FPDATA tca = L * dir;
        FPDATA d2 = L * L - tca * tca;
        if (d2 > s.radius * s.radius){
            //return std::tuple<bool, FPDATA>{false, 0.0f};
            intersection = false;
            d = 0.0f;
        } 
        else{
            FPDATA thc = std::sqrt(s.radius * s.radius - d2);
            FPDATA t0 = tca - thc, t1 = tca + thc;
            if (t0 > .001) {
                //return std::tuple<bool, FPDATA>{true, t0};
                intersection = true;
                d = t0;
            }
            else if (t1 > .001) {
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
        N = (pt - s.center).normalized();
        material = s.material;
    }
    bool is_near = nearest_dist < 1000.;
    return scene_int(is_near, pt, N, material);
}

vec3 cast_ray(const vec3 &orig, const vec3 &dir, const int depth = 0) {

    scene_int val = scene_intersect(orig, dir);
    // auto hit = std::get<0>(val);
    // auto point = std::get<1>(val);
    // auto N = std::get<2>(val);
    // auto material = std::get<3>(val);
    if (depth > 2 || !val.hit)
        return vec3(0.2, 0.7, 0.8);

    vec3 reflect_dir = reflect(dir, val.N).normalized();
    vec3 refract_dir = refract(dir, val.N, val.material.refraction).normalized();
    vec3 reflect_color = cast_ray(val.shadow_pt, reflect_dir, depth + 1);
    vec3 refract_color = cast_ray(val.shadow_pt, refract_dir, depth + 1);

    FPDATA diffuse_light_intensity = 0, specular_light_intensity = 0;
    //for (const vec3 &light: lights) { // checking if the point lies in the shadow of the light
    for (int l_cnt = 0; l_cnt <3; l_cnt++) { // checking if the point lies in the shadow of the light
        const vec3 &light = lights[l_cnt];
        vec3 light_dir = (light - val.shadow_pt).normalized();
        scene_int val2 = scene_intersect(val.shadow_pt, light_dir);

        // auto hit = std::get<0>(val);
        // auto shadow_pt = std::get<1>(val);
        // auto trashnrm = std::get<2>(val);
        // auto trashmat = std::get<3>(val);

        if (val2.hit && (val2.shadow_pt - val.shadow_pt).norm() < (light - val.shadow_pt).norm()) continue;
        diffuse_light_intensity += max(0, light_dir * val.N);
        specular_light_intensity += std::pow(max(0, -reflect(-light_dir, val.N) * dir), val.material.specular);
    }
    return val.material.diffuse * diffuse_light_intensity * val.material.albedo[0] +
           vec3(1., 1., 1.) * specular_light_intensity * val.material.albedo[1] + reflect_color * val.material.albedo[2] +
           refract_color * val.material.albedo[3];
}


