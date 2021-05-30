

typedef unsigned __int128 uint128_t;

#define UINT128_MAX (~(uint128_t)0)

ulong pow3(size_t n)
{
	ulong r = 1;
	ulong b = 3;

	while (n) {
		if (n & 1) {
			r *= b;
		}
		b *= b;
		n >>= 1;
	}

	return r;
}



__kernel void worker(
	__global ulong *indices,
	__global ulong *arrayLarge,      // actually 128-bit integers
	__global uchar *arrayIncreases,
	__global ulong *arrayLarge2,
	__global ushort *arrayDelay
)
{
	size_t id = get_global_id(0);

	__local ulong lut[SIEVE_LOGSIZE2 + 1];
	__local uint128_t maxNs[SIEVE_LOGSIZE2 + 1];

	if (get_local_id(0) == 0) {
		for (size_t i = 0; i < SIEVE_LOGSIZE2 + 1; ++i) {
			lut[i] = pow3(i);
			//maxNs[i] = UINT128_MAX / lut[i];
		}
		maxNs[0]  = ((uint128_t) 18446744073709551615 << 64) | 18446744073709551615;
		maxNs[1]  = ((uint128_t) 6148914691236517205 << 64) | 6148914691236517205;
		maxNs[2]  = ((uint128_t) 2049638230412172401 << 64) | 14347467612885206812;
		maxNs[3]  = ((uint128_t) 683212743470724133 << 64) | 17080318586768103348;
		maxNs[4]  = ((uint128_t) 227737581156908044 << 64) | 11842354220159218321;
#if (SIEVE_LOGSIZE2 >= 5)
		maxNs[5]  = ((uint128_t) 75912527052302681 << 64) | 10096366097956256645;
#endif
#if (SIEVE_LOGSIZE2 >= 6)
		maxNs[6]  = ((uint128_t) 25304175684100893 << 64) | 15663284748458453292;
#endif
#if (SIEVE_LOGSIZE2 >= 7)
		maxNs[7]  = ((uint128_t) 8434725228033631 << 64) | 5221094916152817764;
#endif
#if (SIEVE_LOGSIZE2 >= 8)
		maxNs[8]  = ((uint128_t) 2811575076011210 << 64) | 7889279663287456460;
#endif
#if (SIEVE_LOGSIZE2 >= 9)
		maxNs[9]  = ((uint128_t) 937191692003736 << 64) | 14927589270235519897;
#endif
#if (SIEVE_LOGSIZE2 >= 10)
		maxNs[10] = ((uint128_t) 312397230667912 << 64) | 4975863090078506632;
#endif
#if (SIEVE_LOGSIZE2 >= 11)
		maxNs[11] = ((uint128_t) 104132410222637 << 64) | 7807535721262686082;
#endif
#if (SIEVE_LOGSIZE2 >= 12)
		maxNs[12] = ((uint128_t) 34710803407545 << 64) | 14900341289560596438;
#endif
#if (SIEVE_LOGSIZE2 >= 13)
		maxNs[13] = ((uint128_t) 11570267802515 << 64) | 4966780429853532146;
#endif
#if (SIEVE_LOGSIZE2 >= 14)
		maxNs[14] = ((uint128_t) 3856755934171 << 64) | 13953422859090878459;
#endif
#if (SIEVE_LOGSIZE2 >= 15)
		maxNs[15] = ((uint128_t) 1285585311390 << 64) | 10800055644266810025;
#endif
#if (SIEVE_LOGSIZE2 >= 16)
		maxNs[16] = ((uint128_t) 428528437130 << 64) | 3600018548088936675;
#endif
#if (SIEVE_LOGSIZE2 >= 17)
		maxNs[17] = ((uint128_t) 142842812376 << 64) | 13497835565169346635;
#endif
#if (SIEVE_LOGSIZE2 >= 18)
		maxNs[18] = ((uint128_t) 47614270792 << 64) | 4499278521723115545;
#endif
#if (SIEVE_LOGSIZE2 >= 19)
		maxNs[19] = ((uint128_t) 15871423597 << 64) | 7648674198477555720;
#endif
#if (SIEVE_LOGSIZE2 >= 20)
		maxNs[20] = ((uint128_t) 5290474532 << 64) | 8698472757395702445;
#endif
#if (SIEVE_LOGSIZE2 >= 21)
		maxNs[21] = ((uint128_t) 1763491510 << 64) | 15197320301604935225;
#endif
#if (SIEVE_LOGSIZE2 >= 22)
		maxNs[22] = ((uint128_t) 587830503 << 64) | 11214688125104828947;
#endif
#if (SIEVE_LOGSIZE2 >= 23)
		maxNs[23] = ((uint128_t) 195943501 << 64) | 3738229375034942982;
#endif
#if (SIEVE_LOGSIZE2 >= 24)
		maxNs[24] = ((uint128_t) 65314500 << 64) | 7394991149581498199;
#endif
#if (SIEVE_LOGSIZE2 >= 25)
		maxNs[25] = ((uint128_t) 21771500 << 64) | 2464997049860499399;
#endif
#if (SIEVE_LOGSIZE2 >= 26)
		maxNs[26] = ((uint128_t) 7257166 << 64) | 13119495065759867543;
#endif
#if (SIEVE_LOGSIZE2 >= 27)
		maxNs[27] = ((uint128_t) 2419055 << 64) | 10522079713156473053;
#endif
#if (SIEVE_LOGSIZE2 >= 28)
		maxNs[28] = ((uint128_t) 806351 << 64) | 15805189286858525428;
#endif
#if (SIEVE_LOGSIZE2 >= 29)
		maxNs[29] = ((uint128_t) 268783 << 64) | 17566225811425876220;
#endif
#if (SIEVE_LOGSIZE2 >= 30)
		maxNs[30] = ((uint128_t) 89594 << 64) | 12004323295045142612;
#endif
#if (SIEVE_LOGSIZE2 >= 31)
		maxNs[31] = ((uint128_t) 29864 << 64) | 16299270480821415281;
#endif
#if (SIEVE_LOGSIZE2 >= 32)
		maxNs[32] = ((uint128_t) 9954 << 64) | 17730919542746839504;
#endif
#if (SIEVE_LOGSIZE2 >= 33)
		maxNs[33] = ((uint128_t) 3318 << 64) | 5910306514248946501;
#endif
#if (SIEVE_LOGSIZE2 >= 34)
		maxNs[34] = ((uint128_t) 1106 << 64) | 1970102171416315500;
#endif
#if (SIEVE_LOGSIZE2 >= 35)
		maxNs[35] = ((uint128_t) 368 << 64) | 12954530106278472910;
#endif
#if (SIEVE_LOGSIZE2 >= 36)
		maxNs[36] = ((uint128_t) 122 << 64) | 16616006084565858714;
#endif
	}

	barrier(CLK_LOCAL_MEM_FENCE);

	uint128_t h_minimum = ((uint128_t)(TASK_ID_KERNEL2 + 0)*9 << (TASK_SIZE_KERNEL2 - SIEVE_LOGSIZE));
	uint128_t h_supremum = ((uint128_t)(TASK_ID_KERNEL2 + 1)*9 << (TASK_SIZE_KERNEL2 - SIEVE_LOGSIZE));
	if (h_minimum == 0) h_minimum = 1;

	//int cMod = ((uint128_t)1 << SIEVE_LOGSIZE) % 3;
	int cMod = (SIEVE_LOGSIZE & 1) ? 2 : 1 ;



	size_t index = indices[id];
	if (index == 0) return;

	uint128_t L0 = ((uint128_t)TASK_ID << TASK_SIZE) + index;

	int aMod = 0;
	if (h_minimum == 1) aMod = 1;
	//int bMod = L0 % 3;

        /* trick for mod 3 */
	ulong r = 0;
	r += (uint)(L0);
	r += (uint)(L0 >> 32);
	r += (uint)(L0 >> 64);
	r += (uint)(L0 >> 96);
        int bMod = r%3;



	/* precalculate */
	uint128_t L =  ( (uint128_t)arrayLarge[2*index] << 64 ) | (uint128_t)arrayLarge[2*index + 1] ;
	size_t Salpha = (size_t)arrayIncreases[index];



	/* iterate over highest bits */
	for (uint128_t h = h_minimum; h < h_supremum; ++h) {
		uint128_t N0 = (h << SIEVE_LOGSIZE) + L0;
		int N0Mod = (aMod * cMod + bMod) % 3;
		aMod++;
		if (aMod > 2) aMod -= 3;

		while (N0Mod == 2) {
			h++;
			N0 = (h << SIEVE_LOGSIZE) + L0;
			N0Mod = (aMod * cMod + bMod) % 3;
			aMod++;
			if (aMod > 2) aMod -= 3;
		}
		if (h >= h_supremum) {
			break;
		}



		/* find the N after the first k steps */
		uint128_t N = h;
		size_t Salpha0 = Salpha;
		do {
			size_t alpha = min(Salpha0, (size_t)SIEVE_LOGSIZE2);
			N *= lut[alpha];
			Salpha0 -= alpha;
		} while (Salpha0 > 0);
		N += L;




		int steps = SIEVE_LOGSIZE;

		while ( (N >> SIEVE_LOGSIZE2) > 0 ) {
			index = N & ( ((ulong)1<<SIEVE_LOGSIZE2) - 1 );  // get lowest k2 bits of N
			ulong newL = arrayLarge2[index];
			size_t newSalpha = newL >> 58;    // just 6 bits gives Salpha
			newL &= 0x3ffffffffffffff;        // rest of bits gives L

			/* find the new N */
			N >>= SIEVE_LOGSIZE2;
			if (N > maxNs[newSalpha]) {
				printf("  OVERFLOW: (%llu << 64) | %llu\n", (ulong)(N0 >> 64), (ulong)N0);
				break;
			}
			N *= lut[newSalpha];
			if (N > UINT128_MAX - newL) {
				printf("  OVERFLOW: (%llu << 64) | %llu\n", (ulong)(N0 >> 64), (ulong)N0);
				break;
			}
			N += newL;

			steps += SIEVE_LOGSIZE2;
			//steps += SIEVE_LOGSIZE2 + (int)newSalpha;   // if (3*n + 1)/2 is 2 steps

			if (steps > 1000000) {     // must be checked so that steps doesn't overflow
				printf("  Steps limit reached! (%llu << 64) | %llu\n", (ulong)(N0 >> 64), (ulong)N0);
				break;
			}

		}



		if ((N >> SIEVE_LOGSIZE2) > 0) continue;

		steps += arrayDelay[N];

		if (steps > STEPS_MAX) {

			/* a trick to get N0 % 9 faster */
			ulong r = 0;
			r += (ulong)(N0)        & 0xfffffffffffffff;
			r += (ulong)(N0 >>  60) & 0xfffffffffffffff;
			r += (ulong)(N0 >> 120);
			int N0mod = r%9;

			/* do a last-minute 3^2 sieve */
			if (N0mod != 4)
				printf(" steps = %llu found: (%llu << 64) | %llu\n", (ulong)steps, (ulong)(N0 >> 64), (ulong)N0);

		}





	}


}
