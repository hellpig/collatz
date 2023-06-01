

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




__kernel void worker(
	__global ulong *arrayLarge,     // array length is 2^(TASK_SIZE + 1)
	__global uchar *arrayIncreases, // array length is 2^TASK_SIZE
	ulong task_id
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


	size_t i_minimum  = ((size_t)(id + 0) << (TASK_SIZE - TASK_UNITS));
	size_t i_supremum = ((size_t)(id + 1) << (TASK_SIZE - TASK_UNITS));


	for (size_t index = i_minimum; index < i_supremum; ++index) {

		uint128_t L0 = ((uint128_t)task_id << TASK_SIZE) + index;
		uint128_t L = L0;

		int R = SIEVE_LOGSIZE;  /* counter */

		size_t Salpha = 0; /* sum of alpha, which are the number of increases */

		if (L == 0) goto next;

		do {
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
				if (R == 0) goto next;
			} while (!(L & 1));
		} while (1);

next:

		arrayLarge[2*index] = (ulong)(L >> 64);
		arrayLarge[2*index + 1] = (ulong)L;
		arrayIncreases[index] = (uchar)Salpha;

	}

}
