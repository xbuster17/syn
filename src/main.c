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
#include <stdio.h>

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

static int screen_width =BASE_SCREEN_WIDTH;
static int screen_height=BASE_SCREEN_HEIGHT;
int octave=0;

/* -------------------------------------------------------------------------- */
/* use this api for graphics */
/* -------------------------------------------------------------------------- */
typedef struct sg_rect{
	int16_t x,y, w,h;
	uint8_t r,g,b,a; // if a=0 only draws contour for drawrect
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
typedef union{struct{uint8_t r,g,b,a;}; uint32_t c;} rgba8;
int16_t sg_addtex( rgba8* pixels, int w, int h );
int16_t sg_addtext( char* text );
int16_t sg_modtext( int16_t tex, char* newtext);
void sg_texsize( int16_t tex, int* x, int* y);
void sg_drawtex( uint16_t tex , int x, int y, float ang, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void sg_drawtext( uint16_t tex , int x, int y, float ang, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void sg_clear_area(int x, int y, int w, int h);

char ptbox( int x, int y, sg_rect r); // point-box collision check
int isel(int i); // select active instrument
void astep(int step); // select active step
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */

void key_update(int key, char on);

#ifndef _3DS
struct {
	char audio_running;
	SDL_AudioDeviceID audev;
	SDL_AudioSpec auwant,auhave; // remove
	int sr;
	int format;
	int samples;
	int channels;
	SDL_Window* window;
	SDL_Renderer* renderer;
	int window_focus;
	syn* syn;
	SDL_Texture* screen_tex;
	float time; // seconds
	float dt;
} G;
#else

#include <3ds.h>
#define malloc linearAlloc
#define free linearFree
#define SDL_zero(a) memset(&(a), 0, sizeof(a));
struct {
	char audio_running;
	SDL_Surface* screen;
	int audev;

	SDL_AudioSpec auwant,auhave; // remove
	SDL_PixelFormat* pixel_format; //= RGBA8
	float r,g,b,a;
	SDL_Color color;
	Uint32 color_int;
	syn* syn;
} G;

#endif



#ifndef _3DS
	static inline size_t auformat_size( SDL_AudioFormat format){ return SDL_AUDIO_BITSIZE(format)/8; }
	int sample_rate = 48000;
	int samples_size = 256;
#else
	static inline size_t auformat_size( Uint16 format){ return 2; } // AUDIO_S16SYS
	int sample_rate = 22050;
	// int sample_rate = 32728;
	int samples_size = 256*4;
#endif

int C0 = 0 -9 -12*4;
#define MAX_KEYS (12*9)
noteid voices[MAX_KEYS];


// int testkey=0;
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
		// if(fabs(s[i])>1) printf("hard clip!\n");
		// out[i] = (short)(s[i] * (2<<14)); i++;
		out[i] = (short)(CLAMP(longbufl[getlongbuf_index(floor(i/2))], -1, 1) * (2<<14)); i++;
		// out[i] = CLAMP(longbufl[getlongbuf_index(floor(i/2))], -1, 1); i++;
		// if(fabs(s[i])>1) printf("hard clip!\n");
		// out[i] = (short)(s[i] * (2<<14)); i++;
		out[i] = (short)(CLAMP(longbufr[getlongbuf_index(floor(i/2))], -1, 1) * (2<<14)); i++;
		// out[i] = CLAMP(longbufr[getlongbuf_index(floor(i/2))], -1, 1); i++;
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
#else
int wflag = SDL_WINDOW_RESIZABLE;
#endif

SDL_Joystick* joystick=NULL;

// input decl
int sdl_event_watcher(void* udata, SDL_Event* e);
int sdl_event_watcher_3ds(const SDL_Event* e);
int32_t* kbstate; // timestamps or 0 for every ley
int kbstate_max;
void input_update(void);
int32_t kbget(SDL_Keycode);
void kbset(SDL_Keycode, int32_t);


struct {
	char b0,b1,b2; // left, right, middle
	int wx,wy; // wheel
	int wtx,wty; // wheel total
	int32_t px,py;
	int32_t dx,dy;
} Mouse;

#define MAX_FINGER 11
struct {
	char down; // 1==front, 2==back
	uint16_t px,py;
	int16_t dx,dy;
	int id;
} Finger[MAX_FINGER];












int init_systems(void){
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1"); // linear filtering

	#ifdef __vita__
		wflag = SDL_WINDOW_SHOWN;
		psvDebugScreenInit();
		screen_width=960;
		screen_height=544;
	#endif

	#ifdef _3DS
		wflag = 0;//SDL_FULLSCREEN;
		screen_width=400;
		screen_height=240;
	#endif

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
	#ifdef ANDROID
		wflag = SDL_WINDOW_BORDERLESS|SDL_WINDOW_RESIZABLE|SDL_WINDOW_FULLSCREEN;
		// SDL_DisplayMode mode;
		// SDL_GetDisplayMode(0, 0, &mode);
		// screen_width=mode.w;
		// screen_height=mode.h;
	#endif


#ifndef _3DS

int rflag = SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE;

	if((G.window = SDL_CreateWindow( wname, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			// screen_width, screen_height, wflag)) == NULL){
			BASE_SCREEN_WIDTH, BASE_SCREEN_HEIGHT, wflag)) == NULL){
		SDL_Log("E:%s\n", SDL_GetError());
		return 1;
	}
	if((G.renderer = SDL_CreateRenderer( G.window, -1, rflag)) == NULL){
		SDL_Log("E:%s\n", SDL_GetError());
		return 1;
	}

	SDL_GameControllerEventState(SDL_ENABLE);
	joystick = SDL_NumJoysticks()>0 ? SDL_JoystickOpen(0) : NULL;
#else
	// SDL_Surface* screen0 = NULL; // top screen?
	// SDL_Surface* screen1 = NULL; // bottom screen?
	G.screen = SDL_SetVideoMode( screen_width, screen_height, 32, SDL_SWSURFACE |SDL_CONSOLEBOTTOM);
	SDL_WM_SetCaption( wname, NULL );
	SDL_LockSurface(G.screen);
	G.pixel_format = G.screen->format;
	SDL_UnlockSurface(G.screen);
	SDL_Log("%i\n", G.pixel_format->BitsPerPixel);
	SDL_EnableKeyRepeat(0,0);
	joystick = SDL_JoystickOpen(0);

#endif // _3DS

	G.screen_tex = SDL_CreateTexture(G.renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, screen_width, screen_height);
	SDL_SetRenderTarget(G.renderer, G.screen_tex);
	sg_clear();
#if 1 // sdl audio
	G.auwant.freq     = sample_rate;
	// G.auwant.format   = AUDIO_F32SYS;
	G.auwant.format   = AUDIO_S16;
	G.auwant.channels = 2;
	G.auwant.samples  = samples_size;
	G.auwant.callback = maincb;
	G.auwant.userdata = G.syn;
	#ifndef _3DS
		G.audev = SDL_OpenAudioDevice(NULL, 0, &G.auwant, &G.auhave, 0/*SDL_AUDIO_ALLOW_FORMAT_CHANGE*/);
		if(G.audev == 0){
			SDL_Log("E:%s\n", SDL_GetError());
			SDL_Delay(50000);
			return 1;
		} else {
			G.audio_running = 1;
			SDL_PauseAudioDevice(G.audev, 0);
		}
	SDL_Log("sizeof syn: %li\n", sizeof(syn) );
	#else //_3DS
		G.audev = SDL_OpenAudio(&G.auwant, &G.auhave);
		if(G.audev < 0){
			SDL_Log("E:%s\n", SDL_GetError());
			SDL_Delay(50000);
			return 1;
		} else {
			G.audio_running = 1;
			SDL_PauseAudio(0);
		}
		// SDL_UnlockAudio();
		SDL_Log("auhave:\nfreq:%i\nchann:%i\nsil:%i\nsamples:%hi\nsize:%li\n", G.auhave.freq, G.auhave.channels, G.auhave.silence, G.auhave.samples, G.auhave.size);

	SDL_Log("sizeof syn: %i\n", sizeof(syn) );
	#endif

#else // sdl mixer

	Mix_Init( /*MIX_INIT_FLAC | MIX_INIT_MOD | MIX_INIT_MP3 | MIX_INIT_OGG*/ 0 );
	if ( Mix_OpenAudio ( 48000, AUDIO_S16SYS, 2, 256 ) == -1) {
		printf("Mix_openAudio: %s\n", Mix_GetError());
	}
	G.auhave.freq     = 48000;
	G.auhave.format   = AUDIO_S16SYS;
	G.auhave.channels = 2;
	G.auhave.samples  = 256;
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

	// #ifdef _3DS
	// 	SDL_SetEventFilter(sdl_event_watcher_3ds);
	// #else
	// 	SDL_SetEventFilter(sdl_event_watcher, NULL);
	// #endif

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

	SDL_SetRenderTarget(G.renderer, G.screen_tex);

	#ifdef __EMSCRIPTEN__
		emscripten_set_main_loop(main_loop, 24, 1);
	#else
		main_loop();
	#endif

	{ // quit();
		sg_quit();
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
		exit(0);

	}

	return 0;
}



















int running = 1;
int frame=0;
sg_rect fillRect;

int _isel=0;
int beat=0;
int pbeat=0;
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

/* main gui */
char gup_bpm=1;
char gup_step_add=1;
char gup_spb=1;
char gup_mix=1;
char gup_seq=1;
char gup_osc=1;
char gup_rec=1;
char gup_vkb=1;
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

int giselh=16; // height of intrument select / instrument vumeter
int giselw=16;
int gseq_basey=0;
int gseqh=32+1;
int gosc_basey=0;
int gosch=16;
// int gmodm_basex=0;
int gmodm_basey=0;
int gadsr_basex=0;
int gadst_basey=0;

int key_delay=-1;

char gui_inited =0;
void gui_init(void);

void gui_init(void){
	gui_inited=1;

	memset( bpm_text,0,16);
	memset( step_add_text,0,16);
	memset( spb_text,0,16);

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
}
















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

		beat = G.syn->seq[0].step/4;





/*----------------------------------------------------------------------------*/
/* GUI */
/*----------------------------------------------------------------------------*/

// isel
int giselendx = giselw*SYN_TONES + (giselw/8)*MAX(SYN_TONES/4-1,1);
// sg_clear_area(0,0, giselw*(SYN_TONES+SYN_TONES/8) , giselh);
sg_clear_area(0,0, giselendx , giselh);
int isel_padding=0;
for(int i=0; i<SYN_TONES; i++){
	if((i!=0) && ((i%4)==0)) isel_padding+=giselw/8;
	sg_rect r;
	r.x=isel_padding;
	r.y=0;
	r.w=giselw;
	r.h=giselh;
	if((!mlatch || (mlatch==phony_latch_isel)) && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r))
		{isel(i); mlatch=phony_latch_isel;}
	r.r=(_isel==i) * 255;
	r.g=seq_mute(G.syn->seq+i, -1)? 55: 55-r.r;
	r.b=seq_mute(G.syn->seq+i, -1)? 55: 255;
	r.a=255;
	isel_padding+=giselw;
	sg_drawperim(r);
	// VUmeter
	r.x++;
	r.w=r.w/2-2;
	r.y=giselh - (G.syn->tone[i].vupeakl * giselh)-1;
	r.h=giselh - r.y-1;
	sg_rcol(&r, MIN(G.syn->tone[i].vupeakl, 1.0), MAX(1-G.syn->tone[i].vupeakl, 0),0,1);
	sg_drawrect(r);
	r.x=r.x+r.w+2;
	r.y=giselh - (G.syn->tone[i].vupeakr * giselh)-1;
	r.h=giselh - r.y-1;
	sg_rcol(&r, MIN(G.syn->tone[i].vupeakr, 1.0), MAX(1-G.syn->tone[i].vupeakr, 0),0,1);
	sg_drawrect(r);
}
int gbpmw = 76;
// bpm mouse
if( (!mlatch || (mlatch == phony_latch_bpm)) && Mouse.b0){
	// sg_rect r = {giselw*(SYN_TONES+SYN_TONES/4)+4, 0, 70, 16, 0,0,0,0, 0,0};
	sg_rect r = {giselendx+5, 0, gbpmw, 16, 0,0,0,0, 0,0};
	if((mlatch == phony_latch_bpm) || ptbox(Mouse.px, Mouse.py, r)){
		int shift = kbget(SDLK_LSHIFT) || kbget(SDLK_RSHIFT);
		float bpm = syn_bpm(G.syn, -1);
		syn_bpm(G.syn, bpm + ((float)Mouse.dx)/( shift? 100.0 : 1.0 ));
		gup_bpm=1;
		mlatch = phony_latch_bpm;
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
	snprintf( spb_text, 8+4, "spb:%i", G.syn->seq[_isel].spb);
	sg_modtext( spb_tex, spb_text);
	sg_clear_area( giselendx+5+gbpmw, 0, 40, 8);
	sg_drawtex( spb_tex, giselendx+5+gbpmw, 0, 0, 255, 255, 255, 255);
}

// latch val
if(mlatch>(float*)100){
	if(gup_mlatch){
		gup_mlatch=0;
		snprintf( mlatch_text, 8+4, "val:%3.3f", *mlatch);
		sg_modtext( mlatch_tex, mlatch_text);
		sg_clear_area( giselendx+5, 8, gbpmw, 8);
		sg_drawtex( mlatch_tex, giselendx+5, 8, 0, 155, 255, 155, 255);
	}
}

// seq
if(gup_seq){
	gup_seq=0;
	float hlen = (float)(BASE_SCREEN_WIDTH)/SEQ_LEN;
	float vlen = (float)(gseqh)/(POLYPHONY+1);
	sg_clear_area(0, gseq_basey, BASE_SCREEN_WIDTH, (POLYPHONY+1)*vlen);
	for(int k=0; k<G.syn->seq[_isel].len; k++){
		for(int j=0; j<POLYPHONY+1; j++){
			fillRect.x=k*hlen;
			fillRect.y=j*vlen + gseq_basey;
			fillRect.w=hlen+1;
			fillRect.h=vlen+1;

			sg_rcol(&fillRect, k % G.syn->seq[_isel].spb ? 0 : .5 ,0, 1, 1);
			sg_drawperim( fillRect );
			if(j==POLYPHONY){ // modulation matrix on bottom step
				fillRect.x+=1;
				fillRect.y+=1;
				fillRect.w-=0;
				fillRect.h-=0;
				if(G.syn->seq[_isel].modm[k]){
					sg_rcol(&fillRect, 1,0,0, 1);
					sg_drawrect( fillRect );
				}
			}
			else if(G.syn->seq[_isel].freq[j][k] >0){
				fillRect.x+=1;
				fillRect.y+=1;
				fillRect.w= MAX((hlen)*G.syn->seq[_isel].dur[k]/255.f, 2);
				// fillRect.w-=1;
				fillRect.h-=2;
				sg_rcol(&fillRect, 1,1,0, 1);
				sg_drawrect( fillRect );
			}
		}
	}
}
//seq mouse selection
if((!mlatch || (mlatch==phony_latch_seq)) && Mouse.b0){
	float vlen = (float)(gseqh)/(POLYPHONY+1);
	sg_rect r = {0, gseq_basey, BASE_SCREEN_WIDTH, (POLYPHONY+1)*vlen+8, 0,0,0,0,0,0};
	if(ptbox( Mouse.px, Mouse.py, r)){
		if(Mouse.px < G.syn->seq[_isel].len * ((float)(BASE_SCREEN_WIDTH)/SEQ_LEN)){
			astep(Mouse.px/((float)(BASE_SCREEN_WIDTH)/SEQ_LEN));
			mlatch = phony_latch_seq;
		}
	}
}


//active step
int step=G.syn->seq[_isel].step;
sg_clear_area(0, gseq_basey+gseqh, BASE_SCREEN_WIDTH, 8);
sg_drawtex(seq_step_tex, (float)(BASE_SCREEN_WIDTH)/SEQ_LEN*step+1, gseq_basey+gseqh, 0, 255,255,255,255);
sg_drawtex(seq_step_tex, (float)(BASE_SCREEN_WIDTH)/SEQ_LEN*step_sel+1, gseq_basey+gseqh+4, 0, 255,0,0,255);


// osc
int tw=0,th=0;
sg_texsize(wave_tex[0], &tw, &th);
th=MAX(th, knob_size+1);
if(Mouse.b0 && !mlatch){
	for(int i = 0; i<OSC_PER_TONE; i++){
		sg_rect r = {0, gosc_basey+i*th, tw, th, 0,0,0,0, 0,0};
		if( ptbox(Mouse.px, Mouse.py, r) ){
			Mouse.b0=0;
			if(Mouse.px < tw/2) {G.syn->tone[_isel].osc[i] = MAX(G.syn->tone[_isel].osc[i]-1, 0); gup_osc=1;}
			if(Mouse.px > tw/2) {G.syn->tone[_isel].osc[i] = MIN(G.syn->tone[_isel].osc[i]+1, OSC_MAX-1); gup_osc=1;}
		}
	}
}
if(gup_osc){
	gup_osc=0;
	sg_clear_area(0, gosc_basey, tw, th+gosch*OSC_PER_TONE + th);
	for(int i =0; i<OSC_PER_TONE; i++){
		sg_drawtext(wave_tex[G.syn->tone[_isel].osc[i]], 0, gosc_basey+i*th, 0,   255,0,0,255);
	}
}
int phase_basex = tw;
{ // phase
	for(int i=0; i<OSC_PER_TONE; i++){
		sg_rect r={phase_basex , i*(knob_size+1)+gosc_basey-2, knob_size+1, knob_size+1, 0,0,0,0, 0,0};
		if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
			mlatch = &G.syn->tone[_isel].phase[i];
			mlatch_min=0;
			mlatch_max=1.0;
			mlatch_factor=0.01;
			mlatch_v=1;
			gup_mlatch=1;
		}
		sg_drawtex(knob_tex, r.x, r.y, G.syn->tone[_isel].phase[i]*300, 255,255,155,255);
	}
}
// modm
int modm_basex = phase_basex+knob_size;
for(int i=0; i<OSC_PER_TONE; i++){
	for(int j=0; j<OSC_PER_TONE; j++){
		if(j>i)break;
		sg_rect r={j*(knob_size+1)+modm_basex, i*(knob_size+1)+gosc_basey-2, knob_size+1, knob_size+1, 0,0,0,0, 0,0};
		if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
			mlatch = syn_modm_addr( &(G.syn->tone[_isel].mod_mat), i, j);
			mlatch_min= j==i ? 1 : 0;
			mlatch_max= j==i ? 30 : 90;
			mlatch_factor = 1.0;
			mlatch_v=1;
			gup_mlatch=1;
		}
		if(j==i){
			sg_drawtex(knob_tex, r.x, r.y, (tone_frat(G.syn->tone+_isel, j, -1)-1)*3*3.9f, 255,155,155,255);
		} else {
			sg_drawtex(knob_tex, r.x, r.y, tone_index(G.syn->tone+_isel, i, j, -1)*3.9f, 255,255,255,255);
		}
	}
}
// omix
for(int i=0; i<OSC_PER_TONE; i++){
	sg_rect r={modm_basex+(knob_size+1)*OSC_PER_TONE, i*(knob_size+1)+gosc_basey-2, knob_size+1, knob_size+1, 0,0,0,0, 0,0};
	if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
		mlatch = syn_modm_addr( &(G.syn->tone[_isel].mod_mat), i, -1);
		mlatch_min=0;
		mlatch_max=1.0;
		mlatch_factor=0.01;
		mlatch_v=1;
		gup_mlatch=1;
	}
	sg_drawtex(knob_tex, r.x, r.y, tone_omix(G.syn->tone+_isel, i, -1)*300, 155,155,255,255);
}
// envelopes
int ampenv_basex = modm_basex + (knob_size+1)*OSC_PER_TONE+knob_size+1 +knob_size/4;
for(int i=0; i<OSC_PER_TONE; i++){
	for(int j=0; j<4; j++){
		sg_rect r={j*(knob_size+1)+ampenv_basex, i*(knob_size+1)+gosc_basey-2, knob_size+1, knob_size+1, 0,0,0,0, 0,0};
		if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
			switch(j){
				case 0: mlatch = &(G.syn->tone[_isel].osc_env[i].a); mlatch_adsr=j; mlatch_adsr_osc=i; break;
				case 1: mlatch = &(G.syn->tone[_isel].osc_env[i].d); mlatch_adsr=j; mlatch_adsr_osc=i; break;
				case 2: mlatch = &(G.syn->tone[_isel].osc_env[i].s); mlatch_adsr=j; mlatch_adsr_osc=i; break;
				case 3: mlatch = &(G.syn->tone[_isel].osc_env[i].r); mlatch_adsr=j; mlatch_adsr_osc=i; break;
			}
			mlatch_min= j==2? 0.0: 0.001;
			mlatch_max= j==2? 1.0 : j==3? 10.0 : 3.0;
			mlatch_factor= j==3? .1 : j==2? 0.01 : 0.05;
			mlatch_v=1;
		}
		switch(j){
			case 0: sg_drawtex(knob_tex, r.x, r.y, G.syn->tone[_isel].osc_env[i].a * 310/3 , 255,155,155,255); break;
			case 1: sg_drawtex(knob_tex, r.x, r.y, G.syn->tone[_isel].osc_env[i].d * 310/3 , 255,255,255,255); break;
			case 2: sg_drawtex(knob_tex, r.x, r.y, G.syn->tone[_isel].osc_env[i].s * 310   , 155,155,255,255); break;
			case 3: sg_drawtex(knob_tex, r.x, r.y, G.syn->tone[_isel].osc_env[i].r * 310/10, 155,255,155,255); break;
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
			if(G.syn->seq[_isel].modm[step_sel]==NULL){
				seq_modm(G.syn->seq+_isel, &(G.syn->tone[_isel].mod_mat), step_sel);
				gup_seq=1;
			}
			mlatch = syn_modm_addr( G.syn->seq[_isel].modm[step_sel], i, j );
			mlatch_min= j==i ? 1 : 0;
			mlatch_max= j==i ? 30 : 90;
			mlatch_factor = 1.0;
			mlatch_v=1;
			gup_mlatch=1;
		}
		if(j==i){
			if(G.syn->seq[_isel].modm[step_sel]==NULL)
				sg_drawtex(knob_tex, r.x, r.y, (tone_frat(G.syn->tone+_isel, i, -1)-1)*3*3.9f, 55,55,55,255);
			else
				sg_drawtex(knob_tex, r.x, r.y, ((*syn_modm_addr(G.syn->seq[_isel].modm[step_sel], i, i))-1)*3*3.9, 255,155,155,255);
		} else {
			if(G.syn->seq[_isel].modm[step_sel]==NULL)
				sg_drawtex(knob_tex, r.x, r.y, tone_index(G.syn->tone+_isel, i, j, -1)*3.9f, 55,55,55,255);
			else
				sg_drawtex(knob_tex, r.x, r.y, ((*syn_modm_addr(G.syn->seq[_isel].modm[step_sel], i, j))-1)*3.9, 255,255,255,255);
		}
	}
}
// omix target
for(int i=0; i<OSC_PER_TONE; i++){
	sg_rect r={modm_target_basex+(knob_size+1)*OSC_PER_TONE, i*(knob_size+1)+gosc_basey-2, knob_size+1, knob_size+1, 0,0,0,0, 0,0};
	if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
		if(G.syn->seq[_isel].modm[step_sel]==NULL){
			seq_modm(G.syn->seq+_isel, &(G.syn->tone[_isel].mod_mat), step_sel);
			gup_seq=1;
		}
		mlatch = syn_modm_addr( G.syn->seq[_isel].modm[step_sel], i, -1 );

		mlatch_min=0;
		mlatch_max=1.0;
		mlatch_factor=0.01;
		mlatch_v=1;
		gup_mlatch=1;
	}
	if(G.syn->seq[_isel].modm[step_sel]==NULL)
		sg_drawtex(knob_tex, r.x, r.y, tone_omix(G.syn->tone+_isel, i, -1)*300, 55,55,55,255);
	else
		sg_drawtex(knob_tex, r.x, r.y, *syn_modm_addr(G.syn->seq[_isel].modm[step_sel], i, -1)*300, 155,155,255,255);
}
// tone gain
int gain_basex = modm_target_basex + (knob_size+1)*(OSC_PER_TONE)+knob_size+1 +knob_size/2;
{
	sg_rect r={gain_basex, gosc_basey-2, knob_size+1, knob_size+1, 0,0,0,0, 0,0};
	if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
		mlatch = &G.syn->tone[_isel].gain;
		mlatch_min=0;
		mlatch_max=1;
		mlatch_factor=0.01;
		mlatch_v=1;
		gup_mlatch=1;
	}
	sg_drawtex(knob_tex, r.x, r.y, G.syn->tone[_isel].gain*300, 255,5,5,255);
}
int penv_adsr_basex = gain_basex;
for(int j=0; j<4; j++){
	// if(j>i)break;
	sg_rect r={penv_adsr_basex, (j+1)*(knob_size+1)+gosc_basey-2, knob_size+1, knob_size+1, 0,0,0,0, 0,0};
	if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
		switch(j){
			case 0: mlatch = &(G.syn->tone[_isel].pitch_env.a); mlatch_adsr=j; break;
			case 1: mlatch = &(G.syn->tone[_isel].pitch_env.d); mlatch_adsr=j; break;
			case 2: mlatch = &(G.syn->tone[_isel].pitch_env.s); mlatch_adsr=j; break;
			case 3: mlatch = &(G.syn->tone[_isel].pitch_env.r); mlatch_adsr=j; break;
		}
		mlatch_min=0.001;
		mlatch_max= j==2? 1.0 : j==3? 10.0 : 3.0;
		mlatch_factor= j==3? .1 : j==2? 0.01 : 0.05;
		mlatch_v=1;
		mlatch_adsr_pitch=1;
	}
	switch(j){
		case 0: sg_drawtex(knob_tex, r.x, r.y, G.syn->tone[_isel].pitch_env.a * 310/3 , 255,155,155,255); break;
		case 1: sg_drawtex(knob_tex, r.x, r.y, G.syn->tone[_isel].pitch_env.d * 310/3 , 255,255,255,255); break;
		case 2: sg_drawtex(knob_tex, r.x, r.y, G.syn->tone[_isel].pitch_env.s * 310   , 155,155,255,255); break;
		case 3: sg_drawtex(knob_tex, r.x, r.y, G.syn->tone[_isel].pitch_env.r * 310/10, 155,255,155,255); break;
	}
}
// pitch env amt
{
	sg_rect r={penv_adsr_basex, (5)*(knob_size+1)+gosc_basey-2, knob_size+1, knob_size+1, 0,0,0,0, 0,0};
	if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
		mlatch=&G.syn->tone[_isel].pitch_env_amt;
		mlatch_min=0.0;
		mlatch_max= 15;
		mlatch_factor= 0.05;
		mlatch_v=1;
		mlatch_adsr_pitch=1;
	}
	sg_drawtex(knob_tex, r.x, r.y, G.syn->tone[_isel].pitch_env_amt * 310/15, 255,255,155,255);
}

// quit dialog
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
	if(kbget(SDLK_ESCAPE) || click_no || (Mouse.b0 && !click_yes && !ptbox(Mouse.px, Mouse.py, r)) ) { request_quit=0; sg_clear_area(r.x, r.y, r.w, r.h); }
	if(kbget(SDLK_RETURN) || click_yes) running=0;
}

// virtual keyboard
int vkb_h = 32;
int vkb_basey = BASE_SCREEN_HEIGHT-vkb_h;
int vkb_keys = 12*2+1;
int vkb_endx = 0;
if(gup_vkb || Mouse.py<= vkb_basey){
	sg_rect r = {0, vkb_basey, BASE_SCREEN_WIDTH, vkb_h, 255, 255,255,255, 0,0};
	if(gup_vkb) sg_clear_area(r.x, r.y, r.w, r.h);
	r.w=((float)BASE_SCREEN_WIDTH)/vkb_keys;
	// r.x+=2;
	for(int o = 0 ; o < 3; o++ ){
		int notei=0;
		for(int i = 0 ; i <= 13; i++){
			if(o>=2 && i>=1) break;

			char black = i%2;
			// r.h = black? vkb_h/2 : vkb_h;
			r.r = black? 55 : 255;
			r.g = black? 55 : 255;
			r.b = black? 55 : 255;

			if(i!=0 && (i==5 || i==13)) continue;
			char collision = (!mlatch || mlatch==phony_latch_vkb) && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r);
			if(collision || voices[((-9+notei+o*12)-C0)%MAX_KEYS].tone != -1 ) r.b=0;

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
		// sg_clear_area( 2+sx, vkb_basey-rsx, 40, 8);
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





/*----------------------------------------------------------------------------*/
/* end GUI */
/*----------------------------------------------------------------------------*/

if (kbget(SDLK_LEFT )){
	key_delay++;
	if(key_delay==0){ key_delay=-6;
		if(kbget(SDLK_RCTRL)||kbget(SDLK_LCTRL)){
			G.syn->seq[_isel].len=MAX(G.syn->seq[_isel].len-1, 1)      ;gup_seq=1;}
		else astep( step_sel-1);
	}
}
if (kbget(SDLK_RIGHT)){
	key_delay++;
	if(key_delay==0){ key_delay=-6;
		if(kbget(SDLK_RCTRL)||kbget(SDLK_LCTRL)){
			G.syn->seq[_isel].len=MIN(G.syn->seq[_isel].len+1, SEQ_LEN);gup_seq=1;}
		else astep( step_sel+1);
	}
}
if (kbget(SDLK_UP)){ //kbset(SDLK_UP, 0);
	key_delay++;
	if(key_delay==0){ key_delay=-6;
		if(kbget(SDLK_RCTRL)||kbget(SDLK_LCTRL)){
			G.syn->seq[_isel].spb=MIN(G.syn->seq[_isel].spb+1, SEQ_LEN);
			gup_spb=1;
			gup_seq=1;
		} else {step_add++;gup_step_add=1;}
	}
}
if (kbget(SDLK_DOWN )){ //kbset(SDLK_DOWN, 0);
	key_delay++;
	if(key_delay==0){ key_delay=-6;
		if(kbget(SDLK_RCTRL)||kbget(SDLK_LCTRL)){
			G.syn->seq[_isel].spb=MAX(G.syn->seq[_isel].spb-1, 1);
			gup_spb=1;
			gup_seq=1;
		} else if(step_add>0) {step_add--; gup_step_add=1;}
	}
}


if(kbget(SDLK_DELETE)){
	if(!(kbget(SDLK_RALT) || kbget(SDLK_LALT) || kbget(SDLK_ALTERASE)))
		seq_anof(G.syn->seq+_isel, step_sel);
	seq_modm(G.syn->seq+_isel, NULL, step_sel); gup_seq=1;
}

if(kbget(SDLK_EQUALS)){
	if(kbget(SDLK_LSHIFT) || kbget(SDLK_RSHIFT)){
		kbset(SDLK_EQUALS, 0);
		octave++;
	}
}
if(kbget(SDLK_MINUS)){
	if(kbget(SDLK_LSHIFT) || kbget(SDLK_RSHIFT)){
		kbset(SDLK_MINUS, 0);
		octave--;
	}
}
// case '-': if(kbget(SDLK_LSHIFT) || kbget(SDLK_RSHIFT)) octave--; break;
// case '=': if(kbget(SDLK_LSHIFT) || kbget(SDLK_RSHIFT)) octave++; break;
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

		frame++;
		pbeat = beat;
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
syn_lock(G.syn, 1); // todo make a finer lock
	switch(e.type){
		// case SDL_QUIT: running=0; break;
		case SDL_QUIT:
			if(!request_quit){
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
			if(e.key.repeat) break;
			kbstate[ e.key.keysym.scancode % kbstate_max ] = e.key.timestamp;
			key_delay=-1;

			switch(e.key.keysym.sym){
				#ifdef ANDROID
					case SDLK_AC_BACK: request_quit=1; break;
				#endif
				case SDLK_MENU: syn_pause(G.syn); break;

				case SDLK_ESCAPE:
					if(!request_quit){
						kbset(SDLK_ESCAPE, 0);
						kbset(SDLK_RETURN, 0);
						request_quit =1;
					}
					break;
					// seq_mode = !seq_mode;
					// syn_anof(G.syn, _isel); isel(0); break;

				case SDLK_F1:  if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq+0, !seq_mute(G.syn->seq+0, -1));} else isel(0); break;
				case SDLK_F2:  if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq+1, !seq_mute(G.syn->seq+1, -1));} else isel(1); break;
				case SDLK_F3:  if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq+2, !seq_mute(G.syn->seq+2, -1));} else isel(2); break;
				case SDLK_F4:  if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq+3, !seq_mute(G.syn->seq+3, -1));} else isel(3); break;
				case SDLK_F5:  if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq+4, !seq_mute(G.syn->seq+4, -1));} else isel(4); break;
				case SDLK_F6:  if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq+5, !seq_mute(G.syn->seq+5, -1));} else isel(5); break;
				case SDLK_F7:  if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq+6, !seq_mute(G.syn->seq+6, -1));} else isel(6); break;
				case SDLK_F8:  if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq+7, !seq_mute(G.syn->seq+7, -1));} else isel(7); break;
				case SDLK_F9:  if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq+8, !seq_mute(G.syn->seq+8, -1));} else isel(8); break;
				case SDLK_F10: if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq+9, !seq_mute(G.syn->seq+9, -1));} else isel(9); break;
				case SDLK_F11: if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq+10, !seq_mute(G.syn->seq+10, -1));} else isel(10); break;
				case SDLK_F12: if(kbget(SDLK_SPACE)){ seq_mute(G.syn->seq+11, !seq_mute(G.syn->seq+11, -1));} else isel(11); break;

				case SDLK_RETURN: syn_pause(G.syn); break;
				case SDLK_PAUSE: syn_pause(G.syn); break;
				case SDLK_CAPSLOCK: rec=!rec; gup_rec=1; break;

				case SDLK_HOME: astep(0); break;
				case SDLK_END: astep(G.syn->seq[_isel].len-1); break;

				case SDLK_INSERT: astep(step_sel + step_add); break;

#if 1
				case 'q': key_update(-9+0 +12, 1); break; //do 4
				case '2': key_update(-9+1 +12, 1); break;
				case 'w': key_update(-9+2 +12, 1); break;
				case '3': key_update(-9+3 +12, 1); break;
				case 'e': key_update(-9+4 +12, 1); break;
				case 'r': key_update(-9+5 +12, 1); break;
				case '5': key_update(-9+6 +12, 1); break;
				case 't': key_update(-9+7 +12, 1); break;
				case '6': key_update(-9+8 +12, 1); break; //lab
				case 'y': key_update(-9+9 +12, 1); break; //la
				case '7': key_update(-9+10+12, 1); break; //sib
				case 'u': key_update(-9+11+12, 1); break; //si
				case 'i': key_update(-9+12+12, 1); break; //do 5
				case '9': key_update(-9+13+12, 1); break;
				case 'o': key_update(-9+14+12, 1); break;
				case '0': key_update(-9+15+12, 1); break;
				case 'p': key_update(-9+16+12, 1); break;
				case '[': key_update(-9+17+12, 1); break;
				case '=': if(! (kbget(SDLK_LSHIFT) || kbget(SDLK_RSHIFT)) ) key_update(-9+18+12, 1); break;
				case ']': key_update(-9+19+12, 1); break;

				case 'z': key_update(-9+0, 1); break; //do 3
				case 's': key_update(-9+1, 1); break;
				case 'x': key_update(-9+2, 1); break;
				case 'd': key_update(-9+3, 1); break;
				case 'c': key_update(-9+4, 1); break;
				case 'v': key_update(-9+5, 1); break;
				case 'g': key_update(-9+6, 1); break;
				case 'b': key_update(-9+7, 1); break;
				case 'h': key_update(-9+8, 1); break; //lab
				case 'n': key_update(-9+9, 1); break; //la
				case 'j': key_update(-9+10, 1); break; //sib
				case 'm': key_update(-9+11, 1); break; //si
				case ',': key_update(-9+12, 1); break; //do 4
				case 'l': key_update(-9+13, 1); break;
				case '.': key_update(-9+14, 1); break;
				case ';': key_update(-9+15, 1); break;
				case '/': key_update(-9+16, 1); break;

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
			break;
		case SDL_KEYUP:
			if(e.key.repeat) break;
			kbstate[ e.key.keysym.scancode % kbstate_max ] = 0;
			switch(e.key.keysym.sym){
#if 1
				case 'q': key_update(-9+0 +12, 0); break; //do 4
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
				case 'i': key_update(-9+12+12, 0); break; // do 5
				case '9': key_update(-9+13+12, 0); break;
				case 'o': key_update(-9+14+12, 0); break;
				case '0': key_update(-9+15+12, 0); break;
				case 'p': key_update(-9+16+12, 0); break;
				case '[': key_update(-9+17+12, 0); break;
				case '=': if(! (kbget(SDLK_LSHIFT) || kbget(SDLK_RSHIFT)) ) key_update(-9+18+12, 0); break;
				case ']': key_update(-9+19+12, 0); break;

				case 'z': key_update(-9+0, 0); break; //do 3
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
				case ',': key_update(-9+12, 0); break; //do 4
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
			Mouse.dx = e.motion.xrel;
			Mouse.dy = e.motion.yrel;
			if(mlatch>(float*)100){
				gup_mlatch=1;
				char shift = kbget(SDLK_LSHIFT) || kbget(SDLK_RSHIFT);
				if(mlatch_v) *mlatch = CLAMP(*mlatch - Mouse.dy * (shift?mlatch_factor/10.0:mlatch_factor), mlatch_min, mlatch_max);
				else         *mlatch = CLAMP(*mlatch - Mouse.dx * (shift?mlatch_factor/10.0:mlatch_factor), mlatch_min, mlatch_max);

				if(mlatch_adsr >= 0){
					if(!mlatch_adsr_pitch){
						switch(mlatch_adsr){
							case 0: adsr_a(G.syn->tone[_isel].osc_env + mlatch_adsr_osc, *mlatch); break;
							case 1: adsr_d(G.syn->tone[_isel].osc_env + mlatch_adsr_osc, *mlatch); break;
							case 2: adsr_s(G.syn->tone[_isel].osc_env + mlatch_adsr_osc, *mlatch); break;
							case 3: adsr_r(G.syn->tone[_isel].osc_env + mlatch_adsr_osc, *mlatch); break;
							default: break;
						}
					} else {
						switch(mlatch_adsr){
							case 0: adsr_a(&G.syn->tone[_isel].pitch_env, *mlatch); break;
							case 1: adsr_d(&G.syn->tone[_isel].pitch_env, *mlatch); break;
							case 2: adsr_s(&G.syn->tone[_isel].pitch_env, *mlatch); break;
							case 3: adsr_r(&G.syn->tone[_isel].pitch_env, *mlatch); break;
							default: break;
						}
					}
				}
			} break;

		case SDL_MOUSEBUTTONDOWN:
			switch(e.button.button){
				case SDL_BUTTON_LEFT:   Mouse.b0 = 1; break;
				case SDL_BUTTON_RIGHT:  Mouse.b1 = 1; break;
				case SDL_BUTTON_MIDDLE: Mouse.b2 = 1; break;
				default: break;
			} break;

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
			mlatch=NULL;
			mlatch_adsr=-1;
			mlatch_adsr_pitch=0;
			break;


		case SDL_MOUSEWHEEL:
			Mouse.wx = e.wheel.x;
			Mouse.wy = e.wheel.y;
			Mouse.wtx += e.wheel.x;
			Mouse.wty += e.wheel.y;
		break;

		default: break;
	}
syn_lock(G.syn, 0);

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
// static int sg_font_size = 22;

#ifndef _3DS
static TTF_Font* sg_font=NULL;
#include "elm.h"
#else
static C2D_Font sg_font;
#include "elbcfnt.h"
#endif

//backend functions
//void draw_target( char ); // 3ds only 0 for top screen, 1 for bottom
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
		// SDL_RWops* ttffile = SDL_RWFromMem( elttf_data, sizeof(elttf_data) );
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
	draw_color(0,0,0,255);
	draw_clear();
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

void sg_drawtex( uint16_t tex , int x, int y, float ang, uint8_t r, uint8_t g, uint8_t b, uint8_t a){
	if(x > BASE_SCREEN_WIDTH || y > BASE_SCREEN_HEIGHT || x+sg_texs[tex].w < 0 || y+sg_texs[tex].h < 0)
		return;
	sg_rect t;
	t.x=x;
	t.y=y;
	t.r=r;
	t.g=g;
	t.b=b;
	t.a=a;
	t.w=sg_texs[tex].w;
	t.h=sg_texs[tex].h;
	t.tid=tex;
	t.ang = ang;
	sg_drawr(t);
	// if(!sg_was_init) sg_init();
	// if(sg_rect_count >= sg_rect_capacity){
	// 	sg_rect_capacity*=2;
	// 	sg_rects = realloc(sg_rects, sizeof(sg_rect)*sg_rect_capacity);
	// }
	// sg_rects[sg_rect_count] = t;
	// sg_rect_count++;
}

void sg_drawtext( uint16_t tex , int x, int y, float ang, uint8_t r, uint8_t g, uint8_t b, uint8_t a){
#ifndef _3DS
	sg_drawtex(tex,x,y,ang,r,g,b,a);
#else
#endif
}






















/*----------------------------------------------------------------------------*/
/*sg backend*/
/*----------------------------------------------------------------------------*/
void draw_show( void ){
	#ifndef _3DS
		SDL_SetRenderTarget(G.renderer, NULL);
		sg_clear();

		SDL_RenderCopy(G.renderer, G.screen_tex, NULL, NULL);
		SDL_RenderPresent(G.renderer);
		SDL_SetRenderTarget(G.renderer, G.screen_tex);

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
		// SDL_Point center = {((float)sg_texs[tid].w)/2.0, ((float)sg_texs[tid].h)/2.0};
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
		mlatch = NULL;

		for(int i=0; i<MAX_KEYS; i++){
			syn_nof(G.syn, voices[i]);
			voices[i] = (noteid){-1,-1};
		}
	}
	return ret;
}

void astep(int step){
	step = step % G.syn->seq[_isel].len;
	if(step<0) step = G.syn->seq[_isel].len-1;
	int prev_step = step_sel;
	step_sel = step;
	// check and update mlatch
	if(mlatch > (float*)100 && G.syn->seq[_isel].modm[prev_step]!=NULL){
		if(mlatch >= (float*)G.syn->seq[_isel].modm[prev_step] &&
				mlatch <= syn_modm_addr(G.syn->seq[_isel].modm[prev_step], OSC_PER_TONE-1, OSC_PER_TONE-1 )){
			if(G.syn->seq[_isel].modm[step_sel] == NULL){
				seq_modm(G.syn->seq+_isel, G.syn->seq[_isel].modm[prev_step], step_sel);
				gup_seq=1;
			}
			float prev_val = *mlatch;
			mlatch = (float*) ((intptr_t)mlatch - (intptr_t)syn_modm_addr(G.syn->seq[_isel].modm[prev_step], 0,0));
			mlatch = (float*) ((intptr_t)mlatch + (intptr_t)syn_modm_addr(G.syn->seq[_isel].modm[step_sel], 0,0));
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
	// if(!voices_inited)init_voices();

	int keyi = (key-C0)%MAX_KEYS;
	if(keyi<0) keyi+=MAX_KEYS-1;

	float target_note=0;

	if(on && voices[keyi].voice != -1){
return;
	}

	if(on){
		target_note = key+12*octave;
		voices[keyi] = syn_non(G.syn, _isel, key +12*octave, testvelocity);
	} else {
		if(rec) voice_count=MAX(voice_count-1, 0);
		syn_nof(G.syn, voices[keyi] );
		voices[keyi] = (noteid){-1,-1};
	}

	// if(on && commit_count<POLYPHONY && rec) {
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
				seq_anof(G.syn->seq+_isel, s);
				for(int i=0; i<commit_count; i++){
					seq_non(G.syn->seq+_isel, s, note_commit[i], .9, s<commit_step_end? 1.0 : .5);
				}
			}
		} else {
			seq_anof(G.syn->seq+_isel, step_sel);
			for(int i=0; i<commit_count; i++){
				seq_non(G.syn->seq+_isel, step_sel, note_commit[i], .9, .5);
			}
		}

		astep(step_sel + step_add);
		commit_count=0;
		gup_seq=1;
	}

}

