#include "util.h"
#include "scene.h"
#include <ctime>
#include <fstream>
#include <vector>

#include <esp.h>
#include <esp_accelerator.h>

#include "libesp.h"
#include "cfg.h"
static unsigned in_words_adj;
static unsigned out_words_adj;
static unsigned in_len;
static unsigned out_len;
static unsigned in_size;
static unsigned out_size;
static unsigned out_offset;
static unsigned size;

#define ITERATIONS 1
using namespace std;
// using namespace mats;
int pix_ctr = 0;
// constexpr int width = 40;
// constexpr int height = 40;
constexpr int width = 1200;
constexpr int height = 800;
constexpr float fov = 1.0472; // Radians



/* User-defined code */
static void init_parameters()
{
	if (DMA_WORD_PER_BEAT(sizeof(token_t)) == 0) {
		in_words_adj = 3*img_width*img_height;
		out_words_adj = 3*img_width*img_height;
	} else {
		in_words_adj = round_up(3*img_width*img_height, DMA_WORD_PER_BEAT(sizeof(token_t)));
		out_words_adj = round_up(3*img_width*img_height, DMA_WORD_PER_BEAT(sizeof(token_t)));
	}
	in_len = in_words_adj * (1);
	out_len =  out_words_adj * (1);
	in_size = in_len * sizeof(token_t);
	out_size = out_len * sizeof(token_t);
	out_offset = in_len;
	size = (out_offset * sizeof(token_t)) + out_size;
}

float norm(float x, float y, float z)  {
    return std::sqrt(x * x + y * y + z * z);
}

vec3 normalized(float x, float y, float z)  {
    float n = norm(x, y, z);
    //return (*this) * (1.f / norm());
    return vec3(float_to_fixed32(x/n, 14), float_to_fixed32(y/n, 14), float_to_fixed32(z/n, 14));
    //return (*this) * (1.f / norm());
}

/* User-defined code */
static int validate_buffer(token_t *out, token_t *gold){
    std::ofstream ofs("./out_fp.ppm", std::ios::binary);
    ofs << "P6\n" << width << " " << height << "\n255\n";
    for (int j = 0; j < img_width*img_height; j++){
        FPDATA color[3];
        color[0] = out[3*j]    ;
        color[1] = out[3*j + 1];
        color[2] = out[3*j + 2];
        float color_fp[3];
        for (int chan: {0, 1, 2})
            color_fp[chan] = fixed32_to_float(color[chan], FP_IL);
        
        FPDATA max =  std::max(1.f,  std::max(color_fp[0],  std::max(color_fp[1], color_fp[2])));
        for (int chan: {0, 1, 2}){
    #ifdef VERBOSE
            std::cout<<"channel "<<chan<<" : "<<color_fp[chan]<<", max = "<<max<<endl;
    #endif
            ofs << (char) (255 * color_fp[chan] / max);
        }
    #ifdef VERBOSE
        std::cout<<"-----------\n";
    #endif
    }
}

vec3 reflect(const vec3 &I, const vec3 &N) {
    // return I - N * 2.f * (I * N);
    return I - N * mul(f2, (I * N));
}

vec3 refract(const vec3 &I, const vec3 &N, const FPDATA eta_t, const FPDATA eta_i = 1.f) {
    // FPDATA cosi = -max(-1.f, min(1.f, I * N));
    FPDATA cosi = -1*max(f1_neg, min(f1, I * N));
    if (cosi < 0)
        return refract(I, -N, eta_i, eta_t);
    // FPDATA eta = eta_i / eta_t;
    // FPDATA k = 1 - eta * eta * (1 - cosi * cosi);
    // return k < 0 ? vec3(1, 0, 0) : I * eta + N * (eta * cosi - std::sqrt(k));
    FPDATA eta = rt_div(eta_i , eta_t);
    FPDATA k = f1 - mul(eta , mul(eta, (f1 - mul(cosi,cosi))));
    return k < 0 ? vec3(f1, 0, 0) : I * eta + N * (mul(eta , cosi) - rt_sqrt(k));
}

scene_int scene_intersect_ref(const vec3 &orig, const vec3 &dir) {
    vec3 pt, N;
    Material material;

    FPDATA nearest_dist = INT32_MAX;
    if (std::abs(dir.y) > .001) {
        FPDATA d = -1*rt_div((orig.y + 4) , dir.y);
        vec3 p = orig + dir * d;
        if (d > .001 && d < nearest_dist && std::abs(p.x) < 10 && p.z < -10 && p.z > -30) {
            nearest_dist = d;
            pt = p;
            //N = {0, 1, 0};
            N = vec3(0, 262144, 0);
            //material.diffuse = (int(.5 * pt.x + 1000) + int(.5 * pt.z)) & 1 ? vec3(.3, .3, .3) : vec3(.3, .2, .1);
            material.diffuse = (((mul(131072 , pt.x) + 262144000)>>FP_FL) + ((mul(131072, pt.z))>>FP_FL)) & 1 ? vec3(78643, 78643, 78643) : vec3(78643, 52428, 26214);
        
        }
    }

    for (const Sphere &s: spheres) { // intersect the ray with all spheres
        bool intersection;
        FPDATA d;
        vec3 L = s.center - orig;
        FPDATA tca = L * dir;
        FPDATA d2 = L * L - mul(tca , tca);
        FPDATA sp_sq = mul(s.radius , s.radius);
        if (d2 > sp_sq){
            //return std::tuple<bool, float>{false, 0.0f};
            intersection = false;
            d = 0;
        } 
        else{
            FPDATA thc = std::sqrt(sp_sq - d2);
            FPDATA t0 = tca - thc, t1 = tca + thc;
            if (t0 > 262) {
                //return std::tuple<bool, float>{true, t0};
                intersection = true;
                d = t0;
            }
            else if (t1 > 262) {
                // return std::tuple<bool, float>{true, t1};
                intersection = true;
                d = t1;
            }
            //return std::tuple<bool, float>{false, 0};
        }
        //ray_sphere_intersect ends

        if (!intersection || d > nearest_dist) continue;
        nearest_dist = d;
        pt = orig + dir * nearest_dist;
        // N = (pt - s.center).normalized();
        N = (pt - s.center).normalized();
        material = s.material;
    }
    return scene_int{nearest_dist < 262144000, pt, N, material};
}
   
scene_int scene_intersect(const vec3 &orig, const vec3 &dir) {
    vec3 pt, N;
    Material material;

    FPDATA nearest_dist = INT32_MAX;
    #ifdef VERBOSE
    std::cout<<"scene_intersect std::abs(dir.y):"<<fixed32_to_float(std::abs(dir.y))<<" and in fixed point: "<<std::abs(dir.y)<<std::endl;
    #endif
    if (std::abs(dir.y) > 262) {
        FPDATA d = -1*rt_div((orig.y + 1048576) , dir.y);
        vec3 p = orig + dir * d;
    #ifdef VERBOSE
        std::cout<<"Clauses: "<<(d > 262)<<" "<<(d < nearest_dist)<<" "<<(std::abs(p.x) < 2621440)<<" "<<(p.z < -2621440)<<" "<<(p.z > -7864320)<<" "<<endl;
    #endif
        if (d > 262 && d < nearest_dist && std::abs(p.x) < 2621440 && p.z < -2621440 && p.z > -7864320) {
    #ifdef VERBOSE
            std::cout<<"Inside checkerboard\n";
    #endif
            nearest_dist = d;
            pt = p;
            N = vec3(0, 262144, 0);

    #ifdef VERBOSE
            std::cout<<" Match Check "<<((mul(131072 , pt.x) + 262144000)>>FP_FL)<<" "<<(mul(131072, pt.z)>>FP_FL)<<" FINNNNNNNAL: "<< ((((mul(131072 , pt.x) + 262144000)>>FP_FL) + (mul(131072, pt.z)>>FP_FL)) & 1)<<endl;
    #endif
            material.diffuse = (((mul(131072 , pt.x) + 262144000)>>FP_FL) + (mul(131072, pt.z)>>FP_FL)) & 1 ? vec3(78643, 78643, 78643) : vec3(78643, 52428, 26214);
            //material.diffuse = (int(131072 * pt.x + 262144000) + int(131072 * pt.z)) & 1 ? vec3(78643, 78643, 78643) : vec3(78643, 52428, 26214);
        }
    }

    // for (const Sphere &s: spheres) { // intersect the ray with all spheres
    for (int sp_count = 0; sp_count<4; sp_count++) { // intersect the ray with all spheres
        const Sphere &s = spheres[sp_count];
        bool intersection = false;
        FPDATA d = 0;
//ray_sphere_intersect
        vec3 L = s.center - orig;
        FPDATA tca = L * dir;
        FPDATA d2 = L * L - mul(tca, tca);
        long long sq_temp = s.radius;
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

// vec3 cast_ray(const vec3 &orig, const vec3 &dir, const int depth = 0) {

//     auto val = scene_intersect(orig, dir);
//     if (depth > 4 || !val.hit)
//         return {0.2, 0.7, 0.8};

//     vec3 reflect_dir = reflect(dir, val.N).normalized();
//     vec3 refract_dir = refract(dir, val.N, val.material.refraction).normalized();
//     vec3 reflect_color = cast_ray(val.shadow_pt, reflect_dir, depth + 1);
//     vec3 refract_color = cast_ray(val.shadow_pt, refract_dir, depth + 1);

//     FPDATA diffuse_light_intensity = 0, specular_light_intensity = 0;
//     for (const vec3 &light: lights) { // checking if the point lies in the shadow of the light
//         vec3 light_dir = (light - val.shadow_pt).normalized();
//         auto val2 = scene_intersect(val.shadow_pt, light_dir);
//         if (val2.hit && (val2.shadow_pt - val.shadow_pt).norm() < (light - val.shadow_pt).norm()) continue;
//         diffuse_light_intensity +=  max(0.f, light_dir * val.N);
//         specular_light_intensity += rt_pow( max(0.f, -reflect(-light_dir, val.N) * dir), val.material.specular);
//     }
//     return val.material.diffuse * diffuse_light_intensity * val.material.albedo[0] +
//            vec3(1., 1., 1.) * specular_light_intensity * val.material.albedo[1] + reflect_color * val.material.albedo[2] +
//            refract_color * val.material.albedo[3];
// }


vec3 cast_rayd0(const vec3 &orig, const vec3 &dir, const int depth = 0) {

    #ifdef VERBOSE
    std::printf("RAY CAST [%d] %f, %f, %f\n", (pix_ctr++), fixed32_to_float(dir[0], FP_IL), fixed32_to_float(dir[1], FP_IL), fixed32_to_float(dir[2], FP_IL));
    std::printf("%d\n",dir[0]);
    std::printf("%d\n",dir[1]);
    std::printf("%d\n",dir[2]);
    #endif
    scene_int val = scene_intersect(orig, dir);
    ///////////
    ///////////

    // vec3 reflect_dir = reflect(dir, val.N).normalized();
    // vec3 refract_dir = refract(dir, val.N, val.material.refraction).normalized();
    // if (depth > 2 || !val.hit)
    //     return vec3(052428, 0.7, 0.8);

    if(!val.hit)
     return vec3(52428, 183500, 209715);
     
    vec3 reflect_color = vec3(52428, 183500, 209715); //cast_ray1(val.shadow_pt, reflect_dir);
    vec3 refract_color = vec3(52428, 183500, 209715); //cast_ray1(val.shadow_pt, refract_dir);

    FPDATA diffuse_light_intensity = 0, specular_light_intensity = 0; //PREVIOUSLY 0
    for (int l_cnt = 0; l_cnt <3; l_cnt++) { // checking if the point lies in the shadow of the light
        const vec3 &light = lights[l_cnt];
        vec3 light_dir = (light - val.shadow_pt).normalized();
    #ifdef VERBOSE
        std::printf("RAY CAST Light iter %d\n", l_cnt);
    #endif
        scene_int val2 = scene_intersect(val.shadow_pt, light_dir);
        if (val2.hit && (val2.shadow_pt - val.shadow_pt).norm() <= (light - val.shadow_pt).norm()) continue;
        diffuse_light_intensity += max(0, light_dir * val.N);
        specular_light_intensity += rt_pow(max(0, -reflect(-light_dir, val.N) * dir), val.material.specular);
    }
    return val.material.diffuse * mul(diffuse_light_intensity , val.material.albedo[0]) +
           vec3(f1, f1, f1) * mul(specular_light_intensity , val.material.albedo[1]) + reflect_color * val.material.albedo[2] +
           refract_color * val.material.albedo[3];
}



/* User-defined code */
static void init_buffer(token_t *in, token_t * gold)
{
	int i;
	int j;
	// for (i = 0; i < 1; i++)
	// 	for (j = 0; j < 3*img_width*img_height; j++)
	// 		in[i * in_words_adj + j] = (token_t) j;

	// for (i = 0; i < 1; i++)
	// 	for (j = 0; j < 3*img_width*img_height; j++)
	// 		gold[i * out_words_adj + j] = (token_t) j;

	for (int i = 0; i < 1; i++)
        for (int j = 0; j < img_width*img_height; j++){ // total loop: 3*img_width*img_height
            float dir_x = ((j % img_width + 0.5) - img_width / 2.);
            float dir_y = (-(j / img_width + 0.5) + img_height / 2.);
            float dir_z = (-img_height / (2. * tan(fov / 2.)));

            vec3 data = normalized(dir_x, dir_y, dir_z);

            in[i * in_words_adj + 3*j] =     data[0];
            in[i * in_words_adj + 3*j + 1] = data[1];
            in[i * in_words_adj + 3*j + 2] = data[2];

        }

	for (int i = 0; i < 1; i++)
        for (int j = 0; j < 3*img_width*img_height; j++){
            // std::string token;
            // buffer>>token;
            //gold[i * out_words_adj + j] = std::stof(token);
            gold[i * out_words_adj + j] = in[i * out_words_adj + j];
            // buffer>>token; // , removal
        }
}


int main() {

	int errors;

	token_t *gold;
	token_t *buf;

	init_parameters();

	buf = (token_t *) esp_alloc(size);
	cfg_000[0].hw_buf = buf;
    
	gold = new int32_t[out_size];

	init_buffer(buf, gold);


	printf("\n====== %s ======\n\n", cfg_000[0].devname);
	/* <<--print-params-->> */
	printf("  .img_width = %d\n", img_width);
	printf("  .img_height = %d\n", img_height);
	printf("\n  ** START **\n");

	esp_run(cfg_000, NACC);

	printf("\n  ** DONE **\n");

	errors = validate_buffer(&buf[out_offset], gold);

    delete[] gold;
	// free(gold);
	esp_free(buf);

	if (!errors)
		printf("+ Test PASSED\n");
	else
		printf("+ Test FAILED\n");

	printf("\n====== %s ======\n\n", cfg_000[0].devname);


	return errors;


    // std::vector<std::vector<vec3> > frames;
    // for(int i = 0; i<ITERATIONS; i++){
    //     std::vector<vec3> framebuffer(width * height);

    //     clock_t start_time = clock();
    //     // using namespace std::chrono;
        
        // //#pragma omp parallel for
        // for (int pix = 0; pix < width * height; pix++) {
        //     // FPDATA dir_x = float_to_fixed32(((pix % width + 0.5) - width / 2.), FP_IL);
        //     // FPDATA dir_y = float_to_fixed32((-(pix / width + 0.5) + height / 2.), FP_IL);
        //     // FPDATA dir_z = float_to_fixed32((-height / (2. * tan(fov / 2.))), FP_IL);
        //     // framebuffer[pix] = cast_rayd0(vec3(0, 0, 0), vec3(dir_x, dir_y, dir_z).normalized());

        //     float dir_x =((pix % width + 0.5) - width / 2.) ;
        //     float dir_y =(-(pix / width + 0.5) + height / 2.); 
        //     float dir_z =(-height / (2. * tan(fov / 2.))) ;

        //     // std::printf("-----PIXEL %d--------\n",pix);
        //     // std::printf("%f\n",dir_x);
        //     // std::printf("%f\n",dir_y);
        //     // std::printf("%f\n",dir_z);

        //     framebuffer[pix] = cast_rayd0(vec3(0, 0, 0), vec3(0, 0, 0).normalized(dir_x, dir_y, dir_z));
        //     if(pix==812384) cout<<framebuffer[pix][0]<<" "<<framebuffer[pix][1]<<" "<<framebuffer[pix][2]<<std::endl;
        // }
    //     frames.push_back(framebuffer);
    //     clock_t end_time = clock();
    //     std::cout<<(end_time-start_time)<<" clocks "<<((float)(end_time-start_time))/CLOCKS_PER_SEC<<" seconds since there are "<<CLOCKS_PER_SEC<<" CLOCKS_PER_SEC"<<endl;
    
    // }
  
    // cout<<"Float_to_fixed "<<1.f<<" : "<<float_to_fixed32(1.f, 14)<<endl;
    // cout<<"Float_to_fixed "<<2.f<<" : "<<float_to_fixed32(2.f, 14)<<endl;
    // cout<<"Float_to_fixed "<<0.f<<" : "<<float_to_fixed32(0.f, 14)<<endl;
    // cout<<"Float_to_fixed "<<-1.f<<" : "<<float_to_fixed32(-1.f, 14)<<endl;
    // cout<<"Float_to_fixed "<<0.1f<<" : "<<float_to_fixed32(0.1f, 14)<<endl;
    // cout<<"Float_to_fixed "<<0.2f<<" : "<<float_to_fixed32(0.2f, 14)<<endl;
    // cout<<"Float_to_fixed "<<0.3f<<" : "<<float_to_fixed32(0.3f, 14)<<endl;
    // cout<<"Float_to_fixed "<<0.4f<<" : "<<float_to_fixed32(0.4f, 14)<<endl;
    // cout<<"Float_to_fixed "<<0.5f<<" : "<<float_to_fixed32(0.5f, 14)<<endl;
    // cout<<"Float_to_fixed "<<0.6f<<" : "<<float_to_fixed32(0.6f, 14)<<endl;
    // cout<<"Float_to_fixed "<<0.7f<<" : "<<float_to_fixed32(0.7f, 14)<<endl;
    // cout<<"Float_to_fixed "<<0.8f<<" : "<<float_to_fixed32(0.8f, 14)<<endl;
    // cout<<"Float_to_fixed "<<0.9f<<" : "<<float_to_fixed32(0.9f, 14)<<endl;


    // cout<<"Float_to_fixed "<<1.4f<<" : "<<float_to_fixed32(1.4f, 14)<<endl;
    // cout<<"Float_to_fixed "<<16.f<<" : "<<float_to_fixed32(16.f, 14)<<endl;
    // cout<<"Float_to_fixed "<<50.f<<" : "<<float_to_fixed32(50.f, 14)<<endl;
    // cout<<"Float_to_fixed "<<20.f<<" : "<<float_to_fixed32(20.f, 14)<<endl;
    // cout<<"Float_to_fixed "<<-20.f<<" : "<<float_to_fixed32(-20.f, 14)<<endl;
    // cout<<"Float_to_fixed "<<30.f<<" : "<<float_to_fixed32(30.f, 14)<<endl;
    // cout<<"Float_to_fixed "<<-30.f<<" : "<<float_to_fixed32(-30.f, 14)<<endl;
    // cout<<"Float_to_fixed "<<25.f<<" : "<<float_to_fixed32(25.f, 14)<<endl;
    // cout<<"Float_to_fixed "<<-25.f<<" : "<<float_to_fixed32(-25.f, 14)<<endl;
    // cout<<"Float_to_fixed "<<1.5f<<" : "<<float_to_fixed32(1.5f, 14)<<endl;


    // cout<<"Float_to_fixed "<<3.f<<" : "<<float_to_fixed32(3.f, 14)<<endl;
    // cout<<"Float_to_fixed "<<4.f<<" : "<<float_to_fixed32(4.f, 14)<<endl;
    // cout<<"Float_to_fixed "<<5.f<<" : "<<float_to_fixed32(5.f, 14)<<endl;
    // cout<<"Float_to_fixed "<<7.f<<" : "<<float_to_fixed32(7.f, 14)<<endl;
    // cout<<"Float_to_fixed "<<18.f<<" : "<<float_to_fixed32(18.f, 14)<<endl;
    // cout<<"Float_to_fixed "<<12.f<<" : "<<float_to_fixed32(12.f, 14)<<endl;
    // cout<<"Float_to_fixed "<<10.f<<" : "<<float_to_fixed32(10.f, 14)<<endl;
    // cout<<"Float_to_fixed "<<125.f<<" : "<<float_to_fixed32(125.f, 14)<<endl;
    // cout<<"Float_to_fixed "<<1425.f<<" : "<<float_to_fixed32(1425.f, 14)<<endl;
    // int f1425=float_to_fixed32(10.f, 14);
    // int f1=float_to_fixed32(10.f, 14);
    // FPDATA f10 = fixed32_to_float(f1425, 14);
    // printf("%x\n", 1.f);
    // printf("%x\n", f1>>18);
    // printf("%d\n", f1>>18);
    // cout<<"Float_to_fixed "<<0.001f<<" : "<<float_to_fixed32(0.001f, 14)<<endl;
    // cout<<"Float_to_fixed "<<1e10<<" : "<<float_to_fixed32(1e10, 14)<<endl;
    return 0;
}

