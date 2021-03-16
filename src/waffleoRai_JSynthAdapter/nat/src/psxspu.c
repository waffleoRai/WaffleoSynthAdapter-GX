
#import "psxspu.h"

int16_t psxspu_scaleAmp_16(int16_t sample, int16_t env){
	
	if(env == 0x7FFF || env == 0) return sample;
	int32_t samp32 = (int32_t)sample * (int32_t)env;
	samp32 >>= 15;
	return (int16_t)samp32;
	
}

psxspu_t* psxspu_newspu(){
	psxspu_t* spu = (psxspu_t*)malloc(sizeof(psxspu_t));	
	memset(spu, 0, sizeof(psxspu_t));
	
	//SRAM
	spu->sram = malloc(PSXSRAM_SIZE);
	memset(spu->sram, 0, PSXSRAM_SIZE);
	
	//SPU registers
	spu->mainvol_L = 0x7FFF;
	spu->mainvol_R = 0x7FFF;
	spu->rvboutvol_L = 0x7FFF;
	spu->rvboutvol_R = 0x7FFF;
	spu->cdvol_L = 0x7FFF;
	spu->cdvol_R = 0x7FFF;
	spu->extvol_L = 0x7FFF;
	spu->extvol_R = 0x7FFF;
	
	spu->SPUCNT = 0x4080; //SPU is off by default - turn on when needed
	spu->SPUSTAT = 0x0x0;
	
	//Loads with "hall" reverb preset
	void* src = (void*)RVB_SET_HALL;
	void* dst = (void*)&(spu->rvb_regs);
	memcpy(dst, src, 64);
	spu->rvbarea_addr = mBASE_HALL;

	//Voices
	int v = 0;
	uint32_t vmask = 0x1;
	uint32_t* lastout = NULL;
	for(v = 0; v < PSXSPU_VOICE_COUNT; v++){
		psxspu_voice_t vox = spu->voices[i];
		psxspu_voice_regset_t vreg = spu->voxreg[i];
		
		//Set register values
		vreg.volL = 0x7FFF;
		vreg.volR = 0x7FFF;
		vreg.sampleRate = 0x1000;
		vreg.start_addr = 0x1000 >> 3;
		vreg.repeat_addr = vreg.start_addr;
		vreg.adsr_lo = 0xFF80;
		vreg.adsr_hi = 0xC05F;
		
		//Link to voice
		vox.registers = &vreg;
		vox.sram = spu->sram;
		vox.vox_mask = vmask;
		vox.keyon = &(spu->vox_keyon);
		vox.keyoff = &(spu->vox_keyoff);
		vox.fm = &(spu->vox_fm);
		vox.noise = &(spu->vox_noise);
		vox.reverb = &(spu->vox_reverb);
		vox.stat = &(spu->vox_stat);
		vox.last_out_prev = lastout;
		lastout = &(vox.lastout);
		vox.spos = vox.sram + 0x1000;
		
		psxvoice_initadsr(&vox);
		vmask <<= 1;
	}
	
	return spu;
}

void psxspu_freespu(psxspu_t* spu){
	if(spu == NULL) return;
	
	free(spu->sram);
	free(spu);
	//The rest should have alloc'd with the spu, so that should be it
}

void psxvoice_decomp_next_block(psxspu_voice_t* voice){
	if(voice == NULL) return;
	int i = 0;
	for(i = 0; i < 4; i++){
		voice->last[i] = voice->pcmbuff[27-i];
	}
	
	int filter = (int)((*(voice->spos) >> 4) & 0xF);
	int range = (int)(*(voice->spos++) & 0xF);
	int flags = (int)*(voice->spos++);
	if(flags & VAGP_FLAG_LOOPPOINT){
		voice->registers->repeat_addr = (uint16_t)((voice->spos - 2) - voice->sram) >> 3;
	}
	
	int s0 = (int)(voice->last[0]);
	int s1 = (int)(voice->last[1]);
	
	int f0 = VAGP_FILTER_TABLE_1[filter];
	int f1 = VAGP_FILTER_TABLE_2[filter];
	
	for(i = 0; i < 28; i++){
		//In samples lower nybble comes first
		int s = (int)*(voice->spos);
		if(i % 2){
			//(Hi nybble)
			s >>= 4;
			voice->spos++;
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
		
		voice->pcmbuff[i] = (int16_t)s;
	}
	
	//Move spos back to loop start if necessary
	//Or direct to 0 or something
	if(flags & VAGP_FLAG_END){
		//Jump to loop address
		voice->spos = voice->sram + (voice->registers->repeat_addr << 3);
	}
	
}

int16_t psxvoice_interpolate_next_sample(psxspu_voice_t* voice){
	//Made a lot easier by the documentation of nocash
	if(voice == NULL) return 0;
	
	int last_idx = (int)((voice->pitchCtr >> 12) % 28); //To determine if need new chunk.
	int16_t step = voice->registers->sampleRate;
	if((voice->vox_mask & *(voice->fm)) != 0){
		//FM is set for this voice
		int32_t factor = (int32_t)*(voice->last_out_prev) + 0x8000;
		int32_t val = ((int32_t)step * factor) >> 15;
		step = (int16_t)(val & 0xFFFF);
	}
	if(step > 0x3fff) step = 0x4000;
	voice->pitchCtr += step;
	
	//Now we determine what samples we're working with...
	int now_idx = (int)((voice->pitchCtr >> 12) % 28);
	if(last_idx > now_idx){
		//Need new block
		psxvoice_decomp_next_block(voice);
	}
	int now = (int)voice->pcmbuff[now_idx];
	int old = (int)(now_idx>0?voice->pcmbuff[now_idx-1]:voice->last[0]);
	int older = (int)(now_idx>1?voice->pcmbuff[now_idx-2]:voice->last[1-now_idx]);
	int oldest = (int)(now_idx>2?voice->pcmbuff[now_idx-3]:voice->last[2-now_idx]);
	int gauss_idx = (int)((voice->pitchCtr >> 4) & 0xFF);
	
	//And perform interpolation...
	int s_raw = (PSXSPU_GAUSS_TABLE[0xFF - gauss_idx] * oldest) >> 15;
	s_raw +=    (PSXSPU_GAUSS_TABLE[0x1FF - gauss_idx] * older) >> 15;
	s_raw +=    (PSXSPU_GAUSS_TABLE[0x100 + gauss_idx] * old)   >> 15;
	s_raw +=    (PSXSPU_GAUSS_TABLE[0x000 + gauss_idx] * now)   >> 15;
	
	return (int16_t)s_raw;
}

void psxvoice_initadsr(psxspu_voice_t* voice){
	//Just updates the precalc values to match the ADSR registers
	if(voice == NULL) return;
	
	//Attack
	int shift = (int)((voice->registers->adsr_lo >> 10) & 0x1F);
	int step = (int)((voice->registers->adsr_lo >> 8) & 0x3);
	
	int stepFactor = 11 - shift;
	if(stepFactor < 0) stepFactor = 0;
	int timeFactor = shift - 11;
	if(timeFactor < 0) timeFactor = 0;
	
	voice->adsr_exp.a_cycles = 1 << timeFactor;
	voice->adsr_exp.a_hicycles = voice->adsr_exp.a_cycles << 2;
	voice->adsr_exp.a_ctr = 0;
	voice->adsr_exp.a_trustep = (uint16_t)(step << stepFactor);
	
	
	//Decay
	shift = (int)((voice->registers->adsr_lo >> 4) & 0xF);
	stepFactor = 11 - shift;
	if(stepFactor < 0) stepFactor = 0;
	timeFactor = shift - 11;
	if(timeFactor < 0) timeFactor = 0;
	
	voice->adsr_exp.d_cycles = 1 << timeFactor;
	voice->adsr_exp.d_ctr = 0;
	voice->adsr_exp.d_trustep = (uint16_t)(8 << stepFactor);
	
	//Sustain
	shift = (int)((voice->registers->adsr_hi >> 8) & 0x1F);
	step = (int)((voice->registers->adsr_hi >> 6) & 0x3);
	int16_t lvl = (int16_t)(voice->registers->adsr_lo & 0xF);
	stepFactor = 11 - shift;
	if(stepFactor < 0) stepFactor = 0;
	timeFactor = shift - 11;
	if(timeFactor < 0) timeFactor = 0;
	
	voice->adsr_exp.s_cycles = 1 << timeFactor;
	voice->adsr_exp.s_ctr = 0;
	voice->adsr_exp.s_trustep = (uint16_t)(step << stepFactor);
	voice->adsr_exp.suslvl = (0x8000/15) * lvl;
	
	//Release
	shift = (int)(voice->registers->adsr_hi & 0x1F);
	stepFactor = 11 - shift;
	if(stepFactor < 0) stepFactor = 0;
	timeFactor = shift - 11;
	if(timeFactor < 0) timeFactor = 0;
	
	voice->adsr_exp.r_cycles = 1 << timeFactor;
	voice->adsr_exp.r_ctr = 0;
	voice->adsr_exp.r_trustep = (uint16_t)(8 << stepFactor);
	
}

int16_t psxvoice_advance_adsr(psxspu_voice_t* voice){
	//Both writes it to register, and returns
	if(voice == NULL) return 0;
	
	switch(voice->adsr_phase){
		case PSXSPU_ADSR_PHASE_ATT:
			//If at max, continue to decay...
			if(voice->registers->adsr_lvl >= 0x7FFF){
				voice->adsr_phase = PSXSPU_ADSR_PHASE_DEC;
				voice->registers->adsr_lvl = 0x7FFF;
				returnvoice->registers->adsr_lvl;
			}
			else{
				//Keep on increasing
				//Check mode
				if(voice->registers->adsr_lo & 0x8000){
					//Pseudo-exp
					if(voice->registers->adsr_lvl >= PSXSPU_PEATT_THRESH){
						if(++voice->adsr_exp.a_ctr >= voice->adsr_exp.a_hicycles){
							voice->registers->adsr_lvl += voice->adsr_exp.a_trustep;
							voice->adsr_exp.a_ctr = 0;
							return voice->registers->adsr_lvl;
						}
					}
					else{
						if(++voice->adsr_exp.a_ctr >= voice->adsr_exp.a_cycles){
							voice->registers->adsr_lvl += voice->adsr_exp.a_trustep;
							voice->adsr_exp.a_ctr = 0;
							return voice->registers->adsr_lvl;
						}
					}
				}
				else{
					//Linear
					if(++voice->adsr_exp.a_ctr >= voice->adsr_exp.a_cycles){
						voice->registers->adsr_lvl += voice->adsr_exp.a_trustep;
						voice->adsr_exp.a_ctr = 0;
						return voice->registers->adsr_lvl;
					}
				}
			}
			break;
		case PSXSPU_ADSR_PHASE_DEC:
			//If at sus level, start sustain...
			if(voice->registers->adsr_lvl <= voice->adsr_exp.suslvl) {
				voice->adsr_phase = PSXSPU_ADSR_PHASE_SUS;
				voice->registers->adsr_lvl = voice->adsr_exp.suslvl;
				return voice->registers->adsr_lvl;}
			
			if(++voice->adsr_exp.d_ctr >= voice->adsr_exp.d_cycles){
				
				double fstep = (double)voice->adsr_exp.d_trustep * ((double)voice->registers->adsr_lvl/(double)0x8000);
				if(fstep < PSXSPU_EXPDEC_THRESH) fstep = PSXSPU_EXPDEC_THRESH;
				voice->registers->adsr_lvl = (int16_t)(((double)voice->registers->adsr_lvl) - fstep);
				
				voice->adsr_exp.d_ctr = 0;
				return voice->registers->adsr_lvl;
			}
			
			break;
		case PSXSPU_ADSR_PHASE_SUS:
			if(voice->registers->adsr_hi & 0x4000){
				//Decrease
				if(voice->registers->adsr_lvl <= 0){
					voice->registers->adsr_lvl = 0;
					return voice->registers->adsr_lvl;
				}
				
				if(voice->registers->adsr_hi & 0x8000){
					//Exp
					if(++voice->adsr_exp.s_ctr >= voice->adsr_exp.s_cycles){
				
						double fstep = (double)voice->adsr_exp.s_trustep * ((double)voice->registers->adsr_lvl/(double)0x8000);
						if(fstep < PSXSPU_EXPDEC_THRESH) fstep = PSXSPU_EXPDEC_THRESH;
						voice->registers->adsr_lvl = (int16_t)(((double)voice->registers->adsr_lvl) - fstep);
				
						voice->adsr_exp.s_ctr = 0;
						return voice->registers->adsr_lvl;
					}
				}
				else{
					//Linear
					if(++voice->adsr_exp.s_ctr >= voice->adsr_exp.s_cycles){
						voice->registers->adsr_lvl -= voice->adsr_exp.s_trustep;
						voice->adsr_exp.s_ctr = 0;
						return voice->registers->adsr_lvl;
					}
				}
				
			}
			else{
				//Increase
				if(voice->registers->adsr_lvl >= 0x7FFF){
					voice->registers->adsr_lvl = 0x7FFF;
					return voice->registers->adsr_lvl;
				}
				
				if(voice->registers->adsr_hi & 0x8000){
					//Pseudo-Exp
					if(voice->registers->adsr_lvl >= PSXSPU_PEATT_THRESH){
						if(++voice->adsr_exp.s_ctr >= (voice->adsr_exp.s_hicycles << 2)){
							voice->registers->adsr_lvl += voice->adsr_exp.s_trustep;
							voice->adsr_exp.s_ctr = 0;
							return voice->registers->adsr_lvl;
						}
					}
					else{
						if(++voice->adsr_exp.s_ctr >= voice->adsr_exp.s_cycles){
							voice->registers->adsr_lvl += voice->adsr_exp.s_trustep;
							voice->adsr_exp.s_ctr = 0;
							return voice->registers->adsr_lvl;
						}
					}
				}
				else{
					//Linear
					if(++voice->adsr_exp.s_ctr >= voice->adsr_exp.s_cycles){
						voice->registers->adsr_lvl += voice->adsr_exp.s_trustep;
						voice->adsr_exp.s_ctr = 0;
						return voice->registers->adsr_lvl;
					}
				}
			}
			break;
		case PSXSPU_ADSR_PHASE_REL:
			//If at zero, go to off...
			//Also write to SPU reg that the voice is now off?
			if(voice->registers->adsr_lvl <= 0) {
				voice->adsr_phase = PSXSPU_ADSR_PHASE_OFF;
				*(voice->stat) &= ~voice->vox_mask; //This voice is now off.
				voice->registers->adsr_lvl = 0;
				return voice->registers->adsr_lvl;
			}
			
			if(voice->registers->adsr_hi & 0x0020){
				//Exp
				if(++voice->adsr_exp.r_ctr >= voice->adsr_exp.r_cycles){
				
					double fstep = (double)voice->adsr_exp.r_trustep * ((double)voice->registers->adsr_lvl/(double)0x8000);
					if(fstep < PSXSPU_EXPDEC_THRESH) fstep = PSXSPU_EXPDEC_THRESH;
					voice->registers->adsr_lvl = (int16_t)(((double)voice->registers->adsr_lvl) - fstep);
				
					voice->adsr_exp.r_ctr = 0;
					return voice->registers->adsr_lvl;
				}
			}
			else{
				//Linear
				if(++voice->adsr_exp.r_ctr >= voice->adsr_exp.r_cycles){
					voice->registers->adsr_lvl -= voice->adsr_exp.r_trustep;
					voice->adsr_exp.r_ctr = 0;
					return voice->registers->adsr_lvl;
				}
			}
			break;
		default: 
			voice->registers->adsr_lvl = 0;
			return 0;
	}
	
}

void psxvoice_next_sample(psxspu_voice_t* voice, int16_t* outL, int16_t* outR){
	/*
	1. Decompress
	2. Interpolate
	3. ADSR
	4. Pan (set by volume registers)
	*/
	if(voice == NULL || outL == NULL || outR == NULL) return;
	
	//And don't forget to save to voice.last_out in case the next voice needs it after ADSR, but before pan
	if(voice->adsr_phase == PSXSPU_ADSR_PHASE_OFF){
		*outL = 0;
		*outR = 0;
		return;
	}
	
	int16_t samp = psxvoice_interpolate_next_sample(voice);
	int16_t adsrlvl = psxvoice_advance_adsr(voice);
	samp = psxspu_scaleAmp_16(samp, adsrlvl);
	voice->last_out = samp;
	
	//Volume
	*outL = psxspu_scaleAmp_16(samp, voice->registers->volL);
	*outR = psxspu_scaleAmp_16(samp, voice->registers->volR);
	
}

void psxspu_calcrvb(psxspu_t* spu, int16_t inL, int16_t inR){
	//From nocash documentation
	
	//TODO - should in vol and out vol be SAR 15?
	
	//Base address
	int base_addr = (int)spu->rvb_buffer_addr;
	
	//Input
	int32_t sL = (int32_t)inL * (int32_t)spu->rvb_regs->vIN_L;
	int32_t sR = (int32_t)inR * (int32_t)spu->rvb_regs->vIN_R;
	
	//Same Side
	const int32_t vWALL = (int32_t)spu->rvb_regs->vWALL;
	const int32_t vIIR = (int32_t)spu->rvb_regs->vIIR;
	const int16_t* mSAME_L = (int16_t*)(spu->sram + (base_addr + ((int)spu->rvb_regs->mSAME_L << 3)));
	const int16_t* mSAME_R = (int16_t*)(spu->sram + (base_addr + ((int)spu->rvb_regs->mSAME_R << 3)));
	const int16_t* dSAME_L = (int16_t*)(spu->sram + (base_addr + ((int)spu->rvb_regs->dSAME_L << 3)));
	const int16_t* dSAME_R = (int16_t*)(spu->sram + (base_addr + ((int)spu->rvb_regs->dSAME_R << 3)));
	
	*mSAME_L = *(mSAME_L-1) + psxspu_clamp16s(vIIR * (sL + (int32_t)*dSAME_L*vWALL - (int32_t)*(mSAME_L-1)));
	*mSAME_R = *(mSAME_R-1) + psxspu_clamp16s(vIIR * (sR + (int32_t)*dSAME_R*vWALL - (int32_t)*(mSAME_R-1)));
	
	//Opposite Side
	const int16_t* mDIFF_L = (int16_t*)(spu->sram + (base_addr + ((int)spu->rvb_regs->mDIFF_L << 3)));
	const int16_t* mDIFF_R = (int16_t*)(spu->sram + (base_addr + ((int)spu->rvb_regs->mDIFF_R << 3)));
	const int16_t* dDIFF_L = (int16_t*)(spu->sram + (base_addr + ((int)spu->rvb_regs->dDIFF_L << 3)));
	const int16_t* dDIFF_R = (int16_t*)(spu->sram + (base_addr + ((int)spu->rvb_regs->dDIFF_R << 3)));
	
	*mDIFF_L = *(mDIFF_L-1) + psxspu_clamp16s(vIIR * (sL + (int32_t)*dDIFF_L*vWALL - (int32_t)*(mDIFF_L-1)));
	*mDIFF_R = *(mDIFF_R-1) + psxspu_clamp16s(vIIR * (sR + (int32_t)*dDIFF_R*vWALL - (int32_t)*(mDIFF_R-1)));
	
	//Early Echo
	const int32_t vCOMB1 = (int32_t)spu->rvb_regs->vCOMB1;
	const int32_t vCOMB2 = (int32_t)spu->rvb_regs->vCOMB2;
	const int32_t vCOMB3 = (int32_t)spu->rvb_regs->vCOMB3;
	const int32_t vCOMB4 = (int32_t)spu->rvb_regs->vCOMB4;
	const int16_t* mCOMB1_L = (int16_t*)(spu->sram + (base_addr + ((int)spu->rvb_regs->mCOMB1_L << 3)));
	const int16_t* mCOMB2_L = (int16_t*)(spu->sram + (base_addr + ((int)spu->rvb_regs->mCOMB2_L << 3)));
	const int16_t* mCOMB3_L = (int16_t*)(spu->sram + (base_addr + ((int)spu->rvb_regs->mCOMB3_L << 3)));
	const int16_t* mCOMB4_L = (int16_t*)(spu->sram + (base_addr + ((int)spu->rvb_regs->mCOMB4_L << 3)));
	const int16_t* mCOMB1_R = (int16_t*)(spu->sram + (base_addr + ((int)spu->rvb_regs->mCOMB1_R << 3)));
	const int16_t* mCOMB2_R = (int16_t*)(spu->sram + (base_addr + ((int)spu->rvb_regs->mCOMB2_R << 3)));
	const int16_t* mCOMB3_R = (int16_t*)(spu->sram + (base_addr + ((int)spu->rvb_regs->mCOMB3_R << 3)));
	const int16_t* mCOMB4_R = (int16_t*)(spu->sram + (base_addr + ((int)spu->rvb_regs->mCOMB4_R << 3)));
	
	sL = ((int32_t)*mCOMB1_L * vCOMB1) + ((int32_t)*mCOMB2_L * vCOMB2) + ((int32_t)*mCOMB3_L * vCOMB3) + ((int32_t)*mCOMB4_L * vCOMB4);
	sR = ((int32_t)*mCOMB1_R * vCOMB1) + ((int32_t)*mCOMB2_R * vCOMB2) + ((int32_t)*mCOMB3_R * vCOMB3) + ((int32_t)*mCOMB4_R * vCOMB4);
	
	//Late Reverb APF 1
	const int32_t vAPF1 = (int32_t)spu->rvb_regs->vAPF1;
	const int dAPF1 = (int)spu->rvb_regs->dAPF1 << 3;
	const int16_t* mAPF1_L = (int16_t*)(spu->sram + (base_addr + ((int)spu->rvb_regs->mAPF1_L << 3)));
	const int16_t* mAPF1_R = (int16_t*)(spu->sram + (base_addr + ((int)spu->rvb_regs->mAPF1_R << 3)));
	
	*mAPF1_L = psxspu_clamp16s(sL - (vAPF1 * (int32_t)*(mAPF1_L - dAPF1)));
	*mAPF1_R = psxspu_clamp16s(sR - (vAPF1 * (int32_t)*(mAPF1_R - dAPF1)));
	sL = (int32_t)*(mAPF1_L - dAPF1) + ((int32_t)*mAPF1_L * vAPF1);
	sR = (int32_t)*(mAPF1_R - dAPF1) + ((int32_t)*mAPF1_R * vAPF1);
	
	//Late Reverb APF 2
	const int32_t vAPF2 = (int32_t)spu->rvb_regs->vAPF2;
	const int dAPF2 = (int)spu->rvb_regs->dAPF2 << 3;
	const int16_t* mAPF2_L = (int16_t*)(spu->sram + (base_addr + ((int)spu->rvb_regs->mAPF2_L << 3)));
	const int16_t* mAPF2_R = (int16_t*)(spu->sram + (base_addr + ((int)spu->rvb_regs->mAPF2_R << 3)));
	
	*mAPF2_L = psxspu_clamp16s(sL - (vAPF2 * (int32_t)*(mAPF2_L - dAPF2)));
	*mAPF2_R = psxspu_clamp16s(sR - (vAPF2 * (int32_t)*(mAPF2_R - dAPF2)));
	sL = (int32_t)*(mAPF2_L - dAPF2) + ((int32_t)*mAPF2_L * vAPF2);
	sR = (int32_t)*(mAPF2_R - dAPF2) + ((int32_t)*mAPF2_R * vAPF2);
	
	//Output
	const int32_t vOUT_L = (int32_t)spu->rvboutvol_L;
	const int32_t vOUT_R = (int32_t)spu->rvboutvol_R;
	spu->rvbout_L = psxspu_clamp16s(vOUT_L * sL);
	spu->rvbout_L = psxspu_clamp16s(vOUT_R * sR);
	
	//Increment addr
	spu->rvb_buffer_addr = max((uint32_t)spu->rvbarea_addr << 3, (spu->rvb_buffer_addr + 2) & 0x7FFFE);
	
}

void psxspu_next_sample(psxspu_t* spu, psxspu_out_buffer_t* out_buff, int buffer_offset){

	if(!spu) return;
	
	int32_t primary_L;
	int32_t primary_R;
	int32_t rvbin_L;
	int32_t rvbin_R;
	
	//Pull samples from each voice (that's in use)...
	//Remember to write these samples to voice buffers (if voice is off, write 0)
	//Apply master volume as pulling to reduce risk of clipping?? I'm working in 32-bit so maybe not?? Dunno.
	//Add all to the primary out, then if reverb is set, also to reverb out (will be processed in rvb area)
	//Do reverb calculation every other cycle (can just track this in an spu counter)
	//Output sum of reverb and primary
	
	int i = 0;
	uint32_t vmask = 0x1L;
	uint16_t rvbenabled = spu->SPUCNT & PSXSPU_RVBENABLED_FLAG;
	for(i = 0; i < PSXSPU_VOICE_COUNT; i++){
		//Write to both output buffer and voice level register
		if(spu->vox_stat & vmask){
			int16_t* l_out_ptr = (int16_t)(spu->voice_out + i);
			int16_t* r_out_ptr = l_out_ptr + 1;
			psxvoice_next_sample(spu->voices + i, l_out_ptr, r_out_ptr);
			
			if(out_buff){
				*(out_buff->voices_L[i] + buffer_offset) = *l_out_ptr;
				*(out_buff->voices_R[i] + buffer_offset) = *r_out_ptr;
			}
			
			//Add to sums as appropriate
			primary_L += *l_out_ptr;
			primary_R += *r_out_ptr;
			
			if(rvbenabled && (spu->vox_reverb & vmask)){
				rvbin_L += *l_out_ptr;
				rvbin_R += *r_out_ptr;
			}
		}
		else{
			spu->voice_out[i] = 0;
			if(out_buff){
				*(out_buff->voices_L[i] + buffer_offset) = 0;
				*(out_buff->voices_R[i] + buffer_offset) = 0;
			}
		}
		vmask <<= 1;
	}
	
	//Reverb
	int32_t out32_L = primary_L;
	int32_t out32_R = primary_R;
	if(rvbenabled){
		if((spu->cycle_ctr++ % 2) == 0){
			int16_t rvbin16_L = psxspu_clamp16s(rvbin_L);
			int16_t rvbin16_R = psxspu_clamp16s(rvbin_R);
		
			psxspu_calcrvb(spu, rvbin16_L, rvbin16_R);
		}
		out32_L += spu->rvbout_L;
		out32_R += spu->rvbout_R;
	}
	
	//Don't forget to saturate to 16 bits...
	spu->outvol_L = psxspu_clamp16s(out32_L);
	spu->outvol_R = psxspu_clamp16s(out32_R);
	
	if(out_buff){
		*(out_buff->master_L + buffer_offset) = spu->outvol_L;
		*(out_buff->master_R + buffer_offset) = spu->outvol_R;
	}
	
	//Note: the voices are written separately for the use of callbacks that monitor individual voices, BUUUTTT
	//since the SPU only processes reverb on the master mix, that means these individual voices will NOT include reverb!
	
}

int16_t psxspu_clamp16s(int32_t in){
	int32_t out32 = (int32_t)in;
	if(out32 > VAGP_SMAX) out32 = VAGP_SMAX;
	if(out32 < VAGP_SMIN) out32 = VAGP_SMIN;
	return(int16_t)out32;
}

uint16_t psxspu_getspu_sr_value(int cent_change){
	//TODO
	return 0;
}

void psxspu_scale_pan(int8_t pan, int16_t* reg_volL, int16_t* reg_volR){
	//A standard pan seems to be linear in amplitude
	//Where center is -n dB (often 6 or 3)
	//And hard side is 0.0 that side, -Inf opp.
	//Therefore, if r is the amplitude ratio (where 1.0 is max amplitude)
	//And p_amt is pan to "same" side from 0.0 - 1.0
	//We'll have a_ctr be the amplitude ratio at the center (10^(n/20))
	// r_same = [p_amt * (1 - a_ctr)] + a_ctr
	// r_opp  = [p_amt * -a_ctr] + a_ctr
	
	double panamt = 0.0;
	uint8_t right = 0;
	
	if(pan < 0x40){
		//Left
		panamt = 1.0 - ((double)pan/64.0);
	}
	else{
		//Right
		panamt = (((double)pan - 63.0)/63.0);
		right = 1;
	}
	
	double r_same = (panamt * (1.0 - PSXSPU_PANCTR)) + PSXSPU_PANCTR;
	double r_opp = (panamt * (-1.0 * PSXSPU_PANCTR)) + PSXSPU_PANCTR;
	
	int16_t a_same = (int16_t)(r_same * (double)0x7FFF);
	int16_t a_opp = (int16_t)(r_opp * (double)0x7FFF);
	
	if(right){*reg_volL = a_opp; *reg_volR = a_same;}
	else{*reg_volL = a_same; *reg_volR = a_opp;}
	
}
