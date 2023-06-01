

typedef unsigned __int128 uint128_t;

#define UINT128_MAX (~(uint128_t)0)

uint pow3(size_t n)   // returns 3^n
{
	uint r = 1;
	uint b = 3;

	while (n) {
		if (n & 1) {
			r *= b;
		}
		b *= b;
		n >>= 1;
	}

	return r;
}

#define LUT_SIZE32 21


/* 2^8 sieve */
__constant static const uchar sieve8[16] = {27, 31, 47, 71, 91, 103, 111, 127, 155, 159, 167, 191, 231, 239, 251, 255};



__kernel void worker(
	__global ulong *indices,         // has a much shorter length than the rest of the arrays
	__global ulong *arrayLarge,      // actually 128-bit integers
	__global uchar *arrayIncreases

/*
  index = indices[get_global_id(0)] is the only time indices[] will be used
  Only arrayLarge[index*2] and arrayLarge[index*2 + 1] will be used
  Only arrayIncreases[index] will be used
*/

)
{
	size_t id = get_global_id(0);

	__local uint lut[LUT_SIZE32];
	__local uint128_t maxNs[LUT_SIZE32];

	if (get_local_id(0) == 0) {
		for (size_t i = 0; i < LUT_SIZE32; ++i) {
			lut[i] = pow3(i);
			//maxNs[i] = UINT128_MAX / lut[i];    // division does not compile
		}
		maxNs[0]  = ((uint128_t) 18446744073709551615 << 64) | 18446744073709551615;
		maxNs[1]  = ((uint128_t) 6148914691236517205 << 64) | 6148914691236517205;
		maxNs[2]  = ((uint128_t) 2049638230412172401 << 64) | 14347467612885206812;
		maxNs[3]  = ((uint128_t) 683212743470724133 << 64) | 17080318586768103348;
		maxNs[4]  = ((uint128_t) 227737581156908044 << 64) | 11842354220159218321;
		maxNs[5]  = ((uint128_t) 75912527052302681 << 64) | 10096366097956256645;
		maxNs[6]  = ((uint128_t) 25304175684100893 << 64) | 15663284748458453292;
		maxNs[7]  = ((uint128_t) 8434725228033631 << 64) | 5221094916152817764;
		maxNs[8]  = ((uint128_t) 2811575076011210 << 64) | 7889279663287456460;
		maxNs[9]  = ((uint128_t) 937191692003736 << 64) | 14927589270235519897;
		maxNs[10] = ((uint128_t) 312397230667912 << 64) | 4975863090078506632;
		maxNs[11] = ((uint128_t) 104132410222637 << 64) | 7807535721262686082;
		maxNs[12] = ((uint128_t) 34710803407545 << 64) | 14900341289560596438;
		maxNs[13] = ((uint128_t) 11570267802515 << 64) | 4966780429853532146;
		maxNs[14] = ((uint128_t) 3856755934171 << 64) | 13953422859090878459;
		maxNs[15] = ((uint128_t) 1285585311390 << 64) | 10800055644266810025;
		maxNs[16] = ((uint128_t) 428528437130 << 64) | 3600018548088936675;
		maxNs[17] = ((uint128_t) 142842812376 << 64) | 13497835565169346635;
		maxNs[18] = ((uint128_t) 47614270792 << 64) | 4499278521723115545;
		maxNs[19] = ((uint128_t) 15871423597 << 64) | 7648674198477555720;
		maxNs[20] = ((uint128_t) 5290474532 << 64) | 8698472757395702445;
	}

	barrier(CLK_LOCAL_MEM_FENCE);

	uint128_t h_minimum = ((uint128_t)(TASK_ID_KERNEL2 + 0)*9 << (TASK_SIZE_KERNEL2 - SIEVE_LOGSIZE));
	uint128_t h_supremum = ((uint128_t)(TASK_ID_KERNEL2 + 1)*9 << (TASK_SIZE_KERNEL2 - SIEVE_LOGSIZE));

	//int cMod = ((uint128_t)1 << SIEVE_LOGSIZE) % 3;
	int cMod = (SIEVE_LOGSIZE & 1) ? 2 : 1 ;



	size_t index = indices[id];
	if (index == (~(ulong)0) ) return;

	// deal with the 2^8 sieve that is used to compress the 2^k1 sieve
        //        L0 = TASK_ID * 2^TASK_SIZE             + (index / 16) * 256  + sieve8[index % 16]
	uint128_t L0 = ((uint128_t)TASK_ID << TASK_SIZE) + ((index >> 4) << 8) + sieve8[index & 0xf];

	int aMod = 0;
	//int bMod = L0 % 3;

	// trick for mod 3 if L0 is uint128_t
	ulong r = 0;
	r += (uint)(L0);
	r += (uint)(L0 >> 32);
	r += (uint)(L0 >> 64);
	r += (uint)(L0 >> 96);
	int bMod = r%3;


	/* precalculate */
	uint128_t L =  ( (uint128_t)arrayLarge[2*index] << 64 ) | (uint128_t)arrayLarge[2*index + 1] ;
	uint Salpha = (size_t)arrayIncreases[index];



/*
	uint128_t threeToSalpha = 1;
	do {
		uint alpha = min(Salpha, (uint)LUT_SIZE32 - 1);
		threeToSalpha *= lut[alpha];
		Salpha -= alpha;
	} while (Salpha > 0);
*/

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


/*
		uint128_t N = h * threeToSalpha + L;
*/
		uint128_t N = h;
		uint Salpha0 = Salpha;
		do {
			uint alpha = min(Salpha0, (uint)LUT_SIZE32 - 1);
			N *= lut[alpha];
			Salpha0 -= alpha;
		} while (Salpha0 > 0);
		N += L;

		do {

			do {
				N >>= ctz((uint)N);
			} while (!(N & 1));
			if (N < N0) {
				goto next;
			}

			N++;
			do {
				uint alpha = (uint)ctz((uint)N);
				alpha = min(alpha, (uint)LUT_SIZE32 - 1);
				N >>= alpha;
				if (N > maxNs[alpha]) {
					printf("  OVERFLOW: (%llu << 64) | %llu\n", (ulong)(N0 >> 64), (ulong)N0);
					goto next;
				}
				N *= lut[alpha];
			} while (!(N & 1));
			N--;

		} while (1);
next:
		;
	}


}
