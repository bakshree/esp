// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#ifndef __NMF_FPDATA_HPP__
#define __NMF_FPDATA_HPP__

#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <systemc.h>

// Stratus fixed point

#include "cynw_fixed.h"
#define FX32_IL 14
// Data types


const unsigned int FPDATA_WL = 32;

const unsigned int FPDATA_IL = FX32_IL;

const unsigned int FPDATA_PL = (FPDATA_WL - FPDATA_IL);


typedef cynw_fixed<FPDATA_WL, FPDATA_IL, SC_RND> FPDATA;


// Helper functions

// T <---> sc_dt::sc_int<N>

FPDATA int2fp(int64_t data_in)
{   
    sc_dt::sc_int<1> d_int = data_in;
    FPDATA data_out;

    {
        // HLS_CONSTRAIN_LATENCY(0, HLS_ACHIEVABLE, "int2fp1");

        // for (unsigned i = 0; i < N; i++)
        // {
            // HLS_UNROLL_LOOP(ON, "int2fp-loop");

            data_out = d_int[0].to_bool();
        // }
    }

    return data_out;
}


sc_dt::sc_int<1> fp2int(FPDATA data_in)
{
    sc_dt::sc_int<1> data_out;

    {
        // HLS_CONSTRAIN_LATENCY(0, HLS_ACHIEVABLE, "fp2int1");

        // for (unsigned i = 0; i < N; i++)
        // {
        //     HLS_UNROLL_LOOP(ON, "fp2int-loop");

            data_out = (bool) data_in;
        // }
    }

    return data_out;
}

#endif // __NMF_FPDATA_HPP__
