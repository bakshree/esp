// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#include <sstream>
#include "system.hpp"

#define TB_NUM_TILES 1
int curr_tile = 0;

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
    init_sync_cfg();
    load_memory();
    {
        conf_info_t config;
        // Custom configuration
        /* <<--params-->> */
        config.num_tiles = 1; //num_tiles;
        config.tile_size = 10; //tile_size;
        config.rd_wr_enable = rd_wr_enable;

        wait(); conf_info.write(config);
        conf_done.write(true);
    }

    ESP_REPORT_INFO("config done");

    // Compute
    {
        // Print information about begin time
        sc_time begin_time = sc_time_stamp();
        ESP_REPORT_TIME(begin_time, "BEGIN - tiled_app");

        // Wait the termination of the accelerator
        do { wait();
        
            // ESP_REPORT_INFO("SETTING DUMP MEM BIT");
            // sc_dt::sc_bv<DMA_WIDTH> data_bv0(0);
            // mem[1] = data_bv0;
                if(write_sync()){
                    dump_memory();
                    curr_tile++;
                    if(curr_tile < TB_NUM_TILES){
                        if(read_sync())load_memory();
                    }
                }

     } while (!acc_done.read());
        debug_info_t debug_code = debug.read();

        // Print information about end time
        sc_time end_time = sc_time_stamp();
        ESP_REPORT_TIME(end_time, "END - tiled_app");

        esc_log_latency(sc_object::basename(), clock_cycle(end_time - begin_time));
        wait(); conf_done.write(false);
    }

    // Validate
    {
        // if(curr_tile <= TB_NUM_TILES && write_sync()){
        //     dump_memory();
        //     curr_tile++;
        //     ESP_REPORT_INFO("MEMORY DUMPED");
        //     //break;//TODO: Remove
        // }
    //     dump_memory(); // store the output in more suitable data structure if needed
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

    // Memory initialization:
#if (DMA_WORD_PER_BEAT == 0)
    for (int i = 0; i < in_size; i++)  {
        sc_dt::sc_bv<DATA_WIDTH> data_bv(in[i]);
        for (int j = 0; j < DMA_BEAT_PER_WORD; j++)
            mem[DMA_BEAT_PER_WORD * i + j] = data_bv.range((j + 1) * DMA_WIDTH - 1, j * DMA_WIDTH);
    }
#else
    for (int i = 2; i < in_size / DMA_WORD_PER_BEAT; i++)  {
        sc_dt::sc_bv<DMA_WIDTH> data_bv(in[i]);
        for (int j = 0; j < DMA_WORD_PER_BEAT; j++)
            data_bv.range((j+1) * DATA_WIDTH - 1, j * DATA_WIDTH) = in[i * DMA_WORD_PER_BEAT + j];
        mem[i] = data_bv;
        ESP_REPORT_INFO("SENT %u", in[i]);
    }
#endif  
    sc_dt::sc_bv<DMA_WIDTH> data_bv(1);
    mem[0] = data_bv;
}

bool system_t::read_sync(){
    int64_t read_sync; 

#if (DMA_WORD_PER_BEAT == 0)
    sc_dt::sc_bv<DATA_WIDTH> data_bv;
    for (int j = 0; j < DMA_BEAT_PER_WORD; j++)
        data_bv.range((j + 1) * DMA_WIDTH - 1, j * DMA_WIDTH) = mem[0 + j];
    read_sync = data_bv.to_int64();
#else
    for (int j = 0; j < DMA_WORD_PER_BEAT; j++)
        read_sync = mem[0].range(DATA_WIDTH - 1, 0).to_int64();
#endif

    return read_sync;
}

bool system_t::write_sync(){

    int64_t store_sync; 

#if (DMA_WORD_PER_BEAT == 0)
    sc_dt::sc_bv<DATA_WIDTH> data_bv;
    for (int j = 0; j < DMA_BEAT_PER_WORD; j++)
        data_bv.range((j + 1) * DMA_WIDTH - 1, j * DMA_WIDTH) = mem[1 + j];
    store_sync = data_bv.to_int64();
#else
    //for (int j = 0; j < DMA_WORD_PER_BEAT; j++)
        ESP_REPORT_INFO("Checking Write Sync");
        store_sync = mem[1].range(DATA_WIDTH - 1, 0).to_int64();
        ESP_REPORT_INFO("Write Sync = %u", store_sync);
#endif
        
    return store_sync;
}

void system_t::init_sync_cfg(){
    // Optional usage check
#ifdef CADENCE
    if (esc_argc() != 1)
    {
        ESP_REPORT_INFO("usage: %s\n", esc_argv()[0]);
        sc_stop();
    }
#endif

    // Input data and golden output (aligned to DMA_WIDTH makes your life easier)
#if (DMA_WORD_PER_BEAT == 0)
    in_words_adj = tile_size;
    out_words_adj = tile_size;
#else
    in_words_adj = round_up(tile_size, DMA_WORD_PER_BEAT);
    out_words_adj = round_up(tile_size, DMA_WORD_PER_BEAT);
#endif

    in_size = in_words_adj * (num_tiles) + DMA_WORD_PER_BEAT + 1;
    out_size = out_words_adj * (num_tiles);

    in = new int64_t[in_size];
    // in[0] = 1;
    // in[1] = 1;
    for (int i = 0; i < in_size; i++)
        in[i] = i+1;
    // for (int i = 0; i < num_tiles; i++)
    //     for (int j = 0; j < tile_size; j++)
    //         in[i * in_words_adj + j] = (int64_t) j;
    // for (int i = num_tiles*tile_size; i < in_size; i++)
    //     in[i ] = 21;
    //in[in_words_adj * (num_tiles) + DMA_WORD_PER_BEAT] = 1;

    // Compute golden output
    gold = new int64_t[out_size];
    for (int i = 0; i < out_size; i++)
        gold[i] = i+3;
    // for (int i = 0; i < num_tiles; i++)
    //     for (int j = 0; j < tile_size; j++)
    //         gold[i * out_words_adj + j] = (int64_t) j;


    sc_dt::sc_bv<DMA_WIDTH> data_bv(1);
    sc_dt::sc_bv<DMA_WIDTH> data_bv0(0);
    mem[1] = data_bv0;
    mem[0] = data_bv0;

    ESP_REPORT_INFO("load memory completed");
}

void system_t::dump_memory()
{
    // Get results from memory
    out = new int64_t[out_size];
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
    for (int i = 0; i < out_size / DMA_WORD_PER_BEAT; i++){
        for (int j = 0; j < DMA_WORD_PER_BEAT; j++)
            out[i * DMA_WORD_PER_BEAT + j] = mem[offset + i].range((j + 1) * DATA_WIDTH - 1, j * DATA_WIDTH).to_int64();

        ESP_REPORT_INFO("RECEIVED %u", out[i * DMA_WORD_PER_BEAT]);
    }
#endif
    ESP_REPORT_INFO("dump memory completed");


    sc_dt::sc_bv<DMA_WIDTH> data_bv(0);
    mem[0] = data_bv;

}

int system_t::validate()
{
    // Check for mismatches
    uint32_t errors = 0;

    for (int i = 0; i < num_tiles; i++)
        for (int j = 0; j < tile_size; j++)
            if (gold[i * out_words_adj + j] != out[i * out_words_adj + j]){
                ESP_REPORT_INFO("idx: %u, gold = %u, out = %u", (i * out_words_adj + j), gold[i * out_words_adj + j] , out[i * out_words_adj + j]);
                errors++;
            }

    delete [] in;
    delete [] out;
    delete [] gold;

    return errors;
}
