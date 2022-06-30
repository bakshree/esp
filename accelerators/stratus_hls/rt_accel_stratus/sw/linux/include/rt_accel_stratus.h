// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0
#ifndef _RT_ACCEL_STRATUS_H_
#define _RT_ACCEL_STRATUS_H_

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
#else
#include <sys/ioctl.h>
#include <stdint.h>
#include <math.h>
// #include <iostream.h>
#ifndef __user
#define __user
#endif
#endif /* __KERNEL__ */

#include <esp.h>
#include <esp_accelerator.h>

typedef int FPDATA;


// struct vec3 { //12 bytes
//     FPDATA x ;//= 0;
//     FPDATA y ;//= 0;
//     FPDATA z ;//= 0;

//     vec3()// = default; 
//     {
//         this->x = 0;
//         this->y = 0;
//         this->z = 0;
//     }
//     vec3(FPDATA a, FPDATA b, FPDATA c){
//         this->x = a;
//         this->y = b;
//         this->z = c;
//     }
// };



// float norm(float x, float y, float z)  {
//     return std::sqrt(x * x + y * y + z * z);
// }

// vec3 normalized(float x, float y, float z)  {
//     float n = norm(x, y, z);
//     //return (*this) * (1.f / norm());
//     return vec3(float_to_fixed32(x/n, 14), float_to_fixed32(y/n, 14), float_to_fixed32(z/n), 14);
//     //return (*this) * (1.f / norm());
// }



struct rt_accel_stratus_access {
	struct esp_access esp;
	/* <<--regs-->> */
	unsigned img_width;
	unsigned img_height;
	unsigned src_offset;
	unsigned dst_offset;
};

#define RT_ACCEL_STRATUS_IOC_ACCESS	_IOW ('S', 0, struct rt_accel_stratus_access)

#endif /* _RT_ACCEL_STRATUS_H_ */
