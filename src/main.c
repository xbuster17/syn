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
#define BASE_SCREEN_WIDTH 400
#define BASE_SCREEN_HEIGHT 240
#endif

static int screen_width =BASE_SCREEN_WIDTH;
static int screen_height=BASE_SCREEN_HEIGHT;
int testoctave=0;

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

void key_update(char key, char on);

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


noteid voices[SYN_TONES][POLYPHONY+40];
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
	if(index<0 || index>=longbuf_len) printf("wrong index %i %i:%s\n", index, __LINE__, __func__);
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
		printf("%i:%s\n", i, SDL_GetAudioDeviceName(i,0));
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
	uint16_t px,py;
	int16_t dx,dy;
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

	#ifdef ANDROID
		wflag = SDL_WINDOW_RESIZABLE|SDL_WINDOW_FULLSCREEN;
		SDL_DisplayMode mode;
		SDL_GetDisplayMode(0, 0, &mode);
		screen_width=mode.w;
		screen_height=mode.h;
	#endif


#ifndef _3DS
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
	if((G.window = SDL_CreateWindow( wname, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			screen_width, screen_height, wflag)) == NULL){
		printf("E:%s\n", SDL_GetError());
		return 1;
	}
	if((G.renderer = SDL_CreateRenderer( G.window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE)) == NULL){
		printf("E:%s\n", SDL_GetError());
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
	printf("%i\n", G.pixel_format->BitsPerPixel);
	SDL_EnableKeyRepeat(0,0);
	joystick = SDL_JoystickOpen(0);

#endif // _3DS

	G.screen_tex = SDL_CreateTexture(G.renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, screen_width, screen_height);

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
			printf("E:%s\n", SDL_GetError());
			SDL_Delay(50000);
			return 1;
		} else {
			G.audio_running = 1;
			SDL_PauseAudioDevice(G.audev, 0);
		}
	printf("sizeof syn: %li\n", sizeof(syn) );
	#else //_3DS
		G.audev = SDL_OpenAudio(&G.auwant, &G.auhave);
		if(G.audev < 0){
			printf("E:%s\n", SDL_GetError());
			SDL_Delay(50000);
			return 1;
		} else {
			G.audio_running = 1;
			SDL_PauseAudio(0);
		}
		// SDL_UnlockAudio();
		printf("auhave:\nfreq:%i\nchann:%i\nsil:%i\nsamples:%hi\nsize:%li\n", G.auhave.freq, G.auhave.channels, G.auhave.silence, G.auhave.samples, G.auhave.size);

	printf("sizeof syn: %i\n", sizeof(syn) );
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

	#ifdef _3DS
		SDL_SetEventFilter(sdl_event_watcher_3ds);
	#else
		SDL_SetEventFilter(sdl_event_watcher, NULL);
	#endif

	return 0;

}




void test_add_notes(void);
int testtext=0;
// int SDL_main(int argc, char**argv){ (void)argv, (void)argc;

void main_loop(void);
int main(int argc, char**argv){ (void)argv, (void)argc;

	memset(&G, 0, sizeof(G));
	G.syn = malloc( sizeof (syn) );
	memset( G.syn, 0, sizeof(syn) );
	memset( voices, 1, sizeof(voices) );


	if(!longbufl || !longbufr){
		longbufl=malloc(sizeof(float)*longbuf_len);
		longbufr=malloc(sizeof(float)*longbuf_len);
		memset(longbufl, 0, sizeof(float)*longbuf_len);
		memset(longbufr, 0, sizeof(float)*longbuf_len);
	}

	syn_init(G.syn, sample_rate);

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

int step_sel=0;
char bpm_text[16];
int16_t bpm_tex;

int step_add=0;
char step_add_text[16];
int16_t step_add_tex;

int spb=0;
char spb_text[16];
int16_t spb_tex;

/* main gui */
char gup_bpm=1;
char gup_step_add=1;
char gup_spb=1;
char gup_mix=1;
char gup_seq=1;
char gup_osc=1;
char gup_rec=1;
char rec=0;
char gup_mlatch=1; // updates the value mouse is latched to
int16_t knob_tex;
int16_t modm_tex;
int16_t label_tex[SYN_TONES][10];
int16_t wave_tex[OSC_MAX];
char wave_text[OSC_MAX][10]={
	{"< sine >"},
	{"< tri  >"},
	{"< nois >"},
	{"< sqr  >"},
	{"< puls >"},
	{"< saw  >"}
};
int16_t seq_step_tex;

int16_t rec_tex;

#define knob_size 15
#define knob_thickness 2
rgba8 knob_pixels[knob_size*knob_size];

float* mlatch = NULL; // on click target is selected
float mlatch_factor = 0.0; // mouse delta is multiplied by this
float mlatch_max = 0.0; // mlatch get's clamped to these
float mlatch_min = 0.0;
char mlatch_v=0; // use vertical delta
char mlatch_adsr=-1; // if differ from *latch, call adsr updaters
char mlatch_adsr_osc;
char mlatch_adsr_pitch=0; // set to 1 if latched to pitch envelope


char mlatch_text[16]; // holds text for the value of mouse latch
int16_t mlatch_tex; // updates the value mouse is latched to

float* phony_latch_isel= (float*)1;
float* phony_latch_seq = (float*)2;
float* phony_latch_bpm = (float*)3;

int giselh=16; // height of intrument select / instrument vumeter
int giselw=16;
int gseq_basey;
int gseqh=32;
int gosc_basey;
int gosch=16;
int gmodm_basex;
int gmodm_basey;
int gadsr_basex;
int gadst_basey;

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
			if(((y)==0) && (x>0))
				knob_pixels[i*knob_size+j].c = 0xFFFFFFFF;
			else if(r<knob_size/2-1)
				knob_pixels[i*knob_size+j].c = 0x22222222;
			if(r>knob_size/2-knob_thickness && r<knob_size/2 )
				knob_pixels[i*knob_size+j].c = 0xFFFFFFFF;
			if(r==knob_size/2-knob_thickness)
				knob_pixels[i*knob_size+j].c = 0x000000FF;
		}
	}
	seq_step_tex = sg_addtext("^");
	knob_tex = sg_addtex(knob_pixels, knob_size, knob_size);

	bpm_tex = sg_addtext("0");
	step_add_tex = sg_addtext("0");
	spb_tex = sg_addtext("0");
	mlatch_tex = sg_addtext("0");
	rec_tex = sg_addtext("REC");
// starting point of gui elements
	gseq_basey=giselh+2;
	gosc_basey=gseq_basey+gseqh+8+2/*+knob_size*/;
}


















/*----------------------------------------------------------------------------*/
/*main loop*/
/*----------------------------------------------------------------------------*/
void main_loop(void){
	if(!gui_inited) gui_init();


	while(running){
		input_update();

		beat = G.syn->seq[0].step/4;





/*----------------------------------------------------------------------------*/
/* GUI */
/*----------------------------------------------------------------------------*/

// isel
sg_clear_area(0,0, giselw*(SYN_TONES+SYN_TONES/8) , giselh);
int isel_padding=0;
for(int i=0; i<SYN_TONES; i++){
	if((i!=0) && ((i%4)==0)) isel_padding+=giselw/2;
	sg_rect r;
	r.x=isel_padding;
	r.y=0;
	r.w=giselw;
	r.h=giselh;
	if((!mlatch || mlatch==phony_latch_isel) && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r))
		{isel(i); mlatch=phony_latch_isel;}
	r.r=(_isel==i) * 255;
	r.g=55-r.r;
	r.b=255;
	r.a=255;
	isel_padding+=giselw;
	sg_drawperim(r);
	r.x++;
	r.y=1;
	r.h=MIN(G.syn->tone[i].vupeakl * giselh, giselh-2);
	sg_rcol(&r, MIN(G.syn->tone[i].vupeakl, 1.0), MAX(1-G.syn->tone[i].vupeakl, 0),0,1);
	r.w=r.w/2-1;
	sg_drawrect(r);
	r.x+=r.w;
	r.h=MIN(G.syn->tone[i].vupeakr * giselh, giselh-2);
	sg_rcol(&r, MIN(G.syn->tone[i].vupeakr, 1.0), MAX(1-G.syn->tone[i].vupeakr, 0),0,1);
	sg_drawrect(r);
}

// bpm mouse
if((!mlatch || (mlatch == phony_latch_bpm)) && Mouse.b0){
	sg_rect r = {giselw*(SYN_TONES+SYN_TONES/8)+4, 0, 70, 16, 0,0,0,0, 0,0};
	if((mlatch==phony_latch_bpm) || ptbox(Mouse.px, Mouse.py, r)){
		int shift = kbget(SDLK_LSHIFT) || kbget(SDLK_RSHIFT);
		float bpm = syn_bpm(G.syn, -1);
		syn_bpm(G.syn, bpm + Mouse.dx/( shift? 100.0 : 1 ));
		gup_bpm=1;
		mlatch = phony_latch_bpm;
	}
}
// bpm
if(gup_bpm){
	gup_bpm = 0;
	snprintf( bpm_text, 8+4+2, "bpm:%3.3f", syn_bpm(G.syn, -1));
	sg_modtext( bpm_tex, bpm_text);
	sg_clear_area( giselw*(SYN_TONES+SYN_TONES/8)+4, 0, 66, 8);
	sg_drawtex( bpm_tex, giselw*(SYN_TONES+SYN_TONES/8)+4, 0, 0, 255, 255, 255, 255);
}
// pattern step add
if(gup_step_add){
	gup_step_add = 0;
	snprintf( step_add_text, 8+4, "add:%i", step_add);
	sg_modtext( step_add_tex, step_add_text);
	sg_clear_area( giselw*(SYN_TONES+SYN_TONES/8)+4, 8, 64, 8);
	sg_drawtex( step_add_tex, giselw*(SYN_TONES+SYN_TONES/8)+4, 8, 0, 255, 155, 155, 255);
}
// pattern steps per beat
if(gup_spb){
	gup_spb = 0;
	snprintf( spb_text, 8+4, "spb:%i", G.syn->seq[_isel].spb);
	sg_modtext( spb_tex, spb_text);
	sg_clear_area( giselw*(SYN_TONES+SYN_TONES/8)+4+76, 0, 40, 8);
	sg_drawtex( spb_tex, giselw*(SYN_TONES+SYN_TONES/8)+4+76, 0, 0, 255, 255, 255, 255);
}
// latch val
if(mlatch>(float*)100){
	if(gup_mlatch){
		gup_mlatch=0;
		snprintf( mlatch_text, 8+4, "val:%3.3f", *mlatch);
		sg_modtext( mlatch_tex, mlatch_text);
		sg_clear_area( giselw*(SYN_TONES+SYN_TONES/8)+4+76+50, 0, 128, 16);
		sg_drawtex( mlatch_tex, giselw*(SYN_TONES+SYN_TONES/8)+4+76+50, 0, 0, 155, 255, 155, 255);
	}
}
// rec label
if(gup_rec){
	gup_rec = 0;

	sg_clear_area( giselw*(SYN_TONES+SYN_TONES/8)+4+76, 8, 40, 8);
	sg_drawtex( rec_tex, giselw*(SYN_TONES+SYN_TONES/8)+4+76, 8, 0, 255*rec, 55, 55, 255);
}
{
	sg_rect r={giselw*(SYN_TONES+SYN_TONES/8)+4+76, 8, 40, 16, 0,0,0,0, 0,0};
	if(!mlatch && Mouse.b0 && ptbox(Mouse.px, Mouse.py, r)){
		Mouse.b0=0;
		gup_rec=1;
		rec=!rec;
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
			fillRect.w=hlen-1;
			fillRect.h=vlen-1;

			sg_rcol(&fillRect, 0,0,1, 1);
			sg_drawperim( fillRect );
			if(j==POLYPHONY){
				fillRect.x+=1;
				fillRect.y+=1;
				fillRect.w-=2;
				fillRect.h-=2;
				if(G.syn->seq[_isel].modm[k]){
					sg_rcol(&fillRect, 1,0,0, 1);
					sg_drawrect( fillRect );
				}
			}
			else if(G.syn->seq[_isel].freq[j][k] >0){
				fillRect.x+=1;
				fillRect.y+=1;
				fillRect.w*=G.syn->seq[_isel].dur[k]/255.f;
				fillRect.w--;
				fillRect.h-=2;
				sg_rcol(&fillRect, 1,1,0, 1);
				sg_drawrect( fillRect );
			}
		}
	}
}
//seq mouse selection
if((!mlatch || mlatch==phony_latch_seq) && Mouse.b0){
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
// modm
int modm_basex = tw+2;
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
int ampenv_basex = modm_basex + (knob_size+1)*OSC_PER_TONE+knob_size+1 +knob_size/2;
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
			mlatch_min=0.001;
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
int modm_target_basex = ampenv_basex + 4*(knob_size+1) + knob_size/2;
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
if (kbget(SDLK_UP   )){ kbset(SDLK_UP, 0);
	if(kbget(SDLK_RCTRL)||kbget(SDLK_LCTRL)){
		G.syn->seq[_isel].spb=MIN(G.syn->seq[_isel].spb+1, SEQ_LEN);gup_spb=1;}
	else {step_add++;gup_step_add=1;}
}
if (kbget(SDLK_DOWN )){ kbset(SDLK_DOWN, 0);
	if(kbget(SDLK_RCTRL)||kbget(SDLK_LCTRL)){
		G.syn->seq[_isel].spb=MAX(G.syn->seq[_isel].spb-1, 1)      ;gup_spb=1;}
	else if(step_add>0) {step_add--; gup_step_add=1;}
}

if(kbget(SDLK_DELETE)){
	if(!(kbget(SDLK_RALT) || kbget(SDLK_LALT) || kbget(SDLK_ALTERASE)))
		seq_anof(G.syn->seq+_isel, step_sel);
	seq_modm(G.syn->seq+_isel, NULL, step_sel); gup_seq=1;
}




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
		// SDL_Delay(60);
	#endif
}























/*----------------------------------------------------------------------------*/
/*input*/
/*----------------------------------------------------------------------------*/

void input_update(void){
	Mouse.dx=Mouse.dy=0;
	Mouse.wx=Mouse.wy=0;
	SDL_PumpEvents();
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
		case SDL_QUIT: running=0; break;

#ifndef _3DS
		case SDL_WINDOWEVENT:
			switch(e.window.event){
				case SDL_WINDOWEVENT_SIZE_CHANGED:
				case SDL_WINDOWEVENT_RESIZED:
					screen_width = e.window.data1;
					screen_height= e.window.data2;
					break;
				case SDL_WINDOWEVENT_CLOSE: running=0;
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
					case SDLK_AC_BACK: running=0; break;
				#endif
				case SDLK_MENU: syn_pause(G.syn); break;

				case SDLK_ESCAPE:
					running=0;
					seq_mode = !seq_mode;
					syn_anof(G.syn, _isel); isel(0); break;

				case SDLK_F1:  isel(0); break;
				case SDLK_F2:  isel(1); break;
				case SDLK_F3:  isel(2); break;
				case SDLK_F4:  isel(3); break;
				case SDLK_F5:  isel(4); break;
				case SDLK_F6:  isel(5); break;
				case SDLK_F7:  isel(6); break;
				case SDLK_F8:  isel(7); break;
				case SDLK_F9:  isel(8); break;
				case SDLK_F10: isel(9); break;
				case SDLK_F11: isel(10); break;
				case SDLK_F12: isel(11); break;

				case SDLK_RETURN: syn_pause(G.syn); break;
				case SDLK_SPACE: rec=!rec; gup_rec=1; break;

				case SDLK_HOME: astep(0); break;
				case SDLK_END: astep(G.syn->seq[_isel].len-1); break;

				case SDLK_INSERT: astep(step_sel + step_add); break;


				case 'q': key_update('q', 1); break; //do 4
				case '2': key_update('2', 1); break;
				case 'w': key_update('w', 1); break;
				case '3': key_update('3', 1); break;
				case 'e': key_update('e', 1); break;
				case 'r': key_update('r', 1); break;
				case '5': key_update('5', 1); break;
				case 't': key_update('t', 1); break;
				case '6': key_update('6', 1); break; //lab
				case 'y': key_update('y', 1); break; //la
				case '7': key_update('7', 1); break; //sib
				case 'u': key_update('u', 1); break; //si
				case 'i': key_update('i', 1); break; //do 5

				case 'z': key_update('z', 1); break; //do 3
				case 's': key_update('s', 1); break;
				case 'x': key_update('x', 1); break;
				case 'd': key_update('d', 1); break;
				case 'c': key_update('c', 1); break;
				case 'v': key_update('v', 1); break;
				case 'g': key_update('g', 1); break;
				case 'b': key_update('b', 1); break;
				case 'h': key_update('h', 1); break; //lab
				case 'n': key_update('n', 1); break; //la
				case 'j': key_update('j', 1); break; //sib
				case 'm': key_update('m', 1); break; //si
				case ',': key_update(',', 1); break; //do 4

				case '-': testoctave--; testpitch=0; break;
				case '=': testoctave++; testpitch=0; break;

				case '[': testvelocity = CLAMP(testvelocity-.5, 0.0, 1.0); break;
				case ']': testvelocity = CLAMP(testvelocity+.5, 0.0, 1.0); break;

				default: break;
			}
			break;
		case SDL_KEYUP:
			if(e.key.repeat) break;
			kbstate[ e.key.keysym.scancode % kbstate_max ] = 0;
			switch(e.key.keysym.sym){
				case 'q': key_update('q', 0); break; //do 4
				case '2': key_update('2', 0); break;
				case 'w': key_update('w', 0); break;
				case '3': key_update('3', 0); break;
				case 'e': key_update('e', 0); break;
				case 'r': key_update('r', 0); break;
				case '5': key_update('5', 0); break;
				case 't': key_update('t', 0); break;
				case '6': key_update('6', 0); break;
				case 'y': key_update('y', 0); break;
				case '7': key_update('7', 0); break;
				case 'u': key_update('u', 0); break;
				case 'i': key_update('i', 0); break;

				case 'z': key_update('z', 0); break; //do 3
				case 's': key_update('s', 0); break;
				case 'x': key_update('x', 0); break;
				case 'd': key_update('d', 0); break;
				case 'c': key_update('c', 0); break;
				case 'v': key_update('v', 0); break;
				case 'g': key_update('g', 0); break;
				case 'b': key_update('b', 0); break;
				case 'h': key_update('h', 0); break; //lab
				case 'n': key_update('n', 0); break; //la
				case 'j': key_update('j', 0); break; //sib
				case 'm': key_update('m', 0); break; //si
				case ',': key_update(',', 0); break; //do 4
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
		case SDL_FINGERDOWN:
			voices[5][13+12+1]=syn_non(G.syn, 5, 3 -12+12*testoctave, testvelocity);

		case SDL_FINGERMOTION:

		case SDL_FINGERUP:
			syn_nof(G.syn, voices[5][13+12+1]);

			break;
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
			} break;

		case SDL_MOUSEBUTTONUP:
			switch(e.button.button){
				case SDL_BUTTON_LEFT:   Mouse.b0 = 0; break;
				case SDL_BUTTON_RIGHT:  Mouse.b1 = 0; break;
				case SDL_BUTTON_MIDDLE: Mouse.b2 = 0; break;
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

#ifndef _3DS
static TTF_Font* sg_font=NULL;
#include "elm.h"
#else
static C2D_Font sg_font;
#include "elbcfnt.h"
#endif

//backend functions
//void draw_target( char ); // 3ds only 0 for top screen, 1 for bottom
void draw_color( float r,float g,float b,float a);
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
			printf("[FONT]:%s", TTF_GetError());
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
	draw_color(0,0,0,1);
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
	*x = sg_texs[tex].w;
	*y = sg_texs[tex].h;

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
		SDL_RenderCopy(G.renderer, G.screen_tex, NULL, NULL);
		SDL_RenderPresent(G.renderer);
		SDL_SetRenderTarget(G.renderer, G.screen_tex);

		// SDL_RenderPresent( G.renderer );
	#else
		SDL_Flip( G.screen );
	#endif
}

void draw_color( float r,float g,float b,float a){
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
	char ret = _isel;
	if(i>=0 && i<SYN_TONES) {
		_isel = i;
		astep(step_sel);
		gup_seq = 1;
		gup_spb = 1;
		gup_osc = 1;
		gup_rec = 1;
		mlatch = NULL;

		for(int t=0; t<SYN_TONES; t++){
			for(int p=0; p<POLYPHONY+40; p++){
				syn_nof(G.syn, voices[t][p]);
				voices[t][p] = (noteid){-1,-1};
			}
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
void key_update(char key, char on){
	float target_freq=0;
	if(on){
		switch(key){
			case 'q': target_freq=-9 +12*testoctave; voices[_isel][0]=syn_non(G.syn, _isel, -9 +12*testoctave, testvelocity); break; //do 4
			case '2': target_freq=-8 +12*testoctave; voices[_isel][1]=syn_non(G.syn, _isel, -8 +12*testoctave, testvelocity); break;
			case 'w': target_freq=-7 +12*testoctave; voices[_isel][2]=syn_non(G.syn, _isel, -7 +12*testoctave, testvelocity); break;
			case '3': target_freq=-6 +12*testoctave; voices[_isel][3]=syn_non(G.syn, _isel, -6 +12*testoctave, testvelocity); break;
			case 'e': target_freq=-5 +12*testoctave; voices[_isel][4]=syn_non(G.syn, _isel, -5 +12*testoctave, testvelocity); break;
			case 'r': target_freq=-4 +12*testoctave; voices[_isel][5]=syn_non(G.syn, _isel, -4 +12*testoctave, testvelocity); break;
			case '5': target_freq=-3 +12*testoctave; voices[_isel][6]=syn_non(G.syn, _isel, -3 +12*testoctave, testvelocity); break;
			case 't': target_freq=-2 +12*testoctave; voices[_isel][7]=syn_non(G.syn, _isel, -2 +12*testoctave, testvelocity); break;
			case '6': target_freq=-1 +12*testoctave; voices[_isel][8]=syn_non(G.syn, _isel, -1 +12*testoctave, testvelocity); break; //lab
			case 'y': target_freq= 0 +12*testoctave; voices[_isel][9]=syn_non(G.syn, _isel, 0  +12*testoctave, testvelocity); break; //la
			case '7': target_freq= 1 +12*testoctave; voices[_isel][10]=syn_non(G.syn, _isel, 1 +12*testoctave, testvelocity); break; //sib
			case 'u': target_freq= 2 +12*testoctave; voices[_isel][11]=syn_non(G.syn, _isel, 2 +12*testoctave, testvelocity); break; //si
			case 'i': target_freq= 3 +12*testoctave; voices[_isel][12]=syn_non(G.syn, _isel, 3 +12*testoctave, testvelocity); break; //do 5

			case 'z': target_freq=-9 -12+12*testoctave; voices[_isel][13+0]=syn_non(G.syn, _isel, -9 -12+12*testoctave, testvelocity); break; //do 3
			case 's': target_freq=-8 -12+12*testoctave; voices[_isel][13+1]=syn_non(G.syn, _isel, -8 -12+12*testoctave, testvelocity); break;
			case 'x': target_freq=-7 -12+12*testoctave; voices[_isel][13+2]=syn_non(G.syn, _isel, -7 -12+12*testoctave, testvelocity); break;
			case 'd': target_freq=-6 -12+12*testoctave; voices[_isel][13+3]=syn_non(G.syn, _isel, -6 -12+12*testoctave, testvelocity); break;
			case 'c': target_freq=-5 -12+12*testoctave; voices[_isel][13+4]=syn_non(G.syn, _isel, -5 -12+12*testoctave, testvelocity); break;
			case 'v': target_freq=-4 -12+12*testoctave; voices[_isel][13+5]=syn_non(G.syn, _isel, -4 -12+12*testoctave, testvelocity); break;
			case 'g': target_freq=-3 -12+12*testoctave; voices[_isel][13+6]=syn_non(G.syn, _isel, -3 -12+12*testoctave, testvelocity); break;
			case 'b': target_freq=-2 -12+12*testoctave; voices[_isel][13+7]=syn_non(G.syn, _isel, -2 -12+12*testoctave, testvelocity); break;
			case 'h': target_freq=-1 -12+12*testoctave; voices[_isel][13+8]=syn_non(G.syn, _isel, -1 -12+12*testoctave, testvelocity); break; //lab
			case 'n': target_freq= 0 -12+12*testoctave; voices[_isel][13+9]=syn_non(G.syn, _isel, 0  -12+12*testoctave, testvelocity); break; //la
			case 'j': target_freq= 1 -12+12*testoctave; voices[_isel][13+10]=syn_non(G.syn, _isel, 1 -12+12*testoctave, testvelocity); break; //sib
			case 'm': target_freq= 2 -12+12*testoctave; voices[_isel][13+11]=syn_non(G.syn, _isel, 2 -12+12*testoctave, testvelocity); break; //si
			case ',': target_freq= 3 -12+12*testoctave; voices[_isel][13+12]=syn_non(G.syn, _isel, 3 -12+12*testoctave, testvelocity); break; //do 4
			default: break;
		}
	} else {
		switch(key){
			case 'q': syn_nof(G.syn, voices[_isel][0]     ); voices[_isel][0]     = (noteid){-1,-1}; break; //do 4
			case '2': syn_nof(G.syn, voices[_isel][1]     ); voices[_isel][1]     = (noteid){-1,-1}; break;
			case 'w': syn_nof(G.syn, voices[_isel][2]     ); voices[_isel][2]     = (noteid){-1,-1}; break;
			case '3': syn_nof(G.syn, voices[_isel][3]     ); voices[_isel][3]     = (noteid){-1,-1}; break;
			case 'e': syn_nof(G.syn, voices[_isel][4]     ); voices[_isel][4]     = (noteid){-1,-1}; break;
			case 'r': syn_nof(G.syn, voices[_isel][5]     ); voices[_isel][5]     = (noteid){-1,-1}; break;
			case '5': syn_nof(G.syn, voices[_isel][6]     ); voices[_isel][6]     = (noteid){-1,-1}; break;
			case 't': syn_nof(G.syn, voices[_isel][7]     ); voices[_isel][7]     = (noteid){-1,-1}; break;
			case '6': syn_nof(G.syn, voices[_isel][8]     ); voices[_isel][8]     = (noteid){-1,-1}; break;
			case 'y': syn_nof(G.syn, voices[_isel][9]     ); voices[_isel][9]     = (noteid){-1,-1}; break;
			case '7': syn_nof(G.syn, voices[_isel][10]    ); voices[_isel][10]    = (noteid){-1,-1}; break;
			case 'u': syn_nof(G.syn, voices[_isel][11]    ); voices[_isel][11]    = (noteid){-1,-1}; break;
			case 'i': syn_nof(G.syn, voices[_isel][12]    ); voices[_isel][12]    = (noteid){-1,-1}; break;

			case 'z': syn_nof(G.syn, voices[_isel][13+0]  ); voices[_isel][13+0]  = (noteid){-1,-1}; break; //do 3
			case 's': syn_nof(G.syn, voices[_isel][13+1]  ); voices[_isel][13+1]  = (noteid){-1,-1}; break;
			case 'x': syn_nof(G.syn, voices[_isel][13+2]  ); voices[_isel][13+2]  = (noteid){-1,-1}; break;
			case 'd': syn_nof(G.syn, voices[_isel][13+3]  ); voices[_isel][13+3]  = (noteid){-1,-1}; break;
			case 'c': syn_nof(G.syn, voices[_isel][13+4]  ); voices[_isel][13+4]  = (noteid){-1,-1}; break;
			case 'v': syn_nof(G.syn, voices[_isel][13+5]  ); voices[_isel][13+5]  = (noteid){-1,-1}; break;
			case 'g': syn_nof(G.syn, voices[_isel][13+6]  ); voices[_isel][13+6]  = (noteid){-1,-1}; break;
			case 'b': syn_nof(G.syn, voices[_isel][13+7]  ); voices[_isel][13+7]  = (noteid){-1,-1}; break;
			case 'h': syn_nof(G.syn, voices[_isel][13+8]  ); voices[_isel][13+8]  = (noteid){-1,-1}; break; //lab
			case 'n': syn_nof(G.syn, voices[_isel][13+9]  ); voices[_isel][13+9]  = (noteid){-1,-1}; break; //la
			case 'j': syn_nof(G.syn, voices[_isel][13+10] ); voices[_isel][13+10] = (noteid){-1,-1}; break; //sib
			case 'm': syn_nof(G.syn, voices[_isel][13+11] ); voices[_isel][13+11] = (noteid){-1,-1}; break; //si
			case ',': syn_nof(G.syn, voices[_isel][13+12] ); voices[_isel][13+12] = (noteid){-1,-1}; break; //do 4
			default:break;
		}
		if(rec) voice_count=MAX(voice_count-1, 0);
	}
	if(on && commit_count<POLYPHONY && rec) {
		if(commit_count == 0) commit_step_begin=step_sel;
		note_commit[commit_count] = target_freq;
		voice_count++;
		commit_count++;
	}
	//commit
	if(!on && voice_count==0 && rec){
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

