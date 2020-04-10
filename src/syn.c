#include <assert.h>
#include "syn.h"

void syn_init(syn* s, int sr){
	smath_init();

	s->mutex = SDL_CreateMutex();
	if(!s->mutex) fprintf(stderr, "Couldn't create mutex\n");

	s->sr=sr;
	s->bpm = 120;
	s->seq_play = 0;
	s->modm_lerp_mode = 1;

	for(int i =0; i<SYN_TONES; i++){
		memset(s->tone[i].mod_mat, 0, sizeof(s->tone[i].mod_mat));
		memset(s->modm_target+i, 0, sizeof(syn_mod_mat));
		s->modm_target_active[i] = 0;
		s->modm_lerpt[i] = 0;
		s->modm_lerpms[i] = 0;

		// for (int j = 0; j < VEL_OUT; j++){
		// 	s->tone[i].vel_out[j] = NULL;
		// 	s->tone[i].vel_min[j] = 0;
		// 	s->tone[i].vel_max[j] = 4;
		// }

		for (int j = 0; j < POLYPHONY; j++){
			s->tone[i].pitch_env_amt = 0;

			s->tone[i].vel[j] = 0;

			s->tone[i].gate[j]=0;
			s->tone[i].pitch_state[j]=0;
			s->tone[i].pitch_eg_out[j]=0;
			for(int k=0; k<OSC_PER_TONE; k++){
				s->tone[i].amp_state[j][k]=0;
				s->tone[i].amp_eg_out[j][k]=0;
			}

		}

		s->tone[i].gain=0.75;
		syn_seq_init(&s->seq[i]);

		// s->tone[i].vel_out[0] = &s->tone[i].pitch_env_amt;
		// s->tone[i].vel_out[1] = &s->tone[i].pitch_env_amt;

		s->tone[i].voices=0;

		s->tone[i].pitch_env = adsr_make(sr, .001, .001, 1, 0.001 , .0,0);
		for(int k=0; k<OSC_PER_TONE; k++){
			s->tone[i].osc_env[k] = adsr_make(sr, .001, .001, 1, 0.001 , 0,0);

			s->tone[i].osc[k] = 0;
			tone_omix(s->tone+i, k, k==0? .25 : 0);
			s->tone[i].pwm[k] = .5;
			s->tone[i].mod_mat[k][k] =  1;
		}
	}

}


void syn_quit(syn* s){
	SDL_DestroyMutex(s->mutex);
	for(int i=0; i<SYN_TONES; i++){
		seq_unload(s->seq + i);
	}
	memset(s, 0, sizeof(syn));
}


void syn_seq_init(syn_seq* s){
	memset(s, 0, sizeof(syn_seq));
	for (int i = 0; i < POLYPHONY; ++i){
		s->active[i] = (noteid){-1,-1};
	}
	s->spb=SEQ_STEP_PER_BEAT_DEF;
	s->len=MIN(SEQ_LEN, 16);
	s->time=0;
	s->step=-1;

	s->tone = NULL;
	s->modm_target_step=-1;
	memset(s->modm, 0, sizeof(syn_mod_mat*)*SEQ_LEN);
	s->modm_wait_loop=0;
}



float syn_a4(syn*s , float a4hz){
	float ret = s->a4;
	if(a4hz>0) s->a4 = a4hz;
	return ret;
}

float syn_bpm(syn*s , float bpm){
	// syn_lock(s, 1);
	assert(s);
	float ret = s->bpm;
	if(bpm >= 0)
		s->bpm = bpm;

	// for(int i=0; i<SYN_TONES; i++){
		// s->seq[i].time=0;
		// s->seq[i].step-=1;
		// syn_anof(s, i);
	// }

	// syn_lock(s, 0);
	return ret;
}

void syn_seq_advance(syn* s, float secs){
	float bps = s->bpm/60.0;

	for(int i=0; i<SYN_TONES; i++){
		int prev_step = MIN(floor(s->seq[i].time), s->seq[i].step);

		float stb = secs * bps*s->seq[i].spb;
		s->seq[i].time += stb;

		s->seq[i].time = fmodf(s->seq[i].time, s->seq[i].len);

		int step = floor(s->seq[i].time);
		s->seq[i].step = step;

/* tone interpolation */
		switch(s->modm_lerp_mode){
			case 0:
				if( step != prev_step ){
					if(s->seq[i].modm[step] != NULL){
						syn_modm_lerp(s, i, s->seq[i].modm[step], 0);
					}
				}
				break;
			case 1:
				if( step != prev_step ){
					if(s->seq[i].modm[step] != NULL){
						syn_modm_lerp(s, i, s->seq[i].modm[step], 100.0);
					}
				} break;
			case 2:
				if((prev_step==-1) || (step >= s->seq[i].modm_target_step && !s->seq[i].modm_wait_loop)){
					// float time_per_step = 1.0/(bps/s->seq[i].spb/1000.0);
					// float time_per_step = (1000.0/(bps/s->seq[i].spb));
					float time_per_step = 1000.0*(1.0/(bps*s->seq[i].spb));
					// float time_per_step = 1.0/((s->bpm/s->seq[i].spb)/60000.0);
					float tltime = prev_step!=-1 ? time_per_step : 0;
					// float tltime = 0;
					for(int ti=0; ti<s->seq[i].len; ti++){
						int tim = (ti+(step)+1) % s->seq[i].len;
						if(s->seq[i].modm[tim] != NULL){
							syn_modm_lerp(s, i, s->seq[i].modm[tim], tltime);
							s->seq[i].modm_target_step = tim;
							if(tim < step) s->seq[i].modm_wait_loop = 1;
							// printf("modulating i %i, t %f target step %i currstep %i w4l %i\n", i, tltime, tim, step, s->seq[i].modm_wait_loop);
							break;
						}
						tltime += time_per_step;
					}
				}
				break;
			default: break;
		}

		float nort = s->seq[i].time-floor(s->seq[i].time);
		for (int j = 0; j < POLYPHONY; j++){
			// char tie = prev_step!=-1? s->seq[i].dur[prev_step] > .98 : 0;
			char tie = prev_step!=-1? s->seq[i].dur[prev_step] > 250 : 0;

			// check for note expiration
			// if(nort > s->seq[i].dur[s->seq_step] &&!tie/*&& s->seq[i].dur[s->seq_step] < .98*/){
			if((nort > s->seq[i].dur[s->seq[i].step]/250.f) && (!tie) && (prev_step == s->seq[i].step) && (s->seq[i].active[j].voice > -1)){
				syn_nof(s, s->seq[i].active[j]);
				s->seq[i].active[j] = (noteid){-1,-1};
			}

			if(prev_step == s->seq[i].step) continue;

			if((prev_step != -1) && (s->seq[i].active[j].voice > -1) && !tie){
				syn_nof(s, s->seq[i].active[j]);
				s->seq[i].active[j] = (noteid){-1,-1};
			}

			if(s->seq[i].freq[j][s->seq[i].step]>0){

				if(!tie)
					s->seq[i].active[j] = syn_non(s, i, 0, 0);
				syn_nfreq(s, s->seq[i].active[j], s->seq[i].freq[j][s->seq[i].step]);
				syn_nvel(s, s->seq[i].active[j], s->seq[i].vel[j][s->seq[i].step]);
			}

		}
		if(prev_step == s->seq[i].len-1 && step == 0){
			s->seq[i].modm_target_step = 0;
			s->seq[i].modm_wait_loop = 0;

		}

	}
}

float tone_frat(syn_tone* t, int osc, float freq_ratio){
	assert(t);
	assert(osc < OSC_PER_TONE);
	float prev = t->mod_mat[osc][osc];
	if(freq_ratio > 0)
		t->mod_mat[osc][osc] = freq_ratio;
	return prev;
}

 // <0 to avoid setting; carrier > modulator
float tone_index(syn_tone* t, int car, int mod, float index){
	assert(t);
	assert(car < OSC_PER_TONE);
	assert(mod < car);
	float prev = t->mod_mat[car][mod];
	if(index >= 0)
		t->mod_mat[car][mod] = index;
	return prev;
}

 // <0 to avoid setting
float tone_omix(syn_tone* t, int osc, float mix){
	assert(t);
	assert(osc < OSC_PER_TONE);
	float prev;
	if(osc==0)
		prev = t->mod_mat[1][2];
	else
		prev = t->mod_mat[0][osc];

	if(mix >= 0){
		if(osc==0)
			t->mod_mat[1][2] = mix;
		else
			t->mod_mat[0][osc] = mix;
	}
	return prev;
}



// len: number of stereo samples to process
void syn_run(syn* s, float* buffer, int len){
	syn_lock(s,1);
	float a[OSC_PER_TONE];

	if(s->seq_play) syn_seq_advance(s, ((float)len)/(s->sr) );

	for(int i=0; i<SYN_TONES; i++){

/* update modulation matrix */
if(s->modm_target_active[i]){
	// syn_modm_do_lerp(&s->tone[i].mod_mat, s->modm_target+i, s->modm_lerpt[i]/s->modm_lerpms[i]);
	syn_modm_do_lerp(&s->tone[i].mod_mat, s->modm_target+i, MAPVAL(s->modm_lerpt[i], 0, s->modm_lerpms[i],0,1));
	s->modm_lerpt[i] += ((float)len)/(s->sr)*1000.0;
	if(s->modm_lerpt[i] >= s->modm_lerpms[i]){
		s->modm_target_active[i] = 0;
	}
}
		if(s->tone[i].voices==0) {continue;}

		s->tone[i].vupeakl=0;
		s->tone[i].vupeakr=0;
// char has_lerpt_modm=0;
// syn_mod_mat temp_modm;

		for (int j=0; j<POLYPHONY; j++){

			float vel = CLAMP(s->tone[i].vel[j], 0.0, 1.0);

			// for(int vi=0; vi < VEL_OUT; vi++){
			// 	float* vo = s->tone[i].vel_out[vi];
			// 	if(!vo) continue;
			// 	*vo = MAPVAL( vel , 0.0, 1.0, s->tone[i].vel_min[vi], s->tone[i].vel_max[vi] );
			// }

			// if(s->tone[i].osc_env[j].state==0){
			int any_on =0;
			for(int osc =0; osc<OSC_PER_TONE; osc++)
				any_on += s->tone[i].amp_state[j][osc];

			if(any_on==0){
				if(s->tone[i].freq[j] > 0) s->tone[i].voices--;
				s->tone[i].freq[j]=0;
				continue;
			}

			for(int smp=0; smp<len*2;){

				// int any_on =0;
				for(int osc =0; osc<OSC_PER_TONE; osc++)
					any_on += s->tone[i].amp_state[j][osc];

				if(any_on==0) break;

				float oscaccum=0;
				memset(a, 0, sizeof(a));

				s->tone[i].pitch_env.gate  = s->tone[i].gate[j];
				s->tone[i].pitch_env.state = s->tone[i].pitch_state[j];
				s->tone[i].pitch_env.out   = s->tone[i].pitch_eg_out[j];

				float pitch_env = adsr_run(&s->tone[i].pitch_env)*s->tone[i].pitch_env_amt;

				// s->tone[i].gate[j]         = s->tone[i].pitch_env.gate;
				s->tone[i].pitch_state[j]  = s->tone[i].pitch_env.state;
				s->tone[i].pitch_eg_out[j] = s->tone[i].pitch_env.out;


				// if(s->tone[i].osc_env[j].state==0){
				// 	if(s->tone[i].freq[j] > 0) s->tone[i].voices--;
				// 	s->tone[i].freq[j]=0;
				// 	break;
				// }

				//glide
				// if(s->tone[i].glide_t < s->tone[i].glide)
				// 	if(s->tone[i].poly_type == 0){
				// 		s->tone[i].freq[j] = lerp(s->tone[i].freq[j], s->tone[i].glide_freq_target,
				// 			MAPVAL(s->tone[i].glide_t, 0, s->tone[i].glide, 0, 1) );
				// 		s->tone[i].glide_t += 1.0/s->sr;
				// 	}

				for(int osc=0; osc<OSC_PER_TONE; osc++){
					if(s->tone[i].amp_state[j][osc] == 0) continue;

					s->tone[i].osc_env[osc].gate =     s->tone[i].gate[j];
					s->tone[i].osc_env[osc].state =    s->tone[i].amp_state[j][osc];
					s->tone[i].osc_env[osc].out =      s->tone[i].amp_eg_out[j][osc];
					float osc_env = adsr_run(&s->tone[i].osc_env[osc]);
					s->tone[i].amp_state[j][osc]   = s->tone[i].osc_env[osc].state;
					// s->tone[i].gate[j]             = s->tone[i].osc_env[osc].gate;
					s->tone[i].amp_eg_out[j][osc]  = s->tone[i].osc_env[osc].out;

					if(osc_env<=0) continue;

					float t = s->tone[i].time[j][osc] ;
					switch(s->tone[i].osc[osc]){
						case OSC_SAW:    a[osc]=saw (t); break;
						case OSC_TRI:    a[osc]=tri (t); break;
						case OSC_SINE:   a[osc]=sine(t); break;
						case OSC_PULSE:  a[osc]=pul (t, s->tone[i].pwm[osc]); break;
						case OSC_SQUARE: a[osc]=sqr (t); break;
						case OSC_NOISE:  a[osc]=wnoiz (t); break;
						// case OSC_NOISE:  a=noise(); break;
						default: break;
					}

					float fm=0;
					for(int mod=0; mod<osc; mod++){
						float fmamp = s->tone[i].freq[j] * tone_index(s->tone+i, osc, mod, -1) * tone_frat(s->tone+i, mod, -1);
						fm += a[mod] * fmamp;
					}

					// if(osc > 0){ //alg1?
					// 	fmamp = s->tone[i].freq[j][osc-1]*s->tone[i].freq_ratio[osc-1]*(s->tone[i].fmindex[osc-1])*osc_env;
					// 		fm=a[osc-1]*fmamp;
					// 		// fm=sine(s->tone[i].time[j][0])*fmamp;
					// }

					// accum time
					// float frat= s->tone[i].freq[j][osc] * s->tone[i].freq_ratio[osc];
					float frat= s->tone[i].freq[j] * tone_frat(s->tone+i , osc, -1);
					s->tone[i].time[j][osc] += /*fabsf*/(fm + s->master_detune + frat + frat*pitch_env) / s->sr;
					s->tone[i].time[j][osc] = fmodf(s->tone[i].time[j][osc], 1);
					if(s->tone[i].time[j][osc]<0) s->tone[i].time[j][osc] +=1;

					// printf("%f\n", a);
					// a[osc] *= osc_env;
					// a[osc] *= osc_env[osc]; todo
					// a[osc] *= .25; // count out carriers to average
					// a[osc] *= s->tone[i].osc_mix[osc];
					// oscaccum += a[osc] * s->tone[i].gain;
					// a[osc] *= s->tone[i].gain;
					a[osc] *= osc_env;
					// accum carriers
					oscaccum += a[osc] * tone_omix(s->tone+i, osc, -1);
				}
				oscaccum *= s->tone[i].gain;

				s->tone[i].vupeakl = MAX(fabsf(s->tone[i].vupeakl), oscaccum);
				s->tone[i].vupeakr = MAX(fabsf(s->tone[i].vupeakr), oscaccum);
				// oscaccum = ( a[1] )*s->tone[i].gain*osc_env; // alg1
				buffer[smp] += oscaccum/* * s->tone[i].level[j]*/; smp++;
				buffer[smp] += oscaccum/* * s->tone[i].level[j]*/; smp++;

			}
		}
	}
	s->sample+=len;

	syn_lock(s,0);

}




noteid syn_non(syn* s, int instr, float note, float vel){ // note: semitone distance from A4
	if(instr<0 || instr>SYN_TONES-1) return (noteid){-1,-1};

	float freq = freqn(note);

	int tone = CLAMP(instr, 0, SYN_TONES-1);

	int voice = -1;
	float target_env_out = INFINITY;

	for(int i =0; i<POLYPHONY; i++){
		if( s->tone[tone].gate[i] ) continue;

		int any_on=0;
		for(int osc=0; osc<OSC_PER_TONE; osc++)
			any_on+=s->tone[tone].amp_state[i][osc]!=0;

		if(!any_on){ // new voice
			voice = i;
			s->tone[tone].voices++;
			break;
		}

		// replace the dimmest voice being released
		float out_accum=0;
		for(int osc=0; osc<OSC_PER_TONE; osc++)
			out_accum+=s->tone[tone].amp_eg_out[i][osc];

		if( out_accum < target_env_out ) {
			target_env_out = out_accum;
			voice = i;
		}
	}
	// char mono = s->tone[tone].poly_type == 0;
	// if(!mono){
	if(voice==-1) printf("tone %i ran out of voices %f(%fhz)\n", tone, note, freq);
	if(voice==-1) return (noteid){-1,-1};
	// }

	// if(!mono || s->tone[tone].voices==0){
	for (int k = 0; k < OSC_PER_TONE; ++k){
		s->tone[tone].amp_state[voice][k] = 1; // attack
		s->tone[tone].amp_eg_out[voice][k] = 0;
	}
	s->tone[tone].gate[voice] = 1;
	s->tone[tone].pitch_state[voice] = 1;

	s->tone[tone].freq[voice] = freq;
	s->tone[tone].vel[voice] = vel;
		// s->tone[tone].glide_freq_target = freq;
		// s->tone[tone].glide_t = s->tone[tone].glide;

		// s->note_history[tone][s->note_history_i[tone]] = freq;
		// s->note_history_i[tone]=1;

	// } else {
		// if(s->note_history_i[tone] >= HISTORY_LEN) return (noteid){-1,-1};
		// s->note_history[tone][s->note_history_i[tone]] = freq;
		// voice = s->note_history_i[tone];
		// s->note_history_i[tone]++;
	// 	s->tone[tone].glide_t = 0;
	// 	s->tone[tone].glide_freq_target = freq;
	// }

	return (noteid){tone, voice};
}






void syn_nof(syn* s, noteid nid){
	if(nid.voice<0 || nid.tone<0) return;
	short tone = CLAMP(nid.tone, 0, SYN_TONES-1);
	short voice = CLAMP(nid.voice, 0, POLYPHONY-1);

	for( int osc = 0 ; osc<OSC_PER_TONE; osc++)
		if(s->tone[tone].amp_state[voice][osc] != 0)
			s->tone[tone].amp_state[voice][osc] = 4; //release

	s->tone[tone].gate[voice] = 0;
	if(s->tone[tone].pitch_state[voice] != 0)
		s->tone[tone].pitch_state[voice] = 4; //release
}


void syn_anof(syn* s, int instr){
	if(instr<0 || instr > SYN_TONES-1) return;

	int tone = CLAMP(instr, 0, SYN_TONES-1);
	for(int i =0; i<POLYPHONY; i++){

		for( int osc = 0 ; osc<OSC_PER_TONE; osc++){
			if(s->tone[tone].amp_state[i][osc] != 0)
				s->tone[tone].amp_state[i][osc] = 4; //release
		}
		s->tone[tone].gate[i] = 0;
		if(s->tone[tone].pitch_state[i] != 0)
			s->tone[tone].pitch_state[i] = 4; //release
	}
}


void syn_nvel(syn* s, noteid nid, float vel){
	if(nid.voice<0 || nid.tone<0) return;
	int tone = CLAMP(nid.tone, 0, SYN_TONES-1);
	int voice = CLAMP(nid.voice, 0, POLYPHONY-1);
	s->tone[tone].vel[voice] = vel;
}


void syn_nset(syn* s, noteid nid, float note){
	if(nid.voice<0 || nid.tone<0) return;

	float freq = freqn(note);
	int tone = CLAMP(nid.tone, 0, SYN_TONES-1);
	int voice = CLAMP(nid.voice, 0, POLYPHONY-1);

	s->tone[tone].freq[voice] = freq;
}


void syn_nfreq(syn* s, noteid nid, float freq){
	if(nid.voice<0 || nid.tone<0) return;
	int tone = CLAMP(nid.tone, 0, SYN_TONES-1);
	int voice = CLAMP(nid.voice, 0, POLYPHONY-1);
	s->tone[tone].freq[voice] = freq;
}



void syn_pause(syn* s){ assert(s);
	s->seq_play = !s->seq_play;
	for(int i=0; i<SYN_TONES; i++){
		syn_anof(s, i);
		s->seq[i].time = 0;
		s->seq[i].step = -1;
	}
}


void syn_lock(syn* s, char l){ assert(s);
	if(l){
		if(SDL_LockMutex(s->mutex)!=0) {fprintf(stderr, "Couldn't lock mutex\n");}
	} else
		SDL_UnlockMutex(s->mutex);
}


void seq_non(syn_seq* s, int pos, float note, float vel, float dur){ assert(s);
	pos = CLAMP(pos, 0, s->len);
	int voice = -1;
	for(int i =0; i<POLYPHONY; i++){
		if( s->freq[i][pos] > 0 ) continue;
		voice = i;
		break;
	}
	if(voice==-1) return;

	float freq = freqn(note);
	s->freq[voice][pos]=freq;
	s->vel[voice][pos]=vel*255;
	s->dur[pos]=dur*255;
}

void seq_nof(syn_seq* s, int pos, int voice){ assert(s);
	pos = CLAMP(pos, 0, s->len);
	voice = CLAMP(voice, 0, POLYPHONY);
	s->freq[voice][pos] = 0;
}

void seq_anof(syn_seq* s, int pos){ assert(s);
	pos = CLAMP(pos, 0, s->len);
	for(int i=0; i<POLYPHONY; i++){
		seq_nof(s, pos, i);
	}
}

void seq_clear(syn_seq* s){ assert(s);
	for (int i = 0; i < s->len; i++){
		seq_anof(s, i);
	}
}

char seq_ison(syn_seq* s, int pos, int voice){ assert(s);
	pos = CLAMP(pos, 0, s->len);
	voice = CLAMP(voice, 0, POLYPHONY);
	return s->freq[voice][pos] > 0;
}

char seq_isempty(syn_seq* s, int pos){ assert(s);
	pos = CLAMP(pos, 0, s->len);
	char ret = 0;
	for (int i = 0; i < POLYPHONY; i++){
		ret |= seq_ison(s, pos, i);
	}
	return ret;
}


char seq_isgate(syn_seq* s, int voice){//??
	// pos = CLAMP(pos, 0, s->len);
	voice = CLAMP(voice, 0, POLYPHONY);
	// return s->freq[voice][pos] > 0;
	// s->active[voice].tone != -1;
	char ret = 0;
	for(int i=0; i<POLYPHONY; i++)
		ret |= s->active[i].voice == voice;
	return ret;
}





// amp eg setter/getter
float tone_atk(syn_tone* t, int osc, float a){ assert(t); assert(osc<OSC_PER_TONE && osc>=0);
	float ret = t->osc_env[osc].a_rate / t->osc_env[osc].sr;
	if(a>=0) adsr_a(t->osc_env+osc, a);
	return ret;
}
float tone_dec(syn_tone* t, int osc, float a){ assert(t); assert(osc<OSC_PER_TONE && osc>=0);
	float ret = t->osc_env[osc].d_rate / t->osc_env[osc].sr;
	if(a>=0) adsr_d(t->osc_env+osc, a);
	return ret;
}
float tone_sus(syn_tone* t, int osc, float a){ assert(t); assert(osc<OSC_PER_TONE && osc>=0);
	float ret = t->osc_env[osc].s;
	if(a>=0) adsr_s(t->osc_env+osc, a);
	return ret;
}
float tone_rel(syn_tone* t, int osc, float a){ assert(t); assert(osc<OSC_PER_TONE && osc>=0);
	float ret = t->osc_env[osc].r_rate / t->osc_env[osc].sr;
	if(a>=0) adsr_r(t->osc_env+osc, a);
	return ret;
}
float tone_aexp(syn_tone* t, int osc, float ae){ assert(t); assert(osc<OSC_PER_TONE && osc>=0);
	float ret = t->osc_env[osc].ratioa / t->osc_env[osc].sr;
	if(ae>=0) adsr_aexp(t->osc_env+osc, ae);
	return ret;
}
float tone_dexp(syn_tone* t, int osc, float de){ assert(t); assert(osc<OSC_PER_TONE && osc>=0);
	float ret = t->osc_env[osc].ratiodr / t->osc_env[osc].sr;
	if(de>=0) adsr_drexp(t->osc_env+osc, de);
	return ret;
}
// pitch eg
float tone_patk(syn_tone* t, float a){assert(t);
	float ret = t->pitch_env.a_rate / t->pitch_env.sr;
	if(a>=0) adsr_a(&t->pitch_env, a);
	return ret;
}
float tone_pdec(syn_tone* t, float a){assert(t);
	float ret = t->pitch_env.d_rate / t->pitch_env.sr;
	if(a>=0) adsr_d(&t->pitch_env, a);
	return ret;
}
float tone_psus(syn_tone* t, float a){assert(t);
	float ret = t->pitch_env.s;
	if(a>=0) adsr_s(&t->pitch_env, a);
	return ret;
}
float tone_prel(syn_tone* t, float a){assert(t);
	float ret = t->pitch_env.r_rate / t->pitch_env.sr;
	if(a>=0) adsr_r(&t->pitch_env, a);
	return ret;
}
float tone_paexp(syn_tone* t, float a){assert(t);
	float ret = t->pitch_env.ratioa / t->pitch_env.sr;
	if(a>=0) adsr_aexp(&t->pitch_env, a);
	return ret;
}
float tone_pdexp(syn_tone* t, float a){assert(t);
	float ret = t->pitch_env.ratiodr / t->pitch_env.sr;
	if(a>=0) adsr_drexp(&t->pitch_env, a);
	return ret;
}








void syn_modm_lerp(syn* s, int tone, syn_mod_mat* target, float mstime){
	assert(s); assert(tone < SYN_TONES);
	memcpy(s->modm_target+tone, target, sizeof(syn_mod_mat));
	s->modm_target_active[tone]=1;
	s->modm_lerpt[tone]=0;
	s->modm_lerpms[tone]=MAX(mstime, 0);
	if(mstime<=0.0001){
		s->modm_lerpt[tone]=1;
		s->modm_lerpms[tone]=1;
	}
}



void syn_modm_do_lerp(syn_mod_mat* dst, syn_mod_mat* src, float t){
	for(int n=0; n<OSC_PER_TONE; n++)
		for(int m=0; m<OSC_PER_TONE; m++)
			(*dst)[n][m] = lerp((*dst)[n][m], (*src)[n][m], t);
}




short seq_len(syn_seq* s, short len){ assert(s);
	short ret = s->len;
	if(len>0) s->len=len;
	return ret;
}

short seq_spb(syn_seq* s, short spb){ assert(s);
	short ret = s->spb;
	if(spb>0) s->spb=spb;
	return ret;
}




void seq_modm(syn_seq* s, syn_mod_mat* m, int step){ assert(s);
	assert(step < SEQ_LEN);
	assert(step >= 0);
	if(s->modm[step] == NULL)
		s->modm[step] = malloc(sizeof(syn_mod_mat));
	if(m)
		memcpy( s->modm[step], m, sizeof(syn_mod_mat) );
	else{
		free(s->modm[step]);
		s->modm[step] = NULL;
	}
}

void seq_unload(syn_seq* s){ assert(s);
	for(int i=0; i<SEQ_LEN; i++){
		if(s->modm[i] != NULL){
			free(s->modm[i]);
			s->modm[i] = NULL;
		}
	}
}


// o=m to get ratio of operator o, m<0 to get oscmix,
// m<o to get index of operator o for modulator m
float* syn_modm_addr(syn_mod_mat* modm, int o, int m){ assert(modm);
	assert(m<=o);
	o = CLAMP(o, 0, OSC_PER_TONE-1);
	m = MIN(m, OSC_PER_TONE-1);
	if(m<0 && o==0) return &((*modm)[1][2]);
	else if(m<0) return &((*modm)[0][o]);
	return &((*modm)[o][m]);
}
