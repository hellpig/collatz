
/*
// A very slow ctz() algorithm just to get this to compile on certain crappy computers
uint ctz(uint n) {
  int count = 0;
  while( !(n & 1) ) {   // while even
    count++;
    if( count == 32 ) break;
    n >>= 1;
  }
  return count;
}
*/

uint pow3(size_t n)
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
	__global ulong *arrayLarge2     // array length is 2^SIEVE_LOGSIZE2
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


	size_t i_minimum  = ((size_t)(id + 0) << (SIEVE_LOGSIZE2 - TASK_UNITS));
	size_t i_supremum = ((size_t)(id + 1) << (SIEVE_LOGSIZE2 - TASK_UNITS));


	for (size_t index = i_minimum; index < i_supremum; ++index) {

		ulong L = index;   // index is the initial L

		int R = SIEVE_LOGSIZE2;  /* counter */

		size_t Salpha = 0; /* sum of alpha */

		if (L == 0) { arrayLarge2[index] = (ulong)0; continue; }

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
			} while (!(L & 1) && R);
			L--;
			while (!(L & 1) && R) {
				size_t beta = ctz((uint)L);
				beta = min(beta, (size_t)R);
				R -= beta;
				L >>= beta;
			} 
		} while (R);


		/* stores both L and Salpha */
		arrayLarge2[index] = L + ((ulong)Salpha << 58);

	}

}
