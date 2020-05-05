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
#define OSC_PER_TONE 6 // min 3, max 255
#define VEL_OUT 4
#define ARP_LEN 32
#define SEQ_LEN 64
#define SEQ_STEP_PER_BEAT_DEF 4
#define SONG_MAX 256

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
	float oct[OSC_PER_TONE];
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

	float gain;
	float rmsl;
	float rmsr;
	int rms_window;
	float vupeakl;
	float vupeakr;

	syn_tone* tone[SYN_TONES];
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
	syn_seq* seq[SYN_TONES];

//song
	syn_seq* song;
	syn_tone* song_tones; // tone on a separate array to help  cache proximity
	uint8_t* song_pat;
	uint8_t* song_beat_dur;
	float* song_bpm; // todo: how would gui for this work?
	char* song_tie; // bool, transfers the state (eg,freq,voices) of the next pattern
	char song_advance;
	char song_loop;
	uint8_t song_pos;
	// uint8_t song_beat;
	uint8_t song_loop_begin;
	uint8_t song_loop_end;
	// int song_cap;
	int song_len;
	int song_spb;
	// int song_loop_count;
	float song_time;

} syn;

void syn_init(syn* s, int sr);
void syn_quit(syn* s);
void syn_run(syn* s, float* buffer, int len); // len: number of stereo samples to process
void syn_lock(syn* s, char l); // 1:lock 0:unlock
void syn_song_init(syn* s);
void syn_song_free(syn* s);
void syn_song_advance(syn* s, float secs);
int syn_song_pos(syn* s, int pos); // pos<0 to query, loads target position
int syn_song_pat(syn* syn, int pos, int pat); // pat<0 to query, assigns a pattern to a position
int syn_song_dur(syn* syn, int pos, int dur); // dur<0 to query
int syn_song_len(syn* syn, int len);
char syn_song_tie(syn* syn, int pos, char tie); // tie<0 to query
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

void syn_tone_init(syn_tone* s, int sr);
// osc fm
float tone_frat(syn_tone* t, int osc, float freq_ratio); // <0 to avoid setting
float tone_index(syn_tone* t, int carrier, int modulator, float fm_index); // <0 to avoid setting; carrier > modulator
float tone_omix(syn_tone* t, int osc, float mix); // <0 to avoid setting
float syn_oct_mul(float oct_val); // gets the frequency multiplier from the tone octave value
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



// void syn_song_load(syn* syn, void* data, int len);
// void syn_seq_load(syn* syn, void* data, int len);

/*
TONE FILE
	if file has information for more oscilators than what the current syn was compiled for
	that is, if osc >= OSC_PER_TONE, then it's value is ignored
	FILE description:
		first 4 bytes must equal "SYNT"
		then it's read by chunks of 8 bytes where:
		byte 0 is a flag from enum synt_flag: SYNT_* <255
		bytes 1-3 has additional information for which osc the flag refers to and modulator/carrier ids
		bytes 4-7 is a float value for the current flag
		after an arbitrary amount of chunks it ends with a chunk with SYNT_END==255 as flag (end chunk)
*/
int syn_tone_open(syn* syn, char* path, int instr );
int syn_tone_save(syn* syn, syn_tone* t, char* path);

// returns !0 on error, fills read with the number of bytes read including end chunk
int syn_tone_load(syn* syn, int instr, void* data, int len, int* read);
int syn_tone_read(syn* syn, syn_tone* t, void* data, int len, int* read);
int syn_tone_write(syn* syn, syn_tone* t, FILE* f);


int syn_seq_open(syn*, char* path, int instr);
int syn_seq_save(syn*, syn_seq*, char*path);
int syn_seq_load(syn*, int instr, void* data, int len, int*read);

int syn_seq_write(syn*, syn_seq*, FILE* f);
int syn_seq_read(syn*, syn_seq*, void* data, int len, int*read);


int syn_song_open(syn*, char* path);
int syn_song_save(syn*, char* path);

int syn_song_write(syn*, FILE* f);
int syn_song_load(syn*, void* data, int len, int*read);

//todo
int syn_render(syn* s, FILE* f);
int syn_render_wav(syn* s, char* path);



#endif