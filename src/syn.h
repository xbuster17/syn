#ifndef __SYN_H__
#define __SYN_H__

#include <stdio.h>
#include <stdio.h>

#include <math.h>

#if defined (ANDROID) || defined (_3DS)
#include <SDL.h>
#else
#include <SDL2/SDL.h> // SDL_mutex
// #include <SDL.h> // SDL_mutex
#endif

// typedef unsigned int int;

#include "smath.h"
#include "adsr.h"

#define SYN_TONES 12
#define POLYPHONY 6
#define OSC_PER_TONE 6 // min 3
#define VEL_OUT 4
#define ARP_LEN 32
#define SEQ_LEN 64
#define SEQ_STEP_PER_BEAT_DEF 4

typedef struct {int8_t tone, voice;} noteid;


typedef enum {OSC_NONE=-1,
	OSC_SINE, OSC_TRI, OSC_NOISE, OSC_SQUARE, OSC_PULSE, OSC_SAW,
	OSC_MAX
} syn_osc;

typedef float syn_mod_mat[OSC_PER_TONE][OSC_PER_TONE];

typedef struct {
	float gain;

	int8_t voices;

	// syn_osc osc[OSC_PER_TONE];
	int8_t osc[OSC_PER_TONE];

	float pwm[OSC_PER_TONE];
	float phase[OSC_PER_TONE];
	float time [POLYPHONY][OSC_PER_TONE];

	float freq[POLYPHONY];

	syn_mod_mat mod_mat;
	// [i][i] is the freq_ratio of operator i
	// [i][j] is the index of modulation recieved by operator i from modulator j if j<i
	// index of modulation = modAmp/modFreq. corresponds loosely to number of audible side band pairs
	// unused half matrix used to store osc mix, hence min osc count is 3
	// [0][j] stores osc mix for osc>0 [1][2] for osc 0


	uint8_t vel[POLYPHONY];
	// float* vel_out[VEL_OUT];
	// float vel_min[VEL_OUT];
	// float vel_max[VEL_OUT];


	// float detune;
	// float glide;// in milliseconds
	// float glide_freq_target;
	// float glide_t;
	adsr osc_env[OSC_PER_TONE];
	adsr pitch_env;

// optimize adsr for space, saving state by itself thilse sharing adsr struct
	// adsr amp_env[OSC_PER_TONE];
	// adsr pitch_env[OSC_PER_TONE];
	// adsr fmindex_env[OSC_PER_TONE];

	char gate[POLYPHONY];
	char amp_state[POLYPHONY][OSC_PER_TONE];
	char pitch_state[POLYPHONY];

	float amp_eg_out[POLYPHONY][OSC_PER_TONE];
	float pitch_eg_out[POLYPHONY];

	// char amp_states[POLYPHONY][OSC_PER_TONE];
	// float amp_env_out[POLYPHONY][OSC_PER_TONE];

	// char fmindex_states[POLYPHONY][OSC_PER_TONE];
	// float fmindex_env_out[POLYPHONY][OSC_PER_TONE];

	// adsr filter_env[POLYPHONY];

	float pitch_env_amt;

	float vupeakl;
	float vupeakr;
	// float vupeak_maxl;
	// float vupeak_maxr;

	// syn_arp arp;
} syn_tone;

#define SEQ_MIN_NOTE (-32000)
typedef struct {
	syn_tone* tone; // tone preset
	// syn_mod_mat* modm[SEQ_LEN]; // mod matrix per step
	syn_mod_mat** modm; // mod matrix per step
	short modm_target_step;
	char modm_wait_loop;
	uint8_t len;
	uint8_t spb; // step per beat
	int8_t step;
	float time;
	// float freq[POLYPHONY][SEQ_LEN];
	int16_t note[POLYPHONY][SEQ_LEN];
	uint8_t vel[POLYPHONY][SEQ_LEN];
	uint8_t dur[POLYPHONY][SEQ_LEN];
	noteid active[POLYPHONY]; // internal only, notes currently playing
	char mute;
} syn_seq;

// #define SONG_MAX 0xFF
// typedef struct {
// 	int len;
// 	int cap;
// 	syn_seq seqs;
// 	int index;
// 	int repeat;
// 	short loop_start;
// 	short loop_end;
// } syn_song;

typedef struct syn{
	int sample;
	float a4;
	int sr;
	float master_detune;
	SDL_mutex* mutex;

	float* longbufl; // circular 1 second mono buffer
	float* longbufr; // circular 1 second mono buffer
	int longbuf_len;
//compressor test todo
	float peak;
	float threshold;
	float ratio;

	float rmsl;
	float rmsr;
	int rms_window;
	float vupeakl;
	float vupeakr;
	// float vupeak_maxl;
	// float vupeak_maxr;

	syn_tone tone[SYN_TONES];
	// syn_tone tone_target[SYN_TONES];
	// syn_mod_mat* modm_storage; // stores SYN_TONES * SEQ_LEN mod matrices for automation
	// int8_t modm_storage_active[SYN_TONES][SEQ_LEN];

	syn_mod_mat modm_target[SYN_TONES];
	int8_t modm_target_active[SYN_TONES];
	float modm_lerpt[SYN_TONES];
	float modm_lerpms[SYN_TONES];
	char modm_lerp_mode; // 0: snap instantly to current step, 1: use a fixed time, 2: lerp between 2 targets

//step sequencer
	float bpm;
	char seq_play;
	syn_seq seq[SYN_TONES];
} syn;

void syn_init(syn* s, int sr);
void syn_quit(syn* s);
void syn_run(syn* s, float* buffer, int len); // len: number of stereo samples to process
void syn_lock(syn* s, char l); // 1:lock 0:unlock

//note control
noteid syn_non(syn* s, int instr, float note, float vel);
void syn_nof(syn* s, noteid nid);
void syn_anof(syn* s, int instr);
void syn_nvel(syn* s, noteid nid, float vel);
void syn_nset(syn* s, noteid nid, float note);
void syn_nfreq(syn* s, noteid nid, float freq);
float syn_a4(syn*s , float a4hz);

//seq control
void syn_pause(syn* s);
void syn_stop(syn* s);
float syn_bpm(syn*s , float bpm); // bpm<0 to avoid setting

// osc fm
float tone_frat(syn_tone* t, int osc, float freq_ratio); // <0 to avoid setting
float tone_index(syn_tone* t, int carrier, int modulator, float fm_index); // <0 to avoid setting; carrier > modulator
float tone_omix(syn_tone* t, int osc, float mix); // <0 to avoid setting
// osc amp env adsr
float tone_atk(syn_tone* t, int osc, float a); // a<0 to avoid setting
float tone_dec(syn_tone* t, int osc, float d); // d<0 to avoid setting
float tone_sus(syn_tone* t, int osc, float s); // s<0 to avoid setting
float tone_rel(syn_tone* t, int osc, float r); // r<0 to avoid setting
float tone_aexp(syn_tone* t, int osc, float ae); // <0 to avoid setting, 1.0:linear to 0.0:exponential
float tone_dexp(syn_tone* t, int osc, float de); // <0 to avoid setting, 1.0:linear to 0.0:exponential
// tone pitch env adsr
float tone_patk(syn_tone* t, float a); // a<0 to avoid setting
float tone_pdec(syn_tone* t, float d); // d<0 to avoid setting
float tone_psus(syn_tone* t, float s); // s<0 to avoid setting
float tone_prel(syn_tone* t, float r); // r<0 to avoid setting
float tone_paexp(syn_tone* t, float ae); // <0 to avoid setting, 1.0:linear to 0.0:exponential
float tone_pdexp(syn_tone* t, float de); // <0 to avoid setting, 1.0:linear to 0.0:exponential
//sequencer
void syn_seq_init(syn_seq* s);
void syn_seq_advance(syn* s, float secs);
char seq_non(syn_seq* s, int pos, float note, float vel, float dur); // returns 1 if note couldnt be added
void seq_nof(syn_seq* s, int pos, int voice);
void seq_anof(syn_seq* s, int pos);
void seq_clear(syn_seq* s);
char seq_ison(syn_seq* s, int pos, int voice);
char seq_isempty(syn_seq* s, int pos);

short seq_len(syn_seq* s, short len);
short seq_spb(syn_seq* s, short spb); // step per beat

char seq_isgate(syn_seq* s, int voice);
int seq_mute(syn_seq* s, int mute); // 1: mute, 0:unmute, <1 query

// copies the last _count_ samples into dst
void syn_read(syn*, float* dst, int count, int channel); //channel: 0=left, 1=right
float syn_rms(syn*);
float syn_rms_time(syn*, float ms); // ms<0 to avoid setting. clamps

// set a tone as transform target to be linearly interpolated over time
// void syn_tone_lerp(syn*, int tone, syn_tone* target, float mstime); // mstime<=0 to set instantly
// void seq_tone(syn_seq*, syn_tone* target, int step);

void seq_modm(syn_seq*, syn_mod_mat*, int step);
void seq_unload(syn_seq*);

// set a modulation matrix as transform target to be linearly interpolated over time
void syn_modm_lerp(syn* s, int tone, syn_mod_mat* target, float mstime); // mstime<=0 to set instantly
void syn_modm_do_lerp(syn_mod_mat* dst, syn_mod_mat* src, float t);


void syn_load_seq(syn*, syn_seq*);

float* syn_modm_addr(syn_mod_mat*, int carrier, int modulator); // c=m to get ratio, m<0 to get oscmix



void syn_song_load(syn* syn, void* data, int len);
void syn_seq_load(syn* syn, void* data, int len);







#endif