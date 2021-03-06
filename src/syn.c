#include <assert.h>
#include "syn.h"

void syn_init(syn* s, int sr){
	smath_init();
	memset(s, 0, sizeof(syn));
	s->mutex = SDL_CreateMutex();
	if(!s->mutex) SYNLOG("Couldn't create mutex");

	s->sr=sr;
	s->gain=1.f;
	s->bpm = 120;
	s->seq_play = 0;
	s->modm_lerp_mode = 0;
	s->buflen=sr*SYN_BUFLEN_TIME;
	s->bufpos=0;
	s->filter_order=22;
	for(int i =0; i<SYN_TONES; i++){
		memset(s->modm_target+i, 0, sizeof(syn_mod_mat));
		s->modm_target_active[i] = 0;
		s->modm_lerpt[i] = 0;
		s->modm_lerpms[i] = 0;
		s->bufl[i]=malloc(s->buflen*sizeof(float));
		s->bufr[i]=malloc(s->buflen*sizeof(float));
		s->delay_bufl[i]=malloc(s->buflen*sizeof(float));
		s->delay_bufr[i]=malloc(s->buflen*sizeof(float));
		memset(s->bufl[i], 0, s->buflen*sizeof(float));
		memset(s->bufr[i], 0, s->buflen*sizeof(float));
		memset(s->delay_bufl[i], 0, s->buflen*sizeof(float));
		memset(s->delay_bufr[i], 0, s->buflen*sizeof(float));
	}


	syn_song_init(s);
	syn_song_pos(s, 0);
}


void syn_quit(syn* s){
	SDL_DestroyMutex(s->mutex);
	syn_song_free(s);
	for(int i =0; i<SYN_TONES; i++){
		free(s->bufl[i]); s->bufl[i]=NULL;
		free(s->bufr[i]); s->bufr[i]=NULL;
		free(s->delay_bufl[i]); s->delay_bufl[i]=NULL;
		free(s->delay_bufr[i]); s->delay_bufr[i]=NULL;
	}
	memset(s, 0, sizeof(syn));
}


void syn_song_init(syn* s){
	s->song = malloc(sizeof(syn_seq)*SYN_TONES*SONG_MAX);
	s->song_tones = malloc(sizeof(syn_tone)*SYN_TONES*SONG_MAX);
	for(int i = 0; i<SYN_TONES*SONG_MAX; i++){
		syn_seq_init(s->song+i);
		syn_tone_init(s->song_tones+i, s->sr);
		s->song[i].tone = s->song_tones+i;
	}
	for(int i = 0; i<SYN_TONES; i++){
		s->seq[i]=s->song+i;
		s->tone[i]=s->song_tones+i;
	}
	s->song_pat = malloc(sizeof(uint8_t)*SONG_MAX);
	memset(s->song_pat, 0, sizeof(uint8_t)*SONG_MAX);

	s->song_beat_dur = malloc(sizeof(uint8_t)*SONG_MAX);
	memset(s->song_beat_dur, 4, sizeof(uint8_t)*SONG_MAX);

	s->song_bpm = malloc(sizeof(float)*SONG_MAX);
	memset(s->song_bpm, 0, sizeof(float)*SONG_MAX);

	s->song_tie = malloc(SONG_MAX);
	memset(s->song_tie, 0, SONG_MAX);

	s->song_pos=0;
	s->song_len=SONG_MAX-1;
	s->song_spb=4;
	s->song_loop_begin=0;
	s->song_loop_end=0;
	s->song_loop=0;
	s->song_time=0;
	s->song_advance=1;

}

void syn_song_free(syn* s){ assert(s);
	for(int i=0; i<SYN_TONES*SONG_MAX; i++){
		seq_unload(s->song+i);
	}
	free(s->song); s->song=NULL;
	free(s->song_tones); s->song_tones=NULL;
	free(s->song_pat); s->song_pat=NULL;
	free(s->song_beat_dur); s->song_beat_dur=NULL;
	free(s->song_bpm); s->song_bpm=NULL;
	free(s->song_tie); s->song_tie=NULL;
	s->song_pos=0;
	s->song_len=0;
	s->song_time=0;
}

int syn_song_pos(syn* s, int pos){ assert(s);
	int ret = s->song_pos;
	if(pos>=0){
		pos=MIN(pos,SONG_MAX-1);
		for(int i=0; i<SYN_TONES; i++){
			if(s->seq[i]){
				syn_anof(s, i);
				for(int j=0; j<POLYPHONY; j++)
					s->seq[i]->active[j]=(noteid){-1,-1};
			}
			// transfer envelope state
			if(s->song_tie[s->song_pos]){
				for(int j=0; j<POLYPHONY; j++){
					for(int osc=0; osc<OSC_PER_TONE; osc++){
						s->song[s->song_pat[pos]*SYN_TONES+i].tone->amp_state[j][osc] =
							s->song[s->song_pat[s->song_pos]*SYN_TONES+i].tone->amp_state[j][osc];
						s->song[s->song_pat[s->song_pos]*SYN_TONES+i].tone->amp_state[j][osc]=0;

						s->song[s->song_pat[pos]*SYN_TONES+i].tone->amp_eg_out[j][osc] =
							s->song[s->song_pat[s->song_pos]*SYN_TONES+i].tone->amp_eg_out[j][osc];
						s->song[s->song_pat[s->song_pos]*SYN_TONES+i].tone->amp_eg_out[j][osc]=0;

						s->song[s->song_pat[pos]*SYN_TONES+i].tone->time[j][osc] =
							s->song[s->song_pat[s->song_pos]*SYN_TONES+i].tone->time[j][osc];
						s->song[s->song_pat[s->song_pos]*SYN_TONES+i].tone->time[j][osc]=0;

					}
					s->song[s->song_pat[pos]*SYN_TONES+i].tone->freq[j] =
						s->song[s->song_pat[s->song_pos]*SYN_TONES+i].tone->freq[j];
					s->song[s->song_pat[s->song_pos]*SYN_TONES+i].tone->freq[j]=0;

					s->song[s->song_pat[pos]*SYN_TONES+i].tone->pitch_state[j] =
						s->song[s->song_pat[s->song_pos]*SYN_TONES+i].tone->pitch_state[j];
					s->song[s->song_pat[s->song_pos]*SYN_TONES+i].tone->pitch_state[j]=0;

					s->song[s->song_pat[pos]*SYN_TONES+i].tone->pitch_eg_out[j] =
						s->song[s->song_pat[s->song_pos]*SYN_TONES+i].tone->pitch_eg_out[j];
					s->song[s->song_pat[s->song_pos]*SYN_TONES+i].tone->pitch_eg_out[j]=0;

					s->song[s->song_pat[pos]*SYN_TONES+i].tone->vel[j] =
						s->song[s->song_pat[s->song_pos]*SYN_TONES+i].tone->vel[j];
					s->song[s->song_pat[s->song_pos]*SYN_TONES+i].tone->vel[j]=0;
				}
				s->song[s->song_pat[pos]*SYN_TONES+i].tone->voices =
					s->song[s->song_pat[s->song_pos]*SYN_TONES+i].tone->voices;
				s->song[s->song_pat[s->song_pos]*SYN_TONES+i].tone->voices=0;
			}

			s->seq[i]=s->song+s->song_pat[pos]*SYN_TONES+i;
			s->tone[i]=s->seq[i]->tone;
			s->seq[i]->time=0;
			s->seq[i]->step=-1;
			s->seq[i]->modm_wait_loop=0;
			s->seq[i]->modm_target_step=-1;
		}
		s->song_time=0;
		s->song_pos=pos;

	}
	return ret;
}

int syn_song_pat(syn* syn, int pos, int pat){ assert(syn);
	pos=CLAMP(pos,0,SONG_MAX-1);
	int ret = syn->song_pat[pos];
	if(pat>=0){
		pat=MIN(pat, SONG_MAX-1);
		if(syn->song_pat[pos]!=pat){
			syn->song_pat[pos]=pat;
			if(pos==syn_song_pos(syn,-1)){
				syn_song_pos(syn, pos);
			}
		}
	}
	return ret;
}

int syn_song_dur(syn* syn, int pos, int dur){ assert(syn);
	pos=CLAMP(pos,0,SONG_MAX-1);
	int ret = syn->song_beat_dur[pos];
	if(dur>=0){
		dur=MIN(dur, 64);
		syn->song_beat_dur[pos]=dur;
		// if(pos==syn_song_pos(syn,-1)){
		// 	syn_song_pos(syn, pos);
		// }
	}
	return ret;
}

int syn_song_len(syn* syn, int len){ assert(syn);
	int ret = syn->song_len;
	if(len>=0){
		len=CLAMP(len,0,SONG_MAX-1);
		syn->song_len=len;
	}
	return ret;
}

char syn_song_tie(syn* syn, int pos, char tie){ assert(syn);
	pos=CLAMP(pos, 0, SONG_MAX-1);
	char ret = syn->song_tie[pos];
	if(tie>=0)
		syn->song_tie[pos] = tie;
	return ret;
}

void syn_song_loop(syn* syn, int begin, int end){ if(!syn) return;
	if(begin>=0){
		begin = MIN(begin, SONG_MAX-1);
		syn->song_loop_begin = MIN( end>=0? end: syn->song_loop_end, begin);
	}
	if(end>=0){
		end = MIN(end, SONG_MAX-1);
		syn->song_loop_end = MAX( begin>=0? begin: syn->song_loop_begin, end);
	}
}


void syn_song_advance(syn* s, float secs){
	float bps = s->bpm/60.0;
	float stb = secs * bps*s->song_spb;
	if(s->song_beat_dur[syn_song_pos(s,-1)] != 0 ){
		s->song_time += stb;

		if( s->song_advance && s->song_time+stb >= s->song_beat_dur[ syn_song_pos(s,-1) ] * s->song_spb){

			if(s->song_loop){
				if(syn_song_pos(s,-1)+1 > s->song_loop_end){
					secs=0;
					syn_song_pos( s, s->song_loop_begin);
				} else {
					secs=0;
					syn_song_pos( s, syn_song_pos(s,-1) +1);
				}
			} else {
				if( syn_song_pos(s,-1)+1 > syn_song_len(s,-1) ){
					syn_stop(s);
					return;
				}
				secs=0;
				syn_song_pos( s, syn_song_pos(s,-1) +1);
			}

		}
		s->song_time = fmodf(s->song_time, s->song_beat_dur[ syn_song_pos(s,-1) ] * s->song_spb);
	}
	syn_seq_advance(s, secs);
}

void syn_seq_init(syn_seq* s){
	memset(s, 0, sizeof(syn_seq));
	for (int i = 0; i < POLYPHONY; ++i){
		s->active[i] = (noteid){-1,-1};
	}
	for (int i = 0; i < POLYPHONY; ++i)
		for (int j = 0; j < SEQ_LEN; ++j)
			s->note[i][j] = SEQ_MIN_NOTE;

	s->spb=SEQ_STEP_PER_BEAT_DEF;
	s->len=MIN(SEQ_LEN, 16);
	s->time=0;
	s->step=-1;

	s->tone = NULL;
	s->modm_target_step=-1;
	s->modm = malloc(sizeof(syn_mod_mat*)*SEQ_LEN);
	for(int i=0;i<SEQ_LEN;i++)
		s->modm[i]=NULL;
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
	if(bpm >= 0){
		bpm=MAX(SYN_MIN_BPM, bpm);
		s->bpm = bpm;
	}

	// syn_lock(s, 0);
	return ret;
}

void syn_seq_advance(syn* s, float secs){
	float bps = s->bpm/60.0;

	for(int i=0; i<SYN_TONES; i++){
		int prev_step = MIN(floor(s->seq[i]->time), s->seq[i]->step);

		float stb = secs * bps*s->seq[i]->spb;
		s->seq[i]->time += stb;

		s->seq[i]->time = fmodf(s->seq[i]->time, s->seq[i]->len);

		int step = floor(s->seq[i]->time);
		s->seq[i]->step = step;

/* tone interpolation */
		switch(s->modm_lerp_mode){
			case 0:
				if( step != prev_step ){
					if(s->seq[i]->modm[step] != NULL){
						syn_modm_lerp(s, i, s->seq[i]->modm[step], 0);
					}
				}
				break;
			case 1:
				if( step != prev_step ){
					if(s->seq[i]->modm[step] != NULL){
						syn_modm_lerp(s, i, s->seq[i]->modm[step], 100.0);
					}
				} break;
			case 2: // fixme
				if((prev_step==-1) || (step >= s->seq[i]->modm_target_step && !s->seq[i]->modm_wait_loop)){

					float time_per_step = 1000.0*(1.0/(bps*s->seq[i]->spb)); // fixme

					float tltime = prev_step!=-1 ? time_per_step : 0;

					for(int ti=0; ti<s->seq[i]->len; ti++){
						int tim = (ti+(step)+1) % s->seq[i]->len;
						if(s->seq[i]->modm[tim] != NULL){
							syn_modm_lerp(s, i, s->seq[i]->modm[tim], tltime);
							s->seq[i]->modm_target_step = tim;
							if(tim < step) s->seq[i]->modm_wait_loop = 1;
							break;
						}
						tltime += time_per_step;
					}
				}
				break;
			default: break;
		}

		float nort = s->seq[i]->time-floor(s->seq[i]->time);
		for (int j = 0; j < POLYPHONY; j++){
			char tie = prev_step!=-1? s->seq[i]->dur[j][prev_step] > 250 : 0;
			if(s->seq[i]->mute) tie=0;

			// check for note expiration
			if((nort > s->seq[i]->dur[j][s->seq[i]->step]/250.f) && (!tie) && (prev_step == s->seq[i]->step) && (s->seq[i]->active[j].voice > -1)){
				syn_nof(s, s->seq[i]->active[j]);
				s->seq[i]->active[j] = (noteid){-1,-1};
			}

			if(prev_step == s->seq[i]->step) continue;

			if((prev_step != -1) && (s->seq[i]->active[j].voice > -1) && !tie){
				syn_nof(s, s->seq[i]->active[j]);
				s->seq[i]->active[j] = (noteid){-1,-1};
			}

			if(s->seq[i]->note[j][s->seq[i]->step]>SEQ_MIN_NOTE){

				if(!tie && !s->seq[i]->mute){
					s->seq[i]->active[j] = syn_non(s, i, s->seq[i]->note[j][s->seq[i]->step], 0);
				}
				// 	s->seq[i]->active[j] = syn_non(s, i, 0, 0);
				if(tie)syn_nfreq(s, s->seq[i]->active[j], freqn(s->seq[i]->note[j][s->seq[i]->step]));
				syn_nvel(s, s->seq[i]->active[j], s->seq[i]->vel[j][s->seq[i]->step]);
			}

		}

		if(prev_step == s->seq[i]->len-1 && step == 0){
			s->seq[i]->modm_target_step = 0;
			s->seq[i]->modm_wait_loop = 0;
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

//cfilter test
float* dstl;
float* dstr;
float* templ;
float* tempr;
float* convsrc;
char conv_once=1;
int cs=0;
// len: number of stereo samples to process
void syn_run(syn* s, float* buffer, int len){ assert(s && buffer);
	assert(len < s->buflen);

	syn_lock(s,1);

	if(s->vupeakl>=1)s->vupeakl*=.99; else s->vupeakl*=.9;
	if(s->vupeakr>=1)s->vupeakr*=.99; else s->vupeakr*=.9;

	float a[OSC_PER_TONE];

	if(s->seq_play) syn_song_advance(s, ((float)len)/(s->sr) );

	for(int i=0; i<SYN_TONES; i++){
		memset( s->bufl[i] + s->bufpos, 0, sizeof(float)*len);
		memset( s->bufr[i] + s->bufpos, 0, sizeof(float)*len);
	/* update modulation matrix */
		if(s->modm_target_active[i]){
			syn_modm_do_lerp(&s->tone[i]->mod_mat, s->modm_target+i, s->modm_lerpt[i]/s->modm_lerpms[i]);
			// syn_modm_do_lerp(&s->tone[i]->mod_mat, s->modm_target+i, MAPVAL(s->modm_lerpt[i], 0, s->modm_lerpms[i],0,1));
			s->modm_lerpt[i] += ((float)len)/(s->sr)*1000.0;
			if(s->modm_lerpt[i] >= s->modm_lerpms[i]){
				s->modm_target_active[i] = 0;
			}
		}


		s->tone[i]->vupeakl*=.9;
		s->tone[i]->vupeakr*=.9;


		if(s->tone[i]->voices==0) {continue;}

		for (int j=0; j<POLYPHONY; j++){

			float vel = s->tone[i]->vel[j]/255.f;

			int any_on =0;
			for(int osc =0; osc<OSC_PER_TONE; osc++)
				any_on += s->tone[i]->amp_state[j][osc] != 0;

			if(any_on==0){
				if(s->tone[i]->freq[j] > 0) s->tone[i]->voices--;
				s->tone[i]->freq[j]=0;
				s->tone[i]->pitch_state[j]=0;
				s->tone[i]->pitch_eg_out[j]=0;
				continue;
			}

			for(int smp=0; smp<len; smp++){

				any_on =0;
				for(int osc =0; osc<OSC_PER_TONE; osc++)
					any_on += s->tone[i]->amp_state[j][osc];

				if(any_on==0) break;

				float oscaccum=0;
				memset(a, 0, sizeof(a));

				s->tone[i]->pitch_env.gate  = s->tone[i]->gate[j];
				s->tone[i]->pitch_env.state = s->tone[i]->pitch_state[j];
				s->tone[i]->pitch_env.out   = s->tone[i]->pitch_eg_out[j];

				float pitch_env = adsr_run(&s->tone[i]->pitch_env);
				if(s->tone[i]->pitch_env_amt < 0)
					pitch_env *= MAPVAL(s->tone[i]->pitch_env_amt, SYN_MIN_PITCH_ENV_AMT, 0, -1, 0);
				else
					pitch_env *= s->tone[i]->pitch_env_amt;

				s->tone[i]->pitch_state[j]  = s->tone[i]->pitch_env.state;
				s->tone[i]->pitch_eg_out[j] = s->tone[i]->pitch_env.out;


				for(int osc=0; osc<OSC_PER_TONE; osc++){
					if(s->tone[i]->amp_state[j][osc] == 0) continue;

					s->tone[i]->osc_env[osc].gate =     s->tone[i]->gate[j];
					s->tone[i]->osc_env[osc].state =    s->tone[i]->amp_state[j][osc];
					s->tone[i]->osc_env[osc].out =      s->tone[i]->amp_eg_out[j][osc];

					float osc_env = adsr_run(&s->tone[i]->osc_env[osc]);

					s->tone[i]->amp_state[j][osc]   = s->tone[i]->osc_env[osc].state;
					s->tone[i]->amp_eg_out[j][osc]  = s->tone[i]->osc_env[osc].out;

					if(osc_env<=0) continue;

					float t = s->tone[i]->time[j][osc] ;
					switch(s->tone[i]->osc[osc]){
						case OSC_SAW:    a[osc]=saw (t); break;
						case OSC_TRI:    a[osc]=tri (t); break;
						case OSC_SINE:   a[osc]=sine(t); break;
						case OSC_PULSE:  a[osc]=pul (t, s->tone[i]->pwm[osc]); break;
						case OSC_SQUARE: a[osc]=sqr (t); break;
						case OSC_NOISE:  a[osc]=wnoiz (t); break;
						// case OSC_NOISE:  a=noise(); break;
						default: break;
					}

					float fm=0;
					float oct_mul = syn_oct_mul(s->tone[i]->oct[osc]);
					for(int mod=0; mod<osc; mod++){
						float fmamp = s->tone[i]->freq[j]*oct_mul * tone_index(s->tone[i], osc, mod, -1) * tone_frat(s->tone[i], mod, -1);
						fm += a[mod] * fmamp;
					}

					// accum time
					float frat= s->tone[i]->freq[j]*oct_mul * tone_frat(s->tone[i] , osc, -1);
					s->tone[i]->time[j][osc] += /*fabsf*/(fm + s->master_detune + frat + MAX(frat*pitch_env, -frat+SYN_MIN_PITCH_ENV_FREQ )) / s->sr;
					s->tone[i]->time[j][osc] = fmodf(s->tone[i]->time[j][osc], 1);
					if(s->tone[i]->time[j][osc]<0) s->tone[i]->time[j][osc] +=1;

					a[osc] *= osc_env;

					oscaccum += a[osc] * vel * tone_omix(s->tone[i], osc, -1);
				}
				s->bufl[i][(s->bufpos + smp)%s->buflen]+=oscaccum*sqrt(1.0-s->tone[i]->pan);
				s->bufr[i][(s->bufpos + smp)%s->buflen]+=oscaccum*sqrt(    s->tone[i]->pan);
			}
		}
	}

	syn_mix(s, buffer, len);

	s->bufpos = (s->bufpos + len) % s->buflen;
	if(s->bufpos + len > s->buflen) s->bufpos=0;

	s->sample+=len;

	syn_lock(s,0);
}


void syn_mix(syn* s, float* buffer, int len){
	int cs=s->filter_order;
	float delay_bufl[len+cs];// memset(delay_bufr,0,sizeof(float)*(len+cs));
	float delay_bufr[len+cs];// memset(delay_bufr,0,sizeof(float)*(len+cs));
	float templ[len+cs];
	float tempr[len+cs];
	float dstl[len];
	float dstr[len];
	float convsrc[cs];
	int di;
	float bps=s->bpm/60.f;
	for(int i=0; i<SYN_TONES; i++){
		for(int p=-cs; p<len; p++){
			di= s->bufpos - 2.0/tone_delay_time(s->tone[i], -1)/bps *s->sr;
			// di= s->bufpos - tone_delay_time(s->tone[i], -1)*s->sr;
			di+=p;
			if(di<0) di+=s->buflen;
			di%=s->buflen;

			if(s->tone[i]->delay_stereo>=0){
				delay_bufl[p+cs] = (s->bufl[i][di] * tone_delay_level(s->tone[i], -1) + s->delay_bufl[i][di] * tone_delay_fb(s->tone[i], -1));
				delay_bufr[p+cs] = (s->bufr[i][di] * tone_delay_level(s->tone[i], -1) + s->delay_bufr[i][di] * tone_delay_fb(s->tone[i], -1));
			} else {
				delay_bufr[p+cs] = (s->bufl[i][di] * tone_delay_level(s->tone[i], -1) + s->delay_bufl[i][di] * tone_delay_fb(s->tone[i], -1));
				delay_bufl[p+cs] = (s->bufr[i][di] * tone_delay_level(s->tone[i], -1) + s->delay_bufr[i][di] * tone_delay_fb(s->tone[i], -1));
			}

			if(p>=0){
				s->delay_bufl[i][s->bufpos+p] = delay_bufl[p];
				s->delay_bufr[i][s->bufpos+p] = delay_bufr[p];
			}
		}

		for(int p=-cs; p<len; p++){
			int bi = (s->bufpos+p) % s->buflen;
			if(bi<0) bi += s->buflen;
			templ[p+cs] = s->bufl[i][bi] + delay_bufl[p+cs];
			tempr[p+cs] = s->bufr[i][bi] + delay_bufr[p+cs];
		}

		float freq = tone_filter_freq(s->tone[i], -1);
		float band = tone_filter_band(s->tone[i], -1);

		switch( s->tone[i]->filter_type){
			default:
			case 0 : fir_lp(convsrc, freq, s->sr, cs, 0); break;
			case 1 : fir_hp(convsrc, freq, s->sr, cs, 0); break;
			case 2 : fir_bp(convsrc, MAX(freq-band, 2), MIN(freq+band, 22000), s->sr, cs, 0); break;
			case 3 : fir_bs(convsrc, MAX(freq-band, 2), MIN(freq+band, 22000), s->sr, cs, 0); break;
		}

		conv(dstl, convsrc, cs, templ+cs, templ, len);
		conv(dstr, convsrc, cs, tempr+cs, tempr, len);

		for(int p=0; p<len; p++){
			s->tone[i]->vupeakl = MAX( s->tone[i]->vupeakl, fabsf(dstl[p] * s->tone[i]->gain));
			s->tone[i]->vupeakr = MAX( s->tone[i]->vupeakr, fabsf(dstr[p] * s->tone[i]->gain));
			buffer[p*2  ] += dstl[p] * s->tone[i]->gain;
			buffer[p*2+1] += dstr[p] * s->tone[i]->gain;
		}
	}

	for(int smp=0; smp<len*2;){
		buffer[smp] *= s->gain;
		s->vupeakl = MAX(fabsf(buffer[smp]), s->vupeakl); smp++;
		buffer[smp] *= s->gain;
		s->vupeakr = MAX(fabsf(buffer[smp]), s->vupeakr); smp++;
	}

}


noteid syn_non(syn* s, int instr, float note, float vel){ // note: semitone distance from A4
	if(instr<0 || instr>SYN_TONES-1) return (noteid){-1,-1};

	float freq = freqn(note);

	int tone = CLAMP(instr, 0, SYN_TONES-1);

	int voice = -1;
	float target_env_out = INFINITY;

	for(int i =0; i<POLYPHONY; i++){
		if( s->tone[tone]->gate[i] ) continue;

		int any_on=0;
		for(int osc=0; osc<OSC_PER_TONE; osc++)
			any_on+=s->tone[tone]->amp_state[i][osc]!=0;

		if(!any_on){ // new voice
			voice = i;
			s->tone[tone]->voices++;
			break;
		}

		// replace the dimmest voice being released
		float out_accum=0;
		for(int osc=0; osc<OSC_PER_TONE; osc++)
			out_accum+=s->tone[tone]->amp_eg_out[i][osc];

		if( out_accum < target_env_out ) {
			target_env_out = out_accum;
			voice = i;
		}
	}
	// char mono = s->tone[tone]->poly_type == 0;
	// if(!mono){
	if(voice==-1) SYNLOG("tone %i ran out of voices %f(%fhz)", tone, note, freq);
	if(voice==-1) return (noteid){-1,-1};
	// }

	// if(!mono || s->tone[tone]->voices==0){
	for (int k = 0; k < OSC_PER_TONE; ++k){
		s->tone[tone]->amp_state[voice][k] = 1; // attack
		s->tone[tone]->amp_eg_out[voice][k] = 0;
		s->tone[tone]->time[voice][k] = 0 + s->tone[tone]->phase[k];
	}
	s->tone[tone]->gate[voice] = 1;
	s->tone[tone]->pitch_state[voice] = 1;

	s->tone[tone]->freq[voice] = freq;
	s->tone[tone]->vel[voice] = vel*255;
		// s->tone[tone]->glide_freq_target = freq;
		// s->tone[tone]->glide_t = s->tone[tone]->glide;

		// s->note_history[tone][s->note_history_i[tone]] = freq;
		// s->note_history_i[tone]=1;

	// } else {
		// if(s->note_history_i[tone] >= HISTORY_LEN) return (noteid){-1,-1};
		// s->note_history[tone][s->note_history_i[tone]] = freq;
		// voice = s->note_history_i[tone];
		// s->note_history_i[tone]++;
	// 	s->tone[tone]->glide_t = 0;
	// 	s->tone[tone]->glide_freq_target = freq;
	// }

	return (noteid){tone, voice};
}






void syn_nof(syn* s, noteid nid){
	if(nid.voice<0 || nid.tone<0) return;
	short tone = CLAMP(nid.tone, 0, SYN_TONES-1);
	short voice = CLAMP(nid.voice, 0, POLYPHONY-1);

	for( int osc = 0 ; osc<OSC_PER_TONE; osc++)
		if(s->tone[tone]->amp_state[voice][osc] != 0)
			s->tone[tone]->amp_state[voice][osc] = 4; //release

	s->tone[tone]->gate[voice] = 0;
	if(s->tone[tone]->pitch_state[voice] != 0)
		s->tone[tone]->pitch_state[voice] = 4; //release
}


void syn_anof(syn* s, int instr){
	if(instr<0 || instr > SYN_TONES-1) return;

	int tone = CLAMP(instr, 0, SYN_TONES-1);
	for(int i =0; i<POLYPHONY; i++){

		for( int osc = 0 ; osc<OSC_PER_TONE; osc++){
			if(s->tone[tone]->amp_state[i][osc] != 0)
				s->tone[tone]->amp_state[i][osc] = 4; //release
		}
		s->tone[tone]->gate[i] = 0;
		if(s->tone[tone]->pitch_state[i] != 0)
			s->tone[tone]->pitch_state[i] = 4; //release
	}
}


void syn_nvel(syn* s, noteid nid, float vel){
	if(nid.voice<0 || nid.tone<0) return;
	int tone = CLAMP(nid.tone, 0, SYN_TONES-1);
	int voice = CLAMP(nid.voice, 0, POLYPHONY-1);
	s->tone[tone]->vel[voice] = vel;
}


void syn_nset(syn* s, noteid nid, float note){
	if(nid.voice<0 || nid.tone<0) return;

	float freq = freqn(note);
	int tone = CLAMP(nid.tone, 0, SYN_TONES-1);
	int voice = CLAMP(nid.voice, 0, POLYPHONY-1);

	s->tone[tone]->freq[voice] = freq;
}


void syn_nfreq(syn* s, noteid nid, float freq){
	if(nid.voice<0 || nid.tone<0) return;
	int tone = CLAMP(nid.tone, 0, SYN_TONES-1);
	int voice = CLAMP(nid.voice, 0, POLYPHONY-1);
	s->tone[tone]->freq[voice] = freq;
}



void syn_pause(syn* s){ assert(s);
	s->seq_play = !s->seq_play;
	for(int i=0; i<SYN_TONES; i++){
		syn_anof(s, i);
		// s->seq[i]->time = 0;
		// s->seq[i]->step = -1;
	}
}

void syn_stop(syn* s){ assert(s);
	s->seq_play = 0;
	for(int i=0; i<SYN_TONES; i++){
		syn_anof(s, i);
		s->seq[i]->time = 0;
		s->seq[i]->step = -1;
	}
	s->song_time=0;
}


void syn_lock(syn* s, char l){ assert(s);
	int status =0 ;
	if(l){
		// status = SDL_TryLockMutex(s->mutex);
		status = SDL_LockMutex(s->mutex);
		switch(status) {
			case SDL_MUTEX_TIMEDOUT: SYNLOG("Couldn't lock mutex (TIMEOUT)");
			case 0: break;
			default : SYNLOG("Couldn't lock mutex");
		}
		// if(SDL_LockMutex(s->mutex)!=0) {fprintf(stderr, "Couldn't lock mutex\n");}
	} else
		SDL_UnlockMutex(s->mutex);
}


char seq_non(syn_seq* s, int pos, float note, float vel, float dur){ assert(s);
	pos = CLAMP(pos, 0, SEQ_LEN);
	int voice = -1;
	for(int i =0; i<POLYPHONY; i++){
		// if( s->freq[i][pos] > 0 ) continue;
		if( s->note[i][pos] > SEQ_MIN_NOTE ) continue;
		voice = i;
		break;
	}
	if(voice==-1) return 1;

	// float freq = freqn(note);
	// s->freq[voice][pos]=freq;
	s->note[voice][pos]=note;
	s->vel[voice][pos]=vel*255;
	s->dur[voice][pos]=dur*255;
	return 0;
}

void seq_nof(syn_seq* s, int pos, int voice){ assert(s);
	pos = CLAMP(pos, 0, SEQ_LEN);
	if(voice<0) return;
	voice = CLAMP(voice, 0, POLYPHONY);
	// s->freq[voice][pos] = 0;
	s->note[voice][pos] = SEQ_MIN_NOTE;
	s->dur[voice][pos] = 0;
	s->vel[voice][pos] = 0;
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
	// pos = CLAMP(pos, 0, s->len);
	pos = CLAMP(pos, 0, SEQ_LEN-1);
	voice = CLAMP(voice, 0, POLYPHONY);
	// return s->freq[voice][pos] > 0;
	return s->note[voice][pos] > SEQ_MIN_NOTE;
}

char seq_isempty(syn_seq* s, int pos){ assert(s);
	// pos = CLAMP(pos, 0, s->len);
	pos = CLAMP(pos, 0, SEQ_LEN-1);
	char ret = 0;
	for (int i = 0; i < POLYPHONY; i++){
		ret |= seq_ison(s, pos, i);
	}
	return !ret;
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



float syn_oct_mul(float oct_val){
	return powf(2, oct_val);
}


// amp eg setter/getter
float tone_atk(syn_tone* t, int osc, float a){ assert(t); assert(osc<OSC_PER_TONE); assert(osc>=0);
	float ret = t->osc_env[osc].a_rate / t->osc_env[osc].sr;
	if(a>=0) adsr_a(t->osc_env+osc, a);
	return ret;
}
float tone_dec(syn_tone* t, int osc, float a){ assert(t); assert(osc<OSC_PER_TONE); assert(osc>=0);
	float ret = t->osc_env[osc].d_rate / t->osc_env[osc].sr;
	if(a>=0) adsr_d(t->osc_env+osc, a);
	return ret;
}
float tone_sus(syn_tone* t, int osc, float a){ assert(t); assert(osc<OSC_PER_TONE); assert(osc>=0);
	float ret = t->osc_env[osc].s;
	if(a>=0) adsr_s(t->osc_env+osc, a);
	return ret;
}
float tone_rel(syn_tone* t, int osc, float a){ assert(t); assert(osc<OSC_PER_TONE); assert(osc>=0);
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


float tone_filter_freq(syn_tone* t, float freq){
	float ret = t->mod_mat[1][3];
	if(freq>1) t->mod_mat[1][3] = freq;
	return ret;
}
float tone_filter_band(syn_tone* t, float bw){
	float ret = t->mod_mat[2][3];
	if(bw>1) {
		bw=CLAMP(bw, 100, 10000);
		t->mod_mat[2][3] = bw;
	}
	return ret;
}
float tone_delay_time(syn_tone* t, float ti){
	float ret = t->mod_mat[1][4];
	if(ti>=1) t->mod_mat[1][4] = ti;
	return ret;
}
float tone_delay_fb(syn_tone* t, float fb){
	float ret = t->mod_mat[2][4];
	if(fb>=0) t->mod_mat[2][4] = fb;
	return ret;
}
float tone_delay_level(syn_tone* t, float lv){
	float ret = t->mod_mat[3][4];
	if(lv>=0) t->mod_mat[3][4] = lv;
	return ret;
}

float* tone_filter_freq_addr(syn_mod_mat* m){ return &((*m)[1][3]); }
float* tone_filter_band_addr(syn_mod_mat* m){ return &((*m)[2][3]); }
float* tone_delay_time_addr (syn_mod_mat* m){ return &((*m)[1][4]); }
float* tone_delay_fb_addr   (syn_mod_mat* m){ return &((*m)[2][4]); }
float* tone_delay_level_addr(syn_mod_mat* m){ return &((*m)[3][4]); }






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

int seq_mute(syn_seq* s, int mute){ assert(s);
	int ret = s->mute;
	if(mute>=0) s->mute=mute;
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
	free(s->modm);
	s->modm=NULL;
	// if(s->tone != NULL) free(s->tone);
	// s->tone=NULL;
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

void syn_tone_init(syn_tone* t, int sr){
	memset(t, 0, sizeof(syn_tone));
	memset(t->mod_mat, 0, sizeof(syn_mod_mat));
	for (int j = 0; j < POLYPHONY; j++){
		t->pitch_env_amt = 0;

		t->vel[j] = 0;
		t->freq[j] = 0;

		t->gate[j]=0;
		t->pitch_state[j]=0;
		t->pitch_eg_out[j]=0;
		for(int k=0; k<OSC_PER_TONE; k++){
			t->amp_state[j][k]=0;
			t->amp_eg_out[j][k]=0;
			t->time[j][k]=0;
		}
	}
	tone_filter_freq(t, 22000);
	tone_filter_band(t, 100);
	tone_delay_time(t, 1);
	tone_delay_fb(t, 0);
	tone_delay_level(t, 0);

	t->gain=0.75;
	t->pan=0.5;
	t->delay_stereo=0.0;

	t->vupeakl=0;
	t->vupeakr=0;
	// t->vel_out[0] = &t->pitch_env_amt;
	// t->vel_out[1] = &t->pitch_env_amt;

	t->voices=0;

	t->pitch_env = adsr_make(sr, .001, .001, 0, 0.001 , .0,0);
	for(int k=0; k<OSC_PER_TONE; k++){
		t->osc_env[k] = adsr_make(sr, .001, .001, 1, 0.001 , 0,0);
		t->osc[k] = 0;
		tone_omix(t, k, k==OSC_PER_TONE-1? .75 : 0);
		t->pwm[k] = .5;
		t->phase[k] = 0;
		t->oct[k] = 0;
		t->mod_mat[k][k] =  1;
	}
}














// file io
// do NOT change enum order to assure backwards compatibility

// #define SYN_PATH_LEN 2048
// char _path[SYN_PATH_LEN];
#define SYNT_VERSION 0
#define SYNT_DESC "SYNT"
enum synt_flag{
	SYNT_GAIN, SYNT_WAVE, SYNT_MODM, SYNT_OMIX,
	SYNT_ATK, SYNT_DEC, SYNT_SUS, SYNT_REL,
	SYNT_PATK, SYNT_PDEC, SYNT_PSUS, SYNT_PREL, SYNT_PAMT,
	SYNT_OCT,  SYNT_FTYPE, SYNT_FFREQ, SYNT_FBAND, SYNT_DTIME, SYNT_DFB, SYNT_DLV,
	SYNT_PAN, SYNT_DPAN,
	SYNT_END=255
};


int syn_tone_open(syn* syn, char* path, int instr ){ assert(syn && path && instr<SYN_TONES);
	FILE* f=fopen(path, "rb");
	if(f==NULL){
		SYNLOG("couldnt open file %s", path);
		return 1;
	}
	rewind(f);
	fseek(f, 0, SEEK_END);
	int len = ftell(f);

	void* data=malloc(len);
	rewind(f);
	fread(data, len, 1, f);

	int err=0;
	int read=0;
	err=syn_tone_load(syn, instr, data, len, &read);

	if(err || read!=len){
		SYNLOG("error reading file %s, e:%i, read %i from %i", path, err, read, len);
		free(data);
		return 1;
	}
	free(data);
	return 0;
}


int syn_tone_save(syn* syn, syn_tone* t, char* path){ assert(syn && t && path);
	FILE* f=fopen(path, "wb");
	if(f==NULL){
		SYNLOG("couldnt open file %s", path);
		return 1;
	}
	int err=syn_tone_write(syn, t, f);

	fclose(f);
	return err;
}


int syn_tone_write(syn* syn, syn_tone* t, FILE* f){
	syn_tone tone;
	syn_tone_init(&tone, syn->sr);

	fwrite(SYNT_DESC, 4, 1, f);

	uint8_t token[4]={0,0,0,0};
	float value;

	if(tone.gain != t->gain){
		memset(token, 0, 4);
		token[0] = SYNT_GAIN;
		value = t->gain;
		fwrite(token, 1, 4, f);
		fwrite(&value, 4, 1, f);
	}

	for(int i=0; i<OSC_PER_TONE; i++){
		if(tone.osc[i] != t->osc[i]){
			memset(token, 0, 4);
			token[0] = SYNT_WAVE;
			token[1] = i;
			value = t->osc[i];
			fwrite(token, 1, 4, f);
			fwrite(&value, 4, 1, f);
		}
		if(tone_omix(&tone, i, -1) != tone_omix(t, i, -1)){
			memset(token, 0, 4);
			token[0] = SYNT_OMIX;
			token[1] = i;
			value = tone_omix(t, i, -1);
			fwrite(token, 1, 4, f);
			fwrite(&value, 4, 1, f);
		}
		if(tone_frat(&tone, i, -1) != tone_frat(t, i, -1)){
			memset(token, 0, 4);
			token[0] = SYNT_MODM;
			token[1] = i;
			token[2] = i;
			value = tone_frat(t, i, -1);
			fwrite(token, 1, 4, f);
			fwrite(&value, 4, 1, f);
		}

		for(int j=0; j<i; j++){
			if(tone_index(&tone, i, j,-1) != tone_index(t, i, j,-1) ){
				memset(token, 0, 4);
				token[0] = SYNT_MODM;
				token[1] = i;
				token[2] = j;
				value = tone_index(t, i, j,-1);
				fwrite(token, 1, 4, f);
				fwrite(&value, 4, 1, f);
			}
		}

		if(tone_atk(&tone, i, -1) != tone_atk(t, i, -1)){
			memset(token, 0, 4);
			token[0] = SYNT_ATK;
			token[1] = i;
			value = tone_atk(t, i, -1);
			fwrite(token, 1, 4, f);
			fwrite(&value, 4, 1, f);
		}

		if(tone_dec(&tone, i, -1) != tone_dec(t, i, -1)){
			memset(token, 0, 4);
			token[0] = SYNT_DEC;
			token[1] = i;
			value = tone_dec(t, i, -1);
			fwrite(token, 1, 4, f);
			fwrite(&value, 4, 1, f);
		}

		if(tone_sus(&tone, i, -1) != tone_sus(t, i, -1)){
			memset(token, 0, 4);
			token[0] = SYNT_SUS;
			token[1] = i;
			value = tone_sus(t, i, -1);
			fwrite(token, 1, 4, f);
			fwrite(&value, 4, 1, f);
		}

		if(tone_rel(&tone, i, -1) != tone_rel(t, i, -1)){
			memset(token, 0, 4);
			token[0] = SYNT_REL;
			token[1] = i;
			value = tone_rel(t, i, -1);
			fwrite(token, 1, 4, f);
			fwrite(&value, 4, 1, f);
		}

		if(tone.oct[i] != t->oct[i]){
			memset(token, 0, 4);
			token[0] = SYNT_OCT;
			token[1] = i;
			value = t->oct[i];
			fwrite(token, 1, 4, f);
			fwrite(&value, 4, 1, f);
		}
	}

	if(tone_patk(&tone, -1) != tone_patk(t, -1)){
		memset(token, 0, 4);
		token[0] = SYNT_PATK;
		value = tone_patk(t, -1);
		fwrite(token, 1, 4, f);
		fwrite(&value, 4, 1, f);
	}

	if(tone_pdec(&tone, -1) != tone_pdec(t, -1)){
		memset(token, 0, 4);
		token[0] = SYNT_PDEC;
		value = tone_pdec(t, -1);
		fwrite(token, 1, 4, f);
		fwrite(&value, 4, 1, f);
	}

	if(tone_psus(&tone, -1) != tone_psus(t, -1)){
		memset(token, 0, 4);
		token[0] = SYNT_PSUS;
		value = tone_psus(t, -1);
		fwrite(token, 1, 4, f);
		fwrite(&value, 4, 1, f);
	}

	if(tone_prel(&tone, -1) != tone_prel(t, -1)){
		memset(token, 0, 4);
		token[0] = SYNT_PREL;
		value = tone_prel(t, -1);
		fwrite(token, 1, 4, f);
		fwrite(&value, 4, 1, f);
	}

	if(tone.pitch_env_amt != t->pitch_env_amt){
		memset(token, 0, 4);
		token[0] = SYNT_PAMT;
		value = t->pitch_env_amt;
		fwrite(token, 1, 4, f);
		fwrite(&value, 4, 1, f);
	}

	if(tone.filter_type != t->filter_type){
		memset(token, 0, 4);
		token[0] = SYNT_FTYPE;
		token[1] = t->filter_type;
		fwrite(token, 1, 4, f);
		fwrite(&value, 4, 1, f);
	}

	if(tone_filter_freq(&tone,-1) != tone_filter_freq(t, -1)){
		memset(token, 0, 4);
		token[0] = SYNT_FFREQ;
		value = tone_filter_freq(t, -1);
		fwrite(token, 1, 4, f);
		fwrite(&value, 4, 1, f);
	}

	if(tone_filter_band(&tone,-1) != tone_filter_band(t, -1)){
		memset(token, 0, 4);
		token[0] = SYNT_FBAND;
		value = tone_filter_band(t, -1);
		fwrite(token, 1, 4, f);
		fwrite(&value, 4, 1, f);
	}

	if(tone_delay_time(&tone,-1) != tone_delay_time(t, -1)){
		memset(token, 0, 4);
		token[0] = SYNT_DTIME;
		value = tone_delay_time(t, -1);
		fwrite(token, 1, 4, f);
		fwrite(&value, 4, 1, f);
	}
	if(tone_delay_fb(&tone,-1) != tone_delay_fb(t, -1)){
		memset(token, 0, 4);
		token[0] = SYNT_DFB;
		value = tone_delay_fb(t, -1);
		fwrite(token, 1, 4, f);
		fwrite(&value, 4, 1, f);
	}
	if(tone_delay_level(&tone,-1) != tone_delay_level(t, -1)){
		memset(token, 0, 4);
		token[0] = SYNT_DLV;
		value = tone_delay_level(t, -1);
		fwrite(token, 1, 4, f);
		fwrite(&value, 4, 1, f);
	}
	if(tone.pan != t->pan){
		memset(token, 0, 4);
		token[0] = SYNT_PAN;
		value = t->pan;
		fwrite(token, 1, 4, f);
		fwrite(&value, 4, 1, f);
	}
	if(tone.delay_stereo != t->delay_stereo){
		memset(token, 0, 4);
		token[0] = SYNT_DPAN;
		value = t->delay_stereo;
		fwrite(token, 1, 4, f);
		fwrite(&value, 4, 1, f);
	}

	memset(token, 0, 4);
	token[0] = SYNT_END;
	value = 0;
	fwrite(token, 1, 4, f);
	fwrite(&value, 4, 1, f);

	return 0;
}


int syn_tone_load(syn* syn, int instr, void* data, int len, int* read){
	if(instr >= SYN_TONES) return 1;
	syn_tone* t = syn->tone[instr];
	return syn_tone_read(syn, t, data, len, read);
}


int syn_tone_read(syn* syn, syn_tone* t, void* data, int len, int* read){
	if(len<=4) return 1;
	char desc[5]; desc[4]=0;
	memcpy( desc, data, 4 );
	if(memcmp(desc, SYNT_DESC, 4)){
		SYNLOG("file doesnt begin with \"%s\"", SYNT_DESC);
		return 1;
	}

	int i = 4;
	int err = 0;
	char done=0;
	uint8_t flag = 0;
	uint8_t f0 = 0;
	uint8_t f1 = 0;
	uint8_t f2 = 0;
	float val=0;

	syn_tone tone;
	syn_tone_init(&tone, syn->sr);

	int osc=0;
	int mod=0, car=0; // for modulation matrix
	int envosc=0; // which osc is being set on adsr flag

	while(!err && !done ){
		if(i+8>len){
			err=1; goto end;
		}
		memcpy(&flag, data+i, 1);i++;
		memcpy(&f0, data+i, 1); i++;
		memcpy(&f1, data+i, 1); i++;
		memcpy(&f2, data+i, 1); i++;
		memcpy(&val, data+i, 4);
		i+=4;
		switch(flag){
			case SYNT_GAIN: tone.gain=val; break;
			case SYNT_WAVE:
				if(f0>=OSC_PER_TONE) break;
				tone.osc[f0]=(int)val; break;
			case SYNT_MODM:
				car=f0;
				mod=f1;
				if(mod>=OSC_PER_TONE || car>=OSC_PER_TONE) break;
				if(mod>car){
					err=2;
					goto end;
				}
				if(car==mod) tone_frat(&tone, car, val);
				else tone_index(&tone, car, mod, val);
				break;
			case SYNT_OMIX: osc=f0; if(osc>=OSC_PER_TONE) break;
				tone_omix(&tone, osc, val); break;
			case SYNT_ATK: envosc=f0; if(envosc>=OSC_PER_TONE) break;
				tone_atk(&tone, envosc, val); break;
			case SYNT_DEC: envosc=f0; if(envosc>=OSC_PER_TONE) break;
				tone_dec(&tone, envosc, val); break;
			case SYNT_SUS: envosc=f0; if(envosc>=OSC_PER_TONE) break;
				tone_sus(&tone, envosc, val); break;
			case SYNT_REL: envosc=f0; if(envosc>=OSC_PER_TONE) break;
				tone_rel(&tone, envosc, val); break;
			case SYNT_PATK: tone_patk(&tone, val); break;
			case SYNT_PDEC: tone_pdec(&tone, val); break;
			case SYNT_PSUS: tone_psus(&tone, val); break;
			case SYNT_PREL: tone_prel(&tone, val); break;
			case SYNT_PAMT: tone.pitch_env_amt = val; break;
			case SYNT_OCT: tone.oct[f0] = val; break;

			case SYNT_FTYPE: tone.filter_type = f0; break;
			case SYNT_FFREQ: tone_filter_freq(&tone, val); break;
			case SYNT_FBAND: tone_filter_band(&tone, val); break;
			case SYNT_DTIME: tone_delay_time(&tone, val); break;
			case SYNT_DFB:   tone_delay_fb(&tone, val); break;
			case SYNT_DLV:   tone_delay_level(&tone, val); break;
			case SYNT_DPAN:  tone.delay_stereo=val; break;
			case SYNT_PAN:   tone.pan=val; break;
			case SYNT_END: done=1; break;
			default: break;
		}
	}

	if(read) *read=i;
	// memcpy( syn->tone+instr, &tone, sizeof(syn_tone));
	memcpy( t, &tone, sizeof(syn_tone));

	end:;
	return err;
}















#define SYNP_DESC "SYNP"
enum synp_flag{
	SYNP_TONE, SYNP_MODM,
	SYNP_LEN, SYNP_SPB,
	SYNP_NOTE, SYNP_MUTE,
	SYNP_END=255
};
// int syn_seq_write(syn* syn, syn_seq* seq, FILE* f);
// int syn_seq_read(syn* syn, syn_seq* seq, void* data, int len, int*read);



int syn_seq_open(syn* syn, char* path, int instr){
	FILE* f=fopen(path, "rb");
	if(f==NULL){
		SYNLOG("couldnt open file %s", path);
		return 1;
	}
	rewind(f);
	fseek(f, 0, SEEK_END);
	int len = ftell(f);

	void* data=malloc(len);
	rewind(f);
	fread(data, len, 1, f);

	int err=0;
	int read=0;
	err=syn_seq_load(syn, instr, data, len, &read);

	if(err || read!=len){
		SYNLOG("error reading file %s, e:%i, read %i from %i", path, err, read, len);
		free(data);
		return 1;
	}
	free(data);
	return 0;
}

int syn_seq_save(syn* syn, syn_seq* seq, char*path){ assert(syn && seq && path);
	FILE* f=fopen(path, "wb");
	if(f==NULL){
		SYNLOG("couldnt open file %s", path);
		return 1;
	}
	int err=syn_seq_write(syn, seq, f);

	fclose(f);
	return err;
}

int syn_seq_load(syn* syn, int instr, void* data, int len, int*read){
	if(instr >= SYN_TONES) return 1;
	syn_seq* seq = syn->seq[instr];
	// seq_unload(syn->seq[instr]);
	return syn_seq_read(syn, seq, data, len, read);
}


int syn_seq_write(syn* syn, syn_seq* seq, FILE* f){
	fwrite(SYNP_DESC, 4, 1, f);

	uint8_t token[4]={0,0,0,0};
	float value=0;

	syn_tone tone_base;
	syn_tone_init(&tone_base, syn->sr);

	if(seq->tone && memcmp(seq->tone, &tone_base, sizeof(syn_tone))){
		memset(token, 0, 4);
		token[0] = SYNP_TONE;
		value = 0;
		fwrite(token, 1, 4, f);
		fwrite(&value, 4, 1, f);
		if(syn_tone_write(syn, seq->tone, f)){SYNLOG("error writing tone"); return 2;};
	}

	memset(token, 0, 4);
	token[0] = SYNP_LEN;
	value = seq->len;
	fwrite(token, 1, 4, f);
	fwrite(&value, 4, 1, f);

	memset(token, 0, 4);
	token[0] = SYNP_SPB;
	value = seq->spb;
	fwrite(token, 1, 4, f);
	fwrite(&value, 4, 1, f);

	if(seq->mute){
		memset(token, 0, 4);
		token[0] = SYNP_MUTE;
		value = 0;
		fwrite(token, 1, 4, f);
		fwrite(&value, 4, 1, f);
	}

	syn_mod_mat* prev_modm = NULL;

	for(int step=0; step<seq->len; step++){

		if(seq->modm[step] != NULL){
			// write each value that differs from prev step modm
			for(int i=0; i<OSC_PER_TONE; i++){
				for(int j=0; j<OSC_PER_TONE; j++){
					if(prev_modm)
						if( (*(seq->modm[step]))[i][j] == (*(prev_modm))[i][j]) continue;
					// if( (*(seq->modm[step]))[i][j] != tone_target->mod_mat[i][j]){
					memset(token, 0, 4);
					token[0] = SYNP_MODM;
					token[1] = step;
					token[2] = i;
					token[3] = j;
					value = (*(seq->modm[step]))[i][j];
					fwrite(token, 1, 4, f);
					fwrite(&value, 4, 1, f);
					// }
				}
			}
			prev_modm = seq->modm[step];
		}

		for(int voice=0; voice<POLYPHONY; voice++){
			if(seq->note[voice][step] > SEQ_MIN_NOTE){
				memset(token, 0, 4);
				token[0] = SYNP_NOTE;
				token[1] = step;
				token[2] = seq->vel[voice][step];
				token[3] = seq->dur[voice][step];
				value = seq->note[voice][step];
				fwrite(token, 1, 4, f);
				fwrite(&value, 4, 1, f);
			}
		}
	}
	memset(token, 0, 4);
	token[0] = SYNP_END;
	value = 0;
	fwrite(token, 1, 4, f);
	fwrite(&value, 4, 1, f);

	return 0;
}



int syn_seq_read(syn* syn, syn_seq* seq, void* data, int len, int*read){
	if(len<=4) return 1;
	char desc[5]; desc[4]=0;
	memcpy( desc, data, 4 );
	if(memcmp(desc, SYNP_DESC, 4)){
		SYNLOG("file doesnt begin with \"%s\"", SYNP_DESC);
		return 1;
	}

	int i = 4;
	int err = 0;
	char done=0;
	uint8_t flag = 0;
	uint8_t f0 = 0;
	uint8_t f1 = 0;
	uint8_t f2 = 0;
	float val=0;

	// syn_tone tone;
	// syn_tone_init(&tone, syn->sr);

	// syn_seq seq_read;
	// syn_seq_init(&seq_read);
	for (int i = 0; i < SEQ_LEN; ++i){
		seq_anof(seq, i);
	}

	int iread=0;

	while(!err && !done ){
		if(i+8>len){
			err=1; goto end;
		}
		memcpy(&flag, data+i, 1);i++;
		memcpy(&f0, data+i, 1); i++;
		memcpy(&f1, data+i, 1); i++;
		memcpy(&f2, data+i, 1); i++;
		memcpy(&val, data+i, 4);
		i+=4;

		switch(flag){
			case SYNP_LEN: seq->len=val; break;
			case SYNP_SPB: seq->spb=val; break;
			case SYNP_MUTE: seq->mute=1; break;
			case SYNP_NOTE:
				if(seq_non( seq, f0, val, f1/255.f, f2/255.f)) SYNLOG("warrning seq read too many voices");
				break;
			case SYNP_TONE:
				// if(seq->tone != NULL){ printf("warrning seq read multiple tones\n"); free(seq->tone); }
				// seq->tone = malloc(sizeof(syn_tone));
				syn_tone_init(seq->tone, syn->sr);
				syn_tone_read(syn, seq->tone, data+i, len-i, &iread);
				i+=iread;
				break;
			case SYNP_MODM:
				if(seq->modm[f0] == NULL) {
					// seq_modm(&seq_read, seq->tone? &seq->tone->mod_mat: &tone.mod_mat, f0);
					seq_modm(seq, &seq->tone->mod_mat, f0);
				}
				if(f1>=OSC_PER_TONE || f2>=OSC_PER_TONE) {SYNLOG("warrning seq read too many oscilators"); break;}
				(*(seq->modm[f0]))[f1][f2] = val;
				break;
			case SYNP_END: done=1; break;
			default: break;
		}
	}

	if(read) *read=i;
	// memcpy( seq, &seq_read, sizeof(syn_seq));

	end:;
	return err;
}









#define SYNS_DESC "SYNS"
enum syns_flag{
	SYNS_SEQ=0, SYNS_BPM=1,
	SYNS_LOOP=2, SYNS_PAT=3,
	SYNS_LEN=4, SYNS_BDUR=5, SYNS_TIE=6,
	SYNS_END=255
};

int syn_song_open(syn* s, char* path){
	FILE* f=fopen(path, "rb");
	if(f==NULL){
		SYNLOG("couldnt open file %s", path);
		return 1;
	}
	rewind(f);
	fseek(f, 0, SEEK_END);
	int len = ftell(f);

	void* data=malloc(len);
	rewind(f);
	fread(data, len, 1, f);


	int err=0;
	int read=0;
	err=syn_song_load(s, data, len, &read);

	if(err){
		SYNLOG("error reading file %s, e:%i, read %i", path, err, read);
		free(data);
		return 1;
	}
	free(data);
	return 0;
}

int syn_song_save(syn* s, char* path){
	FILE* f=fopen(path, "wb");
	if(f==NULL){
		SYNLOG("couldnt open file %s", path);
		return 1;
	}
	int err=syn_song_write(s, f);
	fclose(f);
	return err;
}


#include <errno.h>
int syn_song_write(syn* s, FILE* f){
	FILE* ftarget = f;
	FILE* ftemp = tmpfile();
	if(!ftemp){
		switch(errno){
			case EACCES: SYNLOG("couldnt open tmpfile, EACCES: Search permission denied for directory in file's path prefix."); break;
			case EEXIST: SYNLOG("couldnt open tmpfile, EEXIST: Unable to generate a unique filename."); break;
			case EINTR:  SYNLOG("couldnt open tmpfile, EINTR: The call was interrupted by a signal; see signal(7)."); break;
			case EMFILE: SYNLOG("couldnt open tmpfile, EMFILE: The per-process limit on the number of open file descriptors has been reached."); break;
			case ENFILE: SYNLOG("couldnt open tmpfile, ENFILE: The system-wide limit on the total number of open files has been reached."); break;
			case ENOSPC: SYNLOG("couldnt open tmpfile, ENOSPC: There was no room in the directory to add the new filename."); break;
			case EROFS:  SYNLOG("couldnt open tmpfile, EROFS: Read-only filesystem.}"); break;
			default: SYNLOG("couldnt open tmpfile. unknown error"); break;
		}
		return 1;
	}

	f=ftemp;

	fwrite(SYNS_DESC, 4, 1, f);

	uint8_t token[4]={0,0,0,0};

	memset(token, 0, 4);
	token[0] = SYNS_BPM;
	uint16_t ibpm = (int)s->bpm;
	token[1] = (ibpm & 0xFF00) >> 8;
	token[2] = ibpm & 0x00FF;
	token[3] = (int)(fmodf(s->bpm,1.0)*255);
	fwrite(token, 1, 4, f);

	memset(token, 0, 4);
	token[0] = SYNS_LEN;
	token[1] = s->song_len;
	fwrite(token, 1, 4, f);

	syn_tone tone_base; syn_tone_init(&tone_base, s->sr);

	for(int i=0; i<SONG_MAX; i++){

		for(int j=0; j<SYN_TONES; j++){
			// skip unmodified  patterns
			char isempty = 1;
			for(int step=0; step<SEQ_LEN; step++)
				isempty &= seq_isempty(&s->song[i*SYN_TONES+j], step);
			if(isempty && !memcmp(s->song[i*SYN_TONES+j].tone, &tone_base, sizeof(syn_tone)))
				continue;

			memset(token, 0, 4);
			token[0] = SYNS_SEQ;
			token[1] = i;
			token[2] = j;
			fwrite(token, 1, 4, f);
			syn_seq_write(s, &s->song[i*SYN_TONES+j], f);
		}

		if(s->song_pat[i] != 0){
			memset(token, 0, 4);
			token[0] = SYNS_PAT;
			token[1] = i;
			token[2] = s->song_pat[i];
			fwrite(token, 1, 4, f);
		}

		if(s->song_beat_dur[i] != 4){
			memset(token, 0, 4);
			token[0] = SYNS_BDUR;
			token[1] = i;
			token[2] = s->song_beat_dur[i];
			fwrite(token, 1, 4, f);
		}

		if(s->song_tie[i]){
			memset(token, 0, 4);
			token[0] = SYNS_TIE;
			token[1] = i;
			token[2] = s->song_tie[i];
			fwrite(token, 1, 4, f);
		}
	}

	memset(token, 0, 4);
	token[0] = SYNS_LOOP;
	token[1] = s->song_loop_begin;
	token[2] = s->song_loop_end;
	token[3] = s->song_loop;
	fwrite(token, 1, 4, f);

	memset(token, 0, 4);
	token[0] = SYNS_END;
	fwrite(token, 1, 4, f);

	//compress
	uint32_t fsize = ftell(f);
	size_t sizez = fsize * 1.1 + 12;
	uint8_t* zd = malloc(sizez);
	void* ogdata = malloc(fsize);

	rewind(f);

	fread(ogdata, 1, fsize, ftemp);

	fclose(ftemp);

	int z_result = compress(zd, &sizez, ogdata, fsize);
	switch( z_result ){
		case Z_OK:
			break;

		case Z_MEM_ERROR:
			SYNLOG("out of memory");
			free(zd); free(ogdata);
			return 1;
			break;

		case Z_BUF_ERROR:
			SYNLOG("output buffer wasn't large enough!");
			free(zd); free(ogdata);
			return 2;
			break;
	}

	fwrite(&fsize, 1,4, ftarget);
	fwrite(zd, sizez, 1, ftarget);
	free(zd); free(ogdata);

	return 0;
}




int syn_song_load(syn* s, void* data, int len, int*read){
	if(len<=4) {return 1;}
	//uncompress
	uint32_t ogsize = 0;
	memcpy(&ogsize, data, 4);
	if(ogsize<=0) { return 1; }
	void* ogdata = malloc(ogsize);

	size_t ogbig_size = ogsize;
	int z_result = uncompress(ogdata, &ogbig_size, data+4, len);

	switch( z_result ){
		case Z_MEM_ERROR:
			SYNLOG("out of memory");
			free(ogdata);
			return 1;
			break;
		case Z_BUF_ERROR:
			SYNLOG("output buffer wasn't large enough!");
			free(ogdata);
			return 2;
			break;
		default: break;
	}
	if(ogbig_size != ogsize) {free(ogdata); return 1;}


	len = ogsize;
	data = ogdata;

	if(len<=4) {free(ogdata); return 1;}
	char desc[5]; desc[4]=0;
	memcpy( desc, data, 4 );
	if(memcmp(desc, SYNS_DESC, 4)){
		SYNLOG("file doesnt begin with \"%s\"", SYNS_DESC);
		free(ogdata);
		return 3;
	}

	syn_lock(s, 1);
	syn_stop(s);
	syn_song_free(s);
	syn_song_init(s);
	syn_song_pos(s, 0);

	int i = 4;
	int err = 0;
	char done=0;
	uint8_t flag = 0;
	uint8_t f0 = 0;
	uint8_t f1 = 0;
	uint8_t f2 = 0;

	int iread=0;
	uint16_t ibpm=0;
	while(!err && !done ){
		if(i+4>len){
			err=1; goto end;
		}
		memcpy(&flag, data+i, 1);i++;
		memcpy(&f0, data+i, 1); i++;
		memcpy(&f1, data+i, 1); i++;
		memcpy(&f2, data+i, 1); i++;

		switch(flag){
			case SYNS_LEN: s->song_len=f0; break;
			case SYNS_SEQ:
				if(f1>=SYN_TONES){
					SYNLOG("warning song has too tones");
					f1=MIN(f1, SYN_TONES-1);
				}
				if(syn_seq_read(s, s->song+f0*SYN_TONES+f1, data+i, len-i, &iread)){ SYNLOG("invalid song pattern"); err=4; goto end;}
				i+=iread;
				break;

			case SYNS_BPM:
				ibpm=0;
				ibpm = ibpm | f0;
				ibpm = ibpm << 8;
				ibpm = ibpm | f1;
				s->bpm=ibpm;
				s->bpm+=((float)f2)/255;
				break;

			case SYNS_LOOP: s->song_loop_begin=f0; s->song_loop_end=f1;s->song_loop=f2; break;

			case SYNS_PAT: syn_song_pat(s, f0, f1); break;
			case SYNS_BDUR: syn_song_dur(s, f0, f1); break;
			case SYNS_TIE: syn_song_tie(s, f0, f1); break;

			case SYNS_END: done=1; break;
			default: break;
		}
	}

	if(read) *read=i;
	syn_song_pos(s, 0);

	end:;
	free(ogdata);
	syn_lock(s, 0);
	return err;
}













int syn_render(syn* s, FILE* f){
	assert(s);
	(void)f;
	return 0;
}
int syn_render_wav(syn* s, char* path){
	assert(s);
	(void)path;
	return 0;
}