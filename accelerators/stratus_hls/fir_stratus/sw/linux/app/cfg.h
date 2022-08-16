// Copyright (c) 2011-2022 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0
#ifndef __ESP_CFG_000_H__
#define __ESP_CFG_000_H__

#include "libesp.h"
#include "fir_stratus.h"

typedef int64_t token_t;

/* <<--params-def-->> */
#define DATA_LENGTH 1024

/* <<--params-->> */
const int32_t data_length = DATA_LENGTH;

#define NACC 1

struct fir_stratus_access fir_cfg_000[] = {
	{
		/* <<--descriptor-->> */
		.data_length = DATA_LENGTH,
		.src_offset = 0,
		.dst_offset = 0,
		.esp.coherence = ACC_COH_NONE,
		.esp.p2p_store = 0,
		.esp.p2p_nsrcs = 0,
		.esp.p2p_srcs = {"", "", "", ""},
	}
};

esp_thread_info_t cfg_000[] = {
	{
		.run = true,
		.devname = "fir_stratus.0",
		.ioctl_req = FIR_STRATUS_IOC_ACCESS,
		.esp_desc = &(fir_cfg_000[0].esp),
	}
};

#endif /* __ESP_CFG_000_H__ */
