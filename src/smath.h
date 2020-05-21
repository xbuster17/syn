#ifndef __SMATH_H__
#define __SMATH_H__
#include <limits.h>
#include <math.h>

#include <stdlib.h> // rand
#include <stdint.h>

extern float A4;

float sign(float x);
float lerp(float a, float b, float c);
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef CLAMP
#define CLAMP(in,min,max)\
	(((in)>=(max))?(max):(((in)<=(min))?(min):(in)))
#endif
#ifndef MAPVAL
#define MAPVAL(x, a,b, c,d) \
	( (((x) - (a)) / ((b) - (a))) * ((d) - (c)) + (c) )
#endif

void smath_init(void);

float freqn(float n);
float freqna(float a4, float n);
float sine(float t);
float saw(float t);
float tri(float t);
float tri2(float t);
float sqr(float t);
float pul(float t, float d);
uint32_t syn_hashi32(uint32_t x);
float wnoiz(float t);
float noise(void);
float db2vol(float db);
float vol2db(float vol);


void conv(float* dst, float* kernel, int cs, float* src, float* prevsrc, int n );
/*
creates FIR low pass convolution filter of order n to be stored in dst
which should be able to hold n float values
*/
enum swindow{ W_BLACKMAN=0,W_HANNING=1, W_HAMMING=2};
void fir_lp(float* dst, float freq, float sr, int n, char window);
void fir_hp(float* dst, float freq, float sr, int n, char window);
void fir_bp(float* dst, float fl, float fh, float sr, int n, char window);
void fir_bs(float* dst, float fl, float fh, float sr, int n, char window);

void butterworth5(float* dst, float freq, float reso, int sr, char highpass);

void genSinc(float* dst, float freq, int n);
void wBlackman(float* w, int n);
void wHanning(float* w, int n);
void wHamming(float* w, int n);


#endif