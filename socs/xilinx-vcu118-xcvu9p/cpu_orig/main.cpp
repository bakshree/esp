#include "util.h"
#include "scene.h"
#include <ctime>
#include <iostream>
#define ITERATIONS 1
using namespace std;
// using namespace mats;

constexpr int width = 1200;
constexpr int height = 800;
constexpr float fov = 1.0472; // Radians

vec3 reflect(const vec3 &I, const vec3 &N) {
    return I - N * 2.f * (I * N);
}

vec3 refract(const vec3 &I, const vec3 &N, const float eta_t, const float eta_i = 1.f) {
    float cosi = -std::max(-1.f, std::min(1.f, I * N));
    if (cosi < 0)
        return refract(I, -N, eta_i, eta_t);
    float eta = eta_i / eta_t;
    float k = 1 - eta * eta * (1 - cosi * cosi);
    return k < 0 ? vec3(1, 0, 0) : I * eta + N * (eta * cosi - std::sqrt(k));
}

std::tuple<bool, float> ray_sphere_intersect(const vec3 &orig, const vec3 &dir, const Sphere &s) {
    vec3 L = s.center - orig;
    float tca = L * dir;
    float d2 = L * L - tca * tca;
    if (d2 > s.radius * s.radius) return std::tuple<bool, float>{false, 0.0f};
    float thc = std::sqrt(s.radius * s.radius - d2);
    float t0 = tca - thc, t1 = tca + thc;
    if (t0 > .001) return std::tuple<bool, float>{true, t0};
    if (t1 > .001) return std::tuple<bool, float>{true, t1};
    return std::tuple<bool, float>{false, 0};
}

std::tuple<bool, vec3, vec3, Material> scene_intersect(const vec3 &orig, const vec3 &dir) {
    vec3 pt, N;
    Material material;

    float nearest_dist = 1e10;
    if (std::abs(dir.y) > .001) {
        float d = -(orig.y + 4) / dir.y;
        vec3 p = orig + dir * d;
        if (d > .001 && d < nearest_dist && std::abs(p.x) < 10 && p.z < -10 && p.z > -30) {
            nearest_dist = d;
            pt = p;
            N = {0, 1, 0};
            material.diffuse = (int(.5 * pt.x + 1000) + int(.5 * pt.z)) & 1 ? vec3(.3, .3, .3) : vec3(.3, .2, .1);
        }
    }

    for (const Sphere &s: spheres) { // intersect the ray with all spheres
        auto val = ray_sphere_intersect(orig, dir, s);
        auto intersection = std::get<0>(val);
        auto d = std::get<1>(val);
        if (!intersection || d > nearest_dist) continue;
        nearest_dist = d;
        pt = orig + dir * nearest_dist;
        N = (pt - s.center).normalized();
        material = s.material;
    }
    return std::tuple<bool, vec3, vec3, Material>{nearest_dist < 1000, pt, N, material};
}

vec3 cast_ray(const vec3 &orig, const vec3 &dir, const int depth = 0) {

    auto val = scene_intersect(orig, dir);
    auto hit = std::get<0>(val);
    auto point = std::get<1>(val);
    auto N = std::get<2>(val);
    auto material = std::get<3>(val);
    if (depth > 4 || !hit)
        return {0.2, 0.7, 0.8};

    vec3 reflect_dir = reflect(dir, N).normalized();
    vec3 refract_dir = refract(dir, N, material.refraction).normalized();
    vec3 reflect_color = cast_ray(point, reflect_dir, depth + 1);
    vec3 refract_color = cast_ray(point, refract_dir, depth + 1);

    float diffuse_light_intensity = 0, specular_light_intensity = 0;
    for (const vec3 &light: lights) { // checking if the point lies in the shadow of the light
        vec3 light_dir = (light - point).normalized();
        auto val = scene_intersect(point, light_dir);

        auto hit = std::get<0>(val);
        auto shadow_pt = std::get<1>(val);
        auto trashnrm = std::get<2>(val);
        auto trashmat = std::get<3>(val);

        if (hit && (shadow_pt - point).norm() < (light - point).norm()) continue;
        diffuse_light_intensity += std::max(0.f, light_dir * N);
        specular_light_intensity += std::pow(std::max(0.f, -reflect(-light_dir, N) * dir), material.specular);
    }
    return material.diffuse * diffuse_light_intensity * material.albedo[0] +
           vec3(1., 1., 1.) * specular_light_intensity * material.albedo[1] + reflect_color * material.albedo[2] +
           refract_color * material.albedo[3];
}

int main() {
    std::vector<std::vector<vec3> > frames;
    for(int i = 0; i<ITERATIONS; i++){
        std::vector<vec3> framebuffer(width * height);

        clock_t start_time = clock();
        
        #pragma omp parallel for
        for (int pix = 0; pix < width * height; pix++) {
            float dir_x = (pix % width + 0.5) - width / 2.;
            float dir_y = -(pix / width + 0.5) + height / 2.;
            float dir_z = -height / (2. * tan(fov / 2.));
            framebuffer[pix] = cast_ray(vec3(0, 0, 0), vec3(dir_x, dir_y, dir_z).normalized());
        }
        frames.push_back(framebuffer);
        clock_t end_time = clock();
        std::cout<<(end_time-start_time)<<" clocks "<<((float)(end_time-start_time))/CLOCKS_PER_SEC<<" seconds since there are "<<CLOCKS_PER_SEC<<" CLOCKS_PER_SEC";
    
    }
    std::ofstream ofs("./out.ppm", std::ios::binary);
    ofs << "P6\n" << width << " " << height << "\n255\n";
    for (vec3 &color: frames[0]) {
        float max = std::max(1.f, std::max(color[0], std::max(color[1], color[2])));
        for (int chan: {0, 1, 2})
            ofs << (char) (255 * color[chan] / max);
    }
    
    return 0;
}
