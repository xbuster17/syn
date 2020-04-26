#include "smath.h"

#define SINE_TABLE_SIZE 2048

float sine_table[SINE_TABLE_SIZE];

int NOISE_TABLE_SIZE = 1024;
float noise_table[1024];

void smath_init(void){
	for(int i=0; i<SINE_TABLE_SIZE; i++){// init sine_table
		sine_table[i]=sin(((float)i)/SINE_TABLE_SIZE*2*M_PI);
	}
	for(int i=0; i<NOISE_TABLE_SIZE; i++){// init sine_table
		noise_table[i] = (((float)rand())/INT_MAX) *2-1;
	}
}

float sign(float x){
	float e = 0.001;
	if(x > e) return 1;
	if(x <-e) return -1;
	return 0;
}

float lerp(float a, float b, float c){ return a*(1-c)+b*c; }
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define CLAMP(in,min,max)\
	(((in)>=(max))?(max):(((in)<=(min))?(min):(in)))
#define MAPVAL(x, a,b, c,d) \
	( (((x) - (a)) / ((b) - (a))) * ((d) - (c)) + (c) )



// https://pages.mtu.edu/~suits/NoteFreqCalcs.html
// fn = f0 * (a)n
// where
// f0 = the frequency of one fixed note which must be defined. A common choice is setting the A above middle C (A4) at f0 = 440 Hz.
// n = the number of half steps away from the fixed note you are. If you are at a higher note, n is positive. If you are on a lower note, n is negative.
// fn = the frequency of the note n half steps away.
// a = (2)1/12 = the twelth root of 2 = the number which when multiplied by itself 12 times equals 2 = 1.059463094359...
float A4 = 440.0;
float freqn_A = 1.059463094359; // ~= pow(2, 1/12)
float freqn(float n){ return A4 * powf(freqn_A, n); }

float freqna(float a4, float n){ return a4 * powf(freqn_A, n); }

float sine(float t){
	return sine_table[(int)CLAMP(fmodf(t,1)*SINE_TABLE_SIZE, 0, SINE_TABLE_SIZE-1)];
}

float saw(float t){
	return fmodf(t,1)*2-1;
}

float tri(float t){
	return 2*fabs(saw(t))-1;
}

float tri2(float t){
	return 2*pow(saw(t),2)-1;
}

float sqr(float t){
	return ((int)(2*t)%2) *2 -1;
}

float pul(float t, float d){
	return (sqr(t)>0 && saw(t)>1-d)? 1.0 : -1.0;
}


static uint32_t i32offset = 2166136261;
static uint32_t i32prime = 16777619;

uint32_t syn_hashi32(uint32_t x){
	uint32_t hash = i32offset;
	// for(int i = 0; i<4; i++){
	int i = 0;
	hash ^= ((uint8_t*)&x)[i++];
	hash *= i32prime;
	hash ^= ((uint8_t*)&x)[i++];
	hash *= i32prime;
	hash ^= ((uint8_t*)&x)[i++];
	hash *= i32prime;
	hash ^= ((uint8_t*)&x)[i++];
	hash *= i32prime;
	// }
	return hash;
}

float wnoiz(float t){
	uint32_t i = syn_hashi32(UINT_MAX*t);
	uint32_t j = syn_hashi32(UINT_MAX*t+1);
	float ret = lerp(((float)i)/UINT_MAX,((float)j)/UINT_MAX, fmodf(t,1));
	return ret*2-1;
	return ((float)i)/UINT_MAX*2-1;
 }

int noise_table_it=0;
float noise(void){
 	float ret = noise_table[noise_table_it];
 	noise_table_it++;
 	noise_table_it %= NOISE_TABLE_SIZE;
 	return ret;
 }




float db2vol(float db){
	return powf(10.f, .05*db);
}
float vol2db(float vol){
	return 20.f * log10f(vol);
}
