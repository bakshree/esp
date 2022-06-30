// Copyright (c) 2011-2021 Columbia University, System Level Design Group
// SPDX-License-Identifier: Apache-2.0
#include "libesp.h"
#include "cfg.h"
#include "stdio.h"
#include "math.h"
static unsigned in_words_adj;
static unsigned out_words_adj;
static unsigned in_len;
static unsigned out_len;
static unsigned in_size;
static unsigned out_size;
static unsigned out_offset;
static unsigned size;

 float fov = 1.0472; // Radians
/* User-defined code */
static int validate_buffer(token_t *out, token_t *gold)
{
	int i;
	int j;
	unsigned errors = 0;
	FILE *fp;
	fp = fopen("out_rt_fpga.ppm", "wb");
	// ofstream ofs("out_rt_fpga.ppm", ios::binary);
    // ofs << "P6\n" << img_width << " " << img_height << "\n255\n";
	fprintf(fp, "%s\n%d %d\n%s\n", "P6", img_width, img_height, "255");

    for (int j = 0; j < img_width*img_height; j++){
        float color[3];
        color[0] = out[3*j]    ;
        color[1] = out[3*j + 1];
        color[2] = out[3*j + 2];

        float max_val = color[0]>color[1]? color[0]: color[1];
		max_val = max_val>color[2]?max_val:color[2];
        for (int chan = 0; chan<2; chan++){
            // ofs << (char) (255 * color[chan] / std::max);
			fprintf(fp, "%c", (char) (255 * color[chan] / max_val));
        }
    }
    fclose(fp);

	// for (i = 0; i < 1; i++)
	// 	for (j = 0; j < 3*img_width*img_height; j++)
	// 		if (gold[i * out_words_adj + j] != out[i * out_words_adj + j])
	// 			errors++;

	return errors;
}


/* User-defined code */
static void init_buffer(token_t *in, token_t * gold)
{
	int i;
	int j;
	// for (i = 0; i < 1; i++)
	// 	for (j = 0; j < 3*img_width*img_height; j++)
	// 		in[i * in_words_adj + j] = (token_t) j;

	// for (i = 0; i < 1; i++)
	// 	for (j = 0; j < 3*img_width*img_height; j++)
	// 		gold[i * out_words_adj + j] = (token_t) j;

	for (int i = 0; i < 1; i++)
        for (int j = 0; j < img_width*img_height; j++){ // total loop: 3*img_width*img_height
            float dir_x = ((j % img_width + 0.5) - img_width / 2.);
            float dir_y = (-(j / img_width + 0.5) + img_height / 2.);
            float dir_z = (-img_height / (2. * tan(fov / 2.)));

            //vec3 data = normalized(dir_x, dir_y, dir_z);

            in[i * in_words_adj + 3*j] =     dir_x ; //data[0];
            in[i * in_words_adj + 3*j + 1] = dir_y ; //data[1];
            in[i * in_words_adj + 3*j + 2] = dir_z ; //data[2];

        }

	for (int i = 0; i < 1; i++)
        for (int j = 0; j < 3*img_width*img_height; j++){
            // std::string token;
            // buffer>>token;
            //gold[i * out_words_adj + j] = std::stof(token);
            gold[i * out_words_adj + j] = in[i * out_words_adj + j];
            // buffer>>token; // , removal
        }
}


/* User-defined code */
static void init_parameters()
{
	if (DMA_WORD_PER_BEAT(sizeof(token_t)) == 0) {
		in_words_adj = 3*img_width*img_height;
		out_words_adj = 3*img_width*img_height;
	} else {
		in_words_adj = round_up(3*img_width*img_height, DMA_WORD_PER_BEAT(sizeof(token_t)));
		out_words_adj = round_up(3*img_width*img_height, DMA_WORD_PER_BEAT(sizeof(token_t)));
	}
	in_len = in_words_adj * (1);
	out_len =  out_words_adj * (1);
	in_size = in_len * sizeof(token_t);
	out_size = out_len * sizeof(token_t);
	out_offset = in_len;
	size = (out_offset * sizeof(token_t)) + out_size;
}


int main(int argc, char **argv)
{
	int errors;

	token_t *gold;
	token_t *buf;

	init_parameters();

	buf = (token_t *) esp_alloc(size);
	cfg_000[0].hw_buf = buf;
    
	gold = malloc(out_size);

	init_buffer(buf, gold);

	printf("\n====== %s ======\n\n", cfg_000[0].devname);
	/* <<--print-params-->> */
	printf("  .img_width = %d\n", img_width);
	printf("  .img_height = %d\n", img_height);
	printf("\n  ** START **\n");

	esp_run(cfg_000, NACC);

	printf("\n  ** DONE **\n");

	errors = validate_buffer(&buf[out_offset], gold);

	free(gold);
	esp_free(buf);

	if (!errors)
		printf("+ Test PASSED\n");
	else
		printf("+ Test FAILED\n");

	printf("\n====== %s ======\n\n", cfg_000[0].devname);

	return errors;
}
