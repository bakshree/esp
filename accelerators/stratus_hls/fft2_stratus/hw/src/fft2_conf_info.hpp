// Copyright (c) 2011-2019 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#ifndef __FFT2_CONF_INFO_HPP__
#define __FFT2_CONF_INFO_HPP__

#include <systemc.h>

//
// Configuration parameters for the accelerator.
//
class conf_info_t
{
public:

    //
    // constructors
    //
    conf_info_t()
    {
        /* <<--ctor-->> */
        this->output_tile_start_offset = 0;
        this->input_tile_start_offset  = 0;
        this->output_update_sync_offset  = 0;
        this->input_update_sync_offset   = 0;
        this->output_spin_sync_offset    = 0;
        this->input_spin_sync_offset     = 0;
        this->num_tiles = 12;
        this->tile_size = 1024;
        this->logn_samples = 6;
        this->num_ffts = 1;
        this->do_inverse = 0;
        this->do_shift = 0;
        this->scale_factor = 1;
    }

    conf_info_t(
        /* <<--ctor-args-->> */
        int32_t output_tile_start_offset,
        int32_t input_tile_start_offset ,
        int32_t output_update_sync_offset,
        int32_t input_update_sync_offset,
        int32_t output_spin_sync_offset,  
        int32_t input_spin_sync_offset,           
        int32_t num_tiles, 
        int32_t tile_size, 
        int32_t logn_samples,
        int32_t num_ffts,
        int32_t do_inverse,
        int32_t do_shift,
        int32_t scale_factor
        )
    {
        /* <<--ctor-custom-->> */
        this->output_tile_start_offset = output_tile_start_offset;
        this->input_tile_start_offset  = input_tile_start_offset ;
        this->output_update_sync_offset = output_update_sync_offset;
        this->input_update_sync_offset  = input_update_sync_offset ;
        this->output_spin_sync_offset   = output_spin_sync_offset  ;
        this->input_spin_sync_offset    = input_spin_sync_offset   ;
        this->num_tiles = num_tiles;
        this->tile_size = tile_size;
        this->logn_samples = logn_samples;
        this->num_ffts   = num_ffts;
        this->do_inverse = do_inverse;
        this->do_shift   = do_shift;
        this->scale_factor = scale_factor;
    }

    // equals operator
    inline bool operator==(const conf_info_t &rhs) const
    {
        /* <<--eq-->> */
        if (output_tile_start_offset != rhs.output_tile_start_offset ) return false;
        if (input_tile_start_offset  != rhs.input_tile_start_offset  ) return false;       
        if (output_update_sync_offset!= rhs.output_update_sync_offset) return false;
        if (input_update_sync_offset != rhs.input_update_sync_offset ) return false;
        if (output_spin_sync_offset  != rhs.output_spin_sync_offset  ) return false;
        if (input_spin_sync_offset   != rhs.input_spin_sync_offset   ) return false;
        if (num_tiles != rhs.num_tiles) return false;
        if (tile_size != rhs.tile_size) return false;
        if (logn_samples != rhs.logn_samples) return false;
        if (num_ffts != rhs.num_ffts) return false;
        if (do_inverse != rhs.do_inverse) return false;
        if (do_shift != rhs.do_shift) return false;
        if (scale_factor != rhs.scale_factor) return false;
        return true;
    }

    // assignment operator
    inline conf_info_t& operator=(const conf_info_t& other)
    {
        /* <<--assign-->> */
        output_tile_start_offset = other.output_tile_start_offset;
        input_tile_start_offset  = other.input_tile_start_offset ;
        output_update_sync_offset   = other.output_update_sync_offset;
        input_update_sync_offset    = other.input_update_sync_offset ;
        output_spin_sync_offset     = other.output_spin_sync_offset  ;
        input_spin_sync_offset      = other.input_spin_sync_offset   ;
        num_tiles = other.num_tiles;
        tile_size = other.tile_size;
        logn_samples = other.logn_samples;
        num_ffts = other.num_ffts;
        do_inverse = other.do_inverse;
        do_shift = other.do_shift;
        scale_factor = other.scale_factor;
        return *this;
    }

    // VCD dumping function
    friend void sc_trace(sc_trace_file *tf, const conf_info_t &v, const std::string &NAME)
    {}

    // redirection operator
    friend ostream& operator << (ostream& os, conf_info_t const &conf_info)
    {
        os << "{";
        /* <<--print-->> */
        os << "output_tile_start_offset = " << conf_info.output_tile_start_offset;
        os << "input_tile_start_offset = " << conf_info.input_tile_start_offset ;
        os << "output_update_sync_offset = " << conf_info.output_update_sync_offset<< ", ";
        os << "input_update_sync_offset = " << conf_info.input_update_sync_offset<< ", ";
        os << "output_spin_sync_offset = " << conf_info.output_spin_sync_offset<< ", ";
        os << "input_spin_sync_offset = " << conf_info.input_spin_sync_offset<< ", ";
        os << "num_tiles = " << conf_info.num_tiles << ", ";
        os << "tile_size = " << conf_info.tile_size << ", ";
        os << "logn_samples = " << conf_info.logn_samples << ", ";
        os << "num_ffts = " << conf_info.num_ffts << ", ";
        os << "do_inverse = " << conf_info.do_inverse << ", ";
        os << "do_shift = " << conf_info.do_shift << ", ";
        os << "scale_factor = " << conf_info.scale_factor << "";
        os << "}";
        return os;
    }

    /* <<--params-->> */
    int32_t output_tile_start_offset;
    int32_t input_tile_start_offset;
    int32_t output_update_sync_offset;
    int32_t input_update_sync_offset ;
    int32_t output_spin_sync_offset  ;
    int32_t input_spin_sync_offset   ;
    int32_t num_tiles;
    int32_t tile_size;
    int32_t logn_samples;
    int32_t num_ffts;
    int32_t do_inverse;
    int32_t do_shift;
    int32_t scale_factor;
};

#endif // __FFT2_CONF_INFO_HPP__
