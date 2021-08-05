/*
 * utils.c
 *
 * Created: 30.05.2019 11:34:57
 * Author: dkotlyar
 */ 

#include "main.h"
#include "utils.h"
#include "millis.h"
#define getMillis() (millis())

void pTimerReset(ptimer_t * o) {
	o->in = 0;
	o->q = 0;
}

uint8_t pton(ptimer_t * o, uint8_t in, uint32_t delay) {
	uint32_t time = getMillis();
	if (!o->in) {
		o->start = time;
	}
	o->in = in;
	o->q = (o->q || (time - o->start) >= delay) && o->in;
	return o->q;
}

uint8_t ptof(ptimer_t * o, uint8_t in, uint32_t delay) {
	uint32_t time = getMillis();
	if (o->in) {
		o->start = time;
	}
	o->in = in;
	o->q = o->in || (o->q && (time - o->start) < delay);
	return o->q;
}

uint8_t ptp(ptimer_t * o, uint8_t in, uint32_t delay) {
	uint32_t time = getMillis();
	if (!o->in) {
		o->start = time;
	}
	o->in = in;
	o->q = o->in && ((time - o->start) < delay);
	return o->q;
}

void trig(ptrig_t * t, uint8_t clk) {
	t->qRise = (t->clk != clk) && clk && !t->qRise;
	t->qFall = (t->clk != clk) && !clk && !t->qFall;
	t->clk = clk;
}

uint8_t ptrigr(ptrig_t * t, uint8_t clk) {
	trig(t, clk);
	return t->qRise;
}

uint8_t ptrigf(ptrig_t * t, uint8_t clk) {
	trig(t, clk);
	return t->qFall;
}