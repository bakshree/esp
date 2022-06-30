// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#include <sstream>
#include <iostream>
#include "system.hpp"

//constexpr float fov = 1.0472; // Radians

float norm(float x, float y, float z)  {
    return std::sqrt(x * x + y * y + z * z);
}

vec3 normalized(float x, float y, float z)  {
    float n = norm(x, y, z);
    //return (*this) * (1.f / norm());
    return vec3(float_to_fixed32(x/n), float_to_fixed32(y/n), float_to_fixed32(z/n));
    //return (*this) * (1.f / norm());
}

// Process
void system_t::config_proc()
{

    // Reset
    {
        conf_done.write(false);
        conf_info.write(conf_info_t());
        wait();
    }

    ESP_REPORT_INFO("reset done");

    // Config
    load_memory();
    {
        conf_info_t config;
        // Custom configuration
        /* <<--params-->> */
        config.img_width = img_width;
        config.img_height = img_height;

        wait(); conf_info.write(config);
        conf_done.write(true);
    }

    ESP_REPORT_INFO("config done");

    // Compute
    {
        // Print information about begin time
        sc_time begin_time = sc_time_stamp();
        ESP_REPORT_TIME(begin_time, "BEGIN - rt_accel");

        // Wait the termination of the accelerator
        do { wait(); } while (!acc_done.read());
        debug_info_t debug_code = debug.read();

        // Print information about end time
        sc_time end_time = sc_time_stamp();
        ESP_REPORT_TIME(end_time, "END - rt_accel");

        esc_log_latency(sc_object::basename(), clock_cycle(end_time - begin_time));
        wait(); conf_done.write(false);
    }

    // Validate
    {
        dump_memory(); // store the output in more suitable data structure if needed
        // check the results with the golden model
        if (validate())
        {
            ESP_REPORT_ERROR("validation failed!");
        } else
        {
            ESP_REPORT_INFO("validation passed!");
        }
    }

    // Conclude
    {
        sc_stop();
    }
}

// Functions
void system_t::load_memory()
{
    // Optional usage check

    ESP_REPORT_INFO("Inside Load Memory");
#ifdef CADENCE
    if (esc_argc() != 1)
    {
        ESP_REPORT_INFO("usage: %s\n", esc_argv()[0]);
        sc_stop();
    }
#endif

    // Input data and golden output (aligned to DMA_WIDTH makes your life easier)
#if (DMA_WORD_PER_BEAT == 0)
    in_words_adj = 3*img_width*img_height;
    out_words_adj = 3*img_width*img_height;
#else
    in_words_adj = round_up(3*img_width*img_height, DMA_WORD_PER_BEAT);
    out_words_adj = round_up(3*img_width*img_height, DMA_WORD_PER_BEAT);
#endif

    in_size = in_words_adj * (1);
    out_size = out_words_adj * (1);

    ESP_REPORT_INFO("Load Memory:in_size  :%d",in_size);
    ESP_REPORT_INFO("Load Memory:out_size :%d",out_size);

    in = new int[in_size];
    for (int i = 0; i < 1; i++)
        for (int j = 0; j < img_width*img_height; j++){ // total loop: 3*img_width*img_height
            float dir_x = ((j % img_width + 0.5) - img_width / 2.);
            float dir_y = (-(j / img_width + 0.5) + img_height / 2.);
            float dir_z = (-img_height / (2. * tan(fov / 2.)));

            // ESP_REPORT_INFO("-----PIXEL %d--------",j);
            // ESP_REPORT_INFO("%f",dir_x);
            // ESP_REPORT_INFO("%f",dir_y);
            // ESP_REPORT_INFO("%f",dir_z);
            vec3 data = normalized(dir_x, dir_y, dir_z);
            // in[i * in_words_adj + 3*j] =     float_to_fixed32(dir_x, 14);
            // in[i * in_words_adj + 3*j + 1] = float_to_fixed32(dir_y, 14);
            // in[i * in_words_adj + 3*j + 2] = float_to_fixed32(dir_z, 14);
            //vec3 res = cast_ray(vec3(0, 0, 0), data);

            in[i * in_words_adj + 3*j] =     data[0];
            in[i * in_words_adj + 3*j + 1] = data[1];
            in[i * in_words_adj + 3*j + 2] = data[2];

            // ESP_REPORT_INFO("%d",data[0]);
            // ESP_REPORT_INFO("%d",data[1]);
            // ESP_REPORT_INFO("%d",data[2]);
        }

    ESP_REPORT_INFO("Load Memory: Loaded in[%d]",out_size);
    // Compute golden output
    gold = new int[out_size];

    // std::ofstream ifs_text("./output.txt", std::ios::binary);
    // std::stringstream buffer;
    // buffer << ifs_text.rdbuf();
    //std::string tokenstring(buffer);
    // std::string* tokens = tokenstring.split(',');
    for (int i = 0; i < 1; i++)
        for (int j = 0; j < 3*img_width*img_height; j++){
            // std::string token;
            // buffer>>token;
            //gold[i * out_words_adj + j] = std::stof(token);
            gold[i * out_words_adj + j] = in[i * out_words_adj + j];
            // buffer>>token; // , removal
        }

    // Memory initialization:
#if (DMA_WORD_PER_BEAT == 0)
    for (int i = 0; i < in_size; i++)  {
        sc_dt::sc_bv<DATA_WIDTH> data_bv(in[i]);
        for (int j = 0; j < DMA_BEAT_PER_WORD; j++)
            mem[DMA_BEAT_PER_WORD * i + j] = data_bv.range((j + 1) * DMA_WIDTH - 1, j * DMA_WIDTH);
    }
#else
    for (int i = 0; i < in_size / DMA_WORD_PER_BEAT; i++)  {
        sc_dt::sc_bv<DMA_WIDTH> data_bv(in[i]);
        for (int j = 0; j < DMA_WORD_PER_BEAT; j++)
            data_bv.range((j+1) * DATA_WIDTH - 1, j * DATA_WIDTH) = in[i * DMA_WORD_PER_BEAT + j];
        mem[i] = data_bv;
    }
#endif

    ESP_REPORT_INFO("load memory completed");
}

void system_t::dump_memory()
{
    // Get results from memory
    out = new int32_t[out_size];
    uint32_t offset = in_size;

#if (DMA_WORD_PER_BEAT == 0)
    offset = offset * DMA_BEAT_PER_WORD;
    for (int i = 0; i < out_size; i++)  {
        sc_dt::sc_bv<DATA_WIDTH> data_bv;

        for (int j = 0; j < DMA_BEAT_PER_WORD; j++)
            data_bv.range((j + 1) * DMA_WIDTH - 1, j * DMA_WIDTH) = mem[offset + DMA_BEAT_PER_WORD * i + j];

        out[i] = data_bv.to_int64();
    }
#else
    offset = offset / DMA_WORD_PER_BEAT;
    for (int i = 0; i < out_size / DMA_WORD_PER_BEAT; i++)
        for (int j = 0; j < DMA_WORD_PER_BEAT; j++)
            out[i * DMA_WORD_PER_BEAT + j] = mem[offset + i].range((j + 1) * DATA_WIDTH - 1, j * DATA_WIDTH).to_int64();
#endif

    ESP_REPORT_INFO("dump memory completed");
}

int system_t::validate()
{
    // Check for mismatches
    uint32_t errors = 0;

    ESP_REPORT_INFO("DUMPING IMAGE:");
    std::ofstream ofs("/scratch/projects/bmishra3/spandex/esp_cs533/accelerators/stratus_hls/rt_accel_stratus/out_rt_hw.ppm", std::ios::binary);
    ofs << "P6\n" << width << " " << height << "\n255\n";

    for (int j = 0; j < img_width*img_height; j++){
        float color[3];
        color[0] = fixed32_to_float(out[3*j]    , FP_IL);
        color[1] = fixed32_to_float(out[3*j + 1], FP_IL);
        color[2] = fixed32_to_float(out[3*j + 2], FP_IL);

        float max = std::max(1.f, std::max(color[0], std::max(color[1], color[2])));
        for (int chan: {0, 1, 2}){
            ofs << (char) (255 * color[chan] / max);
        }
    }
    

    // for (int i = 0; i < 1; i++)
    //     for (int j = 0; j < 3*img_width*img_height; j++)
    //         if (gold[i * out_words_adj + j] != out[i * out_words_adj + j])
    //             errors++;

    delete [] in;
    delete [] out;
    delete [] gold;

    return errors;
}
