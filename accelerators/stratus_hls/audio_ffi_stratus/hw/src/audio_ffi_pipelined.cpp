// Copyright (c) 2011-2019 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#include "audio_ffi_pipelined.hpp"
#include "audio_ffi_directives.hpp"

// Functions

#include "audio_ffi_pipelined_functions.hpp"

// Processes

void audio_ffi::load_input()
{
    // Reset
    {
        HLS_PROTO("load-reset");

        this->reset_load_input();

        load_state_req_dbg.write(0);

        input_load_start.ack.reset_ack();
        filters_load_start.ack.reset_ack();
        output_load_start.ack.reset_ack();
        load_input_done.req.reset_req();
        load_filters_done.req.reset_req();
        load_output_done.req.reset_req();

        prod_valid = 0;
        flt_valid = 0;
        cons_ready = 0;
        load_state_req = 0;
        load_state_req_module = 0;

        end_input_asi = 0;

        wait();
    }

    // Config
    /* <<--params-->> */
    int32_t logn_samples;
    int32_t num_samples;
    int32_t prod_valid_offset;
    int32_t prod_ready_offset;
    int32_t flt_prod_valid_offset;
    int32_t flt_prod_ready_offset;
    int32_t cons_valid_offset;
    int32_t cons_ready_offset;
    int32_t input_offset;
    int32_t flt_input_offset;
    int32_t twd_input_offset;
    int32_t output_offset;
    bool pingpong;
    bool flt_pingpong;
    int32_t task_arbiter;
    {
        HLS_PROTO("load-config");

        cfg.wait_for_config(); // config process
        conf_info_t config = this->conf_info.read();

        // User-defined config code
        /* <<--local-params-->> */
        logn_samples = config.logn_samples;
        num_samples = 1 << logn_samples;

        // Configured shared memory offsets for sync flags
        prod_valid_offset = config.prod_valid_offset;
        prod_ready_offset = config.prod_ready_offset;
        flt_prod_valid_offset = config.flt_prod_valid_offset;
        flt_prod_ready_offset = config.flt_prod_ready_offset;
        cons_valid_offset = config.cons_valid_offset;
        cons_ready_offset = config.cons_ready_offset;
        input_offset = config.input_offset;
        flt_input_offset = config.flt_input_offset;
        twd_input_offset = config.twd_input_offset;
        output_offset = config.output_offset;

        pingpong = false;
        flt_pingpong = false;
        task_arbiter = 0;
    }

    // Load
    while(true)
    {
        HLS_PROTO("load-dma");

        wait();

        // Wait for either input ASI or output ASI
        while (!(input_load_req_valid || filters_load_req_valid || output_load_req_valid)) wait();

        if (task_arbiter == 0 && input_load_req_valid) {
            this->load_input_start_handshake();
            load_state_req = input_load_req;
            load_state_req_module = INPUT_ASI;
            task_arbiter++;
        } else if (task_arbiter == 1 && filters_load_req_valid) {
            this->load_filters_start_handshake();
            load_state_req = filters_load_req;
            load_state_req_module = FILTERS_ASI;
            task_arbiter++;
        } else if (task_arbiter == 2 && output_load_req_valid) {
            this->load_output_start_handshake();
            load_state_req = output_load_req;
            load_state_req_module = OUTPUT_ASI;
            task_arbiter = 0;
        } else {
            if (task_arbiter == 2) task_arbiter = 0;
            else task_arbiter++;
            continue;
        }

        load_state_req_dbg.write(load_state_req);

        switch (load_state_req)
        {
            case TEST_PROD_VALID_REQ:
            {
                // Test for producer to send new data
                dma_info_t dma_info(prod_valid_offset / DMA_WORD_PER_BEAT, 2 * TEST_VAR_SIZE / DMA_WORD_PER_BEAT, DMA_SIZE);
                sc_dt::sc_bv<DMA_WIDTH> dataBv;

                wait();
                this->dma_read_ctrl.put(dma_info);

                dataBv = this->dma_read_chnl.get();
                wait();
                prod_valid = dataBv.range(DATA_WIDTH - 1, 0).to_int64();
                dataBv = this->dma_read_chnl.get();
                wait();
                end_input_asi = dataBv.range(DATA_WIDTH - 1, 0).to_int64();
            }
            break;
            case TEST_FLT_PROD_VALID_REQ:
            {
                dma_info_t dma_info(flt_prod_valid_offset / DMA_WORD_PER_BEAT, UPDATE_VAR_SIZE / DMA_WORD_PER_BEAT, DMA_SIZE);
                sc_dt::sc_bv<DMA_WIDTH> dataBv;

                wait();
                this->dma_read_ctrl.put(dma_info);

                dataBv = this->dma_read_chnl.get();
                wait();
                flt_valid = dataBv.range(DATA_WIDTH - 1, 0).to_int64();
            }
            break;
            case TEST_CONS_READY_REQ:
            {
                // Test for consumer to accept new data
                dma_info_t dma_info(cons_ready_offset / DMA_WORD_PER_BEAT, UPDATE_VAR_SIZE / DMA_WORD_PER_BEAT, DMA_SIZE);
                sc_dt::sc_bv<DMA_WIDTH> dataBv;

                wait();
                this->dma_read_ctrl.put(dma_info);

                dataBv = this->dma_read_chnl.get();
                wait();
                cons_ready = dataBv.range(DATA_WIDTH - 1, 0).to_int64();
            }
            break;
            case LOAD_DATA_REQ:
            // Load input data
            {
                dma_info_t dma_info(input_offset / DMA_WORD_PER_BEAT, 2 * num_samples / DMA_WORD_PER_BEAT, DMA_SIZE);
                sc_dt::sc_bv<DMA_WIDTH> dataBv;

                wait();

                this->dma_read_ctrl.put(dma_info);

                for (int i = 0; i < 2 * num_samples; i += DMA_WORD_PER_BEAT)
                {
                    HLS_BREAK_DEP(A0);
                    HLS_BREAK_DEP(A1);

                    dataBv = this->dma_read_chnl.get();
                    wait();
                    for (uint16_t k = 0; k < DMA_WORD_PER_BEAT; k++)
                    {
                        HLS_UNROLL_SIMPLE;
                        if (!pingpong)
                            A0[i + k] = dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH).to_int64();
                        else
                            A1[i + k] = dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH).to_int64();
                    }
                }
            }

            pingpong = !pingpong;
            break;
            case LOAD_FILTERS_REQ:
            // Load filters
            {
                dma_info_t dma_info(flt_input_offset / DMA_WORD_PER_BEAT, 2 * (num_samples + 1)/ DMA_WORD_PER_BEAT, DMA_SIZE);
                sc_dt::sc_bv<DMA_WIDTH> dataBv;

                wait();

                this->dma_read_ctrl.put(dma_info);

                for (int i = 0; i < 2 * (num_samples + 1); i += DMA_WORD_PER_BEAT)
                {
                    HLS_BREAK_DEP(F0);
                    HLS_BREAK_DEP(F1);

                    dataBv = this->dma_read_chnl.get();
                    wait();
                    for (uint16_t k = 0; k < DMA_WORD_PER_BEAT; k++)
                    {
                        HLS_UNROLL_SIMPLE;
                        if (!flt_pingpong)
                            F0[i + k] = dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH).to_int64();
                        else
                            F1[i + k] = dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH).to_int64();
                    }
                }
            }
            // Load twiddle factors 
            {
                dma_info_t dma_info(twd_input_offset / DMA_WORD_PER_BEAT, num_samples / DMA_WORD_PER_BEAT, DMA_SIZE);
                sc_dt::sc_bv<DMA_WIDTH> dataBv;

                wait();

                this->dma_read_ctrl.put(dma_info);

                for (int i = 0; i < num_samples; i += DMA_WORD_PER_BEAT)
                {
                    HLS_BREAK_DEP(T0);
                    HLS_BREAK_DEP(T1);

                    dataBv = this->dma_read_chnl.get();
                    wait();
                    for (uint16_t k = 0; k < DMA_WORD_PER_BEAT; k++)
                    {
                        HLS_UNROLL_SIMPLE;
                        if (!flt_pingpong)
                            T0[i + k] = dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH).to_int64();
                        else
                            T1[i + k] = dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH).to_int64();
                    }
                }
            }

            flt_pingpong = !flt_pingpong;
            break;
            default:
            break;
        }

        wait();

        if (load_state_req_module == INPUT_ASI) {
            this->load_input_done_handshake();
        } else if (load_state_req_module == FILTERS_ASI) {
            this->load_filters_done_handshake();
        } else {
            this->load_output_done_handshake();
        }

        load_state_req_module = 0;
    }
} // Function : load_input

void audio_ffi::store_output()
{
    // Reset
    {
        HLS_PROTO("store-reset");

        this->reset_store_output();

        store_state_req_dbg.write(0);

        input_store_start.ack.reset_ack();
        filters_store_start.ack.reset_ack();
        output_store_start.ack.reset_ack();
        store_input_done.req.reset_req();
        store_filters_done.req.reset_req();
        store_output_done.req.reset_req();

        store_state_req = 0;
        store_state_req_module = 0;

        wait();
    }

    // Config
    /* <<--params-->> */
    int32_t logn_samples;
    int32_t num_samples;
    int32_t prod_valid_offset;
    int32_t prod_ready_offset;
    int32_t flt_prod_valid_offset;
    int32_t flt_prod_ready_offset;
    int32_t cons_valid_offset;
    int32_t cons_ready_offset;
    int32_t input_offset;
    int32_t flt_input_offset;
    int32_t twd_input_offset;
    int32_t output_offset;
    bool pingpong;
    {
        HLS_PROTO("store-config");

        cfg.wait_for_config(); // config process

        conf_info_t config = this->conf_info.read();

        // User-defined config code
        /* <<--local-params-->> */
        logn_samples = config.logn_samples;
        num_samples = 1 << logn_samples;

        // Configured shared memory offsets for sync flags
        prod_valid_offset = config.prod_valid_offset;
        prod_ready_offset = config.prod_ready_offset;
        flt_prod_valid_offset = config.flt_prod_valid_offset;
        flt_prod_ready_offset = config.flt_prod_ready_offset;
        cons_valid_offset = config.cons_valid_offset;
        cons_ready_offset = config.cons_ready_offset;
        input_offset = config.input_offset;
        flt_input_offset = config.flt_input_offset;
        twd_input_offset = config.twd_input_offset;
        output_offset = config.output_offset;

        pingpong = false;
    }

    // Store
    while(true)
    {
        HLS_PROTO("store-dma");

        wait();

        // Wait for either input ASI or output ASI
        while (!(input_store_req_valid || filters_store_req_valid || output_store_req_valid)) wait();

        if (input_store_req_valid) {
            this->store_input_start_handshake();
            store_state_req = input_store_req;
            store_state_req_module = INPUT_ASI;
        } else if (filters_store_req_valid) {
            this->store_filters_start_handshake();
            store_state_req = filters_store_req;
            store_state_req_module = FILTERS_ASI;
        } else {
            this->store_output_start_handshake();
            store_state_req = output_store_req;
            store_state_req_module = OUTPUT_ASI;
        }

        store_state_req_dbg.write(store_state_req);

        switch (store_state_req)
        {
            case UPDATE_PROD_READY_REQ:
            {
                dma_info_t dma_info(prod_ready_offset / DMA_WORD_PER_BEAT, UPDATE_VAR_SIZE / DMA_WORD_PER_BEAT, DMA_SIZE);
                sc_dt::sc_bv<DMA_WIDTH> dataBv;
                dataBv.range(DMA_WIDTH - 1, 0) = 1;

                this->dma_write_ctrl.put(dma_info);
                wait();
                this->dma_write_chnl.put(dataBv);
                wait();

                // Wait till the write is accepted at the cache (and previous fences)
                while (!(this->dma_write_chnl.ready)) wait();
                wait();
            }
            break;
            case UPDATE_FLT_PROD_READY_REQ:
            {
                dma_info_t dma_info(flt_prod_ready_offset / DMA_WORD_PER_BEAT, UPDATE_VAR_SIZE / DMA_WORD_PER_BEAT, DMA_SIZE);
                sc_dt::sc_bv<DMA_WIDTH> dataBv;
                dataBv.range(DMA_WIDTH - 1, 0) = 1;

                this->dma_write_ctrl.put(dma_info);
                wait();
                this->dma_write_chnl.put(dataBv);
                wait();

                // Wait till the write is accepted at the cache (and previous fences)
                while (!(this->dma_write_chnl.ready)) wait();
                wait();
            }
            break;
            case UPDATE_PROD_VALID_REQ:
            {
                dma_info_t dma_info(prod_valid_offset / DMA_WORD_PER_BEAT, UPDATE_VAR_SIZE / DMA_WORD_PER_BEAT, DMA_SIZE);
                sc_dt::sc_bv<DMA_WIDTH> dataBv;
                dataBv.range(DMA_WIDTH - 1, 0) = 0;

                this->dma_write_ctrl.put(dma_info);
                wait();
                this->dma_write_chnl.put(dataBv);
                wait();

                // Wait till the write is accepted at the cache (and previous fences)
                while (!(this->dma_write_chnl.ready)) wait();
                wait();
            }
            break;
            case UPDATE_FLT_PROD_VALID_REQ:
            {
                dma_info_t dma_info(flt_prod_valid_offset / DMA_WORD_PER_BEAT, UPDATE_VAR_SIZE / DMA_WORD_PER_BEAT, DMA_SIZE);
                sc_dt::sc_bv<DMA_WIDTH> dataBv;
                dataBv.range(DMA_WIDTH - 1, 0) = 0;

                this->dma_write_ctrl.put(dma_info);
                wait();
                this->dma_write_chnl.put(dataBv);
                wait();

                // Wait till the write is accepted at the cache (and previous fences)
                while (!(this->dma_write_chnl.ready)) wait();
                wait();
            }
            break;
            case UPDATE_CONS_VALID_REQ:
            {
                dma_info_t dma_info(cons_valid_offset / DMA_WORD_PER_BEAT, UPDATE_VAR_SIZE / DMA_WORD_PER_BEAT, DMA_SIZE);
                sc_dt::sc_bv<DMA_WIDTH> dataBv;
                dataBv.range(DMA_WIDTH - 1, 0) = 1;

                this->dma_write_ctrl.put(dma_info);
                wait();
                this->dma_write_chnl.put(dataBv);
                wait();

                // Wait till the write is accepted at the cache (and previous fences)
                while (!(this->dma_write_chnl.ready)) wait();
                wait();
            }
            break;
            case UPDATE_CONS_READY_REQ:
            {
                dma_info_t dma_info(cons_ready_offset / DMA_WORD_PER_BEAT, UPDATE_VAR_SIZE / DMA_WORD_PER_BEAT, DMA_SIZE);
                sc_dt::sc_bv<DMA_WIDTH> dataBv;
                dataBv.range(DMA_WIDTH - 1, 0) = 0;

                this->dma_write_ctrl.put(dma_info);
                wait();
                this->dma_write_chnl.put(dataBv);
                wait();

                // Wait till the write is accepted at the cache (and previous fences)
                while (!(this->dma_write_chnl.ready)) wait();
                wait();
            }
            break;
            case STORE_DATA_REQ:
            {
                dma_info_t dma_info(output_offset / DMA_WORD_PER_BEAT, 2 * num_samples / DMA_WORD_PER_BEAT, DMA_SIZE);
                sc_dt::sc_bv<DMA_WIDTH> dataBv;

                wait();

                this->dma_write_ctrl.put(dma_info);

                for (int i = 0; i < 2 * num_samples; i += DMA_WORD_PER_BEAT)
                {
                    HLS_BREAK_DEP(D0);
                    HLS_BREAK_DEP(D1);

                    wait();

                    for (uint16_t k = 0; k < DMA_WORD_PER_BEAT; k++)
                    {
                        HLS_UNROLL_SIMPLE;
                        if (!pingpong)
                            dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH) = D0[i + k];
                        else
                            dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH) = D1[i + k];
                    }

                    this->dma_write_chnl.put(dataBv);
                }

                // Wait till the last write is accepted at the cache
                wait();
                while (!(this->dma_write_chnl.ready)) wait();
            }

            pingpong = !pingpong;
            break;
            case STORE_FENCE:
            {
                // Block till L2 to be ready to receive a fence, then send
                this->acc_fence.put(0x2);
                wait();
            }
            break;
            case ACC_DONE:
            {
                // Ensure the previous fence was accepted, then acc_done
                while (!(this->acc_fence.ready)) wait();
                wait();
                this->accelerator_done();
                wait();
            }
            break;
            default:
            break;
        }

        wait();

        if (store_state_req_module == INPUT_ASI) {
            this->store_input_done_handshake();
        } else if (store_state_req_module == FILTERS_ASI) {
            this->store_filters_done_handshake();
        } else {
            this->store_output_done_handshake();
        }

        store_state_req_module = 0;
    }
} // Function : store_output

void audio_ffi::compute_kernel()
{
    // Reset
    {
        HLS_PROTO("compute-reset");

        this->reset_compute_kernel();

        wait();
    }

    // Config
    /* <<--params-->> */
    {
        HLS_PROTO("compute-config");

        cfg.wait_for_config(); // config process
        conf_info_t config = this->conf_info.read();

        // User-defined config code
        /* <<--local-params-->> */
    }

    // End operation
    {
        HLS_PROTO("end-compute");

        wait();
        this->process_done();
    }
}

void audio_ffi::input_asi_kernel()
{
    // Reset
    {
        HLS_PROTO("input-asi-reset");

        input_state_req_dbg.write(0);

        input_load_start.req.reset_req();
        input_store_start.req.reset_req();
        load_input_done.ack.reset_ack();
        store_input_done.ack.reset_ack();
        input_to_fft.req.reset_req();

        input_load_req = 0;
        input_store_req = 0;
        input_load_req_valid = false;
        input_store_req_valid = false;

        end_fft = 0;

        wait();
    }

    // Config
    /* <<--params-->> */
    {
        HLS_PROTO("input-asi-config");

        cfg.wait_for_config(); // config process

        // User-defined config code
        /* <<--local-params-->> */
    }

    while(true)
    {
        // Test producer's valid for new task
        {
            HLS_PROTO("test-prod-valid");

            input_load_req = TEST_PROD_VALID_REQ;
            input_load_req_valid = true;

            input_state_req_dbg.write(TEST_PROD_VALID_REQ);

            this->input_load_start_handshake();
            wait();
            input_load_req_valid = false;
            wait();
            this->input_load_done_handshake();
            wait();
        }

        {
            HLS_PROTO("prod-valid-check");

            if (prod_valid == 1)
            {
                prod_valid = 0;

                // Reset producer's valid
                {
                    HLS_PROTO("update-prod-valid");

                    input_store_req = UPDATE_PROD_VALID_REQ;
                    input_store_req_valid = true;

                    input_state_req_dbg.write(UPDATE_PROD_VALID_REQ);

                    this->input_store_start_handshake();
                    wait();
                    input_store_req_valid = false;
                    wait();
                    this->input_store_done_handshake();
                    wait();

                    // Wait for all writes to be done and then issue fence
                    input_store_req = STORE_FENCE;
                    input_store_req_valid = true;

                    input_state_req_dbg.write(STORE_FENCE);

                    this->input_store_start_handshake();
                    wait();
                    input_store_req_valid = false;
                    wait();
                    this->input_store_done_handshake();
                    wait();
                }

                // Load input data
                {
                    HLS_PROTO("load-input-data");

                    input_load_req = LOAD_DATA_REQ;
                    input_load_req_valid = true;

                    input_state_req_dbg.write(LOAD_DATA_REQ);

                    this->input_load_start_handshake();
                    wait();
                    input_load_req_valid = false;
                    wait();
                    this->input_load_done_handshake();
                    wait();
                }

                // update producer's ready to accept new data
                {
                    HLS_PROTO("update-prod-ready");

                    input_store_req = UPDATE_PROD_READY_REQ;
                    input_store_req_valid = true;

                    input_state_req_dbg.write(UPDATE_PROD_READY_REQ);

                    this->input_store_start_handshake();
                    wait();
                    input_store_req_valid = false;
                    wait();
                    this->input_store_done_handshake();
                    wait();

                    // Wait for all writes to be done and then issue fence
                    input_store_req = STORE_FENCE;
                    input_store_req_valid = true;

                    input_state_req_dbg.write(STORE_FENCE);

                    this->input_store_start_handshake();
                    wait();
                    input_store_req_valid = false;
                    wait();
                    this->input_store_done_handshake();
                    wait();
                }

                // Inform FFT to start
                {
                    HLS_PROTO("inform-fft-start");

                    end_fft = end_input_asi;
                    this->input_fft_handshake();
                    wait();
                    
                    if (end_input_asi == 1)
                    {
                        this->process_done();
                    }
                }
            }
        }
    }
}

void audio_ffi::filters_asi_kernel()
{
    // Reset
    {
        HLS_PROTO("filters-asi-reset");

        filters_state_req_dbg.write(0);

        filters_load_start.req.reset_req();
        filters_store_start.req.reset_req();
        load_filters_done.ack.reset_ack();
        store_filters_done.ack.reset_ack();
        filters_to_fir.req.reset_req();

        filters_load_req = 0;
        filters_store_req = 0;
        filters_load_req_valid = false;
        filters_store_req_valid = false;

        wait();
    }

    // Config
    /* <<--params-->> */
    {
        HLS_PROTO("filters-asi-config");

        cfg.wait_for_config(); // config process

        // User-defined config code
        /* <<--local-params-->> */
    }

    while(true)
    {
        // Test filter producer's valid for new filters
        {
            HLS_PROTO("test-flt-valid");

            filters_load_req = TEST_FLT_PROD_VALID_REQ;
            filters_load_req_valid = true;

            filters_state_req_dbg.write(TEST_FLT_PROD_VALID_REQ);

            this->filters_load_start_handshake();
            wait();
            filters_load_req_valid = false;
            wait();
            this->filters_load_done_handshake();
            wait();
        }

        {
            HLS_PROTO("flt-valid-check");

            if (flt_valid == 1)
            {
                flt_valid = 0;

                // Reset filter producer's valid
                {
                    HLS_PROTO("update-flt-prod-valid");

                    filters_store_req = UPDATE_FLT_PROD_VALID_REQ;
                    filters_store_req_valid = true;

                    filters_state_req_dbg.write(UPDATE_FLT_PROD_VALID_REQ);

                    this->filters_store_start_handshake();
                    wait();
                    filters_store_req_valid = false;
                    wait();
                    this->filters_store_done_handshake();
                    wait();

                    // Wait for all writes to be done and then issue fence
                    filters_store_req = STORE_FENCE;
                    filters_store_req_valid = true;

                    filters_state_req_dbg.write(STORE_FENCE);

                    this->filters_store_start_handshake();
                    wait();
                    filters_store_req_valid = false;
                    wait();
                    this->filters_store_done_handshake();
                    wait();
                }

                // Load filters data
                {
                    HLS_PROTO("load-filters-data");

                    filters_load_req = LOAD_FILTERS_REQ;
                    filters_load_req_valid = true;

                    filters_state_req_dbg.write(LOAD_FILTERS_REQ);

                    this->filters_load_start_handshake();
                    wait();
                    filters_load_req_valid = false;
                    wait();
                    this->filters_load_done_handshake();
                    wait();
                }

                // update filter producer's ready to accept new filters
                {
                    HLS_PROTO("update-flt-prod-ready");

                    filters_store_req = UPDATE_FLT_PROD_READY_REQ;
                    filters_store_req_valid = true;

                    filters_state_req_dbg.write(UPDATE_FLT_PROD_READY_REQ);

                    this->filters_store_start_handshake();
                    wait();
                    filters_store_req_valid = false;
                    wait();
                    this->filters_store_done_handshake();
                    wait();

                    // Wait for all writes to be done and then issue fence
                    filters_store_req = STORE_FENCE;
                    filters_store_req_valid = true;

                    filters_state_req_dbg.write(STORE_FENCE);

                    this->filters_store_start_handshake();
                    wait();
                    filters_store_req_valid = false;
                    wait();
                    this->filters_store_done_handshake();
                    wait();
                }

                // Inform FIR that filters are ready
                {
                    HLS_PROTO("inform-fir-start");

                    this->filters_fir_handshake();
                    wait();
                    
                    if (end_input_asi == 1)
                    {
                        this->process_done();
                    }
                }
            }
        }
    }
}

void audio_ffi::output_asi_kernel()
{
    // Reset
    {
        HLS_PROTO("output-asi-reset");

        output_state_req_dbg.write(0);

        output_load_start.req.reset_req();
        output_store_start.req.reset_req();
        load_output_done.ack.reset_ack();
        store_output_done.ack.reset_ack();
        ifft_to_store.ack.reset_ack();

        output_load_req = 0;
        output_store_req = 0;
        output_load_req_valid = false;
        output_store_req_valid = false;

        wait();
    }

    // Config
    /* <<--params-->> */
    {
        HLS_PROTO("output-asi-config");

        cfg.wait_for_config(); // config process

        // User-defined config code
        /* <<--local-params-->> */
    }

    while(true)
    {
        // Wait for IFFT to be complete
        {
            HLS_PROTO("wait-for-ifft");

            this->output_ifft_handshake();
            wait();
        }

        // Poll consumer's ready to know if we can send new data
        {
            HLS_PROTO("poll-for-cons-ready");

            while (cons_ready == 0)
            {
                output_load_req = TEST_CONS_READY_REQ;
                output_load_req_valid = true;

                output_state_req_dbg.write(TEST_CONS_READY_REQ);

                this->output_load_start_handshake();
                wait();
                output_load_req_valid = false;
                wait();
                this->output_load_done_handshake();
                wait();
            }
        }

        {
            HLS_PROTO("cons-ready-check");

            cons_ready = 0;

            // Reset consumer's ready
            {
                HLS_PROTO("update-cons-ready");

                output_store_req = UPDATE_CONS_READY_REQ;
                output_store_req_valid = true;

                output_state_req_dbg.write(UPDATE_CONS_READY_REQ);

                this->output_store_start_handshake();
                wait();
                output_store_req_valid = false;
                wait();
                this->output_store_done_handshake();
                wait();

                // Wait for all writes to be done and then issue fence
                output_store_req = STORE_FENCE;
                output_store_req_valid = true;

                output_state_req_dbg.write(STORE_FENCE);

                this->output_store_start_handshake();
                wait();
                output_store_req_valid = false;
                wait();
                this->output_store_done_handshake();
                wait();
            }
            // Store output data
            {
                HLS_PROTO("store-output-data");

                output_store_req = STORE_DATA_REQ;
                output_store_req_valid = true;

                output_state_req_dbg.write(STORE_DATA_REQ);

                this->output_store_start_handshake();
                wait();
                output_store_req_valid = false;
                wait();
                this->output_store_done_handshake();
                wait();

                // Wait for all writes to be done and then issue fence
                output_store_req = STORE_FENCE;
                output_store_req_valid = true;

                output_state_req_dbg.write(STORE_FENCE);

                this->output_store_start_handshake();
                wait();
                output_store_req_valid = false;
                wait();
                this->output_store_done_handshake();
                wait();
            }
            // update consumer's ready for new data available
            {
                HLS_PROTO("update-cons-ready");

                output_store_req = UPDATE_CONS_VALID_REQ;
                output_store_req_valid = true;

                output_state_req_dbg.write(UPDATE_CONS_VALID_REQ);

                this->output_store_start_handshake();
                wait();
                output_store_req_valid = false;
                wait();
                this->output_store_done_handshake();
                wait();

                // Wait for all writes to be done and then issue fence
                output_store_req = STORE_FENCE;
                output_store_req_valid = true;

                output_state_req_dbg.write(STORE_FENCE);

                this->output_store_start_handshake();
                wait();
                output_store_req_valid = false;
                wait();
                this->output_store_done_handshake();
                wait();
            }

            // End operation
            {
                HLS_PROTO("end-acc");

                if (end_output_asi == 1)
                {
                    output_store_req = ACC_DONE;
                    output_store_req_valid = true;

                    output_state_req_dbg.write(ACC_DONE);

                    this->output_store_start_handshake();
                    wait();
                    output_store_req_valid = false;
                    wait();
                    this->output_store_done_handshake();
                    wait();
                    this->process_done();
                }
            }
        }
    } // while (true)
} // Function : compute_kernel

void audio_ffi::fft_kernel()
{
    // Reset
    {
        HLS_PROTO("fft-reset");

        wait();

        fft_state_dbg.write(0);

        fft_to_fir.req.reset_req();
        input_to_fft.ack.reset_ack();

        end_fir = 0;
    }

    // Config
    /* <<--params-->> */
    int32_t logn_samples;
    int32_t num_samples;
    int32_t do_shift;
    bool pingpong;
    {
        HLS_PROTO("fft-config");

        cfg.wait_for_config(); // config process
        conf_info_t config = this->conf_info.read();

        // User-defined config code
        /* <<--local-params-->> */
        logn_samples = config.logn_samples;
        num_samples = 1 << logn_samples;
        do_shift = config.do_shift;
        pingpong = false;
    }

    while(true)
    {
        // Wait for load to be complete
        {
            HLS_PROTO("wait-for-load");

            this->fft_input_handshake();
            wait();

            fft_state_dbg.write(1);
        }

        // Compute - FFT
        {
            unsigned offset = 0;  // Offset into Mem for start of this FFT
            int sin_sign = 1; // This modifes the mySin

            // Do the bit-reverse
            {
                unsigned int i, s, shift;
                s = 31;
                shift = s - logn_samples + 1;

                for (i = 0; i < num_samples; i++)
                {
                    unsigned int r;
                    FPDATA_WORD t1_real, t1_imag;
                    FPDATA_WORD t2_real, t2_imag;

                    r = fft2_rev(i);
                    r >>= shift;

                    unsigned int iidx = 2*(offset + i);
                    unsigned int ridx = 2*(offset + r);

                    if (!pingpong) {
                        t1_real = A0[iidx];
                        t1_imag = A0[iidx + 1];
                        t2_real = A0[ridx];
                        t2_imag = A0[ridx + 1];
                    } else {
                        t1_real = A1[iidx];
                        t1_imag = A1[iidx + 1];
                        t2_real = A1[ridx];
                        t2_imag = A1[ridx + 1];
                    }

                    if (i < r) {
                        HLS_PROTO("bit-rev-memwrite");
                        HLS_BREAK_DEP(A0);
                        HLS_BREAK_DEP(A1);

                        if (!pingpong) {
                            wait();
                            A0[iidx] = t2_real;
                            A0[iidx + 1] = t2_imag;
                            wait();
                            A0[ridx] = t1_real;
                            A0[ridx + 1] = t1_imag;
                        } else {
                            wait();
                            A1[iidx] = t2_real;
                            A1[iidx + 1] = t2_imag;
                            wait();
                            A1[ridx] = t1_real;
                            A1[ridx + 1] = t1_imag;
                        }
                    }
                }
            }

            // Computing phase implementation
            int m = 1;  // iterative FFT

            FFT2_SINGLE_L1:
                for(unsigned s = 1; s <= logn_samples; s++) {
                    m = 1 << s;
                    CompNum wm(myCos(s), sin_sign*mySin(s));

                FFT2_SINGLE_L2:
                    for(unsigned k = 0; k < num_samples; k +=m) {

                        CompNum w((FPDATA) 1, (FPDATA) 0);
                        int md2 = m / 2;

                    FFT2_SINGLE_L3:
                        for(int j = 0; j < md2; j++) {

                            int kj = offset + k + j;
                            int kjm = offset + k + j + md2;
                            CompNum akj, akjm;
                            CompNum bkj, bkjm;

                            if (!pingpong) {
                                akj.re = int2fp<FPDATA, WORD_SIZE>(A0[2 * kj]);
                                akj.im = int2fp<FPDATA, WORD_SIZE>(A0[2 * kj + 1]);
                                akjm.re = int2fp<FPDATA, WORD_SIZE>(A0[2 * kjm]);
                                akjm.im = int2fp<FPDATA, WORD_SIZE>(A0[2 * kjm + 1]);
                            } else {
                                akj.re = int2fp<FPDATA, WORD_SIZE>(A1[2 * kj]);
                                akj.im = int2fp<FPDATA, WORD_SIZE>(A1[2 * kj + 1]);
                                akjm.re = int2fp<FPDATA, WORD_SIZE>(A1[2 * kjm]);
                                akjm.im = int2fp<FPDATA, WORD_SIZE>(A1[2 * kjm + 1]);
                            }

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
                                HLS_PROTO("compute_write_A0_fft");
                                HLS_BREAK_DEP(A0);
                                HLS_BREAK_DEP(A1);

                                if (!pingpong) {
                                    wait();
                                    A0[2 * kj] = fp2int<FPDATA, WORD_SIZE>(bkj.re);
                                    A0[2 * kj + 1] = fp2int<FPDATA, WORD_SIZE>(bkj.im);
                                    wait();
                                    A0[2 * kjm] = fp2int<FPDATA, WORD_SIZE>(bkjm.re);
                                    A0[2 * kjm + 1] = fp2int<FPDATA, WORD_SIZE>(bkjm.im);
                                } else {
                                    wait();
                                    A1[2 * kj] = fp2int<FPDATA, WORD_SIZE>(bkj.re);
                                    A1[2 * kj + 1] = fp2int<FPDATA, WORD_SIZE>(bkj.im);
                                    wait();
                                    A1[2 * kjm] = fp2int<FPDATA, WORD_SIZE>(bkjm.re);
                                    A1[2 * kjm + 1] = fp2int<FPDATA, WORD_SIZE>(bkjm.im);
                                }
                            }
                        } // for (j = 0 .. md2)
                    } // for (k = 0 .. num_samples)
                } // for (s = 1 .. logn_samples)
            
            for (int p = 0; p < 2 * num_samples; p++) {
                HLS_BREAK_DEP(A0);
                HLS_BREAK_DEP(A1);
                HLS_BREAK_DEP(B0);
                HLS_BREAK_DEP(B1);

                if (!pingpong)
                    B0[p] = A0[p];
                else
                    B1[p] = A1[p];
            }
        }

        // Inform FIR to start
        {
            HLS_PROTO("inform-fir-start");

            fft_state_dbg.write(0);

            end_fir = end_fft;
            this->fft_fir_handshake();
            wait();
        }

        pingpong = !pingpong;
    }
}

void audio_ffi::fir_kernel()
{
    // Reset
    {
        HLS_PROTO("fir-reset");

        wait();

        fir_state_dbg.write(0);

        fir_to_ifft.req.reset_req();
        fft_to_fir.ack.reset_ack();

        end_ifft = 0;
    }

    // Config
    /* <<--params-->> */
    int32_t logn_samples;
    int32_t num_samples;
    int32_t do_shift;
    bool pingpong;
    {
        HLS_PROTO("fir-config");

        cfg.wait_for_config(); // config process
        conf_info_t config = this->conf_info.read();

        // User-defined config code
        /* <<--local-params-->> */
        logn_samples = config.logn_samples;
        num_samples = 1 << logn_samples;
        do_shift = config.do_shift;
        pingpong = false;
    }

    while(true)
    {
        // Wait for FFT to be complete
        {
            HLS_PROTO("wait-for-fft");

            this->fir_fft_handshake();
            wait();
        }

        // Wait for Filters ASI to be complete
        {
            HLS_PROTO("wait-for-filters-asi");

            this->fir_filters_handshake();
            wait();

            fir_state_dbg.write(1);
        }

        // Compute - FIR
        {
            CompNum fpnk, fpk, f1k, f2k, tw, tdc;
            CompNum tf, if0, ifn, of0, flt0, fltn, t0, tn;
            CompNum fk, fnkc, fek, fok, tmp;
            CompNum tmpbuf0, tmpbufn;

            // first and last element
            {
                // Post-process first and last element
                if (!pingpong) {
                    tdc.re = int2fp<FPDATA, WORD_SIZE>(B0[0]);
                    tdc.im = int2fp<FPDATA, WORD_SIZE>(B0[1]);
                } else {
                    tdc.re = int2fp<FPDATA, WORD_SIZE>(B1[0]);
                    tdc.im = int2fp<FPDATA, WORD_SIZE>(B1[1]);
                }

                if0.re = tdc.re + tdc.im;
                if0.im = 0;
                ifn.re = tdc.re - tdc.im;
                ifn.im = 0;

                // Reading filter values
                if (!pingpong) {
                    flt0.re = int2fp<FPDATA, WORD_SIZE>(F0[0]);
                    flt0.im = int2fp<FPDATA, WORD_SIZE>(F0[1]);
                    fltn.re = int2fp<FPDATA, WORD_SIZE>(F0[(2 * num_samples)]);
                    fltn.im = int2fp<FPDATA, WORD_SIZE>(F0[(2 * num_samples) + 1]);
                } else {
                    flt0.re = int2fp<FPDATA, WORD_SIZE>(F1[0]);
                    flt0.im = int2fp<FPDATA, WORD_SIZE>(F1[1]);
                    fltn.re = int2fp<FPDATA, WORD_SIZE>(F1[(2 * num_samples)]);
                    fltn.im = int2fp<FPDATA, WORD_SIZE>(F1[(2 * num_samples) + 1]);
                }

                // fir
                compMul(if0, flt0, t0);
                compMul(ifn, fltn, tn);

                // Pre-process first and last element
                of0.re = t0.re + tn.re;
                of0.im = t0.re - tn.re;

                // Write back element 0 to memory
                {
                    HLS_PROTO("write-back-elem-0");
                    HLS_BREAK_DEP(C0);
                    HLS_BREAK_DEP(C1);

                    if (!pingpong) {
                        wait();
                        C0[0] = fp2int<FPDATA, WORD_SIZE>(of0.re);
                        C0[1] = fp2int<FPDATA, WORD_SIZE>(of0.im);
                    } else {
                        wait();
                        C1[0] = fp2int<FPDATA, WORD_SIZE>(of0.re);
                        C1[1] = fp2int<FPDATA, WORD_SIZE>(of0.im);
                    }
                }
            }

            // Remaining elements
            for (unsigned k = 2; k <= num_samples; k+=2)
            {
                // Read FFT output
                if (!pingpong) {
                    fpk.re = int2fp<FPDATA, WORD_SIZE>(B0[k]);
                    fpk.im = int2fp<FPDATA, WORD_SIZE>(B0[k + 1]);
                    fpnk.re = int2fp<FPDATA, WORD_SIZE>(B0[(2 * num_samples) - k]);
                    fpnk.im = - (int2fp<FPDATA, WORD_SIZE>(B0[(2 * num_samples) - k + 1]));
                } else {
                    fpk.re = int2fp<FPDATA, WORD_SIZE>(B1[k]);
                    fpk.im = int2fp<FPDATA, WORD_SIZE>(B1[k + 1]);
                    fpnk.re = int2fp<FPDATA, WORD_SIZE>(B1[(2 * num_samples) - k]);
                    fpnk.im = - (int2fp<FPDATA, WORD_SIZE>(B1[(2 * num_samples) - k + 1]));
                }

                // Read twiddle factors
                if (!pingpong) {
                    tf.re = int2fp<FPDATA, WORD_SIZE>(T0[k - 2]);
                    tf.im = int2fp<FPDATA, WORD_SIZE>(T0[k - 1]);
                } else {
                    tf.re = int2fp<FPDATA, WORD_SIZE>(T1[k - 2]);
                    tf.im = int2fp<FPDATA, WORD_SIZE>(T1[k - 1]);
                }

                compAdd(fpk, fpnk, f1k);
                compSub(fpk, fpnk, f2k);
                compMul(f2k, tf, tw);

                // Computing freqdata's
                compAdd(f1k, tw, if0);
                if0.re /= 2; if0.im /= 2;

                compSub(f1k, tw, ifn);
                ifn.re /= 2; ifn.im /= 2;
                ifn.im *= -1;

                // Reading filter values
                if (!pingpong) {
                    flt0.re = int2fp<FPDATA, WORD_SIZE>(F0[k]);
                    flt0.im = int2fp<FPDATA, WORD_SIZE>(F0[k + 1]);
                    fltn.re = int2fp<FPDATA, WORD_SIZE>(F0[(2 * num_samples) - k]);
                    fltn.im = int2fp<FPDATA, WORD_SIZE>(F0[(2 * num_samples) - k + 1]);
                } else {
                    flt0.re = int2fp<FPDATA, WORD_SIZE>(F1[k]);
                    flt0.im = int2fp<FPDATA, WORD_SIZE>(F1[k + 1]);
                    fltn.re = int2fp<FPDATA, WORD_SIZE>(F1[(2 * num_samples) - k]);
                    fltn.im = int2fp<FPDATA, WORD_SIZE>(F1[(2 * num_samples) - k + 1]);
                }

                // fir
                compMul(if0, flt0, t0);
                compMul(ifn, fltn, tn);

                if (k == num_samples) {
                    fk = tn;
                } else {
                    fk = t0;
                }

                fnkc.re = tn.re;
                fnkc.im = - (tn.im);

                tf.im *= -1;

                compAdd(fk, fnkc, fek);
                compSub(fk, fnkc, tmp);
                compMul(tmp, tf, fok);

                compAdd(fek, fok, tmpbuf0);
                compSub(fek, fok, tmpbufn);

                {
                    HLS_PROTO("write-back-elem-k");
                    HLS_BREAK_DEP(C0);
                    HLS_BREAK_DEP(C1);
                    
                    if (!pingpong) {
                        wait();
                        C0[k] = fp2int<FPDATA, WORD_SIZE>(tmpbuf0.re);
                        C0[k + 1] = fp2int<FPDATA, WORD_SIZE>(tmpbuf0.im);
                        wait();
                        C0[(2 * num_samples) - k] = fp2int<FPDATA, WORD_SIZE>(tmpbufn.re);
                        C0[(2 * num_samples) - k + 1] = - (fp2int<FPDATA, WORD_SIZE>(tmpbufn.im));
                    } else {
                        wait();
                        C1[k] = fp2int<FPDATA, WORD_SIZE>(tmpbuf0.re);
                        C1[k + 1] = fp2int<FPDATA, WORD_SIZE>(tmpbuf0.im);
                        wait();
                        C1[(2 * num_samples) - k] = fp2int<FPDATA, WORD_SIZE>(tmpbufn.re);
                        C1[(2 * num_samples) - k + 1] = - (fp2int<FPDATA, WORD_SIZE>(tmpbufn.im));
                    }
                }
            } // for (k = 0 .. num_samples)
        }

        // Inform IFFT to start
        {
            HLS_PROTO("inform-ifft-start");

            fir_state_dbg.write(0);

            end_ifft = end_fir;
            this->fir_ifft_handshake();
            wait();
        }

        pingpong = !pingpong;
    }
}

void audio_ffi::ifft_kernel()
{
    // Reset
    {
        HLS_PROTO("ifft-reset");

        wait();

        ifft_state_dbg.write(0);

        ifft_to_store.req.reset_req();
        fir_to_ifft.ack.reset_ack();

        end_output_asi = 0;
    }

    // Config
    /* <<--params-->> */
    int32_t logn_samples;
    int32_t num_samples;
    int32_t do_shift;
    bool pingpong;
    {
        HLS_PROTO("ifft-config");

        cfg.wait_for_config(); // config process
        conf_info_t config = this->conf_info.read();

        // User-defined config code
        /* <<--local-params-->> */
        logn_samples = config.logn_samples;
        num_samples = 1 << logn_samples;
        do_shift = config.do_shift;
        pingpong = false;
    }

    while(true)
    {
        // Wait for FIR to be complete
        {
            HLS_PROTO("wait-for-fir");

            this->ifft_fir_handshake();
            wait();

            ifft_state_dbg.write(1);
        }

        // Compute - IFFT
        {
            unsigned offset = 0;  // Offset into Mem for start of this FFT
            int sin_sign = -1; // This modifes the mySin
                                                  // values used below

            // Do the bit-reverse
            {
                unsigned int i, s, shift;
                s = 31;
                shift = s - logn_samples + 1;

                for (i = 0; i < num_samples; i++)
                {
                    unsigned int r;
                    FPDATA_WORD t1_real, t1_imag;
                    FPDATA_WORD t2_real, t2_imag;

                    r = fft2_rev(i);
                    r >>= shift;

                    unsigned int iidx = 2*(offset + i);
                    unsigned int ridx = 2*(offset + r);

                    if (!pingpong) {
                        t1_real = C0[iidx];
                        t1_imag = C0[iidx + 1];
                        t2_real = C0[ridx];
                        t2_imag = C0[ridx + 1];
                    } else {
                        t1_real = C1[iidx];
                        t1_imag = C1[iidx + 1];
                        t2_real = C1[ridx];
                        t2_imag = C1[ridx + 1];
                    }

                    if (i < r) {
                        HLS_PROTO("bit-rev-memwrite");
                        HLS_BREAK_DEP(C0);
                        HLS_BREAK_DEP(C1);

                        if (!pingpong) {
                            wait();
                            C0[iidx] = t2_real;
                            C0[iidx + 1] = t2_imag;
                            wait();
                            C0[ridx] = t1_real;
                            C0[ridx + 1] = t1_imag;
                        } else {
                            wait();
                            C1[iidx] = t2_real;
                            C1[iidx + 1] = t2_imag;
                            wait();
                            C1[ridx] = t1_real;
                            C1[ridx + 1] = t1_imag;
                        }
                    }
                }
            }

            // Computing phase implementation
            int m = 1;  // iterative FFT

            IFFT2_SINGLE_L1:
                for(unsigned s = 1; s <= logn_samples; s++) {
                    m = 1 << s;
                    CompNum wm(myCos(s), sin_sign*mySin(s));

                IFFT2_SINGLE_L2:
                    for(unsigned k = 0; k < num_samples; k +=m) {

                        CompNum w((FPDATA) 1, (FPDATA) 0);
                        int md2 = m / 2;

                    IFFT2_SINGLE_L3:
                        for(int j = 0; j < md2; j++) {

                            int kj = offset + k + j;
                            int kjm = offset + k + j + md2;
                            CompNum akj, akjm;
                            CompNum bkj, bkjm;

                            if (!pingpong) {
                                akj.re = int2fp<FPDATA, WORD_SIZE>(C0[2 * kj]);
                                akj.im = int2fp<FPDATA, WORD_SIZE>(C0[2 * kj + 1]);
                                akjm.re = int2fp<FPDATA, WORD_SIZE>(C0[2 * kjm]);
                                akjm.im = int2fp<FPDATA, WORD_SIZE>(C0[2 * kjm + 1]);
                            } else {
                                akj.re = int2fp<FPDATA, WORD_SIZE>(C1[2 * kj]);
                                akj.im = int2fp<FPDATA, WORD_SIZE>(C1[2 * kj + 1]);
                                akjm.re = int2fp<FPDATA, WORD_SIZE>(C1[2 * kjm]);
                                akjm.im = int2fp<FPDATA, WORD_SIZE>(C1[2 * kjm + 1]);
                            }

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
                                HLS_PROTO("compute_write_A0_ifft");
                                HLS_BREAK_DEP(D0);
                                HLS_BREAK_DEP(D1);

                                if (!pingpong) {
                                    wait();
                                    C0[2 * kj] = fp2int<FPDATA, WORD_SIZE>(bkj.re);
                                    C0[2 * kj + 1] = fp2int<FPDATA, WORD_SIZE>(bkj.im);
                                    wait();
                                    C0[2 * kjm] = fp2int<FPDATA, WORD_SIZE>(bkjm.re);
                                    C0[2 * kjm + 1] = fp2int<FPDATA, WORD_SIZE>(bkjm.im);
                                } else {
                                    wait();
                                    C1[2 * kj] = fp2int<FPDATA, WORD_SIZE>(bkj.re);
                                    C1[2 * kj + 1] = fp2int<FPDATA, WORD_SIZE>(bkj.im);
                                    wait();
                                    C1[2 * kjm] = fp2int<FPDATA, WORD_SIZE>(bkjm.re);
                                    C1[2 * kjm + 1] = fp2int<FPDATA, WORD_SIZE>(bkjm.im);
                                }
                            }
                        } // for (j = 0 .. md2)
                    } // for (k = 0 .. num_samples)
                } // for (s = 1 .. logn_samples)

            for (int p = 0; p < 2 * num_samples; p++) {
                HLS_BREAK_DEP(C0);
                HLS_BREAK_DEP(C1);
                HLS_BREAK_DEP(D0);
                HLS_BREAK_DEP(D1);

                if (!pingpong)
                    D0[p] = C0[p];
                else
                    D1[p] = C1[p];
            }
        } // Compute

        // Inform store to start
        {
            HLS_PROTO("inform-store-start");

            ifft_state_dbg.write(0);

            end_output_asi = end_ifft;
            this->ifft_output_handshake();
            wait();
        }

        pingpong = !pingpong;
    }
}
