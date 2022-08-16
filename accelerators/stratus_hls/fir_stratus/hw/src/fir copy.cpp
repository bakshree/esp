// Copyright (c) 2011-2022 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#include "fir.hpp"
#include "fir_directives.hpp"

// Functions

#include "fir_functions.hpp"

// Processes

void fir::load_input()
{

    // Reset
    {
        HLS_PROTO("load-reset");

        this->reset_load_input();

        // explicit PLM ports reset if any

        // User-defined reset code

        wait();
    }

    // Config
    /* <<--params-->> */
    int32_t data_length;
    {
        HLS_PROTO("load-config");

        cfg.wait_for_config(); // config process
        conf_info_t config = this->conf_info.read();

        // User-defined config code
        /* <<--local-params-->> */
        data_length = config.data_length;
    }

    // Load
    {
        HLS_PROTO("load-dma");
        wait();

        bool ping = true;
        uint32_t offset = 0;

        // Batching
        //for (uint16_t b = 0; b < 1; b++)
        {
            wait();
#if (DMA_WORD_PER_BEAT == 0)
            uint32_t length = 2*data_length;
#else
            uint32_t length = round_up(2*data_length, DMA_WORD_PER_BEAT);
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
                    if (ping)
                        plm_in_ping[i] = dataBv.to_int64();
                    else
                        plm_in_pong[i] = dataBv.to_int64();
                }
#else
                for (uint16_t i = 0; i < len; i += DMA_WORD_PER_BEAT)
                {
                    HLS_BREAK_DEP(plm_in_ping);
                    HLS_BREAK_DEP(plm_in_pong);

                    sc_dt::sc_bv<DMA_WIDTH> dataBv;

                    dataBv = this->dma_read_chnl.get();
                    wait();

                    // Write to PLM (all DMA_WORD_PER_BEAT words in one cycle)
                    for (uint16_t k = 0; k < DMA_WORD_PER_BEAT; k++)
                    {
                        HLS_UNROLL_SIMPLE;
                        if (ping)
                            plm_in_ping[i + k] = dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH).to_int64();
                        else
                            plm_in_pong[i + k] = dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH).to_int64();
                    }
                }
#endif
                this->load_compute_handshake();
                ping = !ping;
            }
        }
    }

    // Conclude
    {
        this->process_done();
    }
}



void fir::store_output()
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
    int32_t data_length;
    {
        HLS_PROTO("store-config");

        cfg.wait_for_config(); // config process
        conf_info_t config = this->conf_info.read();

        // User-defined config code
        /* <<--local-params-->> */
        data_length = config.data_length;
    }

    // Store
    {
        HLS_PROTO("store-dma");
        wait();

        bool ping = true;
#if (DMA_WORD_PER_BEAT == 0)
        uint32_t store_offset = (2*data_length) * 1;
#else
        uint32_t store_offset = round_up(2*data_length, DMA_WORD_PER_BEAT) * 1;
#endif
        uint32_t offset = 0;

        wait();
        // Batching
        //for (uint16_t b = 0; b < 1; b++)
        {
            wait();
#if (DMA_WORD_PER_BEAT == 0)
            uint32_t length = data_length;
#else
            uint32_t length = round_up(data_length, DMA_WORD_PER_BEAT);
#endif
            // Chunking
            for (int rem = length; rem > 0; rem -= PLM_OUT_WORD)
            {

                this->store_compute_handshake();

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
                    if (ping)
                        data = plm_out_ping[i];
                    else
                        data = plm_out_pong[i];
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
                        if (ping)
                            dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH) = plm_out_ping[i + k];
                        else
                            dataBv.range((k+1) * DATA_WIDTH - 1, k * DATA_WIDTH) = plm_out_pong[i + k];
                    }
                    this->dma_write_chnl.put(dataBv);
                }
#endif
                ping = !ping;
            }
        }
    }

    // Conclude
    {
        this->accelerator_done();
        this->process_done();
    }
}


void fir::compute_kernel()
{
    // Reset
    {
        HLS_PROTO("compute-reset");

        this->reset_compute_kernel();

        // explicit PLM ports reset if any

        // User-defined reset code

        wait();
    }

    // Config
    /* <<--params-->> */
    int32_t data_length;
    {
        HLS_PROTO("compute-config");

        cfg.wait_for_config(); // config process
        conf_info_t config = this->conf_info.read();

        // User-defined config code
        /* <<--local-params-->> */
        data_length = config.data_length;
    }


    // Compute
    bool ping = true;
    {
        //for (uint16_t b = 0; b < 1; b++)
        {
            // uint32_t in_length = 2*data_length;
            uint32_t out_length = data_length;
            uint32_t outlenb2 = out_length>>2; // actual number of samples <r,i>
            int out_rem = out_length;

            //for (int in_rem = in_length; in_rem > 0; in_rem -= PLM_IN_WORD)
            {

                // uint32_t in_len  = in_rem  > PLM_IN_WORD  ? PLM_IN_WORD  : in_rem;
                // uint32_t out_len = out_rem > PLM_OUT_WORD ? PLM_OUT_WORD : out_rem;

                this->compute_load_handshake();

                {
                   // HLS_PROTO("fir-preprocess");
                    // tdc.r = st->tmpbuf[0].r;
                    // tdc.i = st->tmpbuf[0].i;

                    int64_t tdc, last;
                    int32_t tdc_r, tdc_i, last_r, last_i;

                    tdc = plm_in_ping[0];
                   
                    tdc_i = tdc&0xFFFFFFFF;
                    tdc_r = tdc>>32;  
                    // C_FIXDIV(tdc,2);
                    tdc_i >>=1;
                    tdc_r >>=1;

    // CHECK_OVERFLOW_OP(tdc.r ,+, tdc.i);
    // CHECK_OVERFLOW_OP(tdc.r ,-, tdc.i);

    // freqdata[0].r = tdc.r + tdc.i;
                    int64_t first_r = tdc_r + tdc_i;
                    first_r = first_r <<32;
                    plm_out_ping[0] =first_r;


                    // freqdata[ncfft].r = tdc.r - tdc.i;
                    // freqdata[ncfft].i = freqdata[0].i = 0;

                    last = tdc_r - tdc_i;
                    last = last<<32;
                    plm_out_ping[out_length-1] =first_r;
                   
                    for (int k = 1; k <= (out_length/2); k++) { 
                        ////
                        int64_t fpk_i, fpk_r, fpnk_i, fpnk_r, fnkc_r, fek_r,fek_i, fok_i,fok_r, tmp_i, tmp_r;

                        // fpk    = st->tmpbuf[k];
                        
                        if(ping) 
                        fpk_i = plm_out_ping[k];
                        else
                        fpk_i = plm_out_pong[k];
                        wait();
                        fpk_r = fpk_i>>32;
                        fpk_i = fpk_i & 0xFFFFFFFF;


                        // fpnk.r =   st->tmpbuf[ncfft-k].r;
                        // fpnk.i = - st->tmpbuf[ncfft-k].i;
                        fpnk_i = plm_out_ping[(outlenb2 - k)];
                        wait();
                        fpnk_r = fpnk_i >>32;
                        fpnk_i = -1 * (fpnk_i &0xFFFFFFFF);

                        // C_FIXDIV(fpk,2);
                        // C_FIXDIV(fpnk,2);
                        fpk_i = fpk_i >>1;
                        fpk_r = fpk_r >>1;
                        fpnk_i = fpnk_i >>1;
                        fpnk_r = fpnk_r >>1;


                        // C_ADD( f1k, fpk , fpnk );
                        int32_t f1k_r, f1k_i;
                        f1k_r = fpk_r + fpnk_r; 
                        f1k_i = fpk_i + fpnk_i; 

                        // C_SUB( f2k, fpk , fpnk );

                        int32_t f2k_r, f2k_i;
                        f2k_r = fpk_r + fpnk_r; 
                        f2k_i = fpk_i + fpnk_i; 

                        //??
                        // C_MUL( tw , f2k , st->super_twiddles[k-1]);
                        int32_t tw_i, tw_r;
                        tw_r = f2k_r;
                        tw_i = f2k_i;

                        // freqdata[k].r = HALF_OF(f1k.r + tw.r);
                        // freqdata[k].i = HALF_OF(f1k.i + tw.i);
                        int32_t item_i = f1k_r + tw_r; 
                        int64_t item_r = f1k_i + tw_i; 
                        item_r = item_r<<32;
                        item_r |= item_i;

                        // freqdata[ncfft-k].r = HALF_OF(f1k.r - tw.r);
                        // freqdata[ncfft-k].i = HALF_OF(tw.i - f1k.i);
                        int64_t lastitem_r = f1k_r - tw_r;
                        int32_t lastitem_i = tw_i - f1k_i;

                        // st->tmpbuf[ncfft - k].i *= -1;      
                        lastitem_i = lastitem_i*-1;
                        lastitem_r = lastitem_r<<32;
                        lastitem_r = lastitem_r | lastitem_i; 
                        ////
                        if(ping){
                           plm_out_ping[k] = item_r;
                           plm_out_ping[out_length - k] = lastitem_r;
                        }
                        else{
                           plm_out_pong[k] = item_r;
                           plm_out_pong[out_length - k] = lastitem_r; 
                        }
                        wait();
                    }
                }

                //  {
                //     HLS_PROTO("fir-preprocessing");
                //     for (int i = 0; i < out_length; i++) { 
                //         if(ping)
                //            plm_out_ping[i] = plm_in_ping[i];
                //         else
                //            plm_out_pong[i] = plm_in_pong[i]; 
                //     }
                // }

                // Computing phase implementation
                for (int i = 0; i < out_length; i++) {
                    int64_t data, filter, output;
                    int32_t data_r, data_i;
                    int32_t filter_r, filter_i; 
                    if (ping){
                        data = plm_out_ping[i];
                        // data_i = plm_in_ping[i+1];
                        filter = plm_in_ping[data_length+i];
                        // filter_i = plm_in_ping[data_length+i+1];  
                    }
                    else{
                        data = plm_out_pong[i];
                        // data_i = plm_in_pong[i+1];
                        filter = plm_in_pong[data_length+i];
                        // filter_i = plm_in_pong[data_length+i+1];  
                    }
                    data_i = data&0xFFFFFFFF;
                    data_r = data>>32;
                    filter_i = filter&0xFFFFFFFF;
                    filter_r = filter>>32;
                    wait();
                    //r
                    output = data_r*filter_r - data_i*filter_i;
                    // output = output_r;
                    output = output<<32;
                    wait();
                    //i
                    output |= ((data_r*filter_r + data_i*filter_i)&0xFFFFFFFF);
                    // output |= output_i;
                    wait();

                    if (ping){
                        plm_out_ping[i] = output;
                        // plm_out_ping[i+1] = output_i;
                    }
                    else{
                        plm_out_pong[i] = output;
                        // plm_out_pong[i+1] = output_i;
                    }
                }

                {
                    //HLS_PROTO("fir-post-processing");
                    int64_t first, last;

                    first = plm_out_ping[0];
                    last = plm_out_ping[out_length-1];
                    int32_t first_r, first_i, last_r, last_i;
                    first_i = first&0xFFFFFFFF;
                    first_r = first>>32; 
                    last_i = last&0xFFFFFFFF;
                    last_r = last>>32; 

    // st->tmpbuf[0].r = freqdata[0].r + freqdata[ncfft].r;
    // st->tmpbuf[0].i = freqdata[0].r - freqdata[ncfft].r;

                    first = (first_r + last_r);
                    first = first << 32;
                    first_i = first_r - last_r;

                    first = first | first_i;
                    wait();
                    plm_out_ping[0] = first; //plm_in_ping[0] + plm_in_ping[out_length-2]; 
                    // plm_out_ping[1] = plm_in_ping[0] - plm_in_ping[out_length-2]; 

                    for (int k = 1; k <= (out_length/2); k++) { 
                        ////
                        int64_t fk_i, fk_r, fnkc_i,fnkc_r, fek_r,fek_i, fok_i,fok_r, tmp_i, tmp_r;
                        fk_i = plm_out_ping[k];
                        wait();
                        fk_r = fk_i>>32;
                        fk_i = fk_i & 0xFFFFFFFF;


                        fnkc_i = plm_out_ping[(outlenb2 - k)];
                        wait();
                        fnkc_r = fnkc_i >>32;
                        fnkc_i = -1 * (fnkc_i &0xFFFFFFFF);

                        // C_FIXDIV( fk , 2 );
                        fk_i = fk_i >>1;
                        fk_r = fk_r >>1;

                        // C_FIXDIV( fnkc , 2 );
                        fnkc_i = fnkc_i>>1;
                        fnkc_r = fnkc_r>>1;

                        // C_ADD (fek, fk, fnkc);
                        fek_i = fk_i + fnkc_i;
                        fek_r = fk_r + fnkc_r;

                        // C_SUB (tmp, fk, fnkc);
                        tmp_i = fk_i - fnkc_i;
                        tmp_r = fk_r - fnkc_r;

                        // ??
                        // C_MUL (fok, tmp, st->super_twiddles[k-1]);
                        fok_i = tmp_i;
                        fok_r = tmp_r;

                        // C_ADD (st->tmpbuf[k],     fek, fok);
                        int32_t item_i = fek_i + fok_i; 
                        int64_t item_r = fek_r + fok_r; 
                        item_r = item_r<<32;
                        item_r |= item_i;

                        // C_SUB (st->tmpbuf[ncfft - k], fek, fok);
                        int32_t lastitem_i = fek_i - fok_i;
                        int64_t lastitem_r = fek_r - fok_r;

                        // st->tmpbuf[ncfft - k].i *= -1;      
                        lastitem_i = lastitem_i*-1;
                        lastitem_r = lastitem_r<<32;
                        lastitem_r = lastitem_r | lastitem_i; 
                        ////
                        if(ping){
                           plm_out_ping[k] = item_r;
                           plm_out_ping[out_length - k] = lastitem_r;
                        }
                        else{
                           plm_out_pong[k] = item_r;
                           plm_out_pong[out_length - k] = lastitem_r; 
                        }
                        wait();
                    }
                }

                out_rem -= PLM_OUT_WORD;
                this->compute_store_handshake();
                ping = !ping;
            }
        }

        // Conclude
        {
            this->process_done();
        }
    }
}
