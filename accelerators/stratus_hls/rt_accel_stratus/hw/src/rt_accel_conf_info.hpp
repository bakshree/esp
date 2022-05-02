// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#ifndef __RT_ACCEL_CONF_INFO_HPP__
#define __RT_ACCEL_CONF_INFO_HPP__

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
        this->img_width = 40;
        this->img_height = 40;
    }

    conf_info_t(
        /* <<--ctor-args-->> */
        int32_t img_width, 
        int32_t img_height
        )
    {
        /* <<--ctor-custom-->> */
        this->img_width = img_width;
        this->img_height = img_height;
    }

    // equals operator
    inline bool operator==(const conf_info_t &rhs) const
    {
        /* <<--eq-->> */
        if (img_width != rhs.img_width) return false;
        if (img_height != rhs.img_height) return false;
        return true;
    }

    // assignment operator
    inline conf_info_t& operator=(const conf_info_t& other)
    {
        /* <<--assign-->> */
        img_width = other.img_width;
        img_height = other.img_height;
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
        os << "img_width = " << conf_info.img_width << ", ";
        os << "img_height = " << conf_info.img_height << "";
        os << "}";
        return os;
    }

        /* <<--params-->> */
        int32_t img_width;
        int32_t img_height;
};

#endif // __RT_ACCEL_CONF_INFO_HPP__
