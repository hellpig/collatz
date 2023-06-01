

typedef unsigned __int128 uint128_t;


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
	__global ushort *arraySmall,    // array length is 2^(TASK_SIZE - 8)
	__global ulong *arrayLarge,     // array length is 2^(TASK_SIZE - 3)
	__global uchar *arrayIncreases  // array length is 2^(TASK_SIZE - 4)
)
{
	size_t id = get_global_id(0);


	__local uint lut[LUT_SIZE32];

	if (get_local_id(0) == 0) {
		for (size_t i = 0; i < LUT_SIZE32; ++i) {
			lut[i] = pow3(i);
		}
	}

	barrier(CLK_LOCAL_MEM_FENCE);


	size_t i_minimum  = ((size_t)(id + 0) << (TASK_SIZE - TASK_UNITS - 4));
	size_t i_supremum = ((size_t)(id + 1) << (TASK_SIZE - TASK_UNITS - 4));


	/* loop over bits in arraySmall[] */
	size_t pattern = i_minimum >> 4;  /* a group of 256 numbers */
	int bit = 0;                      /* which of the 16 possible numbers in the 2^8 sieve */
	for (size_t index = i_minimum; index < i_supremum; ++index) {

		/* search 2^k1 sieve for next number that could require testing */
		while ( !( arraySmall[pattern] & ((int)1 << bit) ) ) {
			index++;
			if (bit < 15)
				bit++;
			else {
				bit = 0;
				pattern++;
			}
		}
		if (index >= i_supremum) {
			break;
		}

		uint128_t L0 = ((uint128_t)TASK_ID << TASK_SIZE) + pattern * 256 + sieve8[bit];

		uint128_t L = L0;
		size_t Salpha = 0;   /* sum of alpha, which are the number of increases */

		uint128_t nextL = L0 - 1;
		size_t nextSalpha = 0;

		int R = SIEVE_LOGSIZE;  /* counter */
		do {
			/* note that L starts as odd, so we start by increasing */
			L++;
			do {
				size_t alpha = (size_t)ctz((uint)L);
				alpha = min(alpha, (size_t)LUT_SIZE32 - 1);
				alpha = min(alpha, (size_t)R);
				R -= alpha;
				L >>= alpha;
				L *= lut[alpha];
				Salpha += alpha;
				if (R == 0) {
					L--;
					goto next;
				}
			} while (!(L & 1));
			L--;
			do {
				size_t beta = ctz((uint)L);
				beta = min(beta, (size_t)R);
				R -= beta;
				L >>= beta;
				if (L < L0) goto next3;
				if (R == 0) goto next;
			} while (!(L & 1));
		} while (1);

next:

		/* 
		  Try something else.
		  See if L0 - 1 joins paths in SIEVE_LOGSIZE steps
		  Is it worth doing this ???
		*/

		R = SIEVE_LOGSIZE;
		do {
			/* note that nextL starts as even, so we start by decreasing */
			do {
				size_t beta = ctz((uint)nextL);
				beta = min(beta, (size_t)R);
				R -= beta;
				nextL >>= beta;
				if (R == 0) goto next2;
			} while (!(nextL & 1));
			nextL++;
			do {
				size_t alpha = (size_t)ctz((uint)nextL);
				alpha = min(alpha, (size_t)LUT_SIZE32 - 1);
				alpha = min(alpha, (size_t)R);
				R -= alpha;
				nextL >>= alpha;
				nextL *= lut[alpha];
				nextSalpha += alpha;
				if (R == 0) {
					nextL--;
					goto next2;
				}
			} while (!(nextL & 1));
			nextL--;
		} while (1);


next2:

		/* only write to RAM if number needs to be tested */
		if ( L == nextL && Salpha == nextSalpha) goto next3;

		arrayLarge[2*index] = (ulong)(L >> 64);
		arrayLarge[2*index + 1] = (ulong)L;
		arrayIncreases[index] = (uchar)Salpha;

		//printf(" %llu\n", (ulong)L0);

next3:

		if (bit < 15)
			bit++;
		else {
			bit = 0;
			pattern++;
		}

	}

}
