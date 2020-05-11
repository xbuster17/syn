#if defined (ANDROID)
	#include <SDL.h>
	// #include <SDL_mixer.h>
	#include <SDL_ttf.h>
#elif !defined(_3DS)
	#include <SDL2/SDL.h>
	// #include <SDL2/SDL_mixer.h>
	#include <SDL2/SDL_ttf.h>
#else
	#include <3ds.h>
	#include <citro2d.h>
#endif
#ifdef __vita__
	#include <psp2/kernel/processmgr.h>
	#include "debugScreen.h"
	#define printf psvDebugScreenPrintf
	#define exit sceKernelExitProcess
	#define usleep sceKernelDelayThread
#endif

#ifdef __EMSCRIPTEN__
	#include <emscripten.h>
	#include <emscripten/html5.h>
#else
	#define EMSCRIPTEN_KEEPALIVE
#endif

#include <stdio.h>
#include <stdlib.h> // exit
#include <string.h> // strerror
#include <errno.h>
#include <limits.h>
// #include <sys/soundcard.h>
#include <fcntl.h>
#include <unistd.h>

#include "syn.h"

#ifdef __vita__
#define BASE_SCREEN_WIDTH 960
#define BASE_SCREEN_HEIGHT 544
#else
// 3ds' top screen
// #define BASE_SCREEN_WIDTH 400
// #define BASE_SCREEN_HEIGHT 240
#define BASE_SCREEN_WIDTH 320
#define BASE_SCREEN_HEIGHT 240
#endif

int screen_width =BASE_SCREEN_WIDTH;
int screen_height=BASE_SCREEN_HEIGHT;
int octave=4;

/* -------------------------------------------------------------------------- */
/* use this api for graphics */
/* -------------------------------------------------------------------------- */
typedef struct sg_rect{
	int16_t x,y, w,h;
	uint8_t r,g,b,a;
	int16_t tid; // when using texture rgb are color mods
	float ang; // degrees
} sg_rect;

void sg_init(void);
void sg_quit(void);
void sg_rcol(sg_rect*, float r, float g, float b, float a);

void sg_drawrect( sg_rect rect );
void sg_drawperim( sg_rect rect );

void sg_show(void);
void sg_clear(void);


// returns an identifier valid until sg_quit is called
// typedef union{struct{uint8_t r,g,b,a;}; uint32_t c;} rgba8;
typedef union{struct{uint8_t a,b,g,r;}; uint32_t c;} rgba8;
int16_t sg_addtex( rgba8* pixels, int w, int h );
int16_t sg_addtext( char* text );
int16_t sg_modtext( int16_t tex, char* newtext);
void sg_texsize( int16_t tex, int* x, int* y);
void sg_drawtex( uint16_t tex , int x, int y, float ang, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void sg_drawtext( uint16_t tex , int x, int y, float ang, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void sg_clear_area(int x, int y, int w, int h);
void sg_target(SDL_Texture*);

char ptbox( int x, int y, sg_rect r); // point-box collision check
int isel(int i); // select active instrument
void astep(int step); // select active step
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */

void key_update(int key, char on);

struct {
	char audio_running;
	SDL_AudioDeviceID audev;
	SDL_AudioSpec auwant,auhave;
	int sr;
	int format;
	int samples;
	int channels;
	SDL_Window* window;
	SDL_Renderer* renderer;
	int window_focus;
	syn* syn;
	SDL_Texture* current_target_tex;
	SDL_Texture* tone_view_tex;
	SDL_Texture* pattern_view_tex;
	SDL_Texture* lib_view_tex;
	float time; // seconds
	float dt;
} G;




#ifndef _3DS
	static inline size_t auformat_size( SDL_AudioFormat format){ return SDL_AUDIO_BITSIZE(format)/8; }
	int sample_rate = 48000;
	// int sample_rate = 44100;
	int samples_size = 256;
#else
	static inline size_t auformat_size( Uint16 format){ return 2; } // AUDIO_S16SYS
	int sample_rate = 22050;
	// int sample_rate = 32728;
	int samples_size = 256*4;
#endif

int C0 = 0 -9 -12*4;
#define MAX_OCTAVE 10
#define MAX_KEYS (12*MAX_OCTAVE)
noteid voices[MAX_KEYS];


float testpitch=0;
float testvelocity=1;
int sample=0;

float prevl=0;
float prevr=0;

float* longbufl=NULL; // circular 1 second stereo buffer
float* longbufr=NULL; // circular 1 second stereo buffer
int longbuf_len=22050;
// int longbuf_len=48000;

float peak=0;
float threshold=.5;
float ratio=4.0;
float rmsl=0;
float rmsr=0;
int rms_window=960;
float vupeakl=-60;
float vupeakr=-60;
float vupeak_maxl=-60;
float vupeak_maxr=-60;

char seq_mode = 0;



int getlongbuf_index(int i){
	int index = (i + sample-G.auhave.samples)% longbuf_len;
	if(index<0) index = index+longbuf_len;
	if(index<0 || index>=longbuf_len) SDL_Log("wrong index %i %i:%s\n", index, __LINE__, __func__);
	return index;
}


















void maincb(void* userdata, uint8_t* stream, int length){
	int smp = ((float)length) / auformat_size(G.auhave.format);
	float buffer[ smp ];
	memset(buffer, 0, sizeof(buffer));
	memset(stream, 0, length);
	short* out=(short*)stream;
	// float* out=(float*)stream;

	syn_run(userdata, buffer, smp/2);

	int totsmp = ((float)length) / auformat_size(G.auhave.format);

	sample+=totsmp/2;
	// float dtime = .1;
	// float dfeedb = .7;

	// prevr = s[--i];
	// prevl = s[--i];

	int i=0;
	while(i<totsmp){
		longbufl[getlongbuf_index(floor(i/2))]=buffer[i]; i++;
		longbufr[getlongbuf_index(floor(i/2))]=buffer[i]; i++;

		//delay
		// i-=2;
		// longbufl[getlongbuf_index(floor(i/2))] += /*(2<<14) **/ longbufl[getlongbuf_index(i/2-dtime*G.auhave.freq)]*dfeedb;i++;
		// longbufr[getlongbuf_index(floor(i/2))] += /*(2<<14) **/ longbufr[getlongbuf_index(i/2-dtime*G.auhave.freq)]*dfeedb;i++;
		// longbufl[i]+=longbufl[(int)fmodf(i/2-1000,2000)]*dfeedb;i++;
		// longbufr[i]+=longbufr[(int)fmodf(i/2-1000,2000)]*dfeedb;i++;
	}

	i=0;
	while(i < totsmp){ // output
		out[i] = (signed short)(CLAMP(longbufl[getlongbuf_index(floor(i/2))], -1, 1) * (2<<14)); i++;
		out[i] = (signed short)(CLAMP(longbufr[getlongbuf_index(floor(i/2))], -1, 1) * (2<<14)); i++;
	}
}



// for sdl mixer register effect
void mix_cb(int chan, void *stream, int len, void *udata){
	(void)chan;
	maincb(udata, stream, len);
}



#ifndef _3DS
void audevlist(void){
	int n=SDL_GetNumAudioDevices(0);
	for (int i = 0; i < n; ++i)
		SDL_Log("%i:%s\n", i, SDL_GetAudioDeviceName(i,0));
}
#else
void audevlist(void){}
#endif













char* wname = "syn";
#ifdef _3DS
int wflag = SDL_CONSOLEBOTTOM; ///*SDL_WINDOW_RESIZABLE*/ SDL_WINDOW_SHOWN;
#else\
// fixme ms windows
int wflag = SDL_WINDOW_RESIZABLE;///*SDL_WINDOW_FULLSCREEN_DESKTOP|*/ /*SDL_WINDOW_BORDERLESS |*/ SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN;

#endif

SDL_Joystick* joystick=NULL;

// input decl
int sdl_event_watcher(void* udata, SDL_Event* e);
int sdl_event_watcher_3ds(const SDL_Event* e);
int32_t* kbstate; // timestamps or 0 for every key
int kbstate_max;
void input_update(void);
int32_t kbget(SDL_Keycode);
void kbset(SDL_Keycode, int32_t);


struct {
	char b0,b1,b2; // left, right, middle
	int wx,wy; // wheel
	int wtx,wty; // wheel total
	float px,py;
	float dx,dy;
	float sdx,sdy; // scaled delta, based on window size, use when needing smaller resolutions
} Mouse;


#define MAX_FINGER 11
struct {
	char down; // 1==front, 2==back
	uint16_t px,py;
	int16_t dx,dy;
	int id;
} Finger[MAX_FINGER];


void gui_libview(void);
void libview_init(void);
void libview_quit(void);

void gui_quit_dialog(void);






int init_systems(void){
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"); // linear filtering

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
	#ifdef ANDROID
		wflag = SDL_WINDOW_BORDERLESS|SDL_WINDOW_RESIZABLE|SDL_WINDOW_FULLSCREEN;
	#endif

	#if defined _WIN32 || defined __CYGWIN__
		wflag = SDL_WINDOW_FULLSCREEN_DESKTOP;
	#endif


int rflag = SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE;

	if((G.window = SDL_CreateWindow( wname, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			BASE_SCREEN_WIDTH, BASE_SCREEN_HEIGHT, wflag)) == NULL){
		SDL_Log("E:%s\n", SDL_GetError());
		return 1;
	}
	if((G.renderer = SDL_CreateRenderer( G.window, -1, rflag)) == NULL){
		SDL_Log("E:%s\n", SDL_GetError());
		return 1;
	}
SDL_SetRenderDrawBlendMode(G.renderer, SDL_BLENDMODE_BLEND);

	SDL_GameControllerEventState(SDL_ENABLE);
	joystick = SDL_NumJoysticks()>0 ? SDL_JoystickOpen(0) : NULL;

sg_target(G.tone_view_tex);
	G.tone_view_tex = SDL_CreateTexture(G.renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, screen_width, screen_height);
	sg_target(G.tone_view_tex);
	sg_clear();

	G.pattern_view_tex = SDL_CreateTexture(G.renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, screen_width, screen_height);


	G.lib_view_tex = SDL_CreateTexture(G.renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, screen_width, screen_height);


#if 1 // sdl audio
	G.auwant.freq     = sample_rate;
	// G.auwant.format   = AUDIO_F32SYS;
	G.auwant.format   = AUDIO_S16SYS;
	G.auwant.channels = 2;
	G.auwant.samples  = samples_size;
	G.auwant.callback = maincb;
	G.auwant.userdata = G.syn;

	G.audev = SDL_OpenAudioDevice(NULL, 0, &G.auwant, &G.auhave, 0/*SDL_AUDIO_ALLOW_FORMAT_CHANGE*/);
	if(G.audev == 0){
		SDL_Log("E:%s\n", SDL_GetError());
		SDL_Delay(50000);
		return 1;
	} else {
		G.audio_running = 1;
		SDL_PauseAudioDevice(G.audev, 0);
	}

#else // sdl mixer

	Mix_Init( /*MIX_INIT_FLAC | MIX_INIT_MOD | MIX_INIT_MP3 | MIX_INIT_OGG*/ 0 );
	// if ( Mix_OpenAudio ( sample_rate, AUDIO_S16SYS, 2, samples_size ) == -1) {
	if ( Mix_OpenAudio ( sample_rate, AUDIO_F32SYS, 2, samples_size ) == -1) {
		printf("Mix_openAudio: %s\n", Mix_GetError());
	}
	G.auhave.freq     = sample_rate;
	// G.auhave.format   = AUDIO_S16SYS;
	G.auhave.format   = AUDIO_F32SYS;
	G.auhave.channels = 2;
	G.auhave.samples  = samples_size;
	// G.auhave.callback = maincb;

	Mix_AllocateChannels(8);

	if(!Mix_RegisterEffect(1, mix_cb,NULL, G.syn)) printf("Mix_RegisterEffect: %s\n", Mix_GetError());

	int len = 2*2*2*48000;
	short* data = malloc(len);
	memset(data, 0, len);
	Mix_Chunk* mix_chunk = Mix_QuickLoad_RAW((Uint8*)data, len);
	if(Mix_PlayChannel(1, mix_chunk, -1)==-1) printf("Mix_PlayChannel: %s\n",Mix_GetError());

#endif

	{/*input init*/
		SDL_GetKeyboardState(&kbstate_max);
		kbstate = malloc(kbstate_max*sizeof(int32_t));
		memset(kbstate, 0, kbstate_max*sizeof(int32_t));
	}

	return 0;

}



char voices_inited=0;
void init_voices(void){
	voices_inited=1;
	for(int i=0; i<MAX_KEYS;i++)
		voices[i] = (noteid){-1, -1};
}

// int SDL_main(int argc, char**argv){ (void)argv, (void)argc;

void main_loop(void);
void gui_quit(void);

int main(int argc, char**argv){ (void)argv, (void)argc;
	memset(&G, 0, sizeof(G));
	G.syn = malloc( sizeof (syn) );
	memset( G.syn, 0, sizeof(syn) );



	if(!longbufl || !longbufr){
		longbufl=malloc(sizeof(float)*longbuf_len);
		longbufr=malloc(sizeof(float)*longbuf_len);
		memset(longbufl, 0, sizeof(float)*longbuf_len);
		memset(longbufr, 0, sizeof(float)*longbuf_len);
	}

	syn_init(G.syn, sample_rate);
	if(!voices_inited) init_voices();

	if(init_systems()) exit(EXIT_FAILURE);

	sg_init();

	// SDL_SetRenderTarget(G.renderer, G.tone_view_tex);
if(SDL_IsTextInputActive()) SDL_StopTextInput();

	#ifdef __EMSCRIPTEN__
		emscripten_set_main_loop(main_loop, 24, 1);
	#else
		main_loop();
	#endif

	{ // quit();
		sg_quit();
		gui_quit();
		#ifndef _3DS
			if (SDL_JoystickGetAttached(joystick)) {
				SDL_JoystickClose(joystick);
			}
			SDL_PauseAudio(1);
			SDL_CloseAudioDevice(G.audev);
			SDL_DestroyRenderer(G.renderer);
			SDL_DestroyWindow(G.window);
			// SDL_DelEventFilter(sdl_event_watcher, NULL);
		#else

			SDL_JoystickClose(joystick);

		#endif

		if(longbufl) free(longbufl);
		if(longbufr) free(longbufr);
		SDL_Quit();
		syn_quit(G.syn);
		free(G.syn);
		libview_quit();
		exit(0);

	}

	return 0;
}



















int running = 1;
int frame=0;
sg_rect fillRect;

int _isel=0;
char ipeaks[SYN_TONES];
int beat=0;
int pbeat=0;
int step=0;
int pstep=0;
float tap_tempo = -1;
int tap_tempo_key = SDLK_KP_MINUS;
float tap_tempo_min_bpm = 20.0; // will cancel tap_tempo if resulting bpm would be lower than this

int step_sel=0;
char bpm_text[16];
int16_t bpm_tex;

int step_add=0;
char step_add_text[16];
int16_t step_add_tex;

int spb=0;
char spb_text[16];
int16_t spb_tex;

int16_t gplay_tex;

// quit dialog
char* quitd_text = "Are you sure you want to QUIT?";
char* quitd_text_no = "No! (Escape)";
char* quitd_text_yes = "Yes (Enter)";
int16_t quitd_tex;
int16_t quitd_tex_yes;
int16_t quitd_tex_no;
char request_quit=0;

/* virtual keyboard */
int vkb_note = 0;
char vkb_note_active;

/* gui update flags */
char gup_bpm=1;
char gup_step_add=1;
char gup_spb=1;
char gup_mix=1;
char gup_seq=1;
char gup_osc=1;
char gup_rec=1;
char gup_vkb=1;
char gup_shown_notes=1; // which notes are drawn in the pattern view
char gup_step_bar=1; // pattern view
char gup_note_grid=1;
char gup_song=1;
char gup_lib=1;

// save dialog
#define SONG_NAME_MAX 32
char* song_name=NULL;
int16_t song_name_tex;
int16_t song_name_empty_tex;
char gup_song_name=1;
int song_name_cursor=0;
void gui_save_dialog(void);
char save_requested=0;

int16_t save_dialog_text_tex;
// int16_t save_dialog_overwrite_tex;
int16_t save_type_tex[4];

int16_t song_tex[SONG_MAX];
int song_pos=0;
int prev_song_pos=0;
#define num_pat_colors 16
rgba8 pat_colors[num_pat_colors]={
{.c=0xfdffffff},
{.c=0xff8686ff},
{.c=0xfb4771ff},
{.c=0xfef86aff},
{.c=0xfffb5fff},
{.c=0xf3f34bff},
{.c=0xfd6deaff},
{.c=0xffb9ffff},
{.c=0xf67febff},
{.c=0xfa547bff},
{.c=0xf78c7fff},
{.c=0xfce7a7ff},
{.c=0xfcfcadff},
{.c=0xffec6dff},
{.c=0xffa763ff},
{.c=0xff4040ff},

};

rgba8 get_patcol(int i){
	return pat_colors[ i % num_pat_colors ];
}




int note_scrollv=12*4;
int note_scrollh=0;
float note_scrollh_vel=0;
float note_scrollh_pos=0;
int note_cols[SYN_TONES][SEQ_LEN]; // collumns for step bar


char rec=0;
char gup_mlatch=1; // updates the value mouse is latched to
int16_t knob_tex;
int16_t modm_tex;
int16_t label_tex[SYN_TONES][10];
int16_t wave_tex[OSC_MAX];
char wave_text[OSC_MAX][10]={
	{"sine"},
	{"tri "},
	{"nois"},
	{"sqr "},
	{"puls"},
	{"saw "}
};
int16_t seq_step_tex;

int16_t rec_tex;

int16_t step_bar_tex[SEQ_LEN+1];
int16_t Cn_tex[MAX_OCTAVE];

#ifndef __vita__
#define knob_size 13
#else
#define knob_size 41
#endif
#define knob_thickness 2
rgba8 knob_pixels[knob_size*knob_size];

float* mlatch = NULL; // on click target is selected
float mlatch_factor = 0.0; // mouse delta is multiplied by this
float mlatch_max = 0.0; // mlatch get's clamped to these
float mlatch_min = 0.0;
char mlatch_v=0; // use vertical delta
signed char mlatch_adsr=-1; // call adsr updaters
char mlatch_adsr_osc=0;
char mlatch_adsr_pitch=0; // set to 1 if latched to pitch envelope


char mlatch_text[16]; // holds text for the value of mouse latch
int16_t mlatch_tex; // updates the value mouse is latched to

float* phony_latch_isel= (float*)1;
float* phony_latch_seq = (float*)2;
float* phony_latch_bpm = (float*)3;
float* phony_latch_vkb = (float*)4;
float* phony_latch_scrollv = (float*)5;
float* phony_latch_scrollh = (float*)6;
float* phony_latch_note_grid = (float*)7;
float* phony_latch_pat = (float*)8;
float* phony_latch_pat_dur = (float*)9;
float* phony_latch_song_scroll = (float*)10;
float* phony_latch_song_len = (float*)11;
float* phony_latch_loop_begin = (float*)12;
float* phony_latch_loop_end = (float*)13;
int song_scroll=0;
int mlatch_song_pat=0;
int giselh=16; // height of intrument select / instrument vumeter
int giselw=16;
int gseq_basey=0;
int gseqh=POLYPHONY*6;
int gosc_basey=0;
int gosch=16;
// int gmodm_basex=0;
int gmodm_basey=0;
int gadsr_basex=0;
int gadst_basey=0;

int key_delay=-1;
char follow=0; // active step will follow play position

// will replace the last character with a letter for each tone
#define TMP_CP_BUF_NAME "/tmp/.tmp_syn_copy_buffer_ "
char pattern_copy=0; // presence of copy buffer
FILE* pattern_copy_file[SYN_TONES]; // tmp files copy buffer

char gui_inited =0;
void gui_init(void);


void gui_quit(void){
	for(int i=0; i<SYN_TONES; i++){
		fclose(pattern_copy_file[i]);
	}
	char* name = strdup(TMP_CP_BUF_NAME);
	for(int i=0; i<SYN_TONES; i++){
		name[ strlen(name)-1 ]=i+'a';
		remove(name);
	}
	free(name);
}

void gui_init(void){
	gui_inited=1;

	char* name = strdup(TMP_CP_BUF_NAME);
	for(int i=0; i<SYN_TONES; i++){
		name[ strlen(name)-1 ]=i+'a';
		pattern_copy_file[i] = fopen(name, "wb");
	}
	free(name);
	// libview_init();

	memset( bpm_text,0,16);
	memset( step_add_text,0,16);
	memset( spb_text,0,16);
	memset( ipeaks, 0, SYN_TONES );

	char song_text[4];
	memset(song_text,0,4);
	for(int i=0; i<SONG_MAX; i++){
		snprintf(song_text, 4, "%x", i);
		song_tex[i] = sg_addtext(song_text);
	}

	for(int i=0; i<SYN_TONES; i++)
		for(int j=0; j<SEQ_LEN; j++)
			note_cols[i][j]=0;

	for(int i = 0; i<OSC_MAX; i++)
		wave_tex[i] = sg_addtext(wave_text[i]);

	for(int i=0; i<knob_size; i++){
		for(int j=0; j<knob_size; j++){
			float x=(i-knob_size/2);
			float y=(j-knob_size/2);
			knob_pixels[i*knob_size+j].c = 0;
			float r=sqrt(x*x+y*y);

			if(r<=knob_size/2 + 1) // extra bit of black
				knob_pixels[i*knob_size+j].c = 0x000000FF;

			if(((y)==0) && (x>0)) // line
				knob_pixels[i*knob_size+j].c = 0xFFFFFFFF;

			else if(r<knob_size/2-1) // background
				knob_pixels[i*knob_size+j].c = 0x22222222;

			if(r>knob_size/2-knob_thickness && r<knob_size/2 ) // circle
				knob_pixels[i*knob_size+j].c = 0xFFFFFFFF;

		}
	}

// int16_t step_bar_tex[SEQ_LEN];
// int16_t Cn_tex[MAX_OCTAVE];
	char tex_text[8]; memset(tex_text, 0, sizeof(tex_text));

	for (int i = 0; i < SEQ_LEN+1; ++i){
		snprintf(tex_text, 8, "%i", i);
		step_bar_tex[i] = sg_addtext(tex_text);
	}

	for (int i = 0; i < MAX_OCTAVE; ++i){
		snprintf(tex_text, 8, "c%i", i);
		Cn_tex[i] = sg_addtext(tex_text);
	}

	quitd_tex = sg_addtext(quitd_text);
	quitd_tex_yes = sg_addtext(quitd_text_yes);
	quitd_tex_no = sg_addtext(quitd_text_no);
	seq_step_tex = sg_addtext("^");
	knob_tex = sg_addtex(knob_pixels, knob_size, knob_size);

	gplay_tex = sg_addtext(">/||");
	bpm_tex = sg_addtext("0");
	step_add_tex = sg_addtext("0");
	spb_tex = sg_addtext("0");
	mlatch_tex = sg_addtext("0");
	rec_tex = sg_addtext("REC");
// starting point of gui elements
	gseq_basey=giselh+2;
	gosc_basey=gseq_basey+gseqh+8+2/*+knob_size*/;

	save_dialog_text_tex = sg_addtext("SAVE:");
	song_name_empty_tex = sg_addtext("Please enter file name");
	save_type_tex[0] = sg_addtext("Song");
	save_type_tex[1] = sg_addtext("Seq ");
	save_type_tex[2] = sg_addtext("Tone");
	save_type_tex[3] = sg_addtext("Wave");

}














int gui_view = 0;
void gui_toneview(void);
void gui_patternview(void);

double _time=0;
double _prev_time=0;

/*----------------------------------------------------------------------------*/
/*main loop*/
/*----------------------------------------------------------------------------*/
void main_loop(void){
	if(!gui_inited) gui_init();

	_time = SDL_GetPerformanceCounter();

	while(running){
		_prev_time = _time;
		_time = SDL_GetPerformanceCounter();
		G.dt = (float)((_time - _prev_time)*1000.0 / (double)SDL_GetPerformanceFrequency() )/1000.f;
		G.time += G.dt;

		input_update();

		step = G.syn->seq[_isel]->step;
		beat = step/G.syn->seq[_isel]->spb;

		if(G.syn->tone[_isel]->vupeakl > 0.99) ipeaks[_isel]=1;
		if(G.syn->tone[_isel]->vupeakr > 0.99) ipeaks[_isel]=1;

		switch(gui_view){
			case 0: gui_toneview(); break;
			case 1: gui_patternview(); break;
			case 2: gui_libview(); break;
			default:break;
		}


		if(gui_view<2 && kbget(SDLK_TAB)){
			kbset(SDLK_TAB, 0);
			gui_view = !gui_view;
			switch(gui_view){
				case 0:
					sg_target(G.tone_view_tex);
					gup_mlatch=1;
					gup_rec=1;
					gup_seq=1;
					gup_osc=1;
					gup_spb=1;
					gup_step_add=1;
					gup_bpm=1;
					gup_vkb=1;
					gup_mix=1;
					gup_song=1;

					break;
				case 1:
					sg_target(G.pattern_view_tex);
					gup_step_bar=1;
					gup_note_grid=1;
					rec=0;
					break;
				default:break;
			}
			sg_clear();
		}

		if((kbget(SDLK_LCTRL)||kbget(SDLK_RCTRL)) && kbget(SDLK_l)){
			kbset(SDLK_l, 0);
			rec=0;
			gui_view=2; // start lib view
			sg_target(G.lib_view_tex);
			gup_lib=1;
		}


		frame++;
		pbeat = beat;
		pstep = step;
		sg_show();


		#ifdef __EMSCRIPTEN__
			break;
		#endif
		SDL_Delay(16);
	}

	#ifdef __EMSCRIPTEN__
		if(!running){
			emscripten_cancel_main_loop();
			return;
		}
	#endif
}























/*----------------------------------------------------------------------------*/
/*input*/
/*----------------------------------------------------------------------------*/

void input_update(void){
	Mouse.dx=0;Mouse.dy=0;
	Mouse.sdx=0;Mouse.sdy=0;
	Mouse.wx=0;Mouse.wy=0;
	// SDL_PumpEvents();
	SDL_Event e;
	while(SDL_PollEvent(&e)) sdl_event_watcher(NULL, &e);
}
int32_t kbget(SDL_Keycode k){
	return kbstate[SDL_GetScancodeFromKey(k) % kbstate_max];
}
void kbset(SDL_Keycode k, int32_t v){
	kbstate[SDL_GetScancodeFromKey(k) % kbstate_max]=v;
}



int sdl_event_watcher(void* udata, SDL_Event* event){ (void) udata;
	SDL_Event e = *event;
// must lock syn when activating notes from main thread
// syn_lock(G.syn, 1); // todo make a finer lock
	char mod= kbget(SDLK_LSHIFT) || kbget(SDLK_RSHIFT) || kbget(SDLK_LCTRL) || kbget(SDLK_RCTRL) || kbget(SDLK_LALT) || kbget(SDLK_RALT);

	switch(e.type){
		// case SDL_QUIT: running=0; break;
		case SDL_QUIT:
			if(!request_quit && !save_requested){
				kbset(SDLK_ESCAPE, 0);
				kbset(SDLK_RETURN, 0);
				request_quit =1;
			}
			break;

#ifndef _3DS
		case SDL_WINDOWEVENT:
			switch(e.window.event){
				case SDL_WINDOWEVENT_SIZE_CHANGED:
				case SDL_WINDOWEVENT_RESIZED:
					screen_width = e.window.data1;
					screen_height= e.window.data2;

					break;
				case SDL_WINDOWEVENT_CLOSE:
					if(!request_quit){
						kbset(SDLK_ESCAPE, 0);
						kbset(SDLK_RETURN, 0);
						request_quit =1;
					}
					break;
				case SDL_WINDOWEVENT_ENTER:
				case SDL_WINDOWEVENT_TAKE_FOCUS:
				case SDL_WINDOWEVENT_FOCUS_GAINED: G.window_focus = 1;
					break;
				case SDL_WINDOWEVENT_FOCUS_LOST: G.window_focus = 0;
					break;
				default: break;
			}
			break;
#endif
#ifndef _3DS
		case SDL_KEYDOWN:
			if(!SDL_IsTextInputActive()){
				if(e.key.repeat && gui_view!=2) break;
				kbstate[ e.key.keysym.scancode % kbstate_max ] = e.key.timestamp;
				key_delay=-1;
				switch(e.key.keysym.sym){
					#ifdef ANDROID
						case SDLK_AC_BACK: request_quit=1; break;
					#endif
					case SDLK_MENU: syn_pause(G.syn); break;

					case SDLK_KP_ENTER: kbset(SDLK_RETURN, e.key.timestamp); break;

					case SDLK_ESCAPE:
						// if(!request_quit && !save_requested){
						// 	kbset(SDLK_ESCAPE, 0);
						// 	kbset(SDLK_RETURN, 0);
						// 	request_quit =1;
						// }
						break;
						// seq_mode = !seq_mode;
						// syn_anof(G.syn, _isel); isel(0); break;

					case SDLK_F1:  if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq[0], !seq_mute(G.syn->seq[0], -1));} else isel(0); break;
					case SDLK_F2:  if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq[1], !seq_mute(G.syn->seq[1], -1));} else isel(1); break;
					case SDLK_F3:  if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq[2], !seq_mute(G.syn->seq[2], -1));} else isel(2); break;
					case SDLK_F4:  if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq[3], !seq_mute(G.syn->seq[3], -1));} else isel(3); break;
					case SDLK_F5:  if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq[4], !seq_mute(G.syn->seq[4], -1));} else isel(4); break;
					case SDLK_F6:  if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq[5], !seq_mute(G.syn->seq[5], -1));} else isel(5); break;
					case SDLK_F7:  if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq[6], !seq_mute(G.syn->seq[6], -1));} else isel(6); break;
					case SDLK_F8:  if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq[7], !seq_mute(G.syn->seq[7], -1));} else isel(7); break;
					case SDLK_F9:  if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq[8], !seq_mute(G.syn->seq[8], -1));} else isel(8); break;
					case SDLK_F10: if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq[9], !seq_mute(G.syn->seq[9], -1));} else isel(9); break;
					case SDLK_F11: if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq[10], !seq_mute(G.syn->seq[10], -1));} else isel(10); break;
					case SDLK_F12: if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq[11], !seq_mute(G.syn->seq[11], -1));} else isel(11); break;

					case SDLK_PAUSE: syn_pause(G.syn); break;
					case SDLK_CAPSLOCK: rec=!rec; gup_rec=1; break;

					case SDLK_HOME: follow=0; astep(0); break;
					case SDLK_END: follow=0; astep(G.syn->seq[_isel]->len-1); break;

					case SDLK_INSERT: astep(step_sel + step_add); break;

				#if 1
					case 'q': if(!mod) key_update(-9+0 +12, 1); break; //do 5
					case '2': if(!mod) key_update(-9+1 +12, 1); break;
					case 'w': if(!mod) key_update(-9+2 +12, 1); break;
					case '3': if(!mod) key_update(-9+3 +12, 1); break;
					case 'e': if(!mod) key_update(-9+4 +12, 1); break;
					case 'r': if(!mod) key_update(-9+5 +12, 1); break;
					case '5': if(!mod) key_update(-9+6 +12, 1); break;
					case 't': if(!mod) key_update(-9+7 +12, 1); break;
					case '6': if(!mod) key_update(-9+8 +12, 1); break; //lab
					case 'y': if(!mod) key_update(-9+9 +12, 1); break; //la
					case '7': if(!mod) key_update(-9+10+12, 1); break; //sib
					case 'u': if(!mod) key_update(-9+11+12, 1); break; //si
					case 'i': if(!mod) key_update(-9+12+12, 1); break; //do 6
					case '9': if(!mod) key_update(-9+13+12, 1); break;
					case 'o': if(!mod) key_update(-9+14+12, 1); break;
					case '0': if(!mod) key_update(-9+15+12, 1); break;
					case 'p': if(!mod) key_update(-9+16+12, 1); break;
					case '[': if(!mod) key_update(-9+17+12, 1); break;
					case '=': if(!mod) key_update(-9+18+12, 1); break;
					case ']': if(!mod) key_update(-9+19+12, 1); break;

					case 'z': if(!mod) key_update(-9+0, 1); break; //do 4
					case 's': if(!mod) key_update(-9+1, 1); break;
					case 'x': if(!mod) key_update(-9+2, 1); break;
					case 'd': if(!mod) key_update(-9+3, 1); break;
					case 'c': if(!mod) key_update(-9+4, 1); break;
					case 'v': if(!mod) key_update(-9+5, 1); break;
					case 'g': if(!mod) key_update(-9+6, 1); break;
					case 'b': if(!mod) key_update(-9+7, 1); break;
					case 'h': if(!mod) key_update(-9+8, 1); break; //lab
					case 'n': if(!mod) key_update(-9+9, 1); break; //la
					case 'j': if(!mod) key_update(-9+10, 1); break; //sib
					case 'm': if(!mod) key_update(-9+11, 1); break; //si
					case ',': if(!mod) key_update(-9+12, 1); break; //do 5
					case 'l': if(!mod) key_update(-9+13, 1); break;
					case '.': if(!mod) key_update(-9+14, 1); break;
					case ';': if(!mod) key_update(-9+15, 1); break;
					case '/': if(!mod) key_update(-9+16, 1); break;

				#else // ableton style keys
					case 'a':  key_update(e.key.keysym.sym, 1); break; //do 4
					case 'w':  key_update(e.key.keysym.sym, 1); break;
					case 's':  key_update(e.key.keysym.sym, 1); break;
					case 'e':  key_update(e.key.keysym.sym, 1); break;
					case 'd':  key_update(e.key.keysym.sym, 1); break;
					case 'f':  key_update(e.key.keysym.sym, 1); break;
					case 't':  key_update(e.key.keysym.sym, 1); break;
					case 'g':  key_update(e.key.keysym.sym, 1); break;
					case 'y':  key_update(e.key.keysym.sym, 1); break; //lab
					case 'h':  key_update(e.key.keysym.sym, 1); break; //la
					case 'u':  key_update(e.key.keysym.sym, 1); break; //sib
					case 'j':  key_update(e.key.keysym.sym, 1); break; //si
					case 'k':  key_update(e.key.keysym.sym, 1); break; //do 5
					case 'o':  key_update(e.key.keysym.sym, 1); break;
					case 'l':  key_update(e.key.keysym.sym, 1); break;
					case 'p':  key_update(e.key.keysym.sym, 1); break;
					case ';':  key_update(e.key.keysym.sym, 1); break;
					case '\'': key_update(e.key.keysym.sym, 1); break;
				#endif
					// case '[': testvelocity = CLAMP(testvelocity-.5, 0.0, 1.0); break;
					// case ']': testvelocity = CLAMP(testvelocity+.5, 0.0, 1.0); break;

					default: break;
				}
			} else { // text editing
				switch(e.key.keysym.sym){
					case SDLK_KP_ENTER: kbset(SDLK_RETURN, e.key.timestamp); break;
					case SDLK_HOME: song_name_cursor = 0; break;
					case SDLK_END: song_name_cursor = (int)strnlen(song_name, SONG_NAME_MAX-1); break;
					case SDLK_LEFT: song_name_cursor = MAX(song_name_cursor-1, 0); break;
					case SDLK_RIGHT: song_name_cursor = MIN(song_name_cursor+1, (int)strnlen(song_name, SONG_NAME_MAX-1) ); break;
					case SDLK_DELETE:
						memmove(song_name+song_name_cursor, song_name+song_name_cursor+1, strnlen(song_name+song_name_cursor, SONG_NAME_MAX-1-song_name_cursor));
						// song_name_cursor = MIN(song_name_cursor+1, (int)strnlen(song_name, SONG_NAME_MAX-1) );
						break;
					case SDLK_BACKSPACE:
						if(song_name_cursor<=0) break;
						memmove(song_name+song_name_cursor-1, song_name+song_name_cursor, strnlen(song_name+song_name_cursor, SONG_NAME_MAX-1-song_name_cursor));
						song_name[ strnlen(song_name, SONG_NAME_MAX-1)-1 ] =0;
						song_name_cursor = MAX(song_name_cursor-1, 0);
						break;
					default: kbstate[ e.key.keysym.scancode % kbstate_max ] = e.key.timestamp;
						break;
				}
				if(strnlen(song_name, SONG_NAME_MAX-1)==0) song_name[0]=0;
			}
			break;
		case SDL_KEYUP:
			if(e.key.repeat && gui_view!=2) break;
			kbstate[ e.key.keysym.scancode % kbstate_max ] = 0;
			if(!SDL_IsTextInputActive())
			switch(e.key.keysym.sym){
				case SDLK_KP_ENTER: kbset(SDLK_RETURN, 0); break;

#if 1
				case 'q': key_update(-9+0 +12, 0); break; //do 5
				case '2': key_update(-9+1 +12, 0); break;
				case 'w': key_update(-9+2 +12, 0); break;
				case '3': key_update(-9+3 +12, 0); break;
				case 'e': key_update(-9+4 +12, 0); break;
				case 'r': key_update(-9+5 +12, 0); break;
				case '5': key_update(-9+6 +12, 0); break;
				case 't': key_update(-9+7 +12, 0); break;
				case '6': key_update(-9+8 +12, 0); break;
				case 'y': key_update(-9+9 +12, 0); break;
				case '7': key_update(-9+10+12, 0); break;
				case 'u': key_update(-9+11+12, 0); break;
				case 'i': key_update(-9+12+12, 0); break; // do 6
				case '9': key_update(-9+13+12, 0); break;
				case 'o': key_update(-9+14+12, 0); break;
				case '0': key_update(-9+15+12, 0); break;
				case 'p': key_update(-9+16+12, 0); break;
				case '[': key_update(-9+17+12, 0); break;
				case '=': key_update(-9+18+12, 0); break;
				case ']': key_update(-9+19+12, 0); break;

				case 'z': key_update(-9+0, 0); break; //do 4
				case 's': key_update(-9+1, 0); break;
				case 'x': key_update(-9+2, 0); break;
				case 'd': key_update(-9+3, 0); break;
				case 'c': key_update(-9+4, 0); break;
				case 'v': key_update(-9+5, 0); break;
				case 'g': key_update(-9+6, 0); break;
				case 'b': key_update(-9+7, 0); break;
				case 'h': key_update(-9+8, 0); break; //lab
				case 'n': key_update(-9+9, 0); break; //la
				case 'j': key_update(-9+10, 0); break; //sib
				case 'm': key_update(-9+11, 0); break; //si
				case ',': key_update(-9+12, 0); break; //do 5
				case 'l': key_update(-9+13, 0); break;
				case '.': key_update(-9+14, 0); break;
				case ';': key_update(-9+15, 0); break;
				case '/': key_update(-9+16, 0); break;

#else // ableton style keys
				case 'a':  key_update(e.key.keysym.sym, 0); break; //do 4
				case 'w':  key_update(e.key.keysym.sym, 0); break;
				case 's':  key_update(e.key.keysym.sym, 0); break;
				case 'e':  key_update(e.key.keysym.sym, 0); break;
				case 'd':  key_update(e.key.keysym.sym, 0); break;
				case 'f':  key_update(e.key.keysym.sym, 0); break;
				case 't':  key_update(e.key.keysym.sym, 0); break;
				case 'g':  key_update(e.key.keysym.sym, 0); break;
				case 'y':  key_update(e.key.keysym.sym, 0); break;
				case 'h':  key_update(e.key.keysym.sym, 0); break;
				case 'u':  key_update(e.key.keysym.sym, 0); break;
				case 'j':  key_update(e.key.keysym.sym, 0); break;
				case 'k':  key_update(e.key.keysym.sym, 0); break; // do 5
				case 'o':  key_update(e.key.keysym.sym, 0); break;
				case 'l':  key_update(e.key.keysym.sym, 0); break;
				case 'p':  key_update(e.key.keysym.sym, 0); break;
				case ';':  key_update(e.key.keysym.sym, 0); break;
				case '\'': key_update(e.key.keysym.sym, 0); break;
#endif

				default:break;
			}
			break;
#endif // _3DS


#if defined (_3DS) //_3DS
	case SDL_KEYUP:
		if(e.key.keysym.sym==SDLK_RETURN) running=0;
		if(e.key.keysym.sym==SDLK_a) syn_pause(G.syn);
		break;
#endif // _3DS

		case SDL_JOYAXISMOTION:
		case SDL_JOYBUTTONDOWN:
			syn_pause(G.syn); break;
			break;
		case SDL_JOYBUTTONUP:
			break;


	#ifndef _3DS
		// case SDL_FINGERDOWN:
		// 	voices[5][13+12+1]=syn_non(G.syn, 5, 3 -12+12*octave, testvelocity);

		// case SDL_FINGERMOTION:

		// case SDL_FINGERUP:
		// 	syn_nof(G.syn, voices[5][13+12+1]);

		// 	break;
#endif // _3DS

	//mouse
		case SDL_MOUSEMOTION:
			Mouse.px = ((float)e.motion.x)/screen_width * BASE_SCREEN_WIDTH;
			Mouse.py = ((float)e.motion.y)/screen_height * BASE_SCREEN_HEIGHT;
			Mouse.dx = ((float)e.motion.xrel)/screen_width * BASE_SCREEN_WIDTH;
			Mouse.dy = ((float)e.motion.yrel)/screen_height * BASE_SCREEN_HEIGHT;
			Mouse.sdx = e.motion.xrel;
			Mouse.sdy = e.motion.yrel;
			if(mlatch>(float*)100){
				gup_mlatch=1;
				char shift = kbget(SDLK_LSHIFT) || kbget(SDLK_RSHIFT);
				char ctrl  = kbget(SDLK_LCTRL) || kbget(SDLK_RCTRL);
				float lmdx=Mouse.dx;
				float lmdy=Mouse.dy;
				if(ctrl){
					lmdx = lmdx/BASE_SCREEN_WIDTH * screen_width;
					lmdy = lmdy/BASE_SCREEN_HEIGHT * screen_height;
				}
				if(mlatch_v) *mlatch = CLAMP(*mlatch - lmdy * (shift?mlatch_factor/10.0:mlatch_factor), mlatch_min, mlatch_max);
				else         *mlatch = CLAMP(*mlatch + lmdx * (shift?mlatch_factor/10.0:mlatch_factor), mlatch_min, mlatch_max);

				if(Mouse.b1) *mlatch = round(*mlatch);

				if(mlatch_adsr >= 0){
					if(!mlatch_adsr_pitch){
						syn_lock(G.syn, 1);
						switch(mlatch_adsr){
							case 0: adsr_a(G.syn->tone[_isel]->osc_env + mlatch_adsr_osc, *mlatch); break;
							case 1: adsr_d(G.syn->tone[_isel]->osc_env + mlatch_adsr_osc, *mlatch); break;
							case 2: adsr_s(G.syn->tone[_isel]->osc_env + mlatch_adsr_osc, *mlatch); break;
							case 3: adsr_r(G.syn->tone[_isel]->osc_env + mlatch_adsr_osc, *mlatch); break;
							default: break;
						}
						syn_lock(G.syn, 0);
					} else {
						syn_lock(G.syn, 1);
						switch(mlatch_adsr){
							case 0: adsr_a(&G.syn->tone[_isel]->pitch_env, *mlatch); break;
							case 1: adsr_d(&G.syn->tone[_isel]->pitch_env, *mlatch); break;
							case 2: adsr_s(&G.syn->tone[_isel]->pitch_env, *mlatch); break;
							case 3: adsr_r(&G.syn->tone[_isel]->pitch_env, *mlatch); break;
							default: break;
						}
						syn_lock(G.syn, 0);
					}
				}
			} break;

		case SDL_MOUSEBUTTONDOWN:
			switch(e.button.button){
				case SDL_BUTTON_LEFT:   Mouse.b0 = 1; break;
				case SDL_BUTTON_RIGHT:  Mouse.b1 = 1; break;
				case SDL_BUTTON_MIDDLE: Mouse.b2 = 1; break;
				default: break;
			}
			if(mlatch>(float*)100 && Mouse.b1) {*mlatch = round(*mlatch); gup_mlatch=1;}
			break;

		case SDL_MOUSEBUTTONUP:
			switch(e.button.button){
				case SDL_BUTTON_LEFT:   Mouse.b0 = 0; break;
				case SDL_BUTTON_RIGHT:  Mouse.b1 = 0; break;
				case SDL_BUTTON_MIDDLE: Mouse.b2 = 0; break;
				default: break;
			}
			if(vkb_note_active){
				vkb_note_active=0;
				key_update(vkb_note, 0);
			}
			if(!Mouse.b0){
				mlatch=NULL;
				mlatch_adsr=-1;
				mlatch_adsr_pitch=0;
			}
			break;


		case SDL_MOUSEWHEEL:
			Mouse.wx = e.wheel.x;
			Mouse.wy = e.wheel.y;
			Mouse.wtx += e.wheel.x;
			Mouse.wty += e.wheel.y;
		break;



		case SDL_TEXTINPUT:
			/* Add new text onto the end of our text */
			if(strnlen(song_name, SONG_NAME_MAX-1) + strlen(e.text.text) >= SONG_NAME_MAX-1 )
				break;
			// strcat(song_name, e.text.text);
			if(song_name_cursor < (int)strnlen(song_name, SONG_NAME_MAX-1)){
				memmove(song_name+song_name_cursor+strlen(e.text.text), song_name+song_name_cursor, strnlen(song_name+song_name_cursor, SONG_NAME_MAX-1-song_name_cursor));
				for(int i = 0; i < (int)strlen(e.text.text); i++)
					song_name[song_name_cursor+i] = e.text.text[i];
			} else strcat(song_name, e.text.text);
			song_name[SONG_NAME_MAX-1]=0;
			song_name_cursor+=strlen(e.text.text);
			gup_song_name=1;
			break;
		case SDL_TEXTEDITING:
			/*
			Update the composition text.
			Update the cursor position.
			Update the selection length (if any).
			*/
			// composition = event.edit.text;
			song_name_cursor = e.edit.start;
			gup_song_name=1;
			// selection_len = event.edit.length;
		break;

		default: break;
	}
// syn_lock(G.syn, 0);

	return 1;
}

int sdl_event_watcher_3ds(const SDL_Event* e){ return sdl_event_watcher(NULL, (SDL_Event*)e); }























// sg
#include <assert.h>

static int sg_was_init=0;

int sg_rect_count = 0;
int sg_rect_capacity = 256;
sg_rect* sg_rects = NULL;

#ifndef _3DS
	typedef struct{
		SDL_Texture* sdltid;
		int w, h;
	}sg_tex;
#else
	typedef struct{
		C2D_Text* text;
		C2D_Image img;
		int w, h;
	} sg_tex;
#endif

sg_tex* sg_texs;
int sg_tex_count =0;
int sg_tex_capacity = 16; // starting capacity


static int sg_font_size = 12;

#ifndef _3DS
static TTF_Font* sg_font=NULL;
#include "elm.h"
#else
static C2D_Font sg_font;
#include "elbcfnt.h"
#endif

//backend functions
void draw_color( uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void draw_fill_rect( sg_rect );
void draw_rect( sg_rect );
void draw_show( void ); // commit to screen
void draw_clear( void );
sg_tex make_text( char* text );
sg_tex make_tex( rgba8* pixels, int w, int h );
void free_tex( sg_tex* );
void draw_tex( uint16_t tid, int x, int y, float ang, uint8_t r, uint8_t g, uint8_t b, uint8_t a );
void draw_text( uint16_t tid, int x, int y, float ang, uint8_t r, uint8_t g, uint8_t b, uint8_t a );

void sg_init(void){

	if(!sg_was_init){
		sg_was_init=1;
		sg_rects = malloc(sizeof(sg_rect)*256);
		sg_rect_capacity = 256;
		sg_rect_count = 0;
		sg_texs = malloc(sizeof(sg_tex)*16);
		memset(sg_texs, 0, sizeof(sg_tex)*16);
		sg_rect_capacity = 16;
		sg_rect_count = 0;
#ifndef _3DS
		if(!TTF_WasInit()) TTF_Init();
		SDL_RWops* ttffile = SDL_RWFromMem( elm_data, sizeof(elm_data) );
		sg_font = TTF_OpenFontRW(ttffile, 1/*close file when done*/, sg_font_size);
		if(!sg_font) {
			SDL_Log("[FONT]:%s", TTF_GetError());
			exit(1);
#else
		sg_font = C2D_FontLoadFromMem(elbcfnt_data, sizeof(elbcfnt_data));
#endif

		}
	}
}

void sg_quit(void){
	if(sg_was_init){
		free(sg_rects);
		sg_rect_capacity =0;
		sg_rect_count =0;
		for(int i = 0; i<sg_tex_count; i++)
			free_tex(sg_texs+i);
		free(sg_texs);
		sg_tex_count=0;
		sg_tex_capacity=0;
		#ifndef _3DS
				TTF_CloseFont(sg_font);
				TTF_Quit();
		#else
				C2D_FontFree(sg_font);
		#endif
		sg_was_init=0;
	}
}

void sg_clear(void){
	if(!sg_was_init) sg_init();
	sg_rect r;
	r.x=0; r.y=0; r.w=BASE_SCREEN_WIDTH; r.h=BASE_SCREEN_HEIGHT;
	r.r=0; r.g=0; r.b=0; r.a=255;
	sg_drawrect(r);

}

void sg_clear_area(int x, int y, int w, int h){
	sg_rect r;
	r.x=x; r.y=y; r.w=w; r.h=h;
	r.r=0; r.g=0; r.b=0; r.a=255;
	sg_drawrect(r);
}

void sg_show(void){
	if(!sg_was_init) sg_init();

	#ifdef _3DS
	#endif
	char update = 0;
	for(int i =0; i<sg_rect_count; i++){
		update=1;
		sg_rect t = sg_rects[i];
		if(t.tid >= 0){
			draw_text( t.tid, t.x, t.y, t.ang, t.r, t.g, t.b, t.a);
		} else {
			if(t.tid<-1)
				draw_fill_rect( t );
			else
				draw_rect( t );
		}
	}
	if(update) draw_show();
	sg_rect_count = 0;

}


void sg_drawr( sg_rect rect ){
	if(rect.x > BASE_SCREEN_WIDTH || rect.y > BASE_SCREEN_HEIGHT || rect.x+rect.w < 0 || rect.y+rect.h < 0)
		return;
	if(!sg_was_init) sg_init();
	if(sg_rect_count >= sg_rect_capacity){
		sg_rect_capacity*=2;
		sg_rects = realloc(sg_rects, sizeof(sg_rect)*sg_rect_capacity);
	}
	sg_rects[sg_rect_count] = rect;
	sg_rect_count++;
}

void sg_drawrect( sg_rect rect ){
	rect.tid=-2;
	sg_drawr(rect);
}

void sg_drawperim( sg_rect rect ){
	rect.tid=-1;
	sg_drawr(rect);
}

void sg_rcol(sg_rect* s, float r, float g, float b, float a){ assert(s); s->r=r*255; s->g=g*255; s->b=b*255; s->a=a*255;}

void sg_texsize( int16_t tex, int* x, int* y){
	if(!sg_was_init) return;
	assert( tex < sg_tex_count);
	if(x) *x = sg_texs[tex].w;
	if(y) *y = sg_texs[tex].h;

}


int16_t sg_addtex( rgba8* pixels, int w, int h ){
	if(!sg_was_init) sg_init();
	sg_tex t = make_tex( pixels, w, h );
	if(sg_tex_count >= sg_tex_capacity){
		sg_tex_capacity*=2;
		sg_texs = realloc(sg_texs, sizeof(sg_tex)*sg_tex_capacity);
		memset(sg_texs+sg_tex_count, 0, sizeof(sg_tex)*(sg_tex_capacity-sg_tex_count));
	}
	sg_texs[sg_tex_count] = t;
	sg_tex_count++;

	return sg_tex_count-1;
}

int16_t sg_addtext( char* text ){
	if(!sg_was_init) sg_init();
	sg_tex t = make_text( text );
	if(sg_tex_count >= sg_tex_capacity){
		sg_tex_capacity*=2;
		sg_texs = realloc(sg_texs, sizeof(sg_tex)*sg_tex_capacity);
		memset(sg_texs+sg_tex_count, 0, sizeof(sg_tex)*(sg_tex_capacity-sg_tex_count));
	}
	sg_texs[sg_tex_count] = t;
	sg_tex_count++;
	return sg_tex_count-1;
}

int16_t sg_modtext( int16_t tex, char* newtext){ assert(tex < sg_tex_count);
	if(!sg_was_init) sg_init();
	free_tex(sg_texs+tex);
	sg_tex t = make_text( newtext );
	sg_texs[tex] = t;
	return tex;
}

// void sg_modtex( int16_t tex, rgba8* pixels, int x, int y, int w, int h){ assert(tex < sg_tex_count);
// 	if(!sg_was_init) sg_init();
// 	int err=0;
// 	SDL_Rect r={x,y,w,h};
// 	err=SDL_UpdateTexture(sg_texs[tex]->sdltid, &r, pixels, w);
// 	if(err) printf("%s\n", SDL_GetError());
// }

void sg_drawtex( uint16_t tex , int x, int y, float ang, uint8_t r, uint8_t g, uint8_t b, uint8_t a){
	if(x > BASE_SCREEN_WIDTH || y > BASE_SCREEN_HEIGHT || x+sg_texs[tex].w < 0 || y+sg_texs[tex].h < 0)
		return;
	sg_rect t;
	t.x=x; t.y=y;
	t.r=r; t.g=g; t.b=b; t.a=a;
	t.w=sg_texs[tex].w;
	t.h=sg_texs[tex].h;
	t.tid=tex;
	t.ang = ang;
	sg_drawr(t);
}

void sg_drawtext( uint16_t tex , int x, int y, float ang, uint8_t r, uint8_t g, uint8_t b, uint8_t a){
#ifndef _3DS
	sg_drawtex(tex,x,y,ang,r,g,b,a);
#else
#endif
}

void sg_target(SDL_Texture* t){
	G.current_target_tex = t;
	SDL_SetRenderTarget(G.renderer, t);
}




















/*----------------------------------------------------------------------------*/
/*sg backend*/
/*----------------------------------------------------------------------------*/
void draw_show( void ){
	#ifndef _3DS
		SDL_SetRenderTarget(G.renderer, NULL);
		// sg_clear();

		SDL_RenderCopy(G.renderer, G.current_target_tex, NULL, NULL);
		SDL_RenderPresent(G.renderer);
		SDL_SetRenderTarget(G.renderer, G.current_target_tex);

		// SDL_RenderPresent( G.renderer );
	#else
		SDL_Flip( G.screen );
	#endif
}

void draw_color( uint8_t r,uint8_t g,uint8_t b,uint8_t a){
	#ifndef _3DS
		SDL_SetRenderDrawColor(G.renderer, r,g,b,a);
	#else
		G.r=r; G.g=g; G.b=b; G.a=a;
		G.color.r = r;
		G.color.g = g;
		G.color.b = b;
		G.color.a = a;
		Uint32 col = SDL_MapRGBA(G.pixel_format, G.color.r, G.color.g, G.color.b, G.color.a);
		G.color_int = col;
		// G.color_int = 0xFFFF00FF;
	#endif
}

void draw_fill_rect( sg_rect t){
	draw_color( t.r, t.g, t.b , t.a );
	SDL_Rect s;
	s.x = t.x;
	s.y = t.y;
	s.w = t.w;
	s.h = t.h;
	#ifndef _3DS
		SDL_RenderFillRect( G.renderer, &s );
	#else
		SDL_FillRect( G.screen, &s, G.color_int);
	#endif
}

void draw_rect( sg_rect t){
	draw_color( t.r, t.g, t.b , t.a );
	SDL_Rect s;
	s.x = t.x;
	s.y = t.y;
	s.w = t.w;
	s.h = t.h;
	#ifndef _3DS
		SDL_RenderDrawRect( G.renderer, &s );
	#else
		SDL_Rect t = {s.x, s.y+s.h, s.w, 1};
		SDL_Rect b = {s.x, s.y, s.w, 1};
		SDL_Rect l = {s.x, s.y, 1, s.h};
		SDL_Rect r = {s.x+s.w, s.y, 1, s.h};
		SDL_FillRect( G.screen, &t, G.color_int);
		SDL_FillRect( G.screen, &b, G.color_int);
		SDL_FillRect( G.screen, &l, G.color_int);
		SDL_FillRect( G.screen, &r, G.color_int);
	#endif
}

void draw_tex( uint16_t tid, int x, int y, float ang, uint8_t r, uint8_t g, uint8_t b, uint8_t a ){
	#ifndef _3DS
		SDL_Rect dst = {x, y, sg_texs[tid].w, sg_texs[tid].h};
		SDL_SetTextureColorMod( sg_texs[tid].sdltid, r,g,b );
		SDL_SetTextureAlphaMod( sg_texs[tid].sdltid, a );
		// SDL_Point center = {floor((float)sg_texs[tid].w)/2.0, floor((float)sg_texs[tid].h)/2.0};
		// SDL_RenderCopyEx(G.renderer, sg_texs[tid].sdltid, NULL, &dst, ang, &center, 0);
		SDL_RenderCopyEx(G.renderer, sg_texs[tid].sdltid, NULL, &dst, ang, NULL, 0);
	#else
	#endif
}

void draw_text( uint16_t tid, int x, int y, float ang, uint8_t r, uint8_t g, uint8_t b, uint8_t a ){
	#ifndef _3DS
		draw_tex(tid,x,y,ang,r,g,b,a);
	#else
	#endif
}


void draw_clear( void ){
	#ifndef _3DS
		SDL_RenderClear(G.renderer);
	#else
		// SDL_Rect rect = {0,0,400,240};
		// draw_fill_rect(0);
		SDL_FillRect( G.screen, NULL, G.color_int);

	#endif
}



sg_tex make_tex( rgba8* pixels, int w, int h ){
	#ifndef _3DS
		SDL_Texture* tex = SDL_CreateTexture(G.renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, w, h);
		SDL_SetTextureBlendMode( tex, SDL_BLENDMODE_BLEND);
		if(pixels) SDL_UpdateTexture(tex, NULL, pixels, w*sizeof(rgba8));
		sg_tex ret;
		ret.sdltid = tex;
		ret.w = w;
		ret.h = h;
	#else
	#endif
	return ret;
}

sg_tex make_text( char* text ){
	#ifndef _3DS
		SDL_Color textcol={255,255,255,255};
		SDL_Surface* sur= TTF_RenderText_Solid(sg_font, text, textcol);
		SDL_Texture* tex = SDL_CreateTextureFromSurface(G.renderer, sur);
		if (tex == NULL) {
			fprintf(stderr, "CreateTextureFromSurface failed: %s\n", SDL_GetError());
		}
		sg_tex ret;
		ret.sdltid = tex;
		ret.w = sur->w;
		ret.h = sur->h;
		SDL_FreeSurface(sur);
	#else
	#endif
	return ret;
}

void free_tex( sg_tex* t ){ assert(t);
	#ifndef _3DS
		if(t->sdltid)
			SDL_DestroyTexture(t->sdltid);
		t->sdltid = NULL;
	#else
	#endif
}
/*----------------------------------------------------------------------------*/
/*sg backend*/
/*----------------------------------------------------------------------------*/











char ptbox( int x, int y, sg_rect r){
	if(
		x>r.x &&
		x<r.x+r.w &&
		y>r.y &&
		y<r.y+r.h
	){
		return 1;
	} else {
		return 0;
	}
}
char ptboxe( int x, int y, sg_rect r){
	if(
		x>=r.x &&
		x<=r.x+r.w &&
		y>=r.y &&
		y<=r.y+r.h
	){
		return 1;
	} else {
		return 0;
	}
}

int isel(int i){
	// if(!voices_inited)init_voices();
	char ret = _isel;
	if(i>=0 && i<SYN_TONES) {
		_isel = i;
		astep(step_sel);
		gup_seq = 1;
		gup_spb = 1;
		gup_osc = 1;
		gup_rec = 1;
		gup_shown_notes = 1;
		gup_note_grid=1;
		gup_step_bar=1;
		mlatch = NULL;
		ipeaks[i]=0;

		for(int i=0; i<MAX_KEYS; i++){
			syn_nof(G.syn, voices[i]);
			voices[i] = (noteid){-1,-1};
		}
	}
	return ret;
}

void astep(int step){
	step = step % G.syn->seq[_isel]->len;
	if(step<0) step = G.syn->seq[_isel]->len-1;
	int prev_step = step_sel;
	step_sel = step;
	// check and update mlatch
	if(mlatch > (float*)100 && G.syn->seq[_isel]->modm[prev_step]!=NULL){
		if(mlatch >= (float*)G.syn->seq[_isel]->modm[prev_step] &&
				mlatch <= syn_modm_addr(G.syn->seq[_isel]->modm[prev_step], OSC_PER_TONE-1, OSC_PER_TONE-1 )){
			if(G.syn->seq[_isel]->modm[step_sel] == NULL){
				seq_modm(G.syn->seq[_isel], G.syn->seq[_isel]->modm[prev_step], step_sel);
				gup_seq=1;
			}
			float prev_val = *mlatch;
			mlatch = (float*) ((intptr_t)mlatch - (intptr_t)syn_modm_addr(G.syn->seq[_isel]->modm[prev_step], 0,0));
			mlatch = (float*) ((intptr_t)mlatch + (intptr_t)syn_modm_addr(G.syn->seq[_isel]->modm[step_sel], 0,0));
			*mlatch = prev_val;
		}
	}
}




int voice_count=0;
float note_commit[POLYPHONY];
int commit_count=0;
int commit_step_begin=0;
int commit_step_end=0;

void key_update(int key, char on){
	if(!rec){
		voice_count=0;
		commit_count=0;
		for(int i=0; i<POLYPHONY; i++){
			note_commit[i]=SEQ_MIN_NOTE;
		}
		commit_step_begin=0;
		commit_step_end=0;
	}
	int keyi = MAPVAL((float)key+12*(octave-4), C0, C0+12*MAX_OCTAVE, 0, 12*MAX_OCTAVE);

	if( keyi >= 12*MAX_OCTAVE ) return;
	float target_note=0;

	if(on && voices[keyi].voice != -1){
		return;
	}

	syn_lock(G.syn, 1);

	if(on){
		target_note = key+12*(octave-4);
		voices[keyi] = syn_non(G.syn, _isel, key +12*(octave-4), testvelocity);
	} else {
		if(rec) voice_count=MAX(voice_count-1, 0);
		syn_nof(G.syn, voices[keyi] );
		voices[keyi] = (noteid){-1,-1};
	}

	if(on && voice_count<POLYPHONY && rec) {
		if(commit_count == 0) commit_step_begin=step_sel;
		note_commit[commit_count] = target_note;
		voice_count++;
		commit_count++;
	}
	//commit
	if(!on && voice_count==0 && commit_count>0 && rec){
		commit_step_end = step_sel;
		if(commit_step_begin != commit_step_end){
			int tmp=MIN(commit_step_begin, commit_step_end);
			commit_step_end=MAX(commit_step_end, commit_step_begin);
			commit_step_begin=MIN(commit_step_begin, tmp);
			for(int s=commit_step_begin; s<=commit_step_end; s++){
				seq_anof(G.syn->seq[_isel], s);
				note_cols[_isel][s]=0;

				for(int i=0; i<commit_count; i++){
					char err = seq_non(G.syn->seq[_isel], s, note_commit[i], testvelocity, s<commit_step_end? 1.0 : .5);
					if(!err) note_cols[_isel][s]++;
				}
			}
		} else {
			seq_anof(G.syn->seq[_isel], step_sel);
			note_cols[_isel][step_sel]=0;
			for(int i=0; i<commit_count; i++){
				char err = seq_non(G.syn->seq[_isel], step_sel, note_commit[i], testvelocity, .5);
				if(!err) note_cols[_isel][step_sel]++;
			}
		}
		note_cols[_isel][step_sel] = CLAMP(note_cols[_isel][step_sel], 0, POLYPHONY);
		astep(step_sel + step_add);
		commit_count=0;
		gup_seq=1;
		gup_shown_notes=1;
		gup_step_bar=1;
	}

	syn_lock(G.syn, 0);

}

int kb_octave(int o){
	int prev_oct = octave;
	if(o>=0){
		for(int i=0; i<MAX_KEYS;i++){
			if(voices[i].voice>=0) syn_nof(G.syn, voices[i]);
			voices[i] = (noteid){-1, -1};
		}

		octave=o;
		if(octave==0)
			note_scrollv=0;
		else {
			note_scrollv %= 12;
			note_scrollv += octave*12;
			note_scrollv = CLAMP(note_scrollv, 0, (MAX_OCTAVE-1)*12-1);
		}
		octave=CLAMP(octave, 0, MAX_OCTAVE-2);

		gup_shown_notes=1;
		gup_note_grid=1;
	}
	return prev_oct;
}

















void gui_toneview(void){
/*----------------------------------------------------------------------------*/
/* GUI */
/*----------------------------------------------------------------------------*/
	// isel
	int giselendx = giselw*SYN_TONES + (giselw/8)*MAX(SYN_TONES/4-1,1);
	sg_clear_area(0,0, giselendx , giselh);
	int isel_padding=0;
	for(int i=0; i<SYN_TONES; i++){
		if((i!=0) && ((i%4)==0)) isel_padding+=giselw/8;
		sg_rect r;
		r.x=isel_padding;
		r.y=0;
		r.w=giselw;
		r.h=giselh;
		if(!save_requested && (!mlatch || (mlatch==phony_latch_isel)) && Mouse.b0 && ptboxe(Mouse.px, Mouse.py, r))
			{isel(i); mlatch=phony_latch_isel;}

		r.g=(_isel==i) * 255;
		if(ipeaks[i]){
			r.r=255;
		} else {
			r.r=seq_mute(G.syn->seq[i], -1)? 55: 55-r.g;
		}
		r.b=seq_mute(G.syn->seq[i], -1)? 55: 255;
		r.a=255;
		isel_padding+=giselw;
		sg_drawperim(r);
		// VUmeter
		r.x++;
		r.w=r.w/2-2;
		r.y=giselh - (G.syn->tone[i]->vupeakl * giselh)-1;
		r.h=giselh - r.y-1;
		sg_rcol(&r, MIN(G.syn->tone[i]->vupeakl, 1.0), MAX(1-G.syn->tone[i]->vupeakl, 0),0,1);
		sg_drawrect(r);
		r.x=r.x+r.w+2;
		r.y=giselh - (G.syn->tone[i]->vupeakr * giselh)-1;
		r.h=giselh - r.y-1;
		sg_rcol(&r, MIN(G.syn->tone[i]->vupeakr, 1.0), MAX(1-G.syn->tone[i]->vupeakr, 0),0,1);
		sg_drawrect(r);
	}


	int gbpmw = 76;
	// bpm mouse
	if( (!mlatch || (mlatch == phony_latch_bpm)) && Mouse.b0){
		// sg_rect r = {giselw*(SYN_TONES+SYN_TONES/4)+4, 0, 70, 16, 0,0,0,0, 0,0};
		sg_rect r = {giselendx+5, 0, gbpmw, 16, 0,0,0,0, 0,0};
		if((mlatch == phony_latch_bpm) || ptboxe(Mouse.px, Mouse.py, r)){

			int shift = kbget(SDLK_LSHIFT) || kbget(SDLK_RSHIFT);
			float bpm = syn_bpm(G.syn, -1);
			syn_bpm(G.syn, bpm + ((float)Mouse.dx)/( shift? 100.0 : 1.0 ));
			gup_bpm=1;
			mlatch = phony_latch_bpm;

			if(Mouse.b1)
				syn_bpm(G.syn, round(bpm) );
		}
	}
	// bpm
	if(gup_bpm){
		gup_bpm = 0;
		snprintf( bpm_text, 8+4+2, "bpm:%3.3f", syn_bpm(G.syn, -1));
		sg_modtext( bpm_tex, bpm_text);
		// sg_clear_area( giselw*(SYN_TONES+SYN_TONES/4)+4, 0, 76, 8);
		sg_clear_area( giselendx+5, 0, gbpmw, 8);
		// sg_drawtex( bpm_tex, giselw*(SYN_TONES+SYN_TONES/8)+4, 0, 0, 255, 255, 255, 255);
		sg_drawtex( bpm_tex, giselendx+5, 0, 0, 255, 255, 255, 255);
	}
	{ // bpm light
		// sg_rect r = {giselw*(SYN_TONES+SYN_TONES/4)+1, 1, 2, 6, beat!=pbeat?255:50, 50+(tap_tempo>0? 200 : 0), 50, 255, 0,0};
		sg_rect r = {giselendx+1, 1, 3, 6, beat!=pbeat?255:50, 50+(tap_tempo>0? 200 : 0), 50, 255, 0,0};
		sg_drawrect( r );
	}
	// pattern step add
	if(gup_step_add){
		gup_step_add = 0;
		snprintf( step_add_text, 8+4, "add:%i", step_add);
		sg_modtext( step_add_tex, step_add_text);
		sg_clear_area( giselendx+5 +gbpmw, 8, 64, 8);
		sg_drawtex( step_add_tex, giselendx+5+gbpmw, 8, 0, 255, 155, 155, 255);
	}
	// pattern steps per beat
	if(gup_spb){
		gup_spb = 0;
		snprintf( spb_text, 8+4, "spb:%i", G.syn->seq[_isel]->spb);
		sg_modtext( spb_tex, spb_text);
		sg_clear_area( giselendx+5+gbpmw, 0, 40, 8);
		sg_drawtex( spb_tex, giselendx+5+gbpmw, 0, 0, 255, 255, 255, 255);
	}

	// latch val
	if(gup_mlatch){
		if(mlatch>(float*)100){
			gup_mlatch=0;
			snprintf( mlatch_text, 8+4, "val:%3.3f", *mlatch);
			sg_modtext( mlatch_tex, mlatch_text);
			sg_clear_area( giselendx+5, 8, gbpmw, 8);
		}
		sg_drawtex( mlatch_tex, giselendx+5, 8, 0, 155, 255, 155, 255);
	}

	// seq
	if(gup_seq){
		gup_seq=0;
		float hlen = (float)(BASE_SCREEN_WIDTH)/SEQ_LEN;
		float vlen = (float)(gseqh)/(POLYPHONY+1);
		sg_clear_area(0, gseq_basey, BASE_SCREEN_WIDTH, (POLYPHONY+1)*vlen);
		for(int k=0; k<G.syn->seq[_isel]->len; k++){
			char is_beatstep = k % G.syn->seq[_isel]->spb;
			for(int j=0; j<POLYPHONY+1; j++){
				fillRect.x=k*hlen;
				fillRect.y=j*vlen + gseq_basey;
				fillRect.w=hlen+1;
				fillRect.h=vlen+1;
				sg_rcol(&fillRect, is_beatstep ? 0 : .5 ,is_beatstep ? 0 : .5 , 1, 1);
				sg_drawperim( fillRect );
				if(j==POLYPHONY){ // modulation matrix on bottom step
					fillRect.x+=1;
					fillRect.y+=1;
					fillRect.w-=2;
					fillRect.h-=2;
					if(G.syn->seq[_isel]->modm[k]){
						sg_rcol(&fillRect, 1,0,0, 1);
						sg_drawrect( fillRect );
					}
				}
				else if(G.syn->seq[_isel]->note[j][k] >SEQ_MIN_NOTE){
					fillRect.x+=1;
					fillRect.y+=1;
					fillRect.w= MAX((hlen)*G.syn->seq[_isel]->dur[j][k]/255.f, 2);
					fillRect.w-=1;
					fillRect.h-=2;
					sg_rcol(&fillRect, 1,1,0, 1);
					sg_drawrect( fillRect );
				}
			}
		}
	}
	//seq mouse selection
	if(!save_requested && (!mlatch || (mlatch==phony_latch_seq)) && Mouse.b0){
		float vlen = (float)(gseqh)/(POLYPHONY+1);
		sg_rect r = {0, gseq_basey, BASE_SCREEN_WIDTH, (POLYPHONY+1)*vlen+8, 0,0,0,0,0,0};
		if(ptbox( Mouse.px, Mouse.py, r)){
			if(Mouse.px < G.syn->seq[_isel]->len * ((float)(BASE_SCREEN_WIDTH)/SEQ_LEN)){
				astep(Mouse.px/((float)(BASE_SCREEN_WIDTH)/SEQ_LEN));
				mlatch = phony_latch_seq;
			}
		}
	}


	//active step
	int step=G.syn->seq[_isel]->step;
	if(follow) astep(step);

	sg_clear_area(0, gseq_basey+gseqh, BASE_SCREEN_WIDTH, 8);
	sg_drawtex(seq_step_tex, (float)(BASE_SCREEN_WIDTH)/SEQ_LEN*step+1, gseq_basey+gseqh, 0, 255,255,255,255);
	sg_drawtex(seq_step_tex, (float)(BASE_SCREEN_WIDTH)/SEQ_LEN*step_sel+1, gseq_basey+gseqh+4, 0, 255,follow?255:0,0,255);


	// osc
	int tw=0,th=0;
	sg_texsize(wave_tex[0], &tw, &th);
	th=MAX(th, knob_size+1);
	if(Mouse.b0 && !mlatch){
		for(int i = 0; i<OSC_PER_TONE; i++){
			sg_rect r = {0, gosc_basey+i*th, tw, th, 0,0,0,0, 0,0};
			if( ptboxe(Mouse.px, Mouse.py, r) ){
				Mouse.b0=0;
				if(Mouse.px < tw/2) {G.syn->tone[_isel]->osc[i] = MAX(G.syn->tone[_isel]->osc[i]-1, 0); gup_osc=1;}
				if(Mouse.px > tw/2) {G.syn->tone[_isel]->osc[i] = MIN(G.syn->tone[_isel]->osc[i]+1, OSC_MAX-1); gup_osc=1;}
			}
		}
	}
	if(gup_osc){
		gup_osc=0;
		sg_clear_area(0, gosc_basey, tw, gosch*OSC_PER_TONE);
		for(int i =0; i<OSC_PER_TONE; i++){
			sg_drawtext(wave_tex[G.syn->tone[_isel]->osc[i]], 0, gosc_basey+i*th, 0,   255,0,0,255);
		}
	}
	int toct_basex = tw;
	{ // tone oct
		for(int i=0; i<OSC_PER_TONE; i++){
			sg_rect r={toct_basex , i*(knob_size+1)+gosc_basey-2, knob_size+1, knob_size+1, 0,0,0,0, 0,0};
			if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
				mlatch = &G.syn->tone[_isel]->oct[i];
				mlatch_min=-10;
				mlatch_max=10.0;
				mlatch_factor=0.1;
				mlatch_v=1;
				gup_mlatch=1;
			}
			sg_drawtex(knob_tex, r.x, r.y, MAPVAL(G.syn->tone[_isel]->oct[i], -10, 10, 0, 360), 255,255,155,255);
		}
	}
	// modm
	int modm_basex = toct_basex+knob_size;
	for(int i=0; i<OSC_PER_TONE; i++){
		for(int j=0; j<OSC_PER_TONE; j++){
			if(j>i)break;
			sg_rect r={j*(knob_size+1)+modm_basex, i*(knob_size+1)+gosc_basey-2, knob_size+1, knob_size+1, 0,0,0,0, 0,0};
			if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
				mlatch = syn_modm_addr( &(G.syn->tone[_isel]->mod_mat), i, j);
				mlatch_min= j==i ? 1 : 0;
				mlatch_max= j==i ? 30 : 90;
				mlatch_factor = 1.0;
				mlatch_v=1;
				gup_mlatch=1;
			}
			if(j==i){
				sg_drawtex(knob_tex, r.x, r.y, (tone_frat(G.syn->tone[_isel], j, -1)-1)*3*3.9f, 255,155,155,255);
			} else {
				sg_drawtex(knob_tex, r.x, r.y, tone_index(G.syn->tone[_isel], i, j, -1)*3.9f, 255,255,255,255);
			}
		}
	}
	// omix
	for(int i=0; i<OSC_PER_TONE; i++){
		sg_rect r={modm_basex+(knob_size+1)*OSC_PER_TONE, i*(knob_size+1)+gosc_basey-2, knob_size+1, knob_size+1, 0,0,0,0, 0,0};
		if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
			mlatch = syn_modm_addr( &(G.syn->tone[_isel]->mod_mat), i, -1);
			mlatch_min=0;
			mlatch_max=1.0;
			mlatch_factor=0.01;
			mlatch_v=1;
			gup_mlatch=1;
		}
		sg_drawtex(knob_tex, r.x, r.y, tone_omix(G.syn->tone[_isel], i, -1)*300, 155,155,255,255);
	}
	// envelopes
	int ampenv_basex = modm_basex + (knob_size+1)*OSC_PER_TONE+knob_size+1 +knob_size/4;
	for(int i=0; i<OSC_PER_TONE; i++){
		for(int j=0; j<4; j++){
			sg_rect r={j*(knob_size+1)+ampenv_basex, i*(knob_size+1)+gosc_basey-2, knob_size+1, knob_size+1, 0,0,0,0, 0,0};
			if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
				switch(j){
					case 0: mlatch = &(G.syn->tone[_isel]->osc_env[i].a); mlatch_adsr=j; mlatch_adsr_osc=i; break;
					case 1: mlatch = &(G.syn->tone[_isel]->osc_env[i].d); mlatch_adsr=j; mlatch_adsr_osc=i; break;
					case 2: mlatch = &(G.syn->tone[_isel]->osc_env[i].s); mlatch_adsr=j; mlatch_adsr_osc=i; break;
					case 3: mlatch = &(G.syn->tone[_isel]->osc_env[i].r); mlatch_adsr=j; mlatch_adsr_osc=i; break;
				}
				mlatch_min= j==2? 0.0: 0.0002;
				mlatch_max= j==2? 1.0: j==3? 20.0: 10.0;
				mlatch_factor= j==2? 0.01 : 0.1;
				mlatch_v=1;
			}
			switch(j){
				case 0: sg_drawtex(knob_tex, r.x, r.y, G.syn->tone[_isel]->osc_env[i].a * 310/10 , 255,155,155,255); break;
				case 1: sg_drawtex(knob_tex, r.x, r.y, G.syn->tone[_isel]->osc_env[i].d * 310/10 , 255,255,255,255); break;
				case 2: sg_drawtex(knob_tex, r.x, r.y, G.syn->tone[_isel]->osc_env[i].s * 310   , 155,155,255,255); break;
				case 3: sg_drawtex(knob_tex, r.x, r.y, G.syn->tone[_isel]->osc_env[i].r * 310/20, 155,255,155,255); break;
			}
		}
	}
	// modm target
	int modm_target_basex = ampenv_basex + 4*(knob_size+1) + knob_size/4;
	for(int i=0; i<OSC_PER_TONE; i++){
		for(int j=0; j<OSC_PER_TONE; j++){
			if(j>i)break;
			sg_rect r={j*(knob_size+1)+modm_target_basex, i*(knob_size+1)+gosc_basey-2, knob_size+1, knob_size+1, 0,0,0,0, 0,0};
			if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
				if(G.syn->seq[_isel]->modm[step_sel]==NULL){
					seq_modm(G.syn->seq[_isel], &(G.syn->tone[_isel]->mod_mat), step_sel);
					gup_seq=1;
				}
				mlatch = syn_modm_addr( G.syn->seq[_isel]->modm[step_sel], i, j );
				mlatch_min= j==i ? 1 : 0;
				mlatch_max= j==i ? 30 : 90;
				mlatch_factor = 1.0;
				mlatch_v=1;
				gup_mlatch=1;
			}
			if(j==i){
				if(G.syn->seq[_isel]->modm[step_sel]==NULL)
					sg_drawtex(knob_tex, r.x, r.y, (tone_frat(G.syn->tone[_isel], i, -1)-1)*3*3.9f, 55,55,55,255);
				else
					sg_drawtex(knob_tex, r.x, r.y, ((*syn_modm_addr(G.syn->seq[_isel]->modm[step_sel], i, i))-1)*3*3.9, 255,155,155,255);
			} else {
				if(G.syn->seq[_isel]->modm[step_sel]==NULL)
					sg_drawtex(knob_tex, r.x, r.y, tone_index(G.syn->tone[_isel], i, j, -1)*3.9f, 55,55,55,255);
				else
					sg_drawtex(knob_tex, r.x, r.y, ((*syn_modm_addr(G.syn->seq[_isel]->modm[step_sel], i, j))-1)*3.9, 255,255,255,255);
			}
		}
	}
	// omix target
	for(int i=0; i<OSC_PER_TONE; i++){
		sg_rect r={modm_target_basex+(knob_size+1)*OSC_PER_TONE, i*(knob_size+1)+gosc_basey-2, knob_size+1, knob_size+1, 0,0,0,0, 0,0};
		if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
			if(G.syn->seq[_isel]->modm[step_sel]==NULL){
				seq_modm(G.syn->seq[_isel], &(G.syn->tone[_isel]->mod_mat), step_sel);
				gup_seq=1;
			}
			mlatch = syn_modm_addr( G.syn->seq[_isel]->modm[step_sel], i, -1 );

			mlatch_min=0;
			mlatch_max=1.0;
			mlatch_factor=0.01;
			mlatch_v=1;
			gup_mlatch=1;
		}
		if(G.syn->seq[_isel]->modm[step_sel]==NULL)
			sg_drawtex(knob_tex, r.x, r.y, tone_omix(G.syn->tone[_isel], i, -1)*300, 55,55,55,255);
		else
			sg_drawtex(knob_tex, r.x, r.y, *syn_modm_addr(G.syn->seq[_isel]->modm[step_sel], i, -1)*300, 155,155,255,255);
	}
	// tone gain
	int gain_basex = modm_target_basex + (knob_size+1)*(OSC_PER_TONE)+knob_size+1 +knob_size/2;
	{
		sg_rect r={gain_basex, gosc_basey-2, knob_size+1, knob_size+1, 0,0,0,0, 0,0};
		if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
			mlatch = &G.syn->tone[_isel]->gain;
			mlatch_min=0;
			mlatch_max=1;
			mlatch_factor=0.01;
			mlatch_v=1;
			gup_mlatch=1;
		}
		sg_drawtex(knob_tex, r.x, r.y, G.syn->tone[_isel]->gain*300, 255,5,5,255);
	}

	int penv_adsr_basex = gain_basex;
	for(int j=0; j<4; j++){
		// if(j>i)break;
		sg_rect r={penv_adsr_basex, (j+1)*(knob_size+1)+gosc_basey-2, knob_size+1, knob_size+1, 0,0,0,0, 0,0};
		if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
			switch(j){
				case 0: mlatch = &(G.syn->tone[_isel]->pitch_env.a); mlatch_adsr=j; break;
				case 1: mlatch = &(G.syn->tone[_isel]->pitch_env.d); mlatch_adsr=j; break;
				case 2: mlatch = &(G.syn->tone[_isel]->pitch_env.s); mlatch_adsr=j; break;
				case 3: mlatch = &(G.syn->tone[_isel]->pitch_env.r); mlatch_adsr=j; break;
			}
			mlatch_min=0.001;
			mlatch_max= j==2? 1.0 : j==3? 10.0 : 3.0;
			mlatch_factor= j==3? .1 : j==2? 0.01 : 0.05;
			mlatch_v=1;
			mlatch_adsr_pitch=1;
		}
		switch(j){
			case 0: sg_drawtex(knob_tex, r.x, r.y, G.syn->tone[_isel]->pitch_env.a * 310/3 , 255,155,155,255); break;
			case 1: sg_drawtex(knob_tex, r.x, r.y, G.syn->tone[_isel]->pitch_env.d * 310/3 , 255,255,255,255); break;
			case 2: sg_drawtex(knob_tex, r.x, r.y, G.syn->tone[_isel]->pitch_env.s * 310   , 155,155,255,255); break;
			case 3: sg_drawtex(knob_tex, r.x, r.y, G.syn->tone[_isel]->pitch_env.r * 310/10, 155,255,155,255); break;
		}
	}
	// pitch env amt
	{
		sg_rect r={penv_adsr_basex, (5)*(knob_size+1)+gosc_basey-2, knob_size+1, knob_size+1, 0,0,0,0, 0,0};
		if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
			mlatch=&G.syn->tone[_isel]->pitch_env_amt;
			mlatch_min=0.0;
			mlatch_max= 15;
			mlatch_factor= 0.05;
			mlatch_v=1;
			mlatch_adsr_pitch=1;
		}
		sg_drawtex(knob_tex, r.x, r.y, G.syn->tone[_isel]->pitch_env_amt * 310/15, 255,255,155,255);
	}
	// virtual keyboard
	int vkb_h = 32;
	int vkb_basey = BASE_SCREEN_HEIGHT-vkb_h;
	int vkb_keys = 12*2+1;
	int vkb_endx = 0;
	if(gup_vkb || Mouse.py<= vkb_basey){
		sg_rect r = {0, vkb_basey, 25*(((float)BASE_SCREEN_WIDTH)/(vkb_keys)-1), vkb_h, 255, 255,255,255, 0,0};
		if(gup_vkb) sg_clear_area(r.x, r.y, r.w, r.h);
		r.w=((float)BASE_SCREEN_WIDTH)/vkb_keys;
		// r.x+=2;
		for(int o = 0 ; o < 3; o++ ){
			int notei=0;
			for(int i = 0 ; i <= 13; i++){
				if(o>=2 && (i>=1 || octave==MAX_OCTAVE-2)) break;

				char black = i%2;
				// r.h = black? vkb_h/2 : vkb_h;
				r.r = black? 55 : 255;
				r.g = black? 55 : 255;
				r.b = black? 55 : 255;

				if(i!=0 && (i==5 || i==13)) continue;
				char collision = (!mlatch || mlatch==phony_latch_vkb) && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r);
				// if(collision || voices[((-9+notei+o*12)-C0)%MAX_KEYS].tone != -1 ) r.b=0;
				if(collision || voices[((notei+o*12+octave*12))%MAX_KEYS].tone != -1 ) r.b=0;

				if(collision && (!vkb_note_active || vkb_note!=-9+notei+o*12)) {
					mlatch = phony_latch_vkb;
					if(vkb_note!=-9+notei+o*12){
						key_update(vkb_note, 0);
					}
					key_update(-9+notei+o*12, 1);
					vkb_note = -9+notei+o*12;
					vkb_note_active =1;
				}
				notei++;

				if(gup_vkb) sg_drawrect(r);
				r.r = 0;
				r.g = 0;
				r.b = 0;
				if(gup_vkb) sg_drawperim(r);
				if(gup_vkb && i==0) sg_drawtex( Cn_tex[octave+o], r.x, BASE_SCREEN_HEIGHT-10, 0, 50,50,50,255 );
				r.x += ((float)BASE_SCREEN_WIDTH)/(vkb_keys)-1;
			}
		}
		vkb_endx = r.x;
	}


	// play/rec button
	{
		int sx=0,sy=0;
		sg_texsize(gplay_tex, &sx,&sy);
		sg_rect r = {2, vkb_basey-sy, sx, sy, 0,0,0,0, 0,0};
		if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
			Mouse.b0 = 0;
			syn_pause(G.syn);
		}
		char p = G.syn->seq_play;
		sg_drawtex(gplay_tex, r.x, r.y, 0, p?255:55, 55, 55, 255);

		//rec
		int rsx=0,rsy=0;
		sg_texsize(rec_tex, &rsx,&rsy);
		if(gup_rec){
			gup_rec=0;
			sg_drawtex( rec_tex, 2+sx, vkb_basey-rsy, 0, 255*rec, 55, 55, 255);
		}

		sg_rect rrec={2+sx, vkb_basey-rsy, rsx, rsy, 0,0,0,0, 0,0};
		sg_drawperim(rrec);
		if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, rrec)){
			Mouse.b0=0;
			gup_rec=1;
			rec=!rec;
		}
	}

	//master vumeter and gain
	{
		sg_rect r;
		r.x=BASE_SCREEN_WIDTH-giselw;
		r.y=BASE_SCREEN_HEIGHT-giselh;
		r.w=giselw;
		r.h=giselh;
		r.a=255;
		sg_clear_area(r.x, r.y, r.w , r.h);
		if(G.syn->vupeakl>.99 || G.syn->vupeakr>.99){
			r.r=255;
			sg_drawrect(r);
		}
		else
			r.r=0;
		r.g=55;
		r.b=55;
		sg_drawperim(r);
		// VUmeter
		r.x++;
		r.w=r.w/2-2;
		r.y=MAX( BASE_SCREEN_HEIGHT - (G.syn->vupeakl * giselh)-1 , BASE_SCREEN_HEIGHT-giselh);
		r.h=BASE_SCREEN_HEIGHT - r.y-1;
		sg_rcol(&r, MIN(G.syn->vupeakl, 1.0), MAX(1-G.syn->vupeakl, 0),0,1);
		sg_drawrect(r);
		r.x=r.x+r.w+2;
		r.y=MAX( BASE_SCREEN_HEIGHT - (G.syn->vupeakr * giselh)-1 , BASE_SCREEN_HEIGHT-giselh);
		r.h=BASE_SCREEN_HEIGHT - r.y-1;
		sg_rcol(&r, MIN(G.syn->vupeakr, 1.0), MAX(1-G.syn->vupeakr, 0),0,1);
		sg_drawrect(r);
		// main gain knob
		r.x=BASE_SCREEN_WIDTH-giselw+2;
		r.y=BASE_SCREEN_HEIGHT-giselh-knob_size-2;
		r.w=giselw;
		r.h=giselh;
		sg_drawtex(knob_tex, r.x, r.y, G.syn->gain*300 ,r.r,r.g,r.b, 255);
		if(Mouse.b0 && !mlatch && ptbox(Mouse.px, Mouse.py, r)){
			mlatch=&G.syn->gain;
			mlatch_min=0;
			mlatch_max=1.0;
			mlatch_factor=0.1;
			mlatch_v=1;
			gup_mlatch=1;
		}
	}

	//song
	{ // mouse over song area
		sg_rect r = {0, gosc_basey+(knob_size+1)*(OSC_PER_TONE+1)-4, BASE_SCREEN_WIDTH, (8+1)*3+1+9, 255,55,255,255, 0,0};
		// sg_drawperim(r);
		if(Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)) gup_song=1;
	}
	if(gup_song){
		gup_song=0;
		sg_rect r = {0, gosc_basey+(knob_size+1)*(OSC_PER_TONE+1), BASE_SCREEN_WIDTH, (8+1)*3+1, 55,55,55,255, 0,0};
		sg_clear_area(r.x, r.y, r.w, r.h+5);
		sg_drawperim(r);
		r.w=8*2;
		r.h=8;
		for(int i =0; i<20; i++){
			r.y=gosc_basey+(knob_size+1)*(OSC_PER_TONE+1)+1;
			r.x=i*8*2+2;
			if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
				syn_song_pos(G.syn, song_scroll+i);
				Mouse.b0=0;
			}
			// index
			char apat=syn_song_pos(G.syn, -1)==song_scroll+i;
			char post_end=song_scroll+i > syn_song_len(G.syn, -1);
			char in_loop = G.syn->song_loop && song_scroll+i>=G.syn->song_loop_begin && song_scroll+i<=G.syn->song_loop_end;
			rgba8 icol={.c=0xFFFFFFFF};
			{ icol.r =55; icol.g=155; icol.b=155; }
			if(apat){ icol.r =255; icol.g=55; icol.b=55;}
			if(post_end){ icol.g=55; icol.b=55;}
			if(in_loop){ icol.r=255; icol.g=255; icol.b=55;}
			sg_drawtex(song_tex[ song_scroll+i ], r.x, r.y, 0, icol.r, icol.g, icol.b ,255);
			r.y+=9;
			if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
				mlatch=phony_latch_pat;
				mlatch_song_pat=song_scroll+i;
			}
			// patterns
			rgba8 patcol = get_patcol( G.syn->song_pat[song_scroll+i] );
			if(post_end) patcol.c=0x555555FF;
			sg_drawtex(song_tex[ G.syn->song_pat[song_scroll+i] ], r.x, r.y, 0, patcol.r,patcol.g,patcol.b,255);
			r.y+=9;
			// repeat
			if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
				mlatch=phony_latch_pat_dur;
				mlatch_song_pat=song_scroll+i;
			}
			char repeats = G.syn->song_beat_dur[song_scroll+i];
			if(post_end) repeats=0;
			sg_drawtex(step_bar_tex[ G.syn->song_beat_dur[song_scroll+i] ], r.x, r.y, 0, repeats?255:55,repeats?255:55,repeats?255:55,255);
			r.y+=9;
			// active box
			if(apat){
				sg_rect p=r;
				p.y-=9*3;
				p.x--;
				p.h+=9*2+1;
				p.w+=1;
				p.r=255;
				sg_drawperim(p);
			}
			// song tie
			{
				sg_rect p=r;
				p.h=5;
				// p.y--;
				p.w--;
				p.x--;
				if(!mlatch && Mouse.b0 && ptboxe(Mouse.px, Mouse.py, p)){
					syn_song_tie(G.syn, song_scroll+i, !syn_song_tie(G.syn, song_scroll+i, -1));
					Mouse.b0=0;
					// gup_song=1;
				}
				p.r=syn_song_len(G.syn, -1) >= song_scroll+i ? 155 : 55;
				if(syn_song_tie(G.syn, song_scroll+i, -1))
					sg_drawrect(p);
				else
					sg_drawperim(p);
			}
		}
	}
	// mouse song pattern latch
	{
		if(mlatch == phony_latch_song_len){
			syn_song_len(G.syn, syn_song_len(G.syn, -1)-Mouse.sdy );
			gup_song=1;
		}
		if(mlatch == phony_latch_pat){
			syn_song_pat(G.syn, mlatch_song_pat, syn_song_pat(G.syn, mlatch_song_pat, -1)-Mouse.sdy );
			prev_song_pos=-1;
			gup_song=1;
		}
		else if(mlatch == phony_latch_pat_dur){
			gup_song=1;
			syn_song_dur(G.syn, mlatch_song_pat, syn_song_dur(G.syn, mlatch_song_pat, -1)-Mouse.sdy );
		}
	}
	{ // song scrollbar
		sg_rect r = {0, gosc_basey+(knob_size+1)*(OSC_PER_TONE+1)-4, BASE_SCREEN_WIDTH, 5, 55,55,55,255, 0,0};
		if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
			mlatch = phony_latch_song_scroll;
			gup_song=1;
		}
		sg_clear_area(r.x, r.y, r.w, r.h);
		sg_drawperim(r);
		// scroll position
		r.x=((float)song_scroll)/(SONG_MAX-20) * BASE_SCREEN_WIDTH -6;
		r.w=12;
		sg_drawrect(r);
		// song playback position
		r.x=((float)song_pos)/(SONG_MAX) * BASE_SCREEN_WIDTH-1;
		r.w=2;
		r.r=255;
		r.h-=2;
		r.y++;
		sg_drawrect(r);

		if(mlatch == phony_latch_song_scroll){
			song_scroll = MAPVAL(Mouse.px, 0, BASE_SCREEN_WIDTH, 0, SONG_MAX-19);
			gup_song=1;
		}
	}
	{// song len
		// sg_rect r = {BASE_SCREEN_WIDTH-16, gosc_basey+(knob_size+1)*(OSC_PER_TONE+1)+2+1+9*3, 16, 8, 255,255,255,255,0,0};
		sg_rect r = {BASE_SCREEN_WIDTH-16, gosc_basey+(knob_size+1)*(OSC_PER_TONE+1)-5 -8, 16, 8, 255,255,255,255,0,0};
		sg_clear_area(r.x, r.y, r.w, r.h);
		sg_drawtex(song_tex[ G.syn->song_len ], r.x, r.y, 0, 255,255,255,255);
		if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r))
			mlatch=phony_latch_song_len;
	}
	{// song advance
		sg_rect r = {BASE_SCREEN_WIDTH-8, vkb_basey-12, 7, 7, 255,255,255,255,0,0};
		sg_clear_area(r.x, r.y, r.w, r.h);
		if(G.syn->song_advance)
			sg_drawrect(r);
		else
			sg_drawperim(r);
		if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
			G.syn->song_advance = !G.syn->song_advance;
			Mouse.b0=0;
		}
	}
	{// song loop
		sg_rect r = {BASE_SCREEN_WIDTH-8 -32-8, vkb_basey-12, 7, 7, 255,255,255,255,0,0};
		sg_clear_area(r.x, r.y, r.w, r.h);
		if(G.syn->song_loop)
			sg_drawrect(r);
		else
			sg_drawperim(r);
		if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
			G.syn->song_loop = !G.syn->song_loop;
			Mouse.b0=0;
			gup_song=1;
		}
		rgba8 col = {.r=255, .g=255,.b=255,.a=255};
		if(!G.syn->song_loop) {col.r=55;col.g=55;col.b=55;}
		r.w=16;
		r.h=8;

		r.x+=8;
		sg_clear_area(r.x, r.y, r.w, r.h);
		if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
			mlatch = phony_latch_loop_begin;
		}
		sg_drawtext(song_tex[G.syn->song_loop_begin], r.x, r.y, 0, col.r, col.g, col.b, 255);

		r.x+=16;
		sg_clear_area(r.x, r.y, r.w, r.h);
		if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
			mlatch = phony_latch_loop_end;
		}
		sg_drawtext(song_tex[G.syn->song_loop_end], r.x, r.y, 0, col.r, col.g, col.b, 255);
	}
	if(mlatch == phony_latch_loop_begin) gup_song=1, syn_song_loop(G.syn, G.syn->song_loop_begin - Mouse.sdy, -1);
	if(mlatch == phony_latch_loop_end) gup_song=1, syn_song_loop(G.syn, -1, G.syn->song_loop_end - Mouse.sdy);

	// song shortcuts
	{
		if(kbget(SDLK_LCTRL) || kbget(SDLK_RCTRL)){
			if(kbget(SDLK_PAGEUP)){
				syn_song_pos(G.syn, syn_song_pos(G.syn, -1)-1);
				kbset(SDLK_PAGEUP,0);
				gup_song=1;
			}
			if(kbget(SDLK_PAGEDOWN)){
				syn_song_pos(G.syn, syn_song_pos(G.syn, -1)+1);
				kbset(SDLK_PAGEDOWN,0);
				gup_song=1;
			}
		}
		if(kbget(SDLK_LCTRL) || kbget(SDLK_RCTRL)){
			if(kbget(SDLK_HOME)){
				syn_song_pos(G.syn, 0);
				kbset(SDLK_HOME,0);
				gup_song=1;
			}
			if(kbget(SDLK_END)){
				syn_song_pos(G.syn, syn_song_len(G.syn, -1));
				kbset(SDLK_END,0);
				gup_song=1;
			}
		}
		// if(kbget(SDLK_LSHIFT) || kbget(SDLK_RSHIFT)){
			if(kbget(SDLK_PAGEUP)){
				syn_song_pat(G.syn, syn_song_pos(G.syn, -1), syn_song_pat(G.syn, syn_song_pos(G.syn, -1), -1)-1);
				kbset(SDLK_PAGEUP,0);
				prev_song_pos=-1;
				gup_song=1;
			}
			if(kbget(SDLK_PAGEDOWN)){
				syn_song_pat(G.syn, syn_song_pos(G.syn, -1), syn_song_pat(G.syn, syn_song_pos(G.syn, -1), -1)+1);
				kbset(SDLK_PAGEDOWN,0);
				prev_song_pos=-1;
				gup_song=1;
		// }
			}
		if(Mouse.wy){
			song_scroll-=Mouse.wy;
			song_scroll=CLAMP(song_scroll, 0, SONG_MAX-19);
			gup_song=1;
		}

	}

	song_pos = syn_song_pos(G.syn, -1);
	if(song_pos!=prev_song_pos){
		gup_song=1;
		gup_seq=1;
		gup_spb=1;
		gup_bpm=1;
		gup_note_grid=1;
		gup_osc=1;
		gup_shown_notes=1;
		gup_step_bar=1;
	}
	prev_song_pos=song_pos;




/*----------------------------------------------------------------------------*/
/* controls  */
/*----------------------------------------------------------------------------*/
	if (kbget(SDLK_LEFT )){ follow=0;
		key_delay++;
		if(key_delay==0){ key_delay=-6;
			if(kbget(SDLK_RCTRL)||kbget(SDLK_LCTRL)){
				G.syn->seq[_isel]->len=MAX(G.syn->seq[_isel]->len-1, 1)      ;gup_seq=1;}
			else astep( step_sel-1);
		}
	}
	if (kbget(SDLK_RIGHT)){ follow=0;
		key_delay++;
		if(key_delay==0){ key_delay=-6;
			if(kbget(SDLK_RCTRL)||kbget(SDLK_LCTRL)){
				G.syn->seq[_isel]->len=MIN(G.syn->seq[_isel]->len+1, SEQ_LEN);gup_seq=1;}
			else astep( step_sel+1);
		}
	}
	if (kbget(SDLK_UP)){ //kbset(SDLK_UP, 0);
		key_delay++;
		if(key_delay==0){ key_delay=-6;
			if(kbget(SDLK_RCTRL)||kbget(SDLK_LCTRL)){
				G.syn->seq[_isel]->spb=MIN(G.syn->seq[_isel]->spb+1, SEQ_LEN);
				gup_spb=1;
				gup_seq=1;
			} else {step_add++;gup_step_add=1;}
		}
	}
	if (kbget(SDLK_DOWN )){ //kbset(SDLK_DOWN, 0);
		key_delay++;
		if(key_delay==0){ key_delay=-6;
			if(kbget(SDLK_RCTRL)||kbget(SDLK_LCTRL)){
				G.syn->seq[_isel]->spb=MAX(G.syn->seq[_isel]->spb-1, 1);
				gup_spb=1;
				gup_seq=1;
			} else if(step_add>0) {step_add--; gup_step_add=1;}
		}
	}


	if(kbget(SDLK_DELETE)){
		gup_shown_notes=1;
		if(!(kbget(SDLK_RSHIFT) || kbget(SDLK_LSHIFT))){
			seq_anof(G.syn->seq[_isel], step_sel);
			note_cols[_isel][step_sel]=0;
		}
		seq_modm(G.syn->seq[_isel], NULL, step_sel);
		gup_seq=1;
	}

	if(kbget(SDLK_EQUALS)){
		if(kbget(SDLK_LSHIFT) || kbget(SDLK_RSHIFT)){
			kbset(SDLK_EQUALS, 0);
			kb_octave( kb_octave(-1) +1);
		}
	}
	if(kbget(SDLK_MINUS)){
		if(kbget(SDLK_LSHIFT) || kbget(SDLK_RSHIFT)){
			kbset(SDLK_MINUS, 0);
			kb_octave( kb_octave(-1) -1);
		}
	}
	if(kbget(tap_tempo_key)){
		kbset(tap_tempo_key, 0);
		if(tap_tempo == -1){
			tap_tempo=G.time;
		} else {
			float target_bpm = 1.f / ((G.time-tap_tempo) / 60.0);
			syn_bpm(G.syn, target_bpm);
			tap_tempo=-1;
			gup_bpm=1;
		}
	}
	if(1.f / ((G.time-tap_tempo) / 60.0)<tap_tempo_min_bpm) tap_tempo=-1;

	gui_quit_dialog();
	gui_save_dialog();

	if(kbget(SDLK_RETURN)){ kbset(SDLK_RETURN,0);
		if(G.syn->seq_play) syn_stop(G.syn);
		else syn_pause(G.syn);
	}
	if((kbget(SDLK_LCTRL) || kbget(SDLK_RCTRL)) && kbget(SDLK_a)){ kbset(SDLK_a,0);
		G.syn->song_advance = !G.syn->song_advance;
	}
	if((kbget(SDLK_LCTRL) || kbget(SDLK_RCTRL)) && kbget(SDLK_r)){ kbset(SDLK_r,0);
		rec=!rec;
		gup_rec=1;
	}
	if((kbget(SDLK_LCTRL) || kbget(SDLK_RCTRL)) && kbget(SDLK_f)){ kbset(SDLK_f,0);
		follow=!follow;
	}

	if((kbget(SDLK_LCTRL) || kbget(SDLK_RCTRL)) && (kbget(SDLK_LSHIFT) || kbget(SDLK_RSHIFT)) && kbget(SDLK_l)){
		kbset(SDLK_l,0);
		gup_song=1;
		G.syn->song_loop = !G.syn->song_loop;
	}

	// copy/paste
	if((kbget(SDLK_LCTRL) || kbget(SDLK_RCTRL)) && kbget(SDLK_c)){ kbset(SDLK_c,0);
		for(int i=0; i<SYN_TONES; i++){
			pattern_copy_file[i] = freopen(NULL,"wb",pattern_copy_file[i]);
			syn_seq_write(G.syn, G.syn->seq[i], pattern_copy_file[i]);
			fflush(pattern_copy_file[i]);
		}
		pattern_copy=1;
	}
	if((kbget(SDLK_LCTRL) || kbget(SDLK_RCTRL)) && kbget(SDLK_v)){ kbset(SDLK_v,0);
		if(pattern_copy)
			for(int i=0; i<SYN_TONES; i++){
				if(seq_mute(G.syn->seq[i], -1)) continue;
				pattern_copy_file[i] = freopen(NULL, "rb", pattern_copy_file[i]);
				fseek(pattern_copy_file[i], 0, SEEK_END);
				long len = ftell(pattern_copy_file[i]);
				void* mem = alloca(len);
				rewind(pattern_copy_file[i]);
				fread(mem, 1, len, pattern_copy_file[i]);
				int read=0;
				int err=syn_seq_read(G.syn, G.syn->seq[i], mem, len, &read);
				if(err) printf("copy buffer file read error\n");
				gup_seq=1;
				gup_osc=1;
				// free(mem);
			}
	}
}















void gui_quit_dialog(void){
	if(kbget(SDLK_ESCAPE) && !request_quit && !save_requested){
		kbset(SDLK_ESCAPE, 0);
		kbset(SDLK_RETURN, 0);
		request_quit =1;
	}

	if(request_quit){
		int border = 10;
		int sx=0,sy=0, syesx=0, snox=0;
		sg_texsize( quitd_tex, &sx, &sy );
		sg_rect r = {BASE_SCREEN_WIDTH/2 - sx/2 - border, BASE_SCREEN_HEIGHT/2 - sy/2 - border, sx + border*2, sy*2 + border*2, 255,0,0,255, 0,0};
		sg_clear_area(r.x, r.y, r.w, r.h);
		sg_drawtex( quitd_tex, r.x+border, r.y+border, 0, 255,150+100*tri((frame%60)/60.0),150+100*tri((frame%60)/60.0),255);
		sg_drawtex( quitd_tex_no,  r.x+border, r.y+sy+border, 0, 255,255,255,255);

		sg_texsize( quitd_tex_no, &snox, NULL);
		sg_rect rno = {r.x+border-2, r.y+sy+border-2, snox+2, sy+2, 155,155,255,255, 0,0};
		char click_no = ptbox(Mouse.px, Mouse.py, rno);
		if(click_no) sg_drawperim(rno);
		click_no = click_no && Mouse.b0;

		sg_texsize( quitd_tex_yes, &syesx, NULL );
		sg_drawtex( quitd_tex_yes, r.x+r.w-syesx-border, r.y+sy+border, 0, 155,0,0,255);
		sg_rect ryes = {r.x+r.w-syesx-border-2, r.y+sy+border-2, syesx+2, sy+2, 255,0,0,255, 0,0};
		char click_yes = ptbox(Mouse.px, Mouse.py, ryes);
		if(click_yes) sg_drawperim(ryes);
		click_yes = click_yes && Mouse.b0;

		sg_drawperim( r );
		if(kbget(SDLK_ESCAPE) || click_no || (Mouse.b0 && !click_yes && !ptbox(Mouse.px, Mouse.py, r)) ) {
			kbset(SDLK_ESCAPE,0);
			request_quit=0;
			sg_clear_area(r.x, r.y, r.w, r.h);
			gup_shown_notes=1;
			gup_note_grid=1;
			Mouse.b0=0;
		}
		if(kbget(SDLK_RETURN) || click_yes) {
			kbset(SDLK_RETURN,0);
			running=0;
		}
	}
}


















char note_color[12]={2,1,0,1,0,0,1,0,1,0,1,0}; // for vkb


int16_t note_grid_tex;
char note_grid_init=0;
#define note_grid_pxsize_x (BASE_SCREEN_WIDTH-20-11)
#define note_grid_pxsize_y (BASE_SCREEN_HEIGHT-16)
rgba8 note_grid_pixels[note_grid_pxsize_x*note_grid_pxsize_y];
int note_grid_size_x = (BASE_SCREEN_WIDTH-32)/16;
int note_grid_size_y = (BASE_SCREEN_HEIGHT-16) / (12*1+1);

struct {
	uint8_t dur;
	uint8_t vel;
} shown_notes[13][16];



void gui_patternview(void){
	if(!note_grid_init){
		for(int x=0; x<note_grid_pxsize_x; x++){
			for(int y=0; y<note_grid_pxsize_y; y++){

				note_grid_pixels[y*note_grid_pxsize_x+x].c = 0x000000FF;

				if(!(x%note_grid_size_x) || !(y%note_grid_size_y))
					note_grid_pixels[y*note_grid_pxsize_x+x].c = 0xFFFFFFFF;

			}
		}
		note_grid_tex = sg_addtex(note_grid_pixels, note_grid_pxsize_x, note_grid_pxsize_y);
		note_grid_init=1;
	}

	// selected tone
	{
		sg_clear_area(0,0, giselw , giselh);
		sg_rect r;
		r.x=0;
		r.y=0;
		r.w=giselw;
		r.h=giselh;

		r.r=0;
		r.g=seq_mute(G.syn->seq[_isel], -1)? 55: 55-r.r;
		r.b=seq_mute(G.syn->seq[_isel], -1)? 55: 255;
		r.a=255;
		sg_drawperim(r);
		// VUmeter
		r.x++;
		r.w=r.w/2-2;
		r.y=giselh - (G.syn->tone[_isel]->vupeakl * giselh)-1;
		r.h=giselh - r.y-1;
		sg_rcol(&r, MIN(G.syn->tone[_isel]->vupeakl, 1.0), MAX(1-G.syn->tone[_isel]->vupeakl, 0),0,1);
		sg_drawrect(r);
		r.x=r.x+r.w+2;
		r.y=giselh - (G.syn->tone[_isel]->vupeakr * giselh)-1;
		r.h=giselh - r.y-1;
		sg_rcol(&r, MIN(G.syn->tone[_isel]->vupeakr, 1.0), MAX(1-G.syn->tone[_isel]->vupeakr, 0),0,1);
		sg_drawrect(r);
	}


	// vkb
	int vkb_h = BASE_SCREEN_HEIGHT-16;
	int vkb_w = 20;
	int vkb_basey = 16;
	int vkb_keys = 12*1+1;
	int vkb_endx = 0;
	int vkb_endy = 0;
	if(!save_requested)
	if(gup_vkb || Mouse.py <= vkb_basey){
		sg_rect r = {-1, vkb_basey, vkb_w, vkb_h,  255,255,255,255, 0,0};
		if(gup_vkb) sg_clear_area(r.x, r.y, r.w, r.h);
		r.h=((float)BASE_SCREEN_HEIGHT)/vkb_keys;
		r.y=BASE_SCREEN_HEIGHT-((float)vkb_h)/(vkb_keys);

		int notei=0;
		for(int i = 0 ; i < 13; i++){
			int isC=0;
			int color_index = (i+note_scrollv)%12;
			if(color_index<0) color_index+=12;
			char black = note_color[color_index];
			if(black>1) {black=0; isC=1;}
			r.r = black? 55 : 255;
			r.g = black? 55 : 255;
			r.b = black? 55 : 255;

			char collision = (!mlatch || mlatch==phony_latch_vkb) && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r);
			if(collision || voices[notei+note_scrollv].tone != -1 ) r.b=0;

			if(collision && (!vkb_note_active || vkb_note!=C0+notei+note_scrollv-(octave-4)*12)) {
				mlatch = phony_latch_vkb;
				if(vkb_note!=C0+notei+note_scrollv-(octave-4)*12){
					key_update(vkb_note, 0);
				}
				key_update(C0+notei+note_scrollv-(octave-4)*12, 1);
				vkb_note = C0+notei+note_scrollv-(octave-4)*12;
				vkb_note_active =1;
			}
			notei++;

			if(gup_vkb) sg_drawrect(r);
			if(gup_vkb && isC)
				sg_drawtext(Cn_tex[(int)MAPVAL((float)C0+notei+note_scrollv, C0, C0+(MAX_OCTAVE-1)*12, 0, (MAX_OCTAVE-1))],
				r.x+1, r.y+r.h-9, 0, 55,55,55,255);
			r.r = 0;
			r.g = 0;
			r.b = 0;
			if(gup_vkb) sg_drawperim(r);
			r.y -= ((float)vkb_h)/(vkb_keys)-1;
		}
		vkb_endy = r.y + ((float)vkb_h)/(vkb_keys);
	}

	// note grid internal representation
	if(gup_shown_notes){
		gup_shown_notes=0;
		for(int i=0; i<13; i++)
			for(int j=0; j<16; j++)
				shown_notes[i][j].dur = 0;

		for(int i=0; i<13; i++){
			for(int j=0; j<16; j++){
				for(int v=0; v<POLYPHONY; v++){
					if(G.syn->seq[_isel]->note[v][j+note_scrollh] == C0+i+note_scrollv){
						shown_notes[i][j].dur = G.syn->seq[_isel]->dur[v][j+note_scrollh];
						shown_notes[i][j].vel = G.syn->seq[_isel]->vel[v][j+note_scrollh];
					}
				}
			}
		}
	}

	// mouse hover over note grid
	{
		sg_rect r = { vkb_w-1, vkb_endy, note_grid_size_x*16, note_grid_size_y*13, 0,0,0,0, 0,0};
		if((Mouse.b0||Mouse.b1|| Mouse.dx!=0 || Mouse.dy!=0) && ptbox(Mouse.px, Mouse.py, r))
			gup_note_grid=1;
	}
	if(!save_requested && gup_note_grid){
		gup_note_grid=0;
		int ng_basex = vkb_w-1;
		// int ng_basey = vkb_basey;
		int ng_basey = vkb_endy;
		int ng_px = note_grid_size_x;
		int ng_py = note_grid_size_y;
		sg_clear_area(ng_basex, ng_basey, ng_px*16, ng_py*13);
		sg_drawtex(note_grid_tex, ng_basex, ng_basey, 0, 55,55,55,255);
		for(int i = 0; i < 13; i++)
		for(int j = 0; j < 16; j++){
			sg_rect r = {ng_basex + j*ng_px, BASE_SCREEN_HEIGHT -(i+1)*ng_py, ng_px, ng_py, 255,0,0,255, 0,0};
			if((!mlatch && ptbox(Mouse.px, Mouse.py, r))){
				sg_rect rn =r;
				rn.x++;
				int dx = Mouse.px - r.x;
				int dy = Mouse.py - r.y;
				rn.w=dx;
				rn.y+=dy;
				rn.h=ng_py-dy-1;
				rn.g=255;
				sg_drawrect(rn);

				if(Mouse.b0 || Mouse.b1 || kbget(SDLK_DELETE)){
					int overwrite=-1;
					for(int v =0; v<POLYPHONY; v++){
						if(G.syn->seq[_isel]->note[v][j+note_scrollh] == C0+i+note_scrollv){
							overwrite=v;
							break;
						}
					}
					if( overwrite >=0 ){
						seq_nof(G.syn->seq[_isel], j+note_scrollh, overwrite);
						note_cols[_isel][j+note_scrollh]--;
					}
					if(Mouse.b0 && dx>1 && ng_py-dy>1){
						char err=seq_non(G.syn->seq[_isel], j+note_scrollh, C0+i+note_scrollv, ((float)rn.h)/(ng_py-2), ((float)rn.w)/(ng_px-1));
						if(!err)note_cols[_isel][j+note_scrollh]++;
					}
					gup_shown_notes=1;
					gup_step_bar=1;
				}
			}
			if(shown_notes[i][j].dur > 0){
				r.x++;
				r.w--; r.h--;
				r.w *= shown_notes[i][j].dur / 255.f;
				r.h *= shown_notes[i][j].vel / 255.f;
				r.y += ng_py - r.h-1;
				sg_drawrect(r);
			}
			note_cols[_isel][j+note_scrollh] = CLAMP(note_cols[_isel][j+note_scrollh], 0, POLYPHONY);
		}
	}

	// step bar
	if(gup_step_bar || Mouse.b0 || mlatch==phony_latch_scrollh) {
		int ng_px = note_grid_size_x;

		note_scrollh_pos += note_scrollh_vel;
		note_scrollh_pos = CLAMP(note_scrollh_pos, 0, SEQ_LEN-16);
		note_scrollh_vel*=.9;
		if(fabsf(note_scrollh_vel)<0.01) { gup_step_bar=0;}

		sg_rect r = {vkb_w, 0, BASE_SCREEN_WIDTH-vkb_w-11-2, 16, 0,0,0,255, 0,0};
		if(mlatch==phony_latch_scrollh || (Mouse.b0 && ptboxe(Mouse.px, Mouse.py, r))){
			gup_step_bar=1;
			mlatch=phony_latch_scrollh;
			note_scrollh_vel-=((float)Mouse.sdx)/screen_width*ng_px/4;
			if(Mouse.sdx*Mouse.sdx == 0) note_scrollh_vel=0;
		}
		note_scrollh = note_scrollh_pos;
		// note_scrollh = CLAMP(note_scrollh, 0, SEQ_LEN-16);
		if(gup_step_bar)gup_shown_notes=1;
		if(gup_step_bar)gup_note_grid=1;

		sg_clear_area(r.x, r.y, r.w, r.h);
		for(int i=0; i<16; i++){
			if(G.syn->seq[_isel]->step == i+note_scrollh)
				sg_drawtext( step_bar_tex[i+note_scrollh], r.x, r.y, 0, 255, 255, 55, 255 );
			else if(G.syn->seq[_isel]->len <= i+note_scrollh)
				sg_drawtext( step_bar_tex[i+note_scrollh], r.x, r.y, 0, 55, 55, 55, 255 );
			else sg_drawtext( step_bar_tex[i+note_scrollh], r.x, r.y, 0, 150+105*sign(note_cols[_isel][i+note_scrollh]), 150, 150, 255 );

			r.x+=ng_px;
		}
		sg_drawrect(r);
	}

	if(step!=pstep){gup_step_bar=1;}

	// vscroll bar
	if(!save_requested){
		int vs_basex = note_grid_size_x*16+vkb_w;
		sg_rect r = {vs_basex, 16, 12, BASE_SCREEN_HEIGHT, 0,0,0,255, 0 ,0};
		sg_drawrect(r);
		if(mlatch==phony_latch_scrollv || (Mouse.b0 && !mlatch && ptbox(Mouse.px, Mouse.py, r))){
			mlatch = phony_latch_scrollv;
			note_scrollv = MAPVAL(Mouse.py, 16, BASE_SCREEN_HEIGHT, (MAX_OCTAVE-1)*12-1, 0);
			note_scrollv = CLAMP(note_scrollv, 0, (MAX_OCTAVE-1)*12-1);
			gup_shown_notes=1;
			gup_note_grid=1;
		}

		r.y = ((float)((MAX_OCTAVE-1)*12-1)-note_scrollv)/((MAX_OCTAVE-1)*12-1) * (BASE_SCREEN_HEIGHT-16) +8;
		r.h = 16;
		r.b = 255;
		sg_drawperim(r);
	}

	if(Mouse.wy){
		note_scrollv += Mouse.wy;
		note_scrollv = CLAMP(note_scrollv, 0, (MAX_OCTAVE-1)*12-1);
		gup_shown_notes=1;
		gup_note_grid=1;
		gup_step_bar=1;
	}

	if(Mouse.wx){
		note_scrollh += Mouse.wx;
		note_scrollh = CLAMP(note_scrollh, 0, SEQ_LEN-16);
		gup_shown_notes=1;
		gup_note_grid=1;
		gup_step_bar=1;
	}

	if(kbget(SDLK_PAGEUP)){
		kbset(SDLK_PAGEUP, 0);
		kb_octave( kb_octave(-1) +1);
	}
	if(kbget(SDLK_PAGEDOWN)){
		kbset(SDLK_PAGEDOWN, 0);
		kb_octave( kb_octave(-1) -1);
	}

	gui_quit_dialog();
	gui_save_dialog();

	if(kbget(SDLK_RETURN)){ kbset(SDLK_RETURN,0);
		if(G.syn->seq_play) syn_stop(G.syn);
		else syn_pause(G.syn);
	}
}





























char save_type=0; // 0:song, 1:pattern, 2:tone

void gui_save_dialog(void){
	if((kbget(SDLK_LCTRL)||kbget(SDLK_RCTRL)) && kbget(SDLK_s)){
		kbset(SDLK_s, 0);
		save_requested=1;
	}

	if(!save_requested) return;
	sg_rect r = {0, 0, BASE_SCREEN_WIDTH, 32, 255,0,0,255, 0,0};
	sg_clear_area(r.x, r.y, r.w, r.h);
	sg_drawperim(r);

	sg_drawtext(save_dialog_text_tex, 1,1, 0, 155,155,255,255);

	if(!SDL_IsTextInputActive()) SDL_StartTextInput();
	if(song_name==NULL){
		song_name=malloc(SONG_NAME_MAX);
		memset(song_name, 0, SONG_NAME_MAX);
		song_name[0]=' ';
		song_name_tex = sg_addtext(song_name);
		song_name[0]='\0';
		gup_song_name=0;
	}
	if(gup_song_name){
		if(strlen(song_name)!=0)
			sg_modtext(song_name_tex, song_name);
	}

	if(strlen(song_name)!=0) sg_drawtext(song_name_tex, 1, 10, 0, 255,255,255,255);
	else  sg_drawtext(song_name_empty_tex, 1, 10, 0, 155,155,155,255);

	sg_rect cursor_rect = { 6*song_name_cursor+1, 10, 1, 11, 255,0,0,155, 0,0};
	sg_drawperim(cursor_rect);

	int snox=0, syesx=0;

	sg_texsize( quitd_tex_no, &snox, NULL);
	sg_rect rno = {1, 32-11, snox+2, 10, 155,155,255,255, 0,0};
	sg_drawtex( quitd_tex_no, 1, 32-11, 0, 55,55,255,255);
	char click_no = ptbox(Mouse.px, Mouse.py, rno);
	if(click_no) sg_drawperim(rno);
	click_no = click_no && Mouse.b0;

	sg_texsize( quitd_tex_yes, &syesx, NULL );
	sg_drawtex( quitd_tex_yes, BASE_SCREEN_WIDTH-syesx-2, 32-11, 0, 155,0,0,255);
	sg_rect ryes = {BASE_SCREEN_WIDTH-syesx-2, 32-11, syesx+2, 10, 255,0,0,255, 0,0};
	char click_yes = ptbox(Mouse.px, Mouse.py, ryes);
	if(click_yes) sg_drawperim(ryes);
	click_yes = click_yes && Mouse.b0;

	// save type selection
	sg_rect st = {BASE_SCREEN_WIDTH-8*4*4, 1, 4*8, 8, 255,255,255,255, 0,0};
	if(ptbox(Mouse.px, Mouse.py, st)) sg_drawperim(st);
	if(Mouse.b0 && ptbox(Mouse.px, Mouse.py, st))
		save_type=0;
	sg_drawtext(save_type_tex[0], st.x, st.y, 0, 255, save_type==0?55:255, save_type==0?55:255, 255);

	st.x+=4*8;
	if(ptbox(Mouse.px, Mouse.py, st)) sg_drawperim(st);
	if(Mouse.b0 && ptbox(Mouse.px, Mouse.py, st))
		save_type=1;
	sg_drawtext(save_type_tex[1], st.x, st.y, 0, 255, save_type==1?55:255, save_type==1?55:255, 255);

	st.x+=4*8;
	if(ptbox(Mouse.px, Mouse.py, st)) sg_drawperim(st);
	if(Mouse.b0 && ptbox(Mouse.px, Mouse.py, st))
		save_type=2;
	sg_drawtext(save_type_tex[2], st.x, st.y, 0, 255, save_type==2?55:255, save_type==2?55:255, 255);

	st.x+=4*8;
	if(ptbox(Mouse.px, Mouse.py, st)) sg_drawperim(st);
	if(Mouse.b0 && ptbox(Mouse.px, Mouse.py, st))
		save_type=3;
	sg_drawtext(save_type_tex[3], st.x, st.y, 0, 255, save_type==3?55:255, save_type==3?55:255, 255);



	if(kbget(SDLK_ESCAPE) || click_no || (Mouse.b0 && !click_yes && !ptbox(Mouse.px, Mouse.py, r)) ) {
		kbset(SDLK_ESCAPE, 0);
		save_requested=0;
		sg_clear_area(r.x, r.y, r.w, r.h);
		gup_bpm=1; gup_spb=1; gup_mlatch=1; gup_step_add=1;
		gup_seq=1; gup_shown_notes=1; gup_note_grid=1; gup_vkb=1; gup_step_bar=1;
		SDL_StopTextInput();
	}
	if(kbget(SDLK_RETURN) || click_yes){
		kbset(SDLK_RETURN, 0);

		char final_song_name[SONG_NAME_MAX+6];
		int name_len = strnlen(song_name, SONG_NAME_MAX-1);
		memcpy(final_song_name, song_name, name_len);

		int err=0;
		switch(save_type){
			case 0:
				memcpy(final_song_name+name_len, ".syns", 6);
				err=syn_song_save(G.syn, final_song_name);
				break;
			case 1:
				memcpy(final_song_name+name_len, ".synp", 6);
				err=syn_seq_save(G.syn, G.syn->seq[_isel], final_song_name);
				break;
			case 2:
				memcpy(final_song_name+name_len, ".synt", 6);
				err=syn_tone_save(G.syn, G.syn->tone[_isel], final_song_name);
				break;
			case 3:
				memcpy(final_song_name+name_len, ".wav", 5);
				err=syn_render_wav(G.syn, final_song_name);
				break;
		}

		if(err) printf("error saving file %i\n", err);
		else printf("succesfully saved %s\n", final_song_name);

		save_requested=0;
		sg_clear_area(r.x, r.y, r.w, r.h);
		gup_bpm=1; gup_spb=1; gup_mlatch=1; gup_step_add=1;
		gup_seq=1; gup_shown_notes=1; gup_note_grid=1; gup_vkb=1; gup_step_bar=1;
		SDL_StopTextInput();
	}

	Mouse.b0=0;

}




















#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#define FILES_MAX (BASE_SCREEN_HEIGHT/10-2)
#define PATHLEN_MAX 2048
#define FILE_COUNTER_MAX 512
char* dir_path;
char* base_dir_path;
int directory_scroll=0;
int16_t dir_path_tex;

enum libfiletype { LIBFTYPE_FILE=0, LIBFTYPE_DIR, LIBFTYPE_PAT, LIBFTYPE_TONE, LIBFTYPE_SONG};
struct libfile{
	char* name;
	int16_t tex;
	char type;
} files[FILES_MAX];

char libview_was_init=0;
DIR* dir = NULL;

int selection=0;

char gup_lib_names=1;

int libcd(char* d);
void libscroll(int pos);

int file_sort(const void* a, const void* b){
	return strcmp(((struct libfile*)a)->name, ((struct libfile*)b)->name);
}

void libview_init(void){
	libview_was_init=1;
	for(int i=0; i<FILES_MAX; i++) files[i].name=NULL;
	for(int i=0; i<FILES_MAX; i++) files[i].type=0;
	for(int i=0; i<FILES_MAX; i++) files[i].tex=sg_addtext(" ");
	dir_path = getcwd(NULL,0);
	dir_path_tex = sg_addtext(dir_path);

	base_dir_path = getcwd(NULL,0);
	libcd(getcwd(NULL,0));
}

int file_counter=0;
struct libfile* templib;

int libcd(char* d){
	DIR *dirhandle = opendir(d);
	if(!dirhandle) {
		chdir(base_dir_path);
		libcd(".");
		printf("invalid directory, going back to base\n");
		// dirhandle=opendir(".");
	}

	if(dir) closedir(dir);
	dir = dirhandle;
	directory_scroll=0;
	selection=0;
	if(dir_path) free(dir_path);
	dir_path = d;
	sg_modtext(dir_path_tex, dir_path);

	for(int i=0; i<FILES_MAX; i++) files[i].name=NULL;
	for(int i=0; i<FILES_MAX; i++) files[i].type=0;

	if(!dir) return -1;

	gup_lib_names=1;

	struct dirent *de;
	file_counter=0;
	while( (de=readdir(dir)) != NULL)
		file_counter++;
	if(file_counter>=FILE_COUNTER_MAX) printf("Warning! folder has more than %i files. will not read correctly!\n", FILE_COUNTER_MAX);
	file_counter=CLAMP(file_counter, 0, FILE_COUNTER_MAX);
	rewinddir(dir);

	if(templib) free(templib);
	templib=malloc(sizeof(struct libfile)*file_counter);
	memset(templib, 0, sizeof(struct libfile)*file_counter);

	for(int i=0; (de=readdir(dir)) != NULL; i++){
		if(i>=file_counter) break;
		templib[i].name = de->d_name;

		if(templib[i].name == NULL){
			printf("invalid or protected directory, returning to base\n");
			chdir(base_dir_path);
			libcd(".");
		}

		char _is_dir;
#ifdef _DIRENT_HAVE_D_TYPE
		if (de->d_type != DT_UNKNOWN && de->d_type != DT_LNK) {
			_is_dir = (de->d_type == DT_DIR);
		} else
#endif
		{	// the only method if d_type isn't available,
			// otherwise this is a fallback for FSes where the kernel leaves it DT_UNKNOWN.
			struct stat stbuf;
			stat(de->d_name, &stbuf); // TODO: error check
			_is_dir = S_ISDIR(stbuf.st_mode);
		}

		if(_is_dir){
			templib[i].type=LIBFTYPE_DIR;
		} else
			templib[i].type=LIBFTYPE_FILE;

	}

	qsort(templib, file_counter, sizeof(struct libfile), file_sort);


	libscroll(0);
	return 0;
}

void libscroll(int pos){
	directory_scroll = pos;
	for(int i = 0; i<FILES_MAX; i++){
		files[i].name = NULL;
	}

	int fi=0;
	for(int i = 0; i<file_counter; i++){
		if(i<directory_scroll) continue;
		if(i>=directory_scroll+FILES_MAX) continue;
		files[fi].name = templib[i].name;
		files[fi].type = templib[i].type;

		if (strstr(files[fi].name, ".synt"))
			files[fi].type=LIBFTYPE_TONE;
		else if (strstr(files[fi].name, ".synp"))
			files[fi].type=LIBFTYPE_PAT;
		else if (strstr(files[fi].name, ".syns"))
			files[fi].type=LIBFTYPE_SONG;

		sg_modtext(files[fi].tex, files[fi].name);
		fi++;
	}

}

void libview_quit(void){
	if(dir) closedir(dir);
	if(dir_path) free(dir_path);
	dir_path=NULL;
	if(base_dir_path) free(base_dir_path);
	base_dir_path=NULL;
}

void gui_libview(void){
	if(!libview_was_init) libview_init();

	if(gup_lib){
		gup_lib=0;
		chdir(base_dir_path);
		libcd(strdup(base_dir_path));
	}
	if(Mouse.wy){
		directory_scroll-=Mouse.wy;
		directory_scroll = CLAMP(directory_scroll, 0, MAX(file_counter-FILES_MAX, 0));
		libscroll(directory_scroll);
	}
	if(kbget(SDLK_PAGEUP)){
		kbset(SDLK_PAGEUP,0);
		selection-=10;
		selection=CLAMP(selection, 0, MIN(FILES_MAX-1, file_counter-1));
		directory_scroll-=10;
		directory_scroll = CLAMP(directory_scroll, 0, MAX(file_counter-FILES_MAX, 0));
		libscroll(directory_scroll);
	}
	if(kbget(SDLK_PAGEDOWN)){
		kbset(SDLK_PAGEDOWN,0);
		selection+=10;
		selection=CLAMP(selection, 0, MIN(FILES_MAX-1, file_counter-1));
		directory_scroll+=10;
		directory_scroll = CLAMP(directory_scroll, 0, MAX(file_counter-FILES_MAX, 0));
		libscroll(directory_scroll);
	}
	if(Mouse.px>=BASE_SCREEN_WIDTH-4 && Mouse.b0){
		mlatch = phony_latch_scrollv;
	}
	if(	mlatch == phony_latch_scrollv ){
		if(Mouse.sdy){
			directory_scroll = MAPVAL((float)Mouse.py, 0, BASE_SCREEN_HEIGHT-1, 0, MAX(file_counter-FILES_MAX, 0));
			directory_scroll = CLAMP(directory_scroll, 0, MAX(file_counter-FILES_MAX, 0));
			libscroll(directory_scroll);
		}
	}

	if(kbget(SDLK_UP)){ kbset(SDLK_UP,0);
		selection--;
		selection=CLAMP(selection, 0, MIN(FILES_MAX-1, file_counter-1));
	}
	if(kbget(SDLK_DOWN)){ kbset(SDLK_DOWN,0);
		selection++;
		selection=CLAMP(selection, 0, MIN(FILES_MAX-1, file_counter-1));
	}

	if(kbget(SDLK_HOME)){ kbset(SDLK_HOME,0);
		selection=0;
		directory_scroll=0;
		libscroll(directory_scroll);
	}
	if(kbget(SDLK_END)){ kbset(SDLK_END,0);
		selection=FILES_MAX-1;
		selection=CLAMP(selection, 0, MIN(FILES_MAX-1, file_counter-1));
		directory_scroll=file_counter;
		directory_scroll = CLAMP(directory_scroll, 0, MAX(file_counter-FILES_MAX, 0));
		libscroll(directory_scroll);
	}

	if(kbget(SDLK_BACKSPACE)){
		kbset(SDLK_BACKSPACE,0);
		chdir("..");
		libcd(getcwd(NULL,0));
	}

	if((!mlatch && Mouse.b0) || kbget(SDLK_RETURN)){
		Mouse.b0=0; kbset(SDLK_RETURN,0);
		int i = selection;
		if(files[i].type == LIBFTYPE_DIR){
			chdir(files[i].name);
			libcd(getcwd(NULL,0));
		} else if(files[i].type == LIBFTYPE_PAT){
			syn_seq_open(G.syn, files[i].name, _isel);
			kbset(SDLK_ESCAPE,1);
		} else if(files[i].type == LIBFTYPE_SONG){
			syn_song_open(G.syn, files[i].name);
			kbset(SDLK_ESCAPE,1);
		} else if(files[i].type == LIBFTYPE_TONE){
			syn_tone_open(G.syn, files[i].name, _isel);
		}
	}


	if(gup_lib_names){
		// gup_lib_names=0;
		sg_clear();
		sg_drawtext(dir_path_tex,8,1, 0, 155,55,55,255);

		for(int i = 0; i < FILES_MAX; i++){
			if(files[i].name!=NULL){
				rgba8 col;
				switch(files[i].type){
					case LIBFTYPE_FILE: col.c=0xFFFFFFFF; break;
					case LIBFTYPE_DIR:  col.r=0;   col.g=0;   col.b=255; break;
					case LIBFTYPE_SONG: col.r=0;   col.g=255; col.b=0;   break;
					case LIBFTYPE_TONE: col.r=0;   col.g=255; col.b=255; break;
					case LIBFTYPE_PAT:  col.r=255; col.g=255; col.b=0;   break;
					default: col.c = 0x88888888; break;
				}
				sg_drawtext(files[i].tex, 2, (i+1)*10, 0, col.r, col.g, col.b ,255);
				// selection border

				sg_rect r={0, (i+1)*10, BASE_SCREEN_WIDTH-4, 10, 155,155,155,155, 0,0};
				if(!mlatch && ptbox(Mouse.px, Mouse.py, r) && Mouse.dy){
					selection=i;
				}
				sg_rect sr={BASE_SCREEN_WIDTH-4,
					MAPVAL((float)directory_scroll, 0, MAX(file_counter-FILES_MAX, 0), 0, BASE_SCREEN_HEIGHT)-2,
					4, 4, 255,255,255,255,0,0};
				sg_drawrect(sr);
			}
		}
		//draw selection
		sg_rect r={0, (selection+1)*10, BASE_SCREEN_WIDTH-4, 10, 155,155,155,155, 0,0};
		sg_drawperim(r);
	}

	if(kbget(SDLK_ESCAPE) && !request_quit){
		kbset(SDLK_ESCAPE, 0);

		gui_view=0;
		gup_mlatch=1;
		gup_rec=1;
		gup_seq=1;
		gup_osc=1;
		gup_spb=1;
		gup_step_add=1;
		gup_bpm=1;
		gup_vkb=1;
		gup_mix=1;
		gup_song=1;
		sg_clear();
	}

	gui_quit_dialog();
}
