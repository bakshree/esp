// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#include "tiled_app.hpp"
#include "tiled_app_directives.hpp"

// Functions

#include "tiled_app_functions.hpp"

// Processes

void tiled_app::load_input()
{

    // Reset
    {
        HLS_PROTO("load-reset");

        this->reset_load_input();

        wait();
    }

    // Config
    /* <<--params-->> */
    int32_t num_tiles;
    int32_t tile_size;
    int32_t rd_wr_enable;
    {
        HLS_PROTO("load-config");

        cfg.wait_for_config(); // config process
        conf_info_t config = this->conf_info.read();

        // User-defined config code
        /* <<--local-params-->> */
        num_tiles = config.num_tiles;
        tile_size = config.tile_size;
        rd_wr_enable = config.rd_wr_enable;
    }

   
    // Load
    {
        //if(rd_wr_enable == 0){
            HLS_PROTO("load-dma");
            wait();

            //bool ping = true;
            uint32_t offset = 0;
            uint32_t sp_offset = 0;

            // Batching
            for (uint16_t b = 0; b < num_tiles; b++)
            {   
                // if(rd_wr_enable != 0){
                //     while(rd_wr_enable) wait(); // wait for read enable
                // }
                // else
                {   // Read synchronizer   
                    uint64_t data;
                    uint64_t sync_offset = 12*1024;
                    dma_info_t dma_info(sync_offset * DMA_BEAT_PER_WORD, DMA_BEAT_PER_WORD, DMA_SIZE);

                    //Read from DMA
                    this->dma_read_ctrl.put(dma_info);
                    sc_dt::sc_bv<DMA_WIDTH> dataBvin;

                    //Wait for 0
                    do {
                    dataBvin.range(DMA_WIDTH - 1, 0) = this->dma_read_chnl.get();
                    wait();
                    data = dataBvin.range(DMA_WIDTH - 1, 0).to_int64();
                    }
                    while(data==0);

                    //Write to DMA
                    this->dma_write_ctrl.put(dma_info);
                    sc_dt::sc_bv<DMA_WIDTH> dataBvout;
                    wait();
                    //write 1
                    dataBvout.range(DMA_WIDTH - 1, 0) = 1;
                    this->dma_write_chnl.put(dataBvout);
                }  
                wait();
    #if (DMA_WORD_PER_BEAT == 0)
                uint32_t length = tile_size;
    #else
                uint32_t length = round_up(tile_size, DMA_WORD_PER_BEAT);
    #endif
                // Chunking
                for (int rem = length; rem > 0; rem -= PLM_IN_WORD)
                {
                    wait();
                    // Configure DMA transaction
                    uint32_t len = rem > PLM_IN_WORD ? PLM_IN_WORD : rem;
    #if (DMA_WORD_PER_BEAT == 0)
                    // data word is wider than NoC links
                    dma_info_t dma_info(offset * DMA_BEAT_PER_WORD, len * DMA_BEAT_PER_WORD, DMA_SIZE);
    #else
                    dma_info_t dma_info(offset / DMA_WORD_PER_BEAT, len / DMA_WORD_PER_BEAT, DMA_SIZE);
    #endif
                    offset += len;

                    this->dma_read_ctrl.put(dma_info);

    #if (DMA_WORD_PER_BEAT == 0)
                    // data word is wider than NoC links
                    for (uint16_t i = 0; i < len; i++)
                    {
                        sc_dt::sc_bv<DATA_WIDTH> dataBv;

                        for (uint16_t k = 0; k < DMA_BEAT_PER_WORD; k++)
                        {
                            dataBv.range((k+1) * DMA_WIDTH - 1, k * DMA_WIDTH) = this->dma_read_chnl.get();
                            wait();
                        }

                        // Write to PLM
                        plm[sp_offset + i] = dataBv.to_int64();
                        // if (ping)
                        //     plm_in_ping[i] = dataBv.to_int64();
                        // else
                        //     plm_in_pong[i] = dataBv.to_int64();
                    }
    #else
                    for (uint16_t i = 0; i < len; i += DMA_WORD_PER_BEAT)
                    {
                        // HLS_BREAK_DEP(plm_in_ping);
                        // HLS_BREAK_DEP(plm_in_pong);
                        HLS_BREAK_DEP(plm);

                        sc_dt::sc_bv<DMA_WIDTH> dataBv;

                        dataBv = this->dma_read_chnl.get();
                        wait();

                        // Write to PLM (all DMA_WORD_PER_BEAT words in one cycle)
                        for (uint16_t k = 0; k < DMA_WORD_PER_BEAT; k++)
                        {
                            HLS_UNROLL_SIMPLE;
                            plm[sp_offset + i + k] = dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH).to_int64();
                            // if (ping)
                            //     plm_in_ping[i + k] = dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH).to_int64();
                            // else
                            //     plm_in_pong[i + k] = dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH).to_int64();
                        }
                    }
    #endif
                }
                //this->load_compute_handshake();
                this->load_store_handshake();
                sp_offset += length;
                //ping = !ping;
            }
        //}
    }

    // Conclude
    {
        this->process_done();
    }
}



void tiled_app::store_output()
{
    // Reset
    {
        HLS_PROTO("store-reset");

        this->reset_store_output();

        // explicit PLM ports reset if any

        // User-defined reset code

        wait();
    }

    // Config
    /* <<--params-->> */
    int32_t num_tiles;
    int32_t tile_size;
    int32_t rd_wr_enable;
    {
        HLS_PROTO("store-config");

        cfg.wait_for_config(); // config process
        conf_info_t config = this->conf_info.read();

        // User-defined config code
        /* <<--local-params-->> */
        num_tiles = config.num_tiles;
        tile_size = config.tile_size;
        rd_wr_enable = config.rd_wr_enable;
    }

    // Store
    {// Store - only if rd_wr_enable is 1
        //if(rd_wr_enable == 1){
            HLS_PROTO("store-dma");
            wait();

            // bool ping = true;
    #if (DMA_WORD_PER_BEAT == 0)
            uint32_t store_offset = (tile_size) * num_tiles;
    #else
            uint32_t store_offset = round_up(tile_size, DMA_WORD_PER_BEAT) * num_tiles;
    #endif
            uint32_t offset = store_offset;
            uint32_t sp_offset = 0;

            wait();
            // Batching
            for (uint16_t b = 0; b < num_tiles; b++)
            {
                wait();
                //
                // this->store_compute_handshake();
                { //STORE SYNCHRONIZATION
                    uint64_t sync_offset = 12*1024;
                    dma_info_t dma_info(sync_offset * DMA_BEAT_PER_WORD, DMA_BEAT_PER_WORD, DMA_SIZE);
                    uint64_t data;
                    //Read from DMA
                    this->dma_read_ctrl.put(dma_info);
                    sc_dt::sc_bv<DMA_WIDTH> dataBvin;
                        //Wait for 0
                    do {
                    dataBvin.range(DMA_WIDTH - 1, 0) = this->dma_read_chnl.get();
                    wait();
                    data = dataBvin.range(DMA_WIDTH - 1, 0).to_int64();
                    }
                    while(data==0);
                        //Write to DMA
                    this->dma_write_ctrl.put(dma_info);
                    sc_dt::sc_bv<DMA_WIDTH> dataBvout;
                    wait();
                    //write 1
                    dataBvout.range(DMA_WIDTH - 1, 0) = 1;
                    this->dma_write_chnl.put(dataBvout);
                    // compute == synchronizer    
                    //send the output ready ack
                }    
                this->store_load_handshake();
                //send the output ready req
                //this->compute_store_handshake();

    #if (DMA_WORD_PER_BEAT == 0)
                uint32_t length = tile_size;
    #else
                uint32_t length = round_up(tile_size, DMA_WORD_PER_BEAT);
    #endif
                // Chunking
                for (int rem = length; rem > 0; rem -= PLM_OUT_WORD)
                {

                    //this->store_compute_handshake();

                    // Configure DMA transaction
                    uint32_t len = rem > PLM_OUT_WORD ? PLM_OUT_WORD : rem;
    #if (DMA_WORD_PER_BEAT == 0)
                    // data word is wider than NoC links
                    dma_info_t dma_info(offset * DMA_BEAT_PER_WORD, len * DMA_BEAT_PER_WORD, DMA_SIZE);
    #else
                    dma_info_t dma_info(offset / DMA_WORD_PER_BEAT, len / DMA_WORD_PER_BEAT, DMA_SIZE);
    #endif
                    offset += len;

                    this->dma_write_ctrl.put(dma_info);

    #if (DMA_WORD_PER_BEAT == 0)
                    // data word is wider than NoC links
                    for (uint16_t i = 0; i < len; i++)
                    {
                        // Read from PLM
                        sc_dt::sc_int<DATA_WIDTH> data;
                        wait();
                        data = plm[i];
                        // if (ping)
                        //     data = plm_out_ping[i];
                        // else
                        //     data = plm_out_pong[i];
                        sc_dt::sc_bv<DATA_WIDTH> dataBv(data);

                        uint16_t k = 0;
                        for (k = 0; k < DMA_BEAT_PER_WORD - 1; k++)
                        {
                            this->dma_write_chnl.put(dataBv.range((k+1) * DMA_WIDTH - 1, k * DMA_WIDTH));
                            wait();
                        }
                        // Last beat on the bus does not require wait(), which is
                        // placed before accessing the PLM
                        this->dma_write_chnl.put(dataBv.range((k+1) * DMA_WIDTH - 1, k * DMA_WIDTH));
                    }
    #else
                    for (uint16_t i = 0; i < len; i += DMA_WORD_PER_BEAT)
                    {
                        sc_dt::sc_bv<DMA_WIDTH> dataBv;

                        // Read from PLM
                        wait();
                        for (uint16_t k = 0; k < DMA_WORD_PER_BEAT; k++)
                        {
                            HLS_UNROLL_SIMPLE;
                            dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH) = plm[sp_offset + i + k];
                            // if (ping)
                            //     dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH) = plm_out_ping[i + k];
                            // else
                            //     dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH) = plm_out_pong[i + k];
                        }
                        this->dma_write_chnl.put(dataBv);
                    }
    #endif
                    //ping = !ping;
                }

                sp_offset += length;
            }
        //}
    }

    // Conclude
    {
        this->accelerator_done();
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

        this->reset_compute_kernel();

        wait();
    }

    // Config
    /* <<--params-->> */
    //int32_t num_tiles;
    //int32_t tile_size;
    //int32_t rd_wr_enable;
    //uint64_t sync_offset = 12*1024;
    //dma_info_t dma_info(sync_offset * DMA_BEAT_PER_WORD, DMA_BEAT_PER_WORD, DMA_SIZE);

    // {
    //     HLS_PROTO("compute-config");

    //     cfg.wait_for_config(); // config process
    //     conf_info_t config = this->conf_info.read();

    //     // User-defined config code
    //     /* <<--local-params-->> */
    //     //num_tiles = config.num_tiles;
    //     //tile_size = config.tile_size;
    //     rd_wr_enable = config.rd_wr_enable;
    // }

    //calc sync var loc offset
    //read modify write
    // {
    //     //read flow
    //     if(rd_wr_enable == 0){

    //         // compute == synchronizer    
    //         this->compute_load_handshake();
    //         uint64_t data;
    //         //dma_info_t dma_info(sync_offset * DMA_BEAT_PER_WORD, DMA_BEAT_PER_WORD, DMA_SIZE);

    //         //Read from DMA
    //         this->dma_read_ctrl.put(dma_info);
    //         sc_dt::sc_bv<DMA_WIDTH> dataBvin;

    //         //Wait for 0
    //         do {
    //         dataBvin.range(DMA_WIDTH - 1, 0) = this->dma_read_chnl.get();
    //         wait();
    //         data = dataBvin.range(DMA_WIDTH - 1, 0).to_int64();
    //         }
    //         while(data==0);

    //         //Write to DMA
    //         this->dma_write_ctrl.put(dma_info);
    //         sc_dt::sc_bv<DMA_WIDTH> dataBvout;
    //         wait();
    //         //write 1
    //         dataBvout.range(DMA_WIDTH - 1, 0) = 1;
    //         this->dma_write_chnl.put(dataBvout);
    //     }         
    // }


    // {
    //     //write flow
    //     if(rd_wr_enable == 1){
            
    //         uint64_t data;

           
    //         //Read from DMA
    //         this->dma_read_ctrl.put(dma_info);
    //         sc_dt::sc_bv<DMA_WIDTH> dataBvin;

    //         //Wait for 0
    //         do {
    //         dataBvin.range(DMA_WIDTH - 1, 0) = this->dma_read_chnl.get();
    //         wait();
    //         data = dataBvin.range(DMA_WIDTH - 1, 0).to_int64();
    //         }
    //         while(data==0);

    //         //Write to DMA
    //         this->dma_write_ctrl.put(dma_info);
    //         sc_dt::sc_bv<DMA_WIDTH> dataBvout;
    //         wait();
    //         //write 1
    //         dataBvout.range(DMA_WIDTH - 1, 0) = 1;
    //         this->dma_write_chnl.put(dataBvout);
    //         // compute == synchronizer    
    //         //send the output ready ack
    //         this->store_compute_handshake();
    //     }         
    // }

    // // Compute
    // bool ping = true;
    // {
    //     for (uint16_t b = 0; b < num_tiles; b++)
    //     {
    //         uint32_t in_length = tile_size;
    //         uint32_t out_length = tile_size;
    //         int out_rem = out_length;

    //         for (int in_rem = in_length; in_rem > 0; in_rem -= PLM_IN_WORD)
    //         {

    //             uint32_t in_len  = in_rem  > PLM_IN_WORD  ? PLM_IN_WORD  : in_rem;
    //             uint32_t out_len = out_rem > PLM_OUT_WORD ? PLM_OUT_WORD : out_rem;

    //             this->compute_load_handshake();

    //             // Computing phase implementation
    //             for (int i = 0; i < in_len; i++) {
    //                 if (ping)
    //                     plm_out_ping[i] = plm_in_ping[i];
    //                 else
    //                     plm_out_pong[i] = plm_in_pong[i];
    //             }

    //             out_rem -= PLM_OUT_WORD;
    //             this->compute_store_handshake();
    //             ping = !ping;
    //         }
    //     }

        // Conclude
        {
            this->process_done();
        }
    // }
}
