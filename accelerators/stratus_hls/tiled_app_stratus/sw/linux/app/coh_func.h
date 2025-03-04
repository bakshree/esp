///////////////////////////////////////////////////////////////
// Helper unions
///////////////////////////////////////////////////////////////
#ifndef __COH_FUNC__
#define __COH_FUNC__

#ifdef COH_CODE
	#if (COH_CODE==1)
	#define ESP
	#define COH_MODE 0
	#elif (COH_CODE ==2)
	#define ESP
	#define COH_MODE 1
	#elif (COH_CODE==3)
	#define COH_MODE 2
	#endif
#else
#define ESP
#define COH_MODE 0
#endif


typedef union
{
  struct
  {
    unsigned int r_en   : 1;
    unsigned int r_op   : 1;
    unsigned int r_type : 2;
    unsigned int r_cid  : 4;
    unsigned int w_en   : 1;
    unsigned int w_op   : 1;
    unsigned int w_type : 2;
    unsigned int w_cid  : 4;
	uint16_t reserved: 16;
  };
  uint32_t spandex_reg;
} spandex_config_t;

typedef union
{
  struct
  {
    int32_t value_32_1;
    int32_t value_32_2;
  };
  int64_t value_64;
} spandex_token_t;

///////////////////////////////////////////////////////////////
// Choosing the read, write code and coherence register config
///////////////////////////////////////////////////////////////
#define QUAUX(X) #X
#define QU(X) QUAUX(X)

#ifdef ESP
// ESP COHERENCE PROTOCOLS
#define READ_CODE 0x0002B30B
#define WRITE_CODE 0x0062B02B
spandex_config_t spandex_config;

#if (COH_MODE == 3)
unsigned coherence = ACC_COH_NONE;
const char print_coh[] = "Non-Coherent_DMA";
#elif (COH_MODE == 2)
unsigned coherence = ACC_COH_LLC;
const char print_coh[] = "LLC-Coherent_DMA";
#elif (COH_MODE == 1)
unsigned coherence = ACC_COH_RECALL;
const char print_coh[] = "DMA";
#else
unsigned coherence = ACC_COH_FULL;
const char print_coh[] = "MESI";
#endif

#else
//SPANDEX COHERENCE PROTOCOLS
unsigned coherence = ACC_COH_FULL;
#if (COH_MODE == 3)
// Owner Prediction
#define READ_CODE 0x4002B30B
#define WRITE_CODE 0x2262B82B
spandex_config_t spandex_config = {.spandex_reg = 0, .r_en = 1, .r_type = 2, .w_en = 1, .w_op = 1, .w_type = 1};
const char print_coh[] = "Owner_Prediction";
#elif (COH_MODE == 2)
// Write-through forwarding
#define READ_CODE 0x4002B30B
#define WRITE_CODE 0x2062B02B
spandex_config_t spandex_config = {.spandex_reg = 0, .r_en = 1, .r_type = 2, .w_en = 1, .w_type = 1};
const char print_coh[] = "Spandex";
#elif (COH_MODE == 1)
// Baseline Spandex
#define READ_CODE 0x2002B30B
#define WRITE_CODE 0x0062B02B
spandex_config_t spandex_config = {.spandex_reg = 0, .r_en = 1, .r_type = 1};
const char print_coh[] = "Baseline_Spandex_(ReqV)";
#else
// Fully Coherent MESI
#define READ_CODE 0x0002B30B
#define WRITE_CODE 0x0062B02B
spandex_config_t spandex_config= {.spandex_reg = 0};
const char print_coh[] = "Baseline_Spandex";
#endif
#endif



#endif
