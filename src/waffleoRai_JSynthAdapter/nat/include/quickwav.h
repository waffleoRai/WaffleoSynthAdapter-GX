#ifndef QUICKWAV_H_INCLUDED
#define QUICKWAV_H_INCLUDED

#include "byteorder.h"
#include <string.h>

#define WAVWRITE_FLAG_IS_BIGENDIAN 0x1
#define WAVWRITE_FLAG_PRE_INTERLEAVED 0x2

const char* WAVMAG0 = "RIFF";
const char* WAVMAG1 = "WAVE";
const char* WAVMAG_FMT = "fmt ";
const char* WAVMAG_DATA = "data";
const char* WAVMAG_SMPL = "smpl";

typedef struct wavesmpl_sloop{
	
	uint32_t id;
	uint32_t type;
	uint32_t start;
	uint32_t end;
	uint32_t fraction;
	uint32_t playCount;
	
} wavesmpl_sloop_t;

typedef struct wavefmt_std{
	
	uint32_t chunk_size;
	uint16_t comp_code;
	uint16_t channel_count;
	uint32_t sample_rate;
	uint32_t avg_bytes_per_sec;
	uint16_t block_align;
	uint16_t bit_depth;
	
} wavefmt_std_t;

typedef struct wavesmpl_hdr{
	
	uint32_t chunk_size;
	uint32_t manufacturer;
	uint32_t product;
	uint32_t sample_period;
	uint32_t unity_note;
	uint32_t pitch_tune;
	uint32_t smpte_fmt;
	uint32_t smpte_off;
	uint32_t smpl_loop_count;
	uint32_t sampler_data;
	
} wavesmpl_hdr_t;

typedef struct wave_std{
	
	wavefmt_std_t fmt_chunk;
	
	void** sound_channels; //Base pointer
	int frames; //Length
	
	wavesmpl_hdr_t smpl_hdr;
	wavesmpl_sloop_t* smpl_loops;
	
} wave_std_t;

typedef struct wav_writer{
	
	char* outpath;
	
	char* datpath;
	FILE* datfile;
	
	wave_std_t* wav;
	
	int frames_written;
	uint32_t flags; 
	void** data_pos;
	
} wav_writer_t;

void quickwav_defofmtblock(wave_std* wave, int sampleRate, int bitDepth, int channels);
wav_writer* quickwav_openwriter(wave_std* wave, char* output_path, uint32_t datafmt_flags);
size_t quickwav_writeframes(wav_writer* writer, int frames);
void quickwav_completewrite(wav_writer* writer); //Also closes writer


#endif //QUICKWAV_H_INCLUDED