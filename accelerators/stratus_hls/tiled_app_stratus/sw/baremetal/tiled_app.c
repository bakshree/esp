/* Copyright (c) 2011-2021 Columbia University, System Level Design Group */
/* SPDX-License-Identifier: Apache-2.0 */

// input /scratch/projects/bmishra3/spandex/esp_tiled_acc/socs/xilinx-vcu118-xcvu9p/restore.tcl.svcf

#include <stdio.h>
#ifndef __riscv
#include <stdlib.h>
#endif

#include <esp_accelerator.h>
#include <esp_probe.h>
#include <fixed_point.h>

typedef int64_t token_t;

static unsigned DMA_WORD_PER_BEAT(unsigned _st)
{
        return (sizeof(void *) / _st);
}


#define SLD_TILED_APP 0x033
#define DEV_NAME "sld,tiled_app_stratus"

/* <<--params-->> */
const int32_t num_tiles = 2;//12;
const int32_t tile_size = 5;
const int32_t rd_wr_enable = 0;

static unsigned in_words_adj;
static unsigned out_words_adj;
static unsigned in_len;
static unsigned out_len;
static unsigned in_size;
static unsigned out_size;
static unsigned out_offset;
static unsigned mem_size;

static unsigned coherence;

/* Size of the contiguous chunks for scatter/gather */
#define CHUNK_SHIFT 20
#define CHUNK_SIZE BIT(CHUNK_SHIFT)
#define NCHUNK(_sz) ((_sz % CHUNK_SIZE == 0) ?		\
			(_sz / CHUNK_SIZE) :		\
			(_sz / CHUNK_SIZE) + 1)

/* User defined registers */
/* <<--regs-->> */
#define TILED_APP_NUM_TILES_REG 0x48
#define TILED_APP_TILE_SIZE_REG 0x44
#define TILED_APP_RD_WR_ENABLE_REG 0x40


int load_mem(token_t *in, token_t *gold, int* tile){
	int offset = 2;
	if(in[0]!=1 ){ //&& *tile < num_tiles){
		printf("In  sync: %d Loading tile %d: ",in[0], *tile);
		for (int j = 0; j < tile_size; j++)
			in[offset + j] = 2*((*tile) * in_words_adj + j);


        asm volatile ("fence rw, rw");

		// for(int i = 0; i< tile_size; i++)
		// 	printf("%d, ", 2*((*tile) * in_words_adj + i));

		// printf("\n");

		in[0] = 1;

		// asm volatile ("fence rw, rw");
		*tile = *tile + 1;
		return 1;
	}
	return 0;
}


int store_mem(token_t *out, token_t* mem, int offset, int* tile){
	if(mem[1]==1 ){//&& *tile < num_tiles){
		printf("Out sync: %d Storing tile %d at offset %d\n", mem[1], *tile, (*tile) * out_words_adj);
		for (int j = 0; j < tile_size; j++)
			out[(*tile) * out_words_adj + j] = mem[offset + j];

        asm volatile ("fence rw, rw");
		// for(int i = 0; i< tile_size; i++)
		// 	printf("%d, ", mem[offset + i]);

		// printf("\n");

		mem[1] = 0;

        // asm volatile ("fence rw, rw");
		*tile = (*tile) + 1;
		return 1;
	}
	return 0;
}

static int validate_buf(token_t *out, token_t *gold)
{
	int i;
	int j;
	unsigned errors = 0;
	int loc = 0;
	printf("num tiles %d -- tile size %d \n", num_tiles, tile_size);
	for (i = 0; i < num_tiles; i++)
		for (j = 0; j < tile_size; j++){
			if (gold[i * out_words_adj + j] != out[i * out_words_adj + j]){
				printf("tile: %d loc:%d gold: %d -- found: %d \n",i, loc, gold[i * out_words_adj + j], out[i * out_words_adj + j]);
				errors++;
			}
			loc++;
		}

	return errors;
}


static void init_buf (token_t *in, token_t * gold, token_t* out)
{
	int i;
	int j;
	int offset = 2;
	in[0] = 0;
	in[1] = 0;
	// for (i = 0; i < tile_size; i++){
	// 	in[offset + i] = 0;
	// 	in[out_offset + i] = 0;
	// }
	// for (i = 0; i < num_tiles; i++)
	// 	for (j = 0; j < tile_size; j++)
	// 		in[offset + i * in_words_adj + j] = (token_t) j;
	// //in[0] = 1;
	int seq = 0;
	// printf("Gold: ");
	for (i = 0; i < num_tiles; i++)
		for (j = 0; j < tile_size; j++){
			gold[i * out_words_adj + j] = (token_t) 2*(seq++);
			// out[i * out_words_adj + j] = 0;
			//printf("%d, ", gold[i * out_words_adj + j]);
		}
	
    // asm volatile ("fence rw, rw");
	printf("Init buf complete\n");
}


int main(int argc, char * argv[])
{
	// printf("Hello World 123\n");

	#if 1
	int i;
	int n;
	int ndev;
	struct esp_device *espdevs;
	struct esp_device *dev;
	unsigned done;
	unsigned **ptable;
	token_t *mem;
	token_t *gold;
	token_t *out;
	unsigned errors = 0;
	// printf("Checkpoint 1\n");

	if (DMA_WORD_PER_BEAT(sizeof(token_t)) == 0) {
		in_words_adj = tile_size;
		out_words_adj = tile_size;
	} else {
		in_words_adj = round_up(tile_size, DMA_WORD_PER_BEAT(sizeof(token_t)));
		out_words_adj = round_up(tile_size, DMA_WORD_PER_BEAT(sizeof(token_t)));
	}
	// printf("Checkpoint 2\n");

	in_len = in_words_adj+2;
	out_len = out_words_adj;
	in_size = in_len * sizeof(token_t);
	out_size = out_len * sizeof(token_t);
	out_offset  = in_len;
	mem_size = (out_offset * sizeof(token_t)) + out_size;

	// printf("Checkpoint 3\n");


	printf("num_tiles      =  %u\n ",num_tiles     	); //num_tiles);
	printf("in_words_adj   =  %u\n ",in_words_adj   ); //num_tiles);
	printf("out_words_adj  =  %u\n ",out_words_adj  ); //num_tiles);
	printf("in_len         =  %u\n ",in_len     	); //num_tiles);
	printf("out_len        =  %u\n ",out_len   		); // (num_tiles);
	printf("in_size        =  %u\n ",in_size    	); //(token_t);
	printf("out_size       =  %u\n ",out_size   	); //of(token_t);
	printf("out_offset     =  %u\n ",out_offset 	); //
	printf("mem_size       =  %u\n ",mem_size   	); //sizeof(token_t)) + out_size;

	// Search for the device
	printf("Scanning device tree... \n");

	ndev = probe(&espdevs, VENDOR_SLD, SLD_TILED_APP, DEV_NAME);
	if (ndev == 0) {
		printf("tiled_app not found\n");
		return 0;
	}

	for (n = 0; n < ndev; n++) {

		printf("**************** %s.%d ****************\n", DEV_NAME, n);

		dev = &espdevs[n];

		// Check DMA capabilities
		if (ioread32(dev, PT_NCHUNK_MAX_REG) == 0) {
			printf("  -> scatter-gather DMA is disabled. Abort.\n");
			return 0;
		}

		if (ioread32(dev, PT_NCHUNK_MAX_REG) < NCHUNK(mem_size)) {
			printf("  -> Not enough TLB entries available. Abort.\n");
			return 0;
		}

		// Allocate memory
		gold = aligned_malloc(out_size*num_tiles);
		out = aligned_malloc(out_size*num_tiles);
		mem = aligned_malloc(mem_size);
		printf("  memory buffer base-address = %p\n", mem);

		// Alocate and populate page table
		ptable = aligned_malloc(NCHUNK(mem_size) * sizeof(unsigned *));
		for (i = 0; i < NCHUNK(mem_size); i++)
			ptable[i] = (unsigned *) &mem[i * (CHUNK_SIZE / sizeof(token_t))];

		printf("  ptable = %p\n", ptable);
		printf("  nchunk = %lu\n", NCHUNK(mem_size));

        asm volatile ("fence rw, rw");
		//#ifndef __riscv
		// 		for (coherence = ACC_COH_NONE; coherence <= ACC_COH_FULL; coherence++) {
		// #else
		{
			/* TODO: Restore full test once ESP caches are integrated */
			coherence = ACC_COH_FULL;
		// #endif
			printf("  --------------------\n");
			printf("  Generate input...\n");
			init_buf(mem, gold, out);
			//bmmishra3
        	asm volatile ("fence rw, rw");
			// Pass common configuration parameters

			iowrite32(dev, SELECT_REG, ioread32(dev, DEVID_REG));
			iowrite32(dev, COHERENCE_REG, coherence);

#ifndef __sparc
			iowrite32(dev, PT_ADDRESS_REG, (unsigned long long) ptable);
#else
			iowrite32(dev, PT_ADDRESS_REG, (unsigned) ptable);
#endif
			iowrite32(dev, PT_NCHUNK_REG, NCHUNK(mem_size));
			iowrite32(dev, PT_SHIFT_REG, CHUNK_SHIFT);

			// Use the following if input and output data are not allocated at the default offsets
			iowrite32(dev, SRC_OFFSET_REG, 0x0);
			iowrite32(dev, DST_OFFSET_REG, 0x0);

			// Pass accelerator-specific configuration parameters
			/* <<--regs-config-->> */
			iowrite32(dev, TILED_APP_NUM_TILES_REG, num_tiles);
			iowrite32(dev, TILED_APP_TILE_SIZE_REG, tile_size);
			iowrite32(dev, TILED_APP_RD_WR_ENABLE_REG, rd_wr_enable);

			// Flush (customize coherence model here)
			esp_flush(coherence);

			// Start accelerators
			printf("  Start...\n");
			iowrite32(dev, CMD_REG, CMD_MASK_START);
			int read_tile = 0;
			int write_tile = 0;
			// Wait for completion
			done = 0;
			int done_prev = 0;
			int temp_rd_wr_enable = 0;
			int store_done_prev = 0;
        	int load_turn = 1;
			// int tiles_read = 0;
			while ( write_tile < num_tiles) {
			// while ( !done) {
				done = ioread32(dev, STATUS_REG);
				// tiles_read = ioread32(dev, TILED_APP_NUM_TILES_REG);
				// load_mem(mem, &tile); 
				// store_mem(mem, &tile); 
				// temp_rd_wr_enable = ioread32(dev, TILED_APP_RD_WR_ENABLE_REG);
				// printf("RD WR EN: %d\n", temp_rd_wr_enable);
				// //esp_flush(coherence);
				// //if(done != done_prev || store_done_prev != mem[1]){
				// 	done_prev = done;
				// 	store_done_prev = mem[1];
				// 	printf("Done: %d, Store Done bit: %d\n", done, mem[1]);
				// //}
				// printf("tiles: %d load_turn: %d in_sync:%d out_sync:%d read_tile:%d write_tile: %d done: %d\n",num_tiles, load_turn, mem[0], mem[1], read_tile, write_tile, done);
				load_mem(mem, gold, &read_tile);
				store_mem(out, mem, out_offset, &write_tile);
				// if((load_turn && load_mem(mem, gold, &read_tile))||((!load_turn) && store_mem(out, mem, out_offset, &write_tile)) ){
				// 	load_turn = !load_turn;
				// 	//printf("load_turn: %d read_tile: %d done: %d\n", load_turn, read_tile, done);
				// // printf("tiles: %d load_turn: %d in_sync:%d out_sync:%d read_tile:%d write_tile: %d done: %d\n",tiles_read, load_turn, mem[0], mem[1], read_tile, write_tile, done);
				// }

				

				done &= STATUS_MASK_DONE;
				// if(load_turn && load_mem(mem, gold, &read_tile)){
				// 	load_turn = !load_turn;
				// 	printf("load_turn: %d read_tile: %d done: %d\n", load_turn, read_tile, done);
				// }
				// if((!load_turn) && store_mem(out, mem, out_offset, &write_tile)){
				// 	load_turn = !load_turn;
				// 	printf("load_turn: %d write_tile: %d done: %d\n", load_turn, write_tile, done);
				// }

				asm volatile ("fence rw, rw");

			}
			//What is this doing?
			// iowrite32(dev, CMD_REG, 0x0);
			// printf("Load Done bit: %d\n", mem[0]);
			// printf("Store Done bit: %d\n", mem[1]);
			// for(int i = 0; i< in_len; i++)
			// 	printf("in bit %d: %d\n", i-2, mem[i]);
			// for(int i = 0; i< num_tiles*tile_size; i++)
			// 	printf("out bit %d: %d\n", i, out[i]);
			
			printf("  Done\n");
			printf("  validating...\n");

			/* Validation */
			//errors = validate_buf(&mem[out_offset], gold);
			errors = validate_buf(out, gold);
			if (errors)
				printf("  ... FAIL\n");
			else
				printf("  ... PASS\n");
		}
		aligned_free(out);
		aligned_free(ptable);
		aligned_free(mem);
		aligned_free(gold);
	}
	#endif

	return 0;
}
