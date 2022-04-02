// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0
#ifndef __ESP_CFG_000_H__
#define __ESP_CFG_000_H__

#include "libesp.h"
#include "tiled_acc_stratus.h"

typedef int64_t token_t;

/* <<--params-def-->> */
#define NUM_TILES 1
#define TILE_SIZE 16

/* <<--params-->> */
const int32_t num_tiles = NUM_TILES;
const int32_t tile_size = TILE_SIZE;

#define NACC 1

struct tiled_acc_stratus_access tiled_acc_cfg_000[] = {
	{
		/* <<--descriptor-->> */
		.num_tiles = NUM_TILES,
		.tile_size = TILE_SIZE,
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
		.devname = "tiled_acc_stratus.0",
		.ioctl_req = TILED_ACC_STRATUS_IOC_ACCESS,
		.esp_desc = &(tiled_acc_cfg_000[0].esp),
	}
};

#endif /* __ESP_CFG_000_H__ */
