// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#ifndef __TILED_APP_HPP__
#define __TILED_APP_HPP__

#include "tiled_app_conf_info.hpp"
#include "tiled_app_debug_info.hpp"

#include "esp_templates.hpp"

#include "tiled_app_directives.hpp"

#define __round_mask(x, y) ((y)-1)
#define round_up(x, y) ((((x)-1) | __round_mask(x, y))+1)
/* <<--defines-->> */
#define DATA_WIDTH 64
#define DMA_SIZE SIZE_DWORD
#define PLM_OUT_WORD 1024
#define PLM_IN_WORD 1024

class tiled_app : public esp_accelerator_3P<DMA_WIDTH>
{
public:
    // Constructor
    SC_HAS_PROCESS(tiled_app);
    tiled_app(const sc_module_name& name)
    : esp_accelerator_3P<DMA_WIDTH>(name)
        , cfg("config")
        , load_sync_done("load_sync_done")
        , store_sync_done("load_store_cfg_done")
        , load_next_tile("load_next_tile")
        , input_sync_ready("input_sync_ready")
        , output_sync_ready("output_sync_ready")
        // , dma_input_sync_read_ctrl("dma_input_sync_read_ctrl")
        // , dma_output_sync_read_ctrl("dma_output_sync_read_ctrl")
        // , dma_input_sync_read_chnl("dma_input_sync_read_chnl")
        // , dma_output_sync_read_chnl("dma_output_sync_read_chnl")
        // , dma_input_sync_write_ctrl("dma_input_sync_write_ctrl")
        // , dma_output_sync_write_ctrl("dma_output_sync_write_ctrl")
        // , dma_input_sync_write_chnl("dma_input_sync_write_chnl")
        // , dma_output_sync_write_chnl("dma_output_sync_write_chnl")
    {
        // Signal binding
        cfg.bind_with(*this);
	    load_sync_done.bind_with<DMA_WIDTH>(*this);
	    store_sync_done.bind_with<DMA_WIDTH>(*this);
	    load_next_tile.bind_with<DMA_WIDTH>(*this);
	    input_sync_ready.bind_with<DMA_WIDTH>(*this);
	    output_sync_ready.bind_with<DMA_WIDTH>(*this);
        
        // dma_input_sync_read_ctrl.clk_rst(this->clk, this->rst);
        // dma_output_sync_read_ctrl.clk_rst(this->clk, this->rst);
        // dma_input_sync_read_chnl.clk_rst(this->clk, this->rst);
        // dma_output_sync_read_chnl.clk_rst(this->clk, this->rst);
        // dma_input_sync_write_ctrl.clk_rst(this->clk, this->rst);
        // dma_output_sync_write_ctrl.clk_rst(this->clk, this->rst);
        // dma_input_sync_write_chnl.clk_rst(this->clk, this->rst);
        // dma_output_sync_write_chnl.clk_rst(this->clk, this->rst);       

        HLS_PRESERVE_SIGNAL(load_iter_dbg);
        HLS_PRESERVE_SIGNAL(store_iter_dbg);
        HLS_PRESERVE_SIGNAL(load_state_dbg);
        HLS_PRESERVE_SIGNAL(store_state_dbg);
        HLS_PRESERVE_SIGNAL(load_unit_sp_write_dbg);
        HLS_PRESERVE_SIGNAL(store_unit_sp_read_dbg);
        // Map arrays to memories
        /* <<--plm-bind-->> */
        //HLS_MAP_plm(plm, PLM_OUT_NAME);

        HLS_MAP_plm(plm_op_pong, PLM_OP_NAME);
        HLS_MAP_plm(plm_op_ping, PLM_OP_NAME);
        HLS_MAP_plm(plm_in_pong, PLM_OUT_NAME);
        HLS_MAP_plm(plm_in_ping, PLM_OUT_NAME);
        HLS_MAP_plm(plm_out_pong, PLM_OUT_NAME);
        HLS_MAP_plm(plm_out_ping, PLM_OUT_NAME);
        // HLS_MAP_plm(plm_in_pong, PLM_IN_NAME);
        // HLS_MAP_plm(plm_in_ping, PLM_IN_NAME);
        // HLS_MAP_plm(plm_out_pong, PLM_OUT_NAME);
        // HLS_MAP_plm(plm_out_ping, PLM_OUT_NAME);
        // HLS_MAP_plm(plm_in_pong, PLM_IN_NAME);
        // HLS_MAP_plm(plm_in_ping, PLM_IN_NAME);
    }

    // Processes

    sc_signal< sc_int<64> > load_iter_dbg;
    sc_signal< sc_int<64> > store_iter_dbg;
    sc_signal< sc_int<64> > load_state_dbg;
    sc_signal< sc_int<64> > store_state_dbg;
    sc_signal< sc_int<64> > load_unit_sp_write_dbg;
    sc_signal< sc_int<64> > store_unit_sp_read_dbg;
    // Load the input data
    void load_input();

    // Computation
    void compute_kernel();

    // Store the output data
    void store_output();

    // Spin on Input Sync
    void input_sync_spin();

    // // Spin on Output Sync
    // void output_sync_spin();

    // // Spin on Input Sync
    // void input_sync_set();

    // // Spin on Output Sync
    // void output_sync_set();

    // Configure tiled_app
    esp_config_proc cfg;


    // Custom handshakes
    // handshake_t load_store_cfg_done;
    handshake_t load_sync_done;
    handshake_t store_sync_done;
    handshake_t load_next_tile;

    handshake_t input_sync_ready;
    handshake_t output_sync_ready;


    // Functions
    
    inline void load_sync_done_req();
    inline void load_sync_done_ack();
    inline void store_sync_done_req();
    inline void store_sync_done_ack();
    inline void load_next_tile_req(); 
    inline void load_next_tile_ack();

    // inline void reset_dma_input_sync_read();
    // inline void reset_dma_input_sync_write();
    // inline void reset_dma_output_sync_read();
    // inline void reset_dma_output_sync_write();

    // // DMA read control
    // b_put_initiator<dma_info_t> dma_input_sync_read_ctrl;
    // b_put_initiator<dma_info_t> dma_output_sync_read_ctrl;

    // // DMA read channel
    // get_initiator<sc_dt::sc_bv<DMA_WIDTH> > dma_input_sync_read_chnl;
    // get_initiator<sc_dt::sc_bv<DMA_WIDTH> > dma_output_sync_read_chnl;

    // // DMA write control
    // b_put_initiator<dma_info_t> dma_input_sync_write_ctrl;
    // b_put_initiator<dma_info_t> dma_output_sync_write_ctrl;

    // // DMA write channel
    // put_initiator<sc_dt::sc_bv<DMA_WIDTH> > dma_input_sync_write_chnl;
    // put_initiator<sc_dt::sc_bv<DMA_WIDTH> > dma_output_sync_write_chnl;


    // Configuration handshakes
    // inline void load_store_cfg_handshake();
    // inline void store_load_cfg_handshake();

    // Private local memories
    // sc_dt::sc_int<DATA_WIDTH> plm[PLM_IN_WORD];

    sc_dt::sc_int<DATA_WIDTH> plm_op_ping[PLM_IN_WORD];
    sc_dt::sc_int<DATA_WIDTH> plm_op_pong[PLM_IN_WORD];
    sc_dt::sc_int<DATA_WIDTH> plm_in_ping[PLM_IN_WORD];
    sc_dt::sc_int<DATA_WIDTH> plm_in_pong[PLM_IN_WORD];
    sc_dt::sc_int<DATA_WIDTH> plm_out_ping[PLM_OUT_WORD];
    sc_dt::sc_int<DATA_WIDTH> plm_out_pong[PLM_OUT_WORD];

};


#endif /* __TILED_APP_HPP__ */
