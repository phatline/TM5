/*
 * Portable "Mersenne Twister" rand() functions
 * ==========================================================================
 * By Julienne Walker (happyfrosty@hotmail.com)
 * License: Public Domain
 */

#include <limits.h>
#include <time.h>
#include "jsw_rand.h"

#define N 17 // TK: Reduzierung der Einträge zur Speicherersparnis und Beschleunigung der Neuberechnung der Zufallswerte
#define M 397
#define A 0x9908b0dfUL
#define U 0x80000000UL
#define L 0x7fffffffUL

/* Interner Zustand */
static unsigned short x[N];
static int next;

/* Initialisiere den internen Zustand */
void jsw_seed(unsigned int s) {

    int i;

    x[0] = s & 0xFFFF;

    for (i = 1; i < N; i++)	{	x[i] = (A * (x[i - 1] ^ (x[i - 1] >> 15)) + i) & 0xFFFF;	}
}



/* Mersenne Twister */
unsigned short jsw_rand(void) {

    unsigned short y;

    /* Fülle x auf, wenn erschöpft */
    if (next == N){
					int i;

					/* Berechne neue Werte für x */
					for (i = 0; i < N - M; i++){
													y = (x[i] & 0x8000) | (x[i + 1] & 0x7FFF);
													x[i] = x[i + M] ^ (y >> 1) ^ ((y & 0x1) ? A : 0);
													}

					/* Behandle die restlichen Elemente von x */
					for (; i < N - 1; i++){
											y = (x[i] & 0x8000) | (x[i + 1] & 0x7FFF);
											x[i] = x[(i + M - N) % N] ^ (y >> 1) ^ ((y & 0x1) ? A : 0);
											}

					/* Berechne den letzten Wert von x */
					y = (x[N - 1] & 0x8000) | (x[0] & 0x7FFF);
					x[i] = x[(i + M - N) % N] ^ (y >> 1) ^ ((y & 0x1) ? A : 0);

					next = 0; // Setze den Index zurück
					}

    y = x[next++]; // Nimm den nächsten Wert aus x

    /* Verbessere die Verteilung */
    y ^= (y >> 5);
    y ^= (y << 7) & 0x9BFF;
    y ^= (y << 15) & 0xA59B;
    y ^= (y >> 1);

    return y & 0x7F; // Begrenze den Wertebereich auf 0-127
}
