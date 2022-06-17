// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#include "tiled_app.hpp"
#include "tiled_app_directives.hpp"

// Functions

#include "tiled_app_functions.hpp"

//#define PRINT_ALL 1

// Processes

void tiled_app::load_input()
{

    // Reset
    {
        HLS_PROTO("load-reset");

        // this->reset_load_input();

        input_ready.req.reset_req();

        output_ready.ack.reset_ack();
        this->reset_dma_read();
        
	    //load_store_cfg_done.req.reset_req();

	    //load_store_cfg_done.ack.reset_ack();
        wait();
    }

    // Config
    /* <<--params-->> */
    int32_t num_tiles;
    int32_t tile_size;
    // int32_t rd_wr_enable;
    conf_info_t config;
    {
        HLS_PROTO("load-config");

        cfg.wait_for_config(); // config process
        config = this->conf_info.read();

        // User-defined config code
        /* <<--local-params-->> */
        num_tiles = config.num_tiles;
        tile_size = config.tile_size;
        // rd_wr_enable = config.rd_wr_enable;
    }

   
    // Load
    {
        //if(rd_wr_enable == 0){
            HLS_PROTO("load-dma");
            wait();

            bool ping = true;

            //uint32_t store_offset = round_up(tile_size, DMA_WORD_PER_BEAT) * num_tiles;
            // Batching
            for (uint16_t b = 0; b < num_tiles; b++)
            {   
                {
                    //{   // Read synchronizer   
                    uint64_t data = 1;
                    // uint64_t data;
                    //Read from DMA
                    sc_dt::sc_bv<DMA_WIDTH> dataBvin;
                    dma_info_t dma_info2(0, 1, DMA_SIZE);

                    //Read from DMA
                    // sc_dt::sc_bv<DMA_WIDTH> dataBvin;
    // #ifndef STRATUS_HLS
    //     uint32_t sync_offset =  0; // DMA_WORD_PER_BEAT + store_offset; 
    //     ESP_REPORT_INFO("Before Load sync for tile %u/%u sync_offset %u", b, num_tiles, sync_offset);
    // #endif
                    //Wait for 1
                    do {
                        HLS_UNROLL_LOOP(OFF);
                        this->dma_read_ctrl.put(dma_info2);
                        wait();
                        dataBvin.range(DMA_WIDTH - 1, 0) = this->dma_read_chnl.get();
                        wait();
                        data = dataBvin.range(DMA_WIDTH - 1, 0).to_int64();
                        wait();
                    }
                    while(data==0);
                }


                // #ifndef STRATUS_HLS
                //     ESP_REPORT_INFO("Looping Load sync for tile %u/%u, data=%lu DONE", b, num_tiles, data);
                // #endif
                // wait();
                // //}  
                // wait();

                {
                    uint32_t offset = 2* DMA_WORD_PER_BEAT ; //0;
                    uint32_t sp_offset = 0;
                    uint32_t length = round_up(tile_size, DMA_WORD_PER_BEAT);
                    // #ifndef STRATUS_HLS
                    //     ESP_REPORT_INFO("Load sync for rem===%u", length);
                    // #endif
                    // Chunking
                    for (int rem = length; rem > 0; rem -= PLM_IN_WORD)
                    {
                        // #ifndef STRATUS_HLS
                        //     ESP_REPORT_INFO("Load sync for rem=%u", rem);
                        // #endif
                        wait();
                        // Configure DMA transaction
                        uint32_t len = rem > PLM_IN_WORD ? PLM_IN_WORD : rem;
                        dma_info_t dma_info(offset / DMA_WORD_PER_BEAT, len / DMA_WORD_PER_BEAT, DMA_SIZE);
                        offset += len;

                        this->dma_read_ctrl.put(dma_info);
                        for (uint16_t i = 0; i < len; i += DMA_WORD_PER_BEAT)
                        {
                            HLS_BREAK_DEP(plm_in_ping);
                            HLS_BREAK_DEP(plm_in_pong);
                            // HLS_BREAK_DEP(plm);

                            sc_dt::sc_bv<DMA_WIDTH> dataBv;

                            dataBv = this->dma_read_chnl.get();
                            wait();

                            // Write to PLM (all DMA_WORD_PER_BEAT words in one cycle)
                            for (uint16_t k = 0; k < DMA_WORD_PER_BEAT; k++)
                            {
                                HLS_UNROLL_SIMPLE;
                                //plm[sp_offset + i + k] = dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH).to_int64();
                                
                                if (ping)
                                    plm_in_ping[sp_offset + i + k] = dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH).to_int64();
                                else
                                    plm_in_pong[sp_offset + i + k] = dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH).to_int64();
                                
                                // #ifndef STRATUS_HLS
                                // #ifdef PRINT_ALL
                                // uint64_t printval = (ping)?plm_in_ping[sp_offset + i + k].to_int64():plm_in_pong[sp_offset + i + k].to_int64();
                                // ESP_REPORT_INFO("TILE %u: LOAD RECEIVED %u, SP INDEX:%u, ping:%u", b, printval, (sp_offset + i + k), ping);
                                // #endif
                                // #endif
                            }
                        }
                        sp_offset += PLM_IN_WORD;
                    }
                }

                { 
                    int data = 0;
                    sc_dt::sc_bv<DMA_WIDTH> dataBvin;
                    // #ifndef STRATUS_HLS
                    //     ESP_REPORT_INFO("Before Store sync for tile %u offset %u", b, sync_offset);
                    // #endif
                    
                    dma_info_t dma_info1(1, 1, DMA_SIZE);

                    // Wait for 0
                    do {
                        HLS_UNROLL_LOOP(OFF);
                        this->dma_read_ctrl.put(dma_info1);
                        wait();
                        dataBvin.range(DMA_WIDTH - 1, 0) = this->dma_read_chnl.get();
                        wait();
                        data = dataBvin.range(DMA_WIDTH - 1, 0).to_int64();
                        //data = data>>32;
                        // wait();
                    }
                    while(data==1);

                    // #ifndef STRATUS_HLS
                    //     ESP_REPORT_INFO("Looping Store sync for tile %u/%u, offset=%u data=%lu OVER", b, num_tiles, sync_offset, data);
                    // #endif

                    this->load_compute_handshake();
                    // plm_in_ping[0] = 1;
                    //this->load_store_cfg_handshake();
                    // Write to DMA
                    //ping = !ping;

                    this->store_compute_handshake();
                }

               

            }
        //}
        // this->process_done();
        // this->accelerator_done();
    }

    // Conclude
    {
        // this->process_done();
        this->process_done();
        this->accelerator_done();
    }
}



void tiled_app::store_output()
{
    // Reset
    {
        HLS_PROTO("store-reset");

        // this->reset_store_output();
	    // // load_store_cfg_done.ack.reset_ack();
	    // //load_store_cfg_done.req.reset_req();

        // this->reset_compute_kernel();

        input_ready.ack.reset_ack();
        output_ready.req.reset_req();
        wait();
        // explicit PLM ports reset if any

        this->reset_accelerator_done();
        this->reset_dma_write();
        // User-defined reset code

        wait();
    }

    // Config
    /* <<--params-->> */
    int32_t num_tiles;
    int32_t tile_size;
    // int32_t rd_wr_enable;
    {
        HLS_PROTO("store-config");

        cfg.wait_for_config(); // config process
        conf_info_t config = this->conf_info.read();

        // User-defined config code
        /* <<--local-params-->> */
        num_tiles = config.num_tiles;
        tile_size = config.tile_size;
        // rd_wr_enable = config.rd_wr_enable;
    }

    // Store
    {// Store - only if rd_wr_enable is 1
        //if(rd_wr_enable == 1){
            HLS_PROTO("store-dma");
            wait();

            bool ping = true;
            // Batching
            for (uint16_t b = 0; b < num_tiles; b++)
            {
                uint32_t store_offset = round_up(tile_size, DMA_WORD_PER_BEAT)+ 2*DMA_WORD_PER_BEAT;
                uint32_t offset = store_offset;
                uint32_t sp_offset = 0;

                wait();
                this->compute_load_handshake();
                //this->store_load_cfg_handshake();
                wait();

                // uint32_t sync_offset = 1;
                // dma_info_t dma_info2(sync_offset, 1, DMA_SIZE);

                        //Write to DMA
                // this->dma_write_ctrl.put(dma_info2);
                // wait();
                // //write 1
                // dataBvout.range(DMA_WIDTH - 1, 0) = 3;
                // this->dma_write_chnl.put(dataBvout);
                // wait();
                //
               

                // #ifndef STRATUS_HLS
                // uint32_t sync_offset = 1;
                // #endif
                uint32_t length = round_up(tile_size, DMA_WORD_PER_BEAT);

                // Chunking
                for (int rem = length; rem > 0; rem -= PLM_OUT_WORD)
                {
                    #ifndef STRATUS_HLS
                        ESP_REPORT_INFO("Store sync for rem=%lu", rem);
                    #endif
                    // Configure DMA transaction
                    uint32_t len = rem > PLM_OUT_WORD ? PLM_OUT_WORD : rem;
                    dma_info_t dma_info(offset / DMA_WORD_PER_BEAT, len / DMA_WORD_PER_BEAT, DMA_SIZE);
                    offset += len;

                    this->dma_write_ctrl.put(dma_info);
   
                    for (uint16_t i = 0; i < len; i += DMA_WORD_PER_BEAT)
                    {
                        sc_dt::sc_bv<DMA_WIDTH> dataBv;

                        // Read from PLM
                        wait();
                        for (uint16_t k = 0; k < DMA_WORD_PER_BEAT; k++)
                        {
                            HLS_UNROLL_SIMPLE;
                            //dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH) = plm[sp_offset + i + k];
                            if (ping)
                                dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH) = plm_in_ping[sp_offset + i + k];
                            else
                                dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH) = plm_in_pong[sp_offset + i + k];
                            #ifndef STRATUS_HLS
                            #ifdef PRINT_ALL
                            ESP_REPORT_INFO("TILE %u: SENDING FROM HW %u, SP INDEX: %u, ping:%u", b, dataBv.to_int64(), (sp_offset + i + k), ping);
                            #endif
                            #endif
                        }
                        this->dma_write_chnl.put(dataBv);
                        wait();
                    }
                    sp_offset += PLM_OUT_WORD;
                }
                //STORE SYNCHRONIZATION
                //Write to DMA

                //dma_info_t dma_info2(0, 1, DMA_SIZE);
                int syn_len = 2;
                sc_dt::sc_bv<DMA_WIDTH> dataBvout2;
                dma_info_t dma_info3(0, syn_len/ DMA_WORD_PER_BEAT, DMA_SIZE);
                this->dma_write_ctrl.put(dma_info3);
                // wait();
                {
                    dataBvout2.range(DMA_WIDTH - 1, 0) = 0;
                    this->dma_write_chnl.put(dataBvout2);
                    wait();
                }
                {
                    // int syn_len = 1;
                    // sc_dt::sc_bv<DMA_WIDTH> dataBvout2;
                    // dma_info_t dma_info2(1, syn_len/ DMA_WORD_PER_BEAT, DMA_SIZE);
                    // this->dma_write_ctrl.put(dma_info2);
                    // wait();
                    dataBvout2.range(DMA_WIDTH - 1, 0) = 1;
                    this->dma_write_chnl.put(dataBvout2);
                    // wait();
                }



                // wait();

                // dma_info_t dma_info1(0, 1, DMA_SIZE);
                // this->dma_write_ctrl.put(dma_info1);
                // sc_dt::sc_bv<DMA_WIDTH> dataBvout;
                // wait();
                // //write 0
                // dataBvout.range(DMA_WIDTH - 1, 0) = 0;
                // this->dma_write_chnl.put(dataBvout);
                // wait();

                {
                    this->compute_store_handshake();
                }

//uncomment
                

                 // { 
                   

                    // compute == synchronizer    
                    //send the output ready ack
               // }    
                // this->load_store_cfg_handshake();
                //ping = !ping;
            }
        //}
    }

    // Conclude
    {
        this->process_done();
    }
}


// compute == synchronizer   
//compute is invoked after read tile is over.
//compute should read the sync variable and set to 1, and keep spinning for 0
//read should have started with reading the sync variable (for next tile)
//once 0, exit to read_input() again
void tiled_app::compute_kernel()
{
    // Reset
    {
        HLS_PROTO("compute-reset");

    }


    // Conclude
    {
        this->process_done();
    }
}
