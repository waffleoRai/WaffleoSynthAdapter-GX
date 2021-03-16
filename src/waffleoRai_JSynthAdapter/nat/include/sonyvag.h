#ifndef SONYVAG_H_INCLUDED
#define SONYVAG_H_INCLUDED

#include <stdint.h>

#define VAGP_FLAG_END 0x01
#define VAGP_FLAG_LOOPS 0x02
#define VAGP_FLAG_LOOPPOINT 0x04

#define VAGP_SMAX = 0x7FFF;
#define VAGP_SMIN = ~0x7FFF;

extern const int VAGP_FILTER_TABLE_1[];
extern const int VAGP_FILTER_TABLE_2[];

typedef struct vagpdata{
	
	uint8_t* data;
	size_t data_size;
	
	int16_t loop_older;
	int16_t loop_old;
	
	uint8_t* loop_st; //Pointer to loop point (if present, NULL if one-shot)
	
} vagpdata_t;

typedef struct vagpwav(

	int pos; //Offset from data start
	int end_flag; //Set by chunk decomp method when encounters end chunk.
	int loopme;
	
	vagpdata_t* src;
	
	int16_t* pcm_buff; //Current set of decomped samples
	int pcmbuff_off; //Position in pcm buffer

} vagpwav_t

vagpdata_t* vagp_new_data(void* data, size_t sz);
void vagp_free_data(vagpdata_t* snddat);

vagpwav_t* vagp_new_playable(vagpdata_t* snddat);
void vagp_free_playable(vagpwav_t* sound);

void vagp_scan_loop(vagpdata_t* snddat);
void vagp_decompress_next_chunk(vagpwav_t* sound);
int16_t vagp_next_pcm_sample(vagpwav_t* sound);

#endif //SONYVAG_H_INCLUDED