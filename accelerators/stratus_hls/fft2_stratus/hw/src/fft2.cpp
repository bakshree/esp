// Copyright (c) 2011-2019 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#include "fft2.hpp"
#include "fft2_directives.hpp"

// Functions

#include "fft2_functions.hpp"

// Processes

void fft2::load_input()
{

    // Reset
    {
        HLS_PROTO("load-reset");

        this->reset_load_input();
        this->reset_load_to_store();

        // explicit PLM ports reset if any

        load_state_dbg.write(0);
        // User-defined reset code

        load_next_tile.ack.reset_ack();

        wait();
    }

    // Config
    /* <<--params-->> */
    //bmishra3

    int32_t input_tile_start_offset;
    int32_t output_spin_sync_offset  ;
    int32_t input_spin_sync_offset   ;
    int32_t num_tiles;
    int32_t tile_size;

    //int32_t scale_factor;
    //int32_t do_inverse;
    int32_t logn_samples;
    int32_t num_samples;
    //int32_t do_shift;
    int32_t num_ffts;
    {
        HLS_PROTO("load-config");

        cfg.wait_for_config(); // config process
        conf_info_t config = this->conf_info.read();

        // User-defined config code
        /* <<--local-params-->> */
        logn_samples = config.logn_samples;
        num_samples = 1 << logn_samples;
        num_ffts = config.num_ffts;
        //do_inverse = config.do_inverse;
        //do_shift = config.do_shift;
        //scale_factor = config.scale_factor;
        
        //bmishra3
        output_spin_sync_offset = config.output_spin_sync_offset  ;
        input_spin_sync_offset  = config.input_spin_sync_offset   ;
        num_tiles = config.num_tiles;
        tile_size = config.tile_size;
        input_tile_start_offset = config.input_tile_start_offset;
    }

    // Load
    int loads_done = 0;
    {
        HLS_PROTO("load-dma");
        // uint32_t offset = 0;

        wait();
        int64_t load_state = LOAD_STATE_WAIT_FOR_INPUT_SYNC;
        bool ping = true;
        int32_t curr_tile = 0;
        uint32_t offset = input_tile_start_offset; //64 + tile_no*tile_size; //SYNC_BITS; //0;
        uint32_t sp_offset = 0;

        while(true){
            HLS_UNROLL_LOOP(OFF);
            load_state_dbg.write(load_state);
            switch(load_state){
                case LOAD_STATE_WAIT_FOR_INPUT_SYNC: {
                    // printf("State: LOAD_STATE_WAIT_FOR_INPUT_SYNC\n ");
                    int64_t data = 0;
                    sc_dt::sc_bv<DMA_WIDTH> dataBvin;
                    dma_info_t dma_info2(input_spin_sync_offset, 1, DMA_SIZE);
                    this->dma_read_ctrl.put(dma_info2);
                    wait();
                    dataBvin.range(DMA_WIDTH - 1, 0) = this->dma_read_chnl.get();
                    wait();
                    data = dataBvin.range(DMA_WIDTH - 1, 0).to_int64();
                    wait();
                    if(data == 1) load_state = LOAD_STATE_READ_DMA; //LOAD_STATE_INIT_DMA;
                    else load_state = LOAD_STATE_WAIT_FOR_INPUT_SYNC;
                }
                break;
        
                // case LOAD_STATE_INIT_DMA : {
                //     uint32_t len = length > PLM_IN_WORD ? PLM_IN_WORD : length;
                //     dma_info_t dma_info(offset / DMA_WORD_PER_BEAT, len / DMA_WORD_PER_BEAT, DMA_SIZE);
                //     this->dma_read_ctrl.put(dma_info);
                //     load_state = LOAD_STATE_READ_DMA;
                // }
                // break;
                case LOAD_STATE_READ_DMA: {
                    // printf("State: LOAD_STATE_READ_DMA\n ");
        //ORIGINAL DMA READ CODE
#if (DMA_WORD_PER_BEAT == 0)
        uint32_t length = 2 * num_ffts * num_samples;
#else
        uint32_t length = round_up(2 * num_ffts * num_samples, DMA_WORD_PER_BEAT);
#endif
        // Chunking
        int rem = length;
        // for (int rem = length; rem > 0; rem -= PLM_IN_WORD)
        // {
            wait();
            // Configure DMA transaction
            uint32_t len = rem > PLM_IN_WORD ? PLM_IN_WORD : rem;
#if (DMA_WORD_PER_BEAT == 0)
            // data word is wider than NoC links
            dma_info_t dma_info(offset * DMA_BEAT_PER_WORD, len * DMA_BEAT_PER_WORD, DMA_SIZE);
#else
            // printf("LOAD DMA INFO : rem %u : off = %u , len %u\n", rem, (offset/DMA_WORD_PER_BEAT), (len/DMA_WORD_PER_BEAT));
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
                A0[i] = dataBv.to_int64();
            }
#else
            for (uint16_t i = 0; i < len; i += DMA_WORD_PER_BEAT)
            {
                HLS_BREAK_DEP(A0);

                sc_dt::sc_bv<DMA_WIDTH> dataBv;

                dataBv = this->dma_read_chnl.get();
                wait();

                // Write to PLM (all DMA_WORD_PER_BEAT words in one cycle)
                for (uint16_t k = 0; k < DMA_WORD_PER_BEAT; k++)
                {
                    HLS_UNROLL_SIMPLE;
                    A0[i + k] = dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH).to_int64();
                }
            }
#endif

            //ORIGINAL DMA READ CODE ENDS
                    load_state = LOAD_STATE_STORE_SYNC;
                }
                break;
                case LOAD_STATE_STORE_SYNC: {
                    // printf("State: LOAD_STATE_STORE_SYNC\n ");
                    int64_t data = 0;
                    sc_dt::sc_bv<DMA_WIDTH> dataBvin;
                    dma_info_t dma_info2(output_spin_sync_offset, 1, DMA_SIZE); //sync_offset+1
                    this->dma_read_ctrl.put(dma_info2);
                    wait();
                    dataBvin.range(DMA_WIDTH - 1, 0) = this->dma_read_chnl.get();
                    wait();
                    data = dataBvin.range(DMA_WIDTH - 1, 0).to_int64();
                    wait();
                    if(data == 1) load_state = LOAD_STATE_STORE_SYNC; //LOAD_STATE_INIT_DMA;
                    else load_state = LOAD_STATE_STORE_HANDSHAKE;
                }
                break;
                case LOAD_STATE_STORE_HANDSHAKE: {
                    // printf("State: LOAD_STATE_STORE_HANDSHAKE\n ");
                    // printf("LOAD hit the load-compute handshake...\n");
                    this->load_compute_handshake();
                    wait();
                    // this->store_compute_handshake();

                    this->load_next_tile_ack();
                    wait();
                    curr_tile++;
                    if(curr_tile == num_tiles) load_state = LOAD_STATE_PROC_DONE;
                    else load_state = LOAD_STATE_WAIT_FOR_INPUT_SYNC;
                    ping = !ping;
                }
                break;
                case LOAD_STATE_PROC_DONE:{
                    // printf("State: LOAD_STATE_PROC_DONE\n ");
                    this->process_done();
                }
                break;

                default: {
                    break;
                }

            }
            wait();
        }
	}
    //         printf("LOAD hit the load-compute handshake...\n");
    //         this->load_compute_handshake();
    //         this->load_to_store_handshake();
    //         loads_done++;





    //     } // for (rem = length downto 0 )
    // } // Load scope

    // //printf("LOAD process is done -- did %u loads\n", loads_done);
    // // Conclude
    // {
    //     this->process_done();
    // }
}



void fft2::store_output()
{
    // Reset
    {
        HLS_PROTO("store-reset");

        this->reset_store_output();
        this->reset_store_to_load();

        store_state_dbg.write(0);
        load_sync_done.req.reset_req();
        store_sync_done.req.reset_req();
        // explicit PLM ports reset if any

        // User-defined reset code

        wait();
    }

    // Config
    /* <<--params-->> */
    int32_t logn_samples;
    int32_t num_samples;
    int32_t num_ffts;
    //int32_t scale_factor;
    //int32_t do_inverse;
    //int32_t do_shift;
    //bmishra3
    int32_t output_tile_start_offset;
    int32_t output_update_sync_offset;
    int32_t input_update_sync_offset ;
    int32_t num_tiles;
    int32_t tile_size;
    {
        HLS_PROTO("store-config");

        cfg.wait_for_config(); // config process
        conf_info_t config = this->conf_info.read();

        // User-defined config code
        /* <<--local-params-->> */
        logn_samples = config.logn_samples;
        num_samples = 1 << logn_samples;
        num_ffts = config.num_ffts;
        //bmishra3
        output_tile_start_offset = config.output_tile_start_offset;
        output_update_sync_offset = config.output_update_sync_offset;
        input_update_sync_offset  = config.input_update_sync_offset ;
        num_tiles = config.num_tiles;
        tile_size = config.tile_size;
        //do_inverse = config.do_inverse;
        //do_shift = config.do_shift;
        //scale_factor = config.scale_factor;
    }

    // Store
    int stores_done = 0;
    {
        HLS_PROTO("store-dma");
        bool ping = true;
        int32_t curr_tile = 0;        
        uint32_t offset = output_tile_start_offset;
        wait();

        uint32_t store_state = STORE_STATE_WAIT_FOR_HANDSHAKE;

        while(true){
            HLS_UNROLL_LOOP(OFF);
            store_state_dbg.write(store_state);
            // store_state_dbg.write(store_state);
            // store_iter_dbg.write(curr_tile);
            switch(store_state){
                case STORE_STATE_WAIT_FOR_HANDSHAKE:{
                    this->store_sync_done_req();
                    wait();

                    // this->compute_load_handshake();
                    this->store_compute_handshake();
                    wait();
                    store_state = STORE_STATE_LOAD_SYNC;
                    // store_state = STORE_STATE_DMA_SEND;
                }
                break;
                case STORE_STATE_LOAD_SYNC: {
                    sc_dt::sc_bv<DMA_WIDTH> dataBvSync;
                    dataBvSync = 0;
                    dma_info_t dma_info_sync(input_update_sync_offset, 1, DMA_SIZE);//tile_no sync location
                    this->dma_write_ctrl.put(dma_info_sync);
                    wait();
                    this->dma_write_chnl.put(dataBvSync);
                    wait();
                    // store_unit_sp_read_dbg.write(0);
                    // wait();
                    while (!(this->dma_write_chnl.ready)) wait();
                    // // Block till L2 to be ready to receive a fence, then send
                    // this->acc_fence.put(0x2);
                    // wait();
                    // while (!(this->acc_fence.ready)) wait();
                    // wait();
                    store_state = STORE_STATE_LOAD_FENCE;
                }
                break;


                case STORE_STATE_LOAD_FENCE:{
                    this->acc_fence.put(0x2);
                    wait();
                    while (!(this->acc_fence.ready)) wait();
                    wait();
                    // this->compute_store_handshake();
		            // wait();
                    this->load_sync_done_req();
                    store_state = STORE_STATE_DMA_SEND;
                }
                break;

                // case STORE_STATE_LOAD_FENCE_RDY:{
                //     if(!(this->acc_fence.ready)){ 
                //         wait();
                //         store_state = STORE_STATE_LOAD_FENCE_RDY; 
                //     }
                //     else store_state = STORE_STATE_DMA_SEND; 
                // }
                // break;

                case STORE_STATE_DMA_SEND: {

                    //original store code
                    #if (DMA_WORD_PER_BEAT == 0)
                            uint32_t store_offset = (2 * num_samples);
                    #else
                            uint32_t store_offset = round_up(2 * num_samples, DMA_WORD_PER_BEAT);
                    #endif
                            // uint32_t offset = 0;

                            wait();
                    #if (DMA_WORD_PER_BEAT == 0)
                            uint32_t length = 2 * num_ffts * num_samples;
                    #else
                            uint32_t length = round_up(2 * num_ffts * num_samples, DMA_WORD_PER_BEAT);
                    #endif
                            // Chunking
                            int rem = length;
                            // for (int rem = length; rem > 0; rem -= PLM_OUT_WORD)
                            // {
                                // printf("STORE hit the store-compute handshake...\n");
                                // this->store_compute_handshake();

                                // Configure DMA transaction
                                uint32_t len = rem > PLM_OUT_WORD ? PLM_OUT_WORD : rem;
                    #if (DMA_WORD_PER_BEAT == 0)
                                // data word is wider than NoC links
                                dma_info_t dma_info(offset * DMA_BEAT_PER_WORD, len * DMA_BEAT_PER_WORD, DMA_SIZE);
                    #else
                                // printf("STORE DMA INFO : rem %u : off = %u , len = %u\n", rem, (offset/DMA_WORD_PER_BEAT), (len/DMA_WORD_PER_BEAT));
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
                                    data = A0[i];
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
                                        dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH) = A0[i + k];
                                    }
                                    this->dma_write_chnl.put(dataBv);
                                }
                    #endif

            //End of original Store
                   
                    // store_state = STORE_STATE_LOAD_HANDSHAKE;
                    store_state = STORE_STATE_SYNC;
                } 
                break;

                case STORE_STATE_SYNC: {
                
                    sc_dt::sc_bv<DMA_WIDTH> dataBvSync;
                    dataBvSync = 1;
                    dma_info_t dma_info_sync(output_update_sync_offset, 1, DMA_SIZE);//tile_no+1 next sync location
                    wait();
                    this->dma_write_ctrl.put(dma_info_sync);
                    wait();
                    this->dma_write_chnl.put(dataBvSync);
                    wait();
                    // store_unit_sp_read_dbg.write(1);
                    wait();
                    while(!(this->dma_write_chnl.ready)) wait(); 
                    store_state = STORE_STATE_STORE_FENCE;
                }
                break;

                case STORE_STATE_STORE_FENCE:{
                    // if(!(this->dma_write_chnl.ready)){ wait();

                    //     store_state = STORE_STATE_STORE_FENCE_DMA_CHL_RDY; }
                    // else {
                        // Block till L2 to be ready to receive a fence, then send
                        this->acc_fence.put(0x2);
                        wait();
                        while(!(this->acc_fence.ready)) wait(); 
                        // store_state = STORE_STATE_LOAD_HANDSHAKE;
                    // }

                    curr_tile++;
                    ping = !ping;
                    if(curr_tile == num_tiles){
                        store_state = STORE_STATE_PROC_ACC_DONE;
                    }
                    else store_state = STORE_STATE_WAIT_FOR_HANDSHAKE;
                }
                break;

                // case STORE_STATE_STORE_FENCE_RDY:{
                //     if(!(this->acc_fence.ready)){ 
                //         wait();
                //         store_state = STORE_STATE_STORE_FENCE_RDY; }
                //     else store_state = STORE_STATE_LOAD_HANDSHAKE; 
                // }
                // break;

                // case STORE_STATE_LOAD_HANDSHAKE:{
                //     curr_tile++;
                //     ping = !ping;
                //     if(curr_tile == num_tiles){
                //         store_state = STORE_STATE_PROC_ACC_DONE;
                //     }
                //     else store_state = STORE_STATE_WAIT_FOR_HANDSHAKE;
                // }
                // break;
                case STORE_STATE_PROC_ACC_DONE: {
                    this->accelerator_done();
                    this->process_done();
                }break;
                default: {
                    break;
                }
                
            }
            wait();
        }
    }

    //         stores_done++;
    //         //rem = 0;
    //         this->store_to_load_handshake();
    //     } // for (rem = length downto 0
    // } // Store scope
    // //printf("STORE process is done - did %u stores!\n", stores_done);
    // // Conclude
    // {
    //     this->accelerator_done();
    //     this->process_done();
    // }
}


void fft2::compute_kernel()
{
    // Reset
    {
        HLS_PROTO("compute-reset");

        this->reset_compute_kernel();

        load_next_tile.req.reset_req();
        load_sync_done.ack.reset_ack();
        store_sync_done.ack.reset_ack();
        // explicit PLM ports reset if any

        // User-defined reset code

        wait();
    }

    // Config
    /* <<--params-->> */
    int32_t logn_samples;
    int32_t num_samples;
    int32_t num_ffts;
    int32_t do_inverse;
    int32_t do_shift;
    //bmishra3
    int32_t num_tiles;
    //int32_t scale_factor;
    {
        HLS_PROTO("compute-config");

        cfg.wait_for_config(); // config process
        conf_info_t config = this->conf_info.read();

        // User-defined config code
        /* <<--local-params-->> */
        logn_samples = config.logn_samples;
#ifndef STRATUS_HLS
        sc_assert(logn_samples < MAX_LOGN_SAMPLES);
#endif
        num_samples = 1 << logn_samples;
        num_ffts = config.num_ffts;
        do_inverse = config.do_inverse;
        do_shift = config.do_shift;
        //scale_factor = config.scale_factor;
        //bmishra3
        num_tiles = config.num_tiles;
    }
    //printf("COMPUTE: logn_samples %u num_samples %u num_ffts %u inverse %u shift %u\n", logn_samples, num_samples, num_ffts, do_inverse, do_shift);

    // Compute
    // Loop through the num_ffts successive FFT computations
    {
        //bmishra3
        // HLS_PROTO("compute-block");
        uint32_t in_length  = 2 * num_ffts * num_samples;
        uint32_t out_length = in_length;
        uint32_t out_rem = out_length;
        unsigned max_in_ffts = 1 << (MAX_LOGN_SAMPLES - logn_samples);
        unsigned ffts_done = 0;
        // printf("COMPUTE: in_len %u : max_in_ffts %u >> %u = %u\n", in_length, MAX_LOGN_SAMPLES, logn_samples, max_in_ffts); 

        //bmishra3
        for(int32_t b = 0; b < num_tiles; b++){

            this->compute_load_handshake(); // Ack new input tile
            wait();
        //original compute code

        // Chunking : Load/Store Memory transfers (refill memory)
            int in_rem = in_length;
        // for (int in_rem = in_length; in_rem > 0; in_rem -= PLM_IN_WORD)
        // {
            uint32_t in_len  = in_rem  > PLM_IN_WORD  ? PLM_IN_WORD  : in_rem;
            uint32_t out_len = out_rem > PLM_OUT_WORD ? PLM_OUT_WORD : out_rem;

            // Compute FFT(s) in the PLM
            // printf("COMPUTE INFO : in_rem %u : in_len %u :: out_rem %u : out_len %u\n", in_rem, in_len, out_rem, out_len);
            // printf("COMPUTE hit the compute-load handshake...\n");
            // this->compute_load_handshake();
            unsigned rem_ffts = (num_ffts - ffts_done); 
            unsigned in_ffts =  (rem_ffts > max_in_ffts) ? max_in_ffts : rem_ffts;
            // printf("COMPUTE has %u rem_ffts : proceeding to the next %u FFT computations...\n", rem_ffts, in_ffts);
            for (unsigned fftn = 0; fftn < in_ffts; fftn++) {
                unsigned offset = fftn * num_samples;  // Offset into Mem for start of this FFT
                // printf("COMPUTE: starting FFT %u of %u = %u : offset = %u\n", fftn, in_ffts, ffts_done, offset);
                int sin_sign = (do_inverse) ? -1 : 1; // This modifes the mySin
                                                      // values used below
                if (do_inverse && do_shift) {
                    //printf("ACCEL: Calling Inverse-Do-Shift\n");
                    fft2_do_shift(offset, num_samples, logn_samples);
                }

                // Do the bit-reverse
                fft2_bit_reverse(offset, num_samples, logn_samples);

                // Computing phase implementation
                int m = 1;  // iterative FFT

            FFT2_SINGLE_L1:
                for(unsigned s = 1; s <= logn_samples; s++) {
                    //printf(" L1 : FFT %u = %u : s %u\n", fftn, ffts_done, s);
                    m = 1 << s;
                    CompNum wm(myCos(s), sin_sign*mySin(s));
                    // printf("s: %d\n", s);
                    // printf("wm.re: %.15g, wm.im: %.15g\n", wm.re, wm.im);

                FFT2_SINGLE_L2:
                    for(unsigned k = 0; k < num_samples; k +=m) {
                        // if (k < 2) {
                        //     printf("  L2 : FFT %u = %u : s %u : k %u\n", fftn, ffts_done, s, k);
                        // }

                        CompNum w((FPDATA) 1, (FPDATA) 0);
                        int md2 = m / 2;

                    FFT2_SINGLE_L3:
                        for(int j = 0; j < md2; j++) {

                            int kj = offset + k + j;
                            int kjm = offset + k + j + md2;
                            // if ((k == 0) && (j == 0)) {
                            //     printf("  L3 : FFT %u = %u : k %u j %u md2 %u : kj %u kjm %u : kji %u kjmi %u\n", fftn, ffts_done, k, j, md2, kj, kjm, 2*kj, 2*kjm);
                            // }
                            CompNum akj, akjm;
                            CompNum bkj, bkjm;

                            akj.re = int2fp<FPDATA, WORD_SIZE>(A0[2 * kj]);
                            akj.im = int2fp<FPDATA, WORD_SIZE>(A0[2 * kj + 1]);
                            akjm.re = int2fp<FPDATA, WORD_SIZE>(A0[2 * kjm]);
                            akjm.im = int2fp<FPDATA, WORD_SIZE>(A0[2 * kjm + 1]);

                            CompNum t;
                            compMul(w, akjm, t);
                            CompNum u(akj.re, akj.im);
                            compAdd(u, t, bkj);
                            compSub(u, t, bkjm);
                            CompNum wwm;
                            wwm.re = w.re - (wm.im * w.im + wm.re * w.re);
                            wwm.im = w.im + (wm.im * w.re - wm.re * w.im);
                            w = wwm;

                            {
                                HLS_PROTO("compute_write_A0");
                                HLS_BREAK_DEP(A0);
                                wait();
                                A0[2 * kj] = fp2int<FPDATA, WORD_SIZE>(bkj.re);
                                A0[2 * kj + 1] = fp2int<FPDATA, WORD_SIZE>(bkj.im);
                                wait();
                                A0[2 * kjm] = fp2int<FPDATA, WORD_SIZE>(bkjm.re);
                                A0[2 * kjm + 1] = fp2int<FPDATA, WORD_SIZE>(bkjm.im);
                                // cout << "DFT: A0 " << kj << ": " << A0[kj].re.to_hex() << " " << A0[kj].im.to_hex() << endl;
                                // cout << "DFT: A0 " << kjm << ": " << A0[kjm].re.to_hex() << " " << A0[kjm].im.to_hex() << endl;
                                // if ((k == 0) && (j == 0)) {
                                //         printf("  L3 : WROTE A0 %u and %u and %u and %u\n", 2*kj, 2*kj + 1, 2*kjm, 2*kjm + 1);
                                // }
                            }
                        } // for (j = 0 .. md2)
                    } // for (k = 0 .. num_samples)
                } // for (s = 1 .. logn_samples)

                if ((!do_inverse) && (do_shift)) {
                    //printf("ACCEL: Calling Non-Inverse Do-Shift\n");
                    fft2_do_shift(offset, num_samples, logn_samples);
                }
                //printf("COMPUTE: done with FF %u = %u\n", fftn, ffts_done);
                /*cout << "ACCEL-END : FFT " << ffts_done << " : A0[0] = " << A0[0].to_double() << endl;
                  cout << "ACCEL-END : FFT " << ffts_done << " : A0[1] = " << A0[1].to_double() << endl;
                  cout << "ACCEL-END : FFT " << ffts_done << " : A0[2] = " << A0[2].to_double() << endl;
                  cout << "ACCEL-END : FFT " << ffts_done << " : A0[3] = " << A0[3].to_double() << endl;*/
                ffts_done++;
                offset += num_samples;  // Offset into Mem for start of this FFT
            } // for( n = 0 .. mnum_ffts)
            out_rem -= PLM_OUT_WORD;
            //original compute code ends

            this->store_sync_done_ack(); //Block till Store has finished writing previous data over DMA
            wait();
            this->compute_store_handshake(); //Non blocking signal to Store to resume
            wait();
            this->load_sync_done_ack();  //Block till Store has updated sync bit for Read tile
            wait();
            this->load_next_tile_req();  //Enable next iteration of input data tile
            wait();
        }

    }
        //     printf("COMPUTE hit the compute-store handshake...\n");
        //     this->compute_store_handshake();
        // // } // for (in_rem = in_length downto 0)

        // Conclude
        {
            this->process_done();
        }
    // } // Compute scope
} // Function : compute_kernel
