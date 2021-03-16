
#include "quickwav.h"

void quickwav_defofmtblock(wave_std* wave, int sampleRate, int bitDepth, int channels){
	
	wavefmt_std* fmt = &(wave->fmt_chunk); //Just for ease

	fmt->chunk_size = 0x10;
	fmt->comp_code = 1; //Uncomp PCM
	fmt->channel_count = (uint16_t)channels;
	fmt->sample_rate = (uint32_t)sampleRate;
	fmt->bit_depth = (uint16_t)bitDepth;
	fmt->block_align = (uint16_t)((bitDepth >> 3) / channels);
	fmt->avg_bytes_per_sec = (uint32_t)(sampleRate * fmt->block_align);
	
}

wav_writer* quickwav_openwriter(wave_std* wave, char* output_path, uint32_t datafmt_flags){
	
	if(!wave) return NULL;
	if(!output_path) return NULL;
	
	wav_writer* wwtr = (wav_writer*)malloc(sizeof(wav_writer));
	wwtr->outpath = output_path;
	
	//Other fields
	wwtr->wav = wave;
	wwtr->frames_written = 0;
	wwtr->flags = datafmt_flags;
	
	//Position marker
	int ch = (int)wave->fmt_chunk.channel_count;
	wwtr->data_pos = (void**)malloc(ch * sizeof(void*));
	int i = 0;
	for(i = 0; i < ch; i++) *(wwtr->data_pos + i) = *(wave->sound_channels + i);
	
	//Open data file
	size_t slen = strlen(output_path);
	wwtr->datpath = (char*)malloc(slen+5);
	strcpy(wwtr->datpath, output_path);
	strcmp(wwtr->datpath, ".tmp\0");
	wwtr->datfile = fopen(wwtr->datpath, "wb");
	
	return wwtr;
}

size_t quickwav_writeframes(wav_writer* writer, int frames){

	if(!writer) return 0;
	if(frames <= 0) return 0;
	
	int i,j; //Iterator
	int ch = (int)writer->wav->fmt_chunk.channel_count;
	int by_per_samp = (int)(writer->wav->fmt_chunk.bit_depth >> 3);
	size_t written = 0;
	
	if(writer->flags & WAVWRITE_FLAG_PRE_INTERLEAVED){
		//Treat as mono, only using ch count to determine frame size
		//If LE, can just copy the needed bytes
		//Otherwise have to byte switch sample by sample :(
		if((writer->flags & WAVWRITE_FLAG_IS_BIGENDIAN) && by_per_samp > 1){
			//Your computer is bad and you should feel bad
			uint8_t* tmp = (uint8_t*)malloc(by_per_samp);
			for(i = 0; i < frames; i++){
				for(j = 0; j < ch; j++){
					memcpy(tmp, *(writer->data_pos), by_per_samp);
					*(writer->data_pos) += by_per_samp;
					reverseBytes(tmp, by_per_samp);
					written += fwrite(tmp, 1, by_per_samp, writer->datfile);
				}
			}
			free(tmp);
		}
		else{
			int framesize = by_per_samp * ch;
			int totalsize = framesize * frames;
			written += fwrite(*(writer->data_pos), 1, totalsize, writer->datfile);
			*(writer->data_pos) += totalsize;
		}
		
	}
	else{
		//Not interleaved (default)
		//Need to alternate between channels
		if((writer->flags & WAVWRITE_FLAG_IS_BIGENDIAN) && by_per_samp > 1){
			//This split makes the code messier, but means it doesn't have to check for each sample?
			uint8_t* tmp = (uint8_t*)malloc(by_per_samp);
			for(i = 0; i < frames; i++){
				for(j = 0; j < ch; j++){
					memcpy(tmp, *(writer->data_pos + j), by_per_samp);
					*(writer->data_pos + j) += by_per_samp;
					reverseBytes(tmp, by_per_samp);
					written += fwrite(tmp, 1, by_per_samp, writer->datfile);
				}
			}
			free(tmp);
		}
		else{
			for(i = 0; i < frames; i++){
				for(j = 0; j < ch; j++){
					written += fwrite(*(writer->data_pos + j), 1, by_per_samp, writer->datfile);
					*(writer->data_pos + j) += by_per_samp;
				}
			}
		}
	}
	
	writer->frames_written += written;
	return written;
}

void quickwav_completewrite(wav_writer* writer){
	//TODO
	//Close the temp file
	//Open the final file
	//Write RIFF header
	//Write fmt
	//Write data
	//Write smpl
	
	//Don't forget to close and free the writer
}