#ifndef PSXSPU_H_INCLUDED
#define PSXSPU_H_INCLUDED

#include <math.h>

#include "psxspu_tables.h"
#include "sonyvag.h"

//512 kb of SRAM
#define PSXSRAM_SIZE 0x80000
#define PSXSPU_VOICE_COUNT 24

#define PSXSPU_RVBENABLED_FLAG 0x0080;

#define PSXSPU_ADSR_PHASE_ATT 1
#define PSXSPU_ADSR_PHASE_DEC 2
#define PSXSPU_ADSR_PHASE_SUS 3
#define PSXSPU_ADSR_PHASE_REL 4
#define PSXSPU_ADSR_PHASE_OFF 0

#define PSXSPU_PEATT_THRESH 0x6000
#define PSXSPU_EXPDEC_THRESH 0x888
#define PSXSPU_PANDB -6.0

const double PSXSPU_PANCTR = pow(10.0, (PSXSPU_PANDB)/20.0);

//TODO Add noise generation

typedef struct psxspu_mallocheapnode{
	
	
	
} psxspu_mallocheapnode_t;

typedef struct psxspu_out_buffer{
	
	int16_t* master_L;
	int16_t* master_R;
	
	int16_t* cd_L;
	int16_t* cd_R;
	
	int16_t* voices_L[PSXSPU_VOICE_COUNT];
	int16_t* voices_R[PSXSPU_VOICE_COUNT];
	
} psxspu_out_buffer_t;

typedef struct psxspu_voice_regset{
	
	int16_t volL;
	int16_t volR;
	int16_t sampleRate;
	uint16_t start_addr; //SRAM pointer to sound data start (in DWORDs, so need to SLL 3)
	uint16_t adsr_lo;
	uint16_t adsr_hi;
	int16_t adsr_lvl;
	uint16_t repeat_addr;
	
} psxspu_voice_regset_t;

typedef struct psxspu_rvb_regset{
	
	int16_t dAPF1;
	int16_t dAPF2;
	int16_t vIIR;
	int16_t vCOMB1;
	int16_t vCOMB2;
	int16_t vCOMB3;
	int16_t vCOMB4;
	int16_t vWALL;
	int16_t vAPF1;
	int16_t vAPF2;
	int16_t mSAME_L;
	int16_t mSAME_R;
	int16_t mCOMB1_L;
	int16_t mCOMB1_R;
	int16_t mCOMB2_L;
	int16_t mCOMB2_R;
	int16_t dSAME_L;
	int16_t dSAME_R;
	int16_t mDIFF_L;
	int16_t mDIFF_R;
	int16_t mCOMB3_L;
	int16_t mCOMB3_R;
	int16_t mCOMB4_L;
	int16_t mCOMB4_R;
	int16_t dDIFF_L;
	int16_t dDIFF_R;
	int16_t mAPF1_L;
	int16_t mAPF1_R;
	int16_t mAPF2_L;
	int16_t mAPF2_R;
	int16_t vIN_L;
	int16_t vIN_R;
	
} psxspu_rvb_regset_t;

typedef struct psxspu_adsr{
	
	//For storing ADSR values once expanded
	//Attack
	uint16_t a_trustep;
	int a_cycles;
	int a_hicycles;
	int a_ctr;
	
	//Decay
	uint16_t d_trustep;
	int d_cycles;
	int d_ctr;
	
	//Sustain
	int16_t suslvl;
	uint16_t s_trustep;
	int s_cycles;
	int s_ctr;
	
	//Release
	uint16_t r_trustep;
	int r_cycles;
	int r_ctr;
	
} psxspu_adsr_t;

typedef struct psxspu_voice{
	
	//Links
	psxspu_voice_regset_t* registers;
	void* sram;
	
	uint32_t vox_mask; //To know which bit to look at
	uint32_t* keyon;
	uint32_t* keyoff;
	uint32_t* fm;
	uint32_t* noise;
	uint32_t* reverb;
	uint32_t* stat;
	
	//Interpolator
	uint32_t pitchCtr;
	int16_t last_out; //Last ADSR adjusted sample output (for next voice to use for LFO)
	int16_t* last_out_prev; //Pointer to previous voice's last out for LFO on this voice
	
	//ADSR
	int adsr_phase;
	psxspu_adsr_t adsr_exp;
	
	//Decomp buffer
	uint8_t* spos; //Current pointer to ADPCM data
	int16_t pcmbuff[28]; //PCM buffer
	int16_t last[4]; //Last 4 samples from previous block
	
} psxspu_voice_t;


typedef struct psxspu{
	
	//Voice registers
	psxspu_voice_regset_t voxreg[PSXSPU_VOICE_COUNT];
	
	//Control registers
	int16_t mainvol_L;
	int16_t mainvol_R;
	int16_t rvboutvol_L;
	int16_t rvboutvol_R;
	uint32_t vox_keyon;
	uint32_t vox_keyoff;
	uint32_t vox_fm;
	uint32_t vox_noise;
	uint32_t vox_reverb;
	uint32_t vox_stat;
	uint16_t reserved_1f801da0;
	uint16_t rvbarea_addr;
	uint16_t irq_addr;
	uint16_t dattr_addr;
	uint16_t dat_fifo;
	uint16_t SPUCNT;
	uint16_t dattr_ctrl;
	uint16_t SPUSTAT;
	int16_t cdvol_L;
	int16_t cdvol_R;
	int16_t extvol_L;
	int16_t extvol_R;
	int16_t outvol_L;
	int16_t outvol_R;
	
	//Internal registers
	uint32_t voice_out[PSXSPU_VOICE_COUNT];
	
	//Reverb registers
	psxspu_rvb_regset_t rvb_regs;
	uint32_t rvb_buffer_addr; //Not /8
	uint8_t cycle_ctr;
	int16_t rvbout_L;
	int16_t rvbout_R;
	
	psxspu_voice_t voices[PSXSPU_VOICE_COUNT];
	void* sram; //Pointer to sram start
	
} psxspu_t;

psxspu_t* psxspu_newspu();
void psxspu_freespu(psxspu_t* spu);

void psxvoice_decomp_next_block(psxspu_voice_t* voice);
int16_t psxvoice_interpolate_next_sample(psxspu_voice_t* voice);
void psxvoice_initadsr(psxspu_voice_t* voice);
int16_t psxvoice_advance_adsr(psxspu_voice_t* voice);

void psxvoice_next_sample(psxspu_voice_t* voice, int16_t* outL, int16_t* outR);
void psxspu_calcrvb(psxspu_t* spu, int16_t inL, int16_t inR);
void psxspu_next_sample(psxspu_t* spu, psxspu_out_buffer_t* out_buff, int buffer_offset);

//SRAM management (need to decide where to load samples - also make sure to keep a 0 buffer for one-shots)
//This shouldn't be needed for a full emulation, but probably will for stand-alone SPU emulator

//External control

//Utilities
uint16_t psxspu_getspu_sr_value(int cent_change); //The value to set "sample rate" register to for the given pitch change
void psxspu_scale_pan(int8_t pan, int16_t* reg_volL, int16_t* reg_volR);
int16_t psxspu_clamp16s(int32_t in);

#endif //PSXSPU_H_INCLUDED