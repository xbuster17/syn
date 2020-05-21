#include "smath.h"

#define SINE_TABLE_SIZE 2000

float sine_table[SINE_TABLE_SIZE];

int NOISE_TABLE_SIZE = 1000;
float noise_table[1000];

void smath_init(void){
	for(int i=0; i<SINE_TABLE_SIZE; i++){
		sine_table[i]=sin(((float)i)/SINE_TABLE_SIZE*2*M_PI);
	}
	for(int i=0; i<NOISE_TABLE_SIZE; i++){
		noise_table[i] = (((float)rand())/INT_MAX) *2-1;
	}
}

float sign(float x){
	float e = 0.00001;
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
	// return sine_table[(int)CLAMP(fmodf(t,1)*SINE_TABLE_SIZE, 0, SINE_TABLE_SIZE-1)];
	int i = (int)(t*SINE_TABLE_SIZE) % SINE_TABLE_SIZE;
	return (sine_table[i] + sine_table[(i+1)%SINE_TABLE_SIZE])/2;
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






#include <assert.h>
#include <string.h>



void conv(float* dst, float* conv, int cs, float* src, float* prevsrc, int n ){
	assert(dst);
	assert(conv);
	assert(src);
	for(int i = 0; i<n; i++){
		float sum=0;
		for(int j=0; j<cs; j++){
			if(i-j>=0)
				sum+= conv[j]*src[i-j];
			else
				sum+= conv[j]*prevsrc[cs+i-j];
		}
		dst[i] = sum;
	}
}

#define MAX_WCACHE 256
float wcacheBlackman[MAX_WCACHE];
float wcacheHanning[MAX_WCACHE];
float wcacheHamming[MAX_WCACHE];
int wcacheBlackmanN = 0;
int wcacheHanningN = 0;
int wcacheHammingN = 0;

void wBlackman(float* w, int N){
	if(wcacheBlackmanN == N) memcpy(w, wcacheBlackman, N*sizeof(float));
	else{
		int i;
		const double M = N-1;
		const double PI = 3.14159265358979323846;

		for (i = 0; i < N; i++) {
			w[i] = 0.42 - (0.5 * cos(2.0*PI*(double)i/M)) + (0.08*cos(4.0*PI*(double)i/M));
		}
		if(N<MAX_WCACHE) {memcpy(wcacheBlackman, w, N*sizeof(float)); wcacheBlackmanN=N; }
	}
}

void wHanning(float* w, int N){
	if(wcacheHanningN == N) memcpy(w, wcacheHanning, N*sizeof(float));
	else{
		int i;
		const double M = N-1;
		const double PI = 3.14159265358979323846;

		for (i = 0; i < N; i++) {
			w[i] = 0.5 * (1.0 - cos(2.0*PI*(double)i/M));
		}
		if(N<MAX_WCACHE) {memcpy(wcacheHanning, w, N*sizeof(float)); wcacheHanningN=N; }
	}
}

void wHamming(float* w, int N){
	if(wcacheHammingN == N) memcpy(w, wcacheHamming, N*sizeof(float));
	else{
		int i;
		const double M = N-1;
		const double PI = 3.14159265358979323846;

		for (i = 0; i < N; i++) {
			w[i] = 0.54 - (0.46*cos(2.0*PI*(double)i/M));
		}
		if(N<MAX_WCACHE) {memcpy(wcacheHamming, w, N*sizeof(float)); wcacheHammingN=N; }
	}
}


void genSinc(float* dst, float fc, int N){
	int i;
	const float M = N-1;
	float n;

	// Generate sinc delayed by (N-1)/2
	for (i = 0; i < N; i++) {
		if (i == M/2.0) {
			dst[i] = 2.0 * fc;
		}
		else {
			n = (float)i - M/2.0;
			dst[i] = sin(2.0*M_PI*fc*n) / (M_PI*n);
		}
	}
}

/*
creates FIR low pass convolution filter of order n to be stored in dst
which should be able to hold n float values
*/
void fir_lp(float* dst, float freq, float sr, int n, char window){
	freq = freq/sr;

	// float o_c = 2*M_PI*freq;
	// int middle = n/2;
	// for(int i = -middle; i<=middle; i++){
	// 	if(i==0) dst[middle]=2*freq;
	// 	else dst[i+middle] = sin(o_c*i)/(M_PI*i);
	// }

	float h[n]; memset(h, 0, sizeof(h));

	genSinc(h, freq, n);
	float w[n]; memset(w, 0, sizeof(w));

	switch (window) {
		case W_BLACKMAN:
			wBlackman(w, n);
			break;
		case W_HANNING:
			wHanning(w, n);
			break;
		case W_HAMMING:
			wHamming(w, n);
			break;
		default:
			break;
	}
	float sum=0;
	for (int i = 0; i < n; i++) {
		dst[i] = h[i] * w[i];
		sum+=dst[i];
	}
	for (int i = 0; i < n; i++) {
		dst[i]/=sum;
	}

}

void fir_hp(float* dst, float freq, float sr, int n, char window){
	// freq = freq/sr;
	// float o_c = 2*M_PI*freq;
	// int middle = n/2;
	// for(int i = -middle; i<middle; i++){
	// 	if(i==0) dst[middle]=1.0-2.0*freq;
	// 	else dst[i+middle] = -sin(o_c*i)/(M_PI*i);
	// }

	// 1. Generate lowpass filter
	fir_lp(dst, freq, sr, n, window);

	// 2. Spectrally invert (negate all samples and add 1 to center sample) lowpass filter
	// = delta[n-((N-1)/2)] - dst[n], in the time domain
	for (int i = 0; i < n; i++) {
		dst[i] *= -1.0;
	}
	dst[(n-1)/2] += 1.0;

}


void fir_bs(float* dst, float fl, float fh, float sr, int n, char window){
	float h1[n];
	float h2[n];
		// 1. Generate lowpass filter at first (low) cutoff frequency
	fir_lp(h1, fl, sr, n, window);

	// 2. Generate highpass filter at second (high) cutoff frequency
	fir_hp(h2, fh, sr, n, window);

	// 3. Add the 2 filters together
	for (int i = 0; i < n; i++) {
		dst[i] = h1[i] + h2[i];
	}
}

void fir_bp(float* dst, float fl, float fh, float sr, int n, char window){
	// 1. Generate bandstop filter
	fir_bs(dst, fl, fh, sr, n, window);

	// 2. Spectrally invert bandstop (negate all, and add 1 to center sample)
	for (int i = 0; i < n; i++) {
		dst[i] *= -1.0;
	}
	dst[(n-1)/2] += 1.0;
}




