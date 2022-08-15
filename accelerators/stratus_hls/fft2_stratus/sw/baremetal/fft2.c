/* Copyright (c) 2011-2019 Columbia University, System Level Design Group */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#ifndef __riscv
#include <stdlib.h>
#endif

#include <esp_accelerator.h>
#include <esp_probe.h>
#include "utils/fft2_utils.h"

#define NUM_DEVICES 1
#define PRINT_DEBUG
// #define VALIDATE
// #define MEM_DUMP 1
#define NUM_TILES 1
#define TILE_SIZE 2048
#define SYNC_BITS 1
#define SYNC_VAR_SIZE 4

const int32_t num_tiles = NUM_TILES;//12;
const int32_t tile_size = TILE_SIZE;

static unsigned	int read_tile ;
static unsigned	int write_tile;

/* Coherence Modes */
#define COH_MODE 3
#define ESP

#ifdef ESP
// ESP COHERENCE PROTOCOLS
#define READ_CODE 0x0002B30B
#define WRITE_CODE 0x0062B02B
#if(COH_MODE == 3)
#define COHERENCE_MODE ACC_COH_NONE
#elif (COH_MODE == 2)
#define COHERENCE_MODE ACC_COH_LLC
#elif (COH_MODE == 1)
#define COHERENCE_MODE ACC_COH_RECALL
#else
#define COHERENCE_MODE ACC_COH_FULL
#endif
#else
//SPANDEX COHERENCE PROTOCOLS
#define COHERENCE_MODE ACC_COH_FULL
#if (COH_MODE == 3)
// Owner Prediction
#define READ_CODE 0x4002B30B
#define WRITE_CODE 0x2262B82B
// spandex_config_t spandex_config = {.spandex_reg = 0, .r_en = 1, .r_type = 2, .w_en = 1, .w_op = 1, .w_type = 1};
#elif (COH_MODE == 2)
// Write-through forwarding
#define READ_CODE 0x4002B30B
#define WRITE_CODE 0x2062B02B
// spandex_config_t spandex_config = {.spandex_reg = 0, .r_en = 1, .r_type = 2, .w_en = 1, .w_type = 1};
#elif (COH_MODE == 1)
// Baseline Spandex
#define READ_CODE 0x2002B30B
#define WRITE_CODE 0x0062B02B
// spandex_config_t spandex_config = {.spandex_reg = 0, .r_en = 1, .r_type = 1};
#else
// Fully Coherent MESI
#define READ_CODE 0x0002B30B
#define WRITE_CODE 0x0062B02B
// spandex_config_t spandex_config = {.spandex_reg = 0};
#endif
#endif


	unsigned coherence;
int32_t accel_read_sync_spin_offset[NUM_DEVICES];
int32_t accel_write_sync_spin_offset[NUM_DEVICES];
int32_t accel_read_sync_write_offset[NUM_DEVICES];
int32_t accel_write_sync_write_offset[NUM_DEVICES];
int32_t input_buffer_offset[NUM_DEVICES];
int32_t output_buffer_offset[NUM_DEVICES];
int32_t num_dev = NUM_DEVICES;

uint64_t start_write;
uint64_t stop_write;
uint64_t intvl_write;
uint64_t start_read;
uint64_t stop_read;
uint64_t intvl_read;
uint64_t start_flush;
uint64_t stop_flush;
uint64_t intvl_flush;
uint64_t start_tiling;
uint64_t stop_tiling;
uint64_t intvl_tiling;
uint64_t start_sync;
uint64_t stop_sync;
uint64_t intvl_sync;
uint64_t spin_flush;

uint64_t start_acc_write;
uint64_t stop_acc_write;
uint64_t intvl_acc_write;
uint64_t start_acc_read;
uint64_t stop_acc_read;
uint64_t intvl_acc_read;

#if (FFT2_FX_WIDTH == 64)
typedef long long token_t;
typedef double native_t;
#define fx2float fixed64_to_double
#define float2fx double_to_fixed64
#define FX_IL 42
#else // (FFT2_FX_WIDTH == 32)
typedef int token_t;
typedef float native_t;
#define fx2float fixed32_to_float
#define float2fx float_to_fixed32
#define FX_IL 14
#endif /* FFT2_FX_WIDTH */

const float ERR_TH = 0.05;

token_t *mem;

static unsigned DMA_WORD_PER_BEAT(unsigned _st)
{
        return (sizeof(void *) / _st);
}


#define SLD_FFT2 0x057
#define DEV_NAME "sld,fft2_stratus"

/* <<--params-->> */
const int32_t logn_samples = 3;
const int32_t num_samples = (1 << logn_samples);
const int32_t num_ffts = 1;
const int32_t do_inverse = 0;
const int32_t do_shift = 0;
const int32_t scale_factor = 1;
int32_t len;

static unsigned in_words_adj;
static unsigned out_words_adj;
static unsigned in_len;
static unsigned out_len;
static unsigned in_size;
static unsigned out_size;
static unsigned out_offset;
static unsigned mem_size;

/* Size of the contiguous chunks for scatter/gather */
#define CHUNK_SHIFT 20
#define CHUNK_SIZE BIT(CHUNK_SHIFT)
#define NCHUNK(_sz) ((_sz % CHUNK_SIZE == 0) ?		\
			(_sz / CHUNK_SIZE) :		\
			(_sz / CHUNK_SIZE) + 1)

/* User defined registers */
/* <<--regs-->> */
#define FFT2_LOGN_SAMPLES_REG 0x40
#define FFT2_NUM_FFTS_REG 0x44
#define FFT2_DO_INVERSE_REG 0x48
#define FFT2_DO_SHIFT_REG 0x4c
#define FFT2_SCALE_FACTOR_REG 0x50

#define TILED_APP_OUTPUT_TILE_START_OFFSET_REG 0x70
#define TILED_APP_INPUT_TILE_START_OFFSET_REG 0x6C
#define TILED_APP_OUTPUT_UPDATE_SYNC_OFFSET_REG 0x68
#define TILED_APP_INPUT_UPDATE_SYNC_OFFSET_REG 0x64
#define TILED_APP_OUTPUT_SPIN_SYNC_OFFSET_REG 0x60
#define TILED_APP_INPUT_SPIN_SYNC_OFFSET_REG 0x5C
#define TILED_APP_NUM_TILES_REG 0x58
#define TILED_APP_TILE_SIZE_REG 0x54

inline static void print_mem(int read ){
	int loc_tilesize = tile_size%1024;
	if(read == 1)
	printf("Read  Tile: %d\t||", read_tile);
	else if(read == 0)
	printf("Store Tile: %d\t||", write_tile);
	else
	printf("Polled Tile (%d): %d\t||", ( mem_size/8), write_tile);

	int cntr = 0;
	void* dst = (void*)(mem);
	int row_boundary = accel_write_sync_write_offset[0];
	int n = 0;
	for (int j = 0; j < mem_size/8; j++){
		int64_t value_64 = 0;
		#if 0
		asm volatile (
			"mv t0, %1;"
			".word " QU(READ_CODE) ";"
			"mv %0, t1"
			: "=r" (value_64)
			: "r" (dst)
			: "t0", "t1", "memory"
		);
		#endif
		value_64 = mem[j];
			if(cntr == 0){
				printf("\nInput for Accel %d : \n", n);
			}
			printf("\t%d",value_64);
			if(cntr==SYNC_VAR_SIZE-1){
				printf(" || ");
			}
			// if( j>64 && ((j-64)%tile_size == 0)) printf("\n ==================================\n");
			if( cntr == row_boundary-1){ 
				printf("\n ==================================\n");
				cntr = 0;
				n++;
			}else cntr++;
			dst += 8; 
	// cntr++;
	}

	printf("\n");
}


static uint64_t get_counter() {
  uint64_t counter;
  asm volatile (
    "li t0, 0;"
    "csrr t0, mcycle;"
    "mv %0, t0"
    : "=r" ( counter )
    :
    : "t0"
  );

  return counter;
}

static int validate_buf(token_t *out, float *gold)
{
	int j;
	unsigned errors = 0;

	for (j = 0; j < 2 * len; j++) {
		native_t val = fx2float(out[input_buffer_offset[NUM_DEVICES-1] + j], FX_IL);
		uint32_t ival = *((uint32_t*)&val);
		printf("  GOLD[%u] = 0x%08x  :  OUT[%u] = 0x%08x\n", j, ((uint32_t*)gold)[j], j, ival);
		if ((fabs(gold[j] - val) / fabs(gold[j])) > ERR_TH)
			errors++;
	}

	//printf("  %u errors\n", errors);
	return errors;
}

static inline uint32_t read_sync(){
	#ifdef ESP
	#if (COH_MODE==3 || COH_MODE==2 )
		// Flush because Non Coherent DMA/ LLC Coherent DMA
		start_flush = get_counter();
		esp_flush(coherence);
		stop_flush = get_counter();
		spin_flush += stop_flush-start_flush;
	#endif
	#endif
	// void* dst = (void*)(mem);
	void* dst = (void*)(mem + accel_read_sync_write_offset[0]);
	int64_t value_64 = 0;
	#if 1
	//!defined(ESP) && COH_MODE == 0
	// value_64 = mem[num_dev];
	value_64 = mem[accel_read_sync_write_offset[0]];
	#else
	asm volatile (
		"mv t0, %1;"
		".word " QU(READ_CODE) ";"
		"mv %0, t1"
		: "=r" (value_64)
		: "r" (dst)
		: "t0", "t1", "memory"
	);
	#endif
	return (value_64 == 0);
}


static inline uint32_t write_sync(){
	#ifdef ESP
	#if 1
	//(COH_MODE==3 || COH_MODE==2 )
		// Flush because Non Coherent DMA/ LLC Coherent DMA
		start_flush = get_counter();
		esp_flush(coherence);
		stop_flush = get_counter();
		spin_flush += stop_flush-start_flush;
	#endif
	#endif
	// void* dst = (void*)(mem+num_dev);
	void* dst = (void*)(mem+accel_write_sync_write_offset[NUM_DEVICES-1]);
	int64_t value_64 = 0;
	#if 1
	// !defined(ESP) && COH_MODE == 0
	// value_64 = mem[num_dev];
	value_64 = mem[accel_write_sync_write_offset[NUM_DEVICES-1]]; 
	#else
	asm volatile (
		"mv t0, %1;"
		".word " QU(READ_CODE) ";"
		"mv %0, t1"
		: "=r" (value_64)
		: "r" (dst)
		: "t0", "t1", "memory"
	);
	#endif
	return (value_64 == 1);
}


static inline void update_load_sync(){
	#ifdef PRINT_DEBUG
	printf("Inside update load 1\n");
	#endif
	// void* dst = (void*)(mem);
	void* dst = (void*)(mem+accel_read_sync_spin_offset[0]);
	int64_t value_64 = 1;
	#if 1
	//!defined(ESP) && COH_MODE == 0
//(COH_MODE == 0) && 
	// mem[0] = value_64;
	mem[accel_read_sync_spin_offset[0]] = value_64;
	#else
	asm volatile (
		"mv t0, %0;"
		"mv t1, %1;"
		".word " QU(WRITE_CODE) 
		: 
		: "r" (dst), "r" (value_64)
		: "t0", "t1", "memory"
	);
	#endif

	#ifdef PRINT_DEBUG
	printf("Inside update load 2\n");
	#endif
	//Fence to push the write out from the write buffer
	asm volatile ("fence w, w");	
	#ifdef PROFILE
	stop_write = get_counter();
	intvl_write += stop_write - start_write;
	start_sync = stop_write ; //get_counter();
	#endif
	#ifdef ESP
	#if (COH_MODE==3 || COH_MODE==2 )
		// Flush because Non Coherent DMA/ LLC Coherent DMA
		start_flush = get_counter();
		esp_flush(coherence);
		stop_flush = get_counter();
		intvl_flush += (stop_flush - start_flush);
		start_sync = stop_flush;
	#endif
	#endif
	#ifdef PRINT_DEBUG
	printf("Inside update load 3\n");
	#endif
}

static inline void update_store_sync(){
	// void* dst = (void*)(mem+num_dev);
	void* dst = (void*)(mem+accel_write_sync_spin_offset[NUM_DEVICES-1]);
	int64_t value_64 = 0; //Finished reading store_tile
	#if 1
	//!defined(ESP) && COH_MODE == 0
//(COH_MODE == 0) && 
	// mem[0+num_dev] = value_64
	mem[accel_write_sync_spin_offset[NUM_DEVICES-1]] = value_64;;
	#else
	asm volatile (
		"mv t0, %0;"
		"mv t1, %1;"
		".word " QU(WRITE_CODE) 
		: 
		: "r" (dst), "r" (value_64)
		: "t0", "t1", "memory"
	);
	#endif

	//Fence to push the write out from the write buffer
	asm volatile ("fence w, w");
	#ifdef PROFILE	
	stop_read = get_counter();
	intvl_read += stop_read - start_read;
	start_sync = stop_read ; //get_counter();
	#endif
	#ifdef ESP
	#if (COH_MODE==3 || COH_MODE==2 )
	      // Flush because Non Coherent DMA/ LLC Coherent DMA
	      start_flush = get_counter();
	      esp_flush(coherence);
	      stop_flush = get_counter();
	      intvl_flush += (stop_flush - start_flush);
	      start_sync = stop_flush;
	#endif
	#endif
}

//load_mem
static void write_accel_input(token_t *in, float *gold){
	// convert input to fixed point
	for (int j = 0; j < 2 * len; j++)
		in[input_buffer_offset[NUM_DEVICES-1]+j] = float2fx((native_t) gold[j], FX_IL);
}

//store_mem
static inline void read_accel_output(){
	// void *src = (void*)(mem+64+num_dev*tile_size);
	void *src = (void*)(mem+output_buffer_offset[NUM_DEVICES-1]);
	// out [i] = mem[j];
	int64_t out_val;
#if defined(VALIDATE) || defined(MEM_DUMP)
	static int64_t curTile = 0; //write_tile*tile_size;
#endif
	for (int j = 0; j < tile_size; j++){
		//int64_t value_64 = gold[(read_tile)*out_words_adj + j]
		#if 1
		// ((COH_MODE == 0) && !defined(ESP))
			// mem[64+j] = val_64;i
			out_val = mem[output_buffer_offset[NUM_DEVICES-1] + j]; 
		#else
			asm volatile (
				"mv t0, %1;"
				".word " QU(READ_CODE) ";"
				"mv %0, t1"
				: "=r" (out_val)
				: "r" (src)
				: "t0", "t1", "memory"
			);//: "=r" (out[(write_tile) * out_words_adj + j])
			src += 8;
		#endif
#if defined(VALIDATE) || defined(MEM_DUMP)
		// out[curTile++] = out_val; //mem[tile_size + j];
#endif
	}

#if defined(VALIDATE) || defined(MEM_DUMP)
		printf("%d ", out_val);
		printf("\n");
#endif	
	asm volatile ("fence rw, rw");
	// write_tile++;
}

static void init_buf(token_t *in, float *gold)
{
	int j;
	const float LO = -10.0;
	const float HI = 10.0;

	/* srand((unsigned int) time(NULL)); */

	for (j = 0; j < 2 * len; j++) {
		float scaling_factor = (float) rand () / (float) RAND_MAX;
		gold[j] = LO + scaling_factor * (HI - LO);
		uint32_t ig = ((uint32_t*)gold)[j];
		printf("  IN[%u] = 0x%08x\n", j, ig);
	}

	// // convert input to fixed point
	// for (j = 0; j < 2 * len; j++)
	// 	in[j] = float2fx((native_t) gold[j], FX_IL);

	// Compute golden output
	fft2_comp(gold, num_ffts, num_samples, logn_samples, do_inverse, do_shift);
}


int main(int argc, char * argv[])
{
	int i;
	int n;
	int ndev;
	struct esp_device *espdevs;
	struct esp_device *dev;
	unsigned done;
	unsigned spin_ct;
	unsigned **ptable = NULL;
	unsigned tile_size = 2*len;
	// token_t *mem;
	float *gold;
	unsigned errors = 0;
	// unsigned coherence;
        const float ERROR_COUNT_TH = 0.001;

	len = num_ffts * (1 << logn_samples);
	printf("logn %u nsmp %u nfft %u inv %u shft %u len %u\n", logn_samples, num_samples, num_ffts, do_inverse, do_shift, len);
	if (DMA_WORD_PER_BEAT(sizeof(token_t)) == 0) {
		in_words_adj = 2 * len;
		out_words_adj = 2 * len;
	} else {
		in_words_adj = round_up(2 * len, DMA_WORD_PER_BEAT(sizeof(token_t)));
		out_words_adj = round_up(2 * len, DMA_WORD_PER_BEAT(sizeof(token_t)));
	}
	in_len = in_words_adj + SYNC_VAR_SIZE;
	out_len = out_words_adj + SYNC_VAR_SIZE;
	in_size = in_len * sizeof(token_t);
	out_size = out_len * sizeof(token_t);
	out_offset  = 0;
	mem_size = (out_offset * sizeof(token_t)) + out_size;

	printf("ilen %u isize %u o_off %u olen %u osize %u msize %u\n", in_len, out_len, in_size, out_size, out_offset, mem_size);
	// Search for the device
	printf("Scanning device tree... \n");

	ndev = probe(&espdevs, VENDOR_SLD, SLD_FFT2, DEV_NAME);
	if (ndev == 0) {
		printf("%s not found\n", DEV_NAME);
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
		gold = aligned_malloc(out_len * sizeof(float));
		mem = aligned_malloc(mem_size);
		printf("  memory buffer base-address = %p\n", mem);

		// Allocate and populate page table
		ptable = aligned_malloc(NCHUNK(mem_size) * sizeof(unsigned *));
		for (i = 0; i < NCHUNK(mem_size); i++)
			ptable[i] = (unsigned *) &mem[i * (CHUNK_SIZE / sizeof(token_t))];

		printf("  ptable = %p\n", ptable);
		printf("  nchunk = %lu\n", NCHUNK(mem_size));

// #ifndef __riscv
// 		for (coherence = ACC_COH_NONE; coherence <= ACC_COH_FULL; coherence++) {
// #else
		{
			/* TODO: Restore full test once ESP caches are integrated */
			coherence = COHERENCE_MODE;
// #endif
			printf("  --------------------\n");
			printf("  Generate input...\n");
			init_buf(mem, gold);

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
			int32_t rel_input_buffer_offset = SYNC_VAR_SIZE;
			int32_t rel_output_buffer_offset = rel_input_buffer_offset; //tile_size + 2*SYNC_VAR_SIZE;
			int32_t rel_accel_write_sync_write_offset = 2;//tile_size + SYNC_VAR_SIZE;
			int32_t rel_accel_read_sync_write_offset = 0;
			int32_t rel_accel_write_sync_spin_offset = rel_accel_write_sync_write_offset;//+2
			int32_t rel_accel_read_sync_spin_offset = rel_accel_read_sync_write_offset;	//+2

			//debug, sync
			accel_read_sync_spin_offset[n]  = n*(tile_size + SYNC_VAR_SIZE) + rel_accel_read_sync_spin_offset;
			accel_write_sync_spin_offset[n] = n*(tile_size + SYNC_VAR_SIZE) + rel_accel_write_sync_spin_offset;
			accel_read_sync_write_offset[n] = n*(tile_size + SYNC_VAR_SIZE) + rel_accel_read_sync_write_offset;
			accel_write_sync_write_offset[n]= n*(tile_size + SYNC_VAR_SIZE) + rel_accel_write_sync_write_offset;
			input_buffer_offset[n]  		= n*(tile_size + SYNC_VAR_SIZE) + rel_input_buffer_offset;
			output_buffer_offset[n] 		= n*(tile_size + SYNC_VAR_SIZE) + rel_output_buffer_offset;
			
			iowrite32(dev, TILED_APP_OUTPUT_TILE_START_OFFSET_REG , output_buffer_offset[n]);
			iowrite32(dev, TILED_APP_INPUT_TILE_START_OFFSET_REG  , input_buffer_offset[n]);
			iowrite32(dev, TILED_APP_OUTPUT_UPDATE_SYNC_OFFSET_REG, accel_write_sync_write_offset[n]);
			iowrite32(dev, TILED_APP_INPUT_UPDATE_SYNC_OFFSET_REG , accel_read_sync_write_offset[n]);
			iowrite32(dev, TILED_APP_OUTPUT_SPIN_SYNC_OFFSET_REG  , accel_write_sync_spin_offset[n]); //
			iowrite32(dev, TILED_APP_INPUT_SPIN_SYNC_OFFSET_REG   , accel_read_sync_spin_offset[n]);
			iowrite32(dev, TILED_APP_NUM_TILES_REG, num_tiles);//ndev-n- +ndev-n-1
			iowrite32(dev, TILED_APP_TILE_SIZE_REG, tile_size);
			
			iowrite32(dev, FFT2_LOGN_SAMPLES_REG, logn_samples);
			iowrite32(dev, FFT2_NUM_FFTS_REG, num_ffts);
			iowrite32(dev, FFT2_SCALE_FACTOR_REG, scale_factor);
			iowrite32(dev, FFT2_DO_SHIFT_REG, do_shift);
			iowrite32(dev, FFT2_DO_INVERSE_REG, do_inverse);

			// Flush (customize coherence model here)
			esp_flush(coherence);

			// Start accelerators
			printf("  Start...\n");
			iowrite32(dev, CMD_REG, CMD_MASK_START);

			write_accel_input(mem, gold);
			update_load_sync();
			printf("  Start...22\n");

			// Wait for completion
			done = 0;
			spin_ct = 0;
			while (!done) {
				done = ioread32(dev, STATUS_REG);
				done &= STATUS_MASK_DONE;
				int32_t store_done = write_sync();
				#ifdef PRINT_DEBUG
					printf("Done: %d store_done:%d spin_cnt:%d\n", done, store_done, spin_ct);
				#endif
				if(store_done==1 ){
					#ifdef PROFILE
					stop_sync = get_counter();
					intvl_sync += stop_sync- start_sync - spin_flush;
					spin_flush = 0;
					start_read = get_counter();
					#endif
					// store_mem();
					read_accel_output();
					update_store_sync();
					#ifdef PRINT_DEBUG
					print_mem(0);
					#endif
				}
				spin_ct++;
			}
			iowrite32(dev, CMD_REG, 0x0);

			printf("  Done : spin_count = %u\n", spin_ct);
			printf("  validating...\n");

			/* Validation */
			errors = validate_buf(&mem[out_offset], gold);
			if ((float)((float)errors / (2.0 * (float)len)) > ERROR_COUNT_TH)
				printf("  ... FAIL : %u errors out of %u\n", errors, 2*len);
			else
				printf("  ... PASS : %u errors out of %u\n", errors, 2*len);
		}
		aligned_free(ptable);
		aligned_free(mem);
		aligned_free(gold);
	}

	return 0;
}
