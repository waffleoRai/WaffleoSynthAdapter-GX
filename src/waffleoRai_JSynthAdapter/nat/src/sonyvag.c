
#include "sonyvag.h"

const int VAGP_FILTER_TABLE_1[] = {0, 60, 115, 98, 122};
const int VAGP_FILTER_TABLE_2[] = {0, 0, -52, -55, -60};

vagpdata_t* vagp_new_data(void* data, size_t sz){
	vagpdata_t* sdat = (vagpdata_t*)malloc(sizeof(vagpdata_t));
	sdat->data = (uint8_t*)data;
	sdat->data_size = sz;
	sdat->loop_older = 0;
	sdat->loop_old = 0;
	sdat->loop_st = NULL;
	
	vagp_scan_loop(sdat);
	
	return sdat;
}

void vagp_free_data(vagpdata_t* snddat){
	if(snddat == NULL) return;
	free(snddat);
}

vagpwav_t* vagp_new_playable(vagpdata_t* snddat){
	if(snddat == NULL) return NULL;
	
	vagpwav_t* snd = (vagpwav_t*)malloc(sizeof(vagpwav_t));
	snd->pos = 0;
	snd->src = snddat;
	snd->pcmbuff_off = -1;
	snd->end_flag = 0;
	snd->loopme = 1;
	
	snd->pcm_buff = (int16_t*)malloc(28 << 1); //28 samples 2 bytes each
	
	return snd;
}

void vagp_free_playable(vagpwav_t* sound){
	if(sound == NULL) return;
	free(sound->pcm_buff);
	free(sound);
}

void vagp_scan_loop(vagpdata_t* snddat){
	if(snddat == NULL) return NULL;
	sdat->loop_older = 0;
	sdat->loop_old = 0;
	sdat->loop_st = NULL;
	
	int nowoff = 0;
	vagpwav_t* snd = vagp_new_playable(snddat);
	while((snddat->loop_st == NULL) &&  (nowoff < snddat->data_size)){
		//Check flags
		uint8_t cflags = *(sdat->data + nowoff + 1);
		
		//Decomp (so can have samples)
		vagp_decompress_next_chunk(snd);
		
		if(cflags & VAGP_FLAG_LOOPPOINT){
			snddat->loop_st = sdat->data + nowoff;
			loop_older = *(snd->pcm_buff + 26);
			loop_old = *(snd->pcm_buff + 27);
			break;
		}
		
		if(cflags & VAGP_FLAG_END) break; //This is the last block
		nowoff+=16;
	}
	
	vagp_free_playable(snd);
}

void vagp_decompress_next_chunk(vagpwav_t* sound){

	uint8_t* dataptr = sound->src->data + sound->pos;
	
	//Filter is higher nybble, range is lower
	int filter = (int)((*dataptr >> 4) & 0xF);
	int range = (int)(*dataptr++ & 0xF);
	int flags = (int)*(dataptr++);
	sound->end_flag = flags & VAGP_FLAG_END;
	
	int s0 = (int)*(sound->pcm_buff + 27);
	int s1 = (int)*(sound->pcm_buff + 26);
	
	int f0 = VAGP_FILTER_TABLE_1[filter];
	int f1 = VAGP_FILTER_TABLE_2[filter];
	
	int i = 0;
	for(i = 0; i < 28; i++){
		//In samples lower nybble comes first
		int s = (int)*(dataptr);
		if(i % 2){
			//(Hi nybble)
			s >>= 4;
			dataptr++;
		}
		s &= 0xF;
		if(s > 7) s -= 16; //Sign extend
		
		s >>= range;
		s += (((s0 * f0) + (s1 * f1)) >> 6);
		
		//Clamp to 16 bits
		if(s > VAGP_SMAX) s = VAGP_SMAX;
		if(s < VAGP_SMIN) s = VAGP_SMIN;
		s1 = s0;
		s0 = s;
		
		*(sound->pcm_buff + i) = (uint16_t)s;
	}
	
	sound->pos += 16;
}

int16_t vagp_next_pcm_sample(vagpwav_t* sound){

	if(sound->pcmbuff_off > 27 || sound->pcmbuff_off < 0){
		//Need to pull the next chunk]
		//Don't forget to check if we need to loop or end
		sound->pcmbuff_off = 0;
		if(sound->end_flag){
			//Loop/ end
			//Don't clear flag - if it loops the next chunk decomp will clear it
			if(sound->src->loop_st && sound->loopme){
				//Move loop pointer
				sound->pos = (sound->src->loop_st) - (sound->src->data);
				//Load ref samples
				*(sound->pcm_buff + 27) = sound->src->loop_old;
				*(sound->pcm_buff + 26) = sound->src->loop_older;
			}
			else{
				//Zero fill buffer and return
				memset(sound->pcm_buff, 0, (28 << 1));
				sound->pcmbuff_off++;
				return 0;
			}
		}
		//Chunk
		vagp_decompress_next_chunk(sound);
	}
	
	return *(sound->pcm_buff + sound->pcmbuff_off++);
}