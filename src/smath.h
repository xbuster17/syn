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
// #define SINE_TABLE_SIZE 48000
// #define SINE_TABLE_SIZE 2048

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

#endif