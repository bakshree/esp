// Copyright (c) 2011-2022 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0

#include "fir.hpp"

// Optional application-specific helper functions

// complex number multiplication
// inline void compMul(const CompNum &x, const CompNum &y, CompNum &res)
// {
//     res.re = x.re * y.re - x.im * y.im;
//     res.im = x.re * y.im + x.im * y.re;
// }

// // complex number addition
// inline void compAdd(const CompNum &x, const CompNum &y, CompNum &res)
// {
//     res.re = x.re + y.re;
//     res.im = x.im + y.im;
// }

// // complex number substraction
// inline void compSub(const CompNum &x, const CompNum &y, CompNum &res)
// {
//     res.re = x.re - y.re;
//     res.im = x.im - y.im;
// }