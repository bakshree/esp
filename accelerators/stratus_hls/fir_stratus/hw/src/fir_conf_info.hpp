// Copyright (c) 2011-2022 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#ifndef __FIR_CONF_INFO_HPP__
#define __FIR_CONF_INFO_HPP__

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
        this->data_length = 1024;
    }

    conf_info_t(
        /* <<--ctor-args-->> */
        int32_t data_length
        )
    {
        /* <<--ctor-custom-->> */
        this->data_length = data_length;
    }

    // equals operator
    inline bool operator==(const conf_info_t &rhs) const
    {
        /* <<--eq-->> */
        if (data_length != rhs.data_length) return false;
        return true;
    }

    // assignment operator
    inline conf_info_t& operator=(const conf_info_t& other)
    {
        /* <<--assign-->> */
        data_length = other.data_length;
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
        os << "data_length = " << conf_info.data_length << "";
        os << "}";
        return os;
    }

        /* <<--params-->> */
        int32_t data_length;
};

#endif // __FIR_CONF_INFO_HPP__
