#ifndef __ADSR_H__
#define __ADSR_H__
#include <math.h>
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

typedef struct {
	float out;//[0,1]
	float sr;//samplerate
	float a_rate;//sec*sr
	float d_rate;//sec*sr
	float r_rate;//sec*sr
	float a_coef;
	float d_coef;
	float r_coef;
	float a_base;
	float d_base;
	float r_base;
	float a,d,r;
	float s;//level [0,1]
	char state; // idle=0, attack=1, decay=2, sustain=3, release=4;
	char gate;//[0,1]
	float ratioa;//e.g 0.0001 to 0.01 for mostly-exponential, 100 for virtually linear
	float ratiodr;
} adsr;


void adsr_a(adsr* a, float atk);// in:seconds
void adsr_d(adsr* a, float d);// in:seconds
void adsr_r(adsr* a, float r);// in:seconds

#define MAX_RAT 100
#define MIN_RAT 0.0001
// #define MIN_RAT 0.000001
// #define MIN_RAT 0.0000001
// #define MIN_RAT 0.000000001

adsr adsr_make(int sr, float a,float d,float s,float r, float aexp,float drexp);
void adsr_gate(adsr* a, int g);
void adsr_arate(adsr* a, float atkrate);
void adsr_drate(adsr* a, float drate);
void adsr_rrate(adsr* a, float rrate);
void adsr_rata(adsr* a, float rata);
void adsr_ratdr(adsr* a, float ratdr);
void adsr_a(adsr* a, float atk);
void adsr_d(adsr* a, float d);
void adsr_s(adsr* a, float s);
void adsr_r(adsr* a, float r);
// 1.0:linear, 0.0:exponential
void adsr_aexp(adsr* a, float ae);
void adsr_drexp(adsr* a, float dre);
float adsr_run(adsr* a);
#endif