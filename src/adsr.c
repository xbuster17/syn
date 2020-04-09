#include "adsr.h"


static float _calc_coef(float rate, float tratio){
	return (rate <= 0) ? 0.0 : exp(-log((1.0 + tratio) / tratio) / rate);
}

adsr adsr_make(int sr, float a,float d,float s,float r, float aexp,float drexp){
	adsr ret;
	// aexp =  MAPVAL( CLAMP(aexp,0,1), 0,1, MIN_RAT, MAX_RAT);
	aexp =  CLAMP(aexp,MIN_RAT,1) * MAX_RAT;
	// drexp = MAPVAL( CLAMP(drexp,0,1), 0,1, MIN_RAT, MAX_RAT);
	drexp = CLAMP(drexp,MIN_RAT,1) * MAX_RAT;
		// dr=MAPVAL(dr,0,1,MAX_RAT,MIN_RAT);
	ret.sr=sr;
	ret.out=0;
	ret.state=0;
	ret.gate=0;
	ret.s=s;
	ret.ratioa=aexp;
	ret.ratiodr=drexp;
	adsr_a(&ret, a);
	adsr_d(&ret, d);
	adsr_r(&ret, r);
	return ret;
}

void adsr_gate(adsr* a, int g){
	a->gate = g;
	if(g)
		a->state=1;
	else if(a->state!=0)
		a->state=4;
}

void adsr_arate(adsr* a, float atkrate){
	a->a_rate = atkrate;
	a->a_coef = _calc_coef(atkrate, a->ratioa);
	a->a_base = (1.0 + a->ratioa) * (1.0 - a->a_coef);
}
void adsr_drate(adsr* a, float drate){
	a->d_rate = drate;
	a->d_coef = _calc_coef(drate, a->ratiodr);
	a->d_base = (a->s - a->ratiodr) * (1.0 - a->d_coef);
}
void adsr_rrate(adsr* a, float rrate){
	a->r_rate = rrate;
	a->r_coef = _calc_coef(rrate, a->ratiodr);
	a->r_base = -a->ratiodr * (1.0 - a->r_coef);
}
void adsr_rata(adsr* a, float rata){
	a->ratioa=rata;
	adsr_arate(a, a->a_rate);
}

void adsr_ratdr(adsr* a, float ratdr){
	a->ratiodr = ratdr;
	adsr_drate(a, a->d_rate);
	adsr_rrate(a, a->r_rate);
}


void adsr_a(adsr* a, float atk){
	a->a=atk;
	adsr_arate(a, atk*a->sr);
}

void adsr_d(adsr* a, float d){
	a->d=d;
	adsr_drate(a, d * a->sr);
}

void adsr_s(adsr* a, float s){
	a->s=s;
	adsr_drate(a, a->d_rate);
}

void adsr_r(adsr* a, float r){
	a->r=r;
	adsr_rrate(a, r * a->sr);
}


// 1.0:linear, 0.0:exponential
void adsr_aexp(adsr* a, float ae){
	ae=CLAMP(ae,MIN_RAT,1.0) * MAX_RAT;
	adsr_rata(a, ae);
}

void adsr_drexp(adsr* a, float dre){
	dre=CLAMP(dre,MIN_RAT,1.0) * MAX_RAT;
	adsr_ratdr(a, dre);
}



float adsr_run(adsr* a){
	switch (a->state) {
		case 0:
			break;
		case 1:
			a->out = a->a_base + a->out * a->a_coef;
			if (a->out >= 1.0) {
				a->out = 1.0;
				a->state = 2;
			}
			break;
		case 2:
			a->out = a->d_base + a->out * a->d_coef;
			if (a->out <= a->s) {
				a->out = a->s;
				a->state = 3;
			}
			break;
		case 3:
			break;
		case 4:
			a->out = a->r_base + a->out * a->r_coef;
			if (a->out <= 0.0) {
				a->out = 0.0;
				a->state = 0;
			}
	}
	return a->out;
}
