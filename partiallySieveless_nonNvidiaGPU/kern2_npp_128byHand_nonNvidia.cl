
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


/* 2^8 sieve */
__constant static const uchar sieve8[16] = {27, 31, 47, 71, 91, 103, 111, 127, 155, 159, 167, 191, 231, 239, 251, 255};








/*

    Here is my implementation of 128-bit integers

    Many of the functions below are defined via "#define".
    Be sure to not pass a calculation into these functions!
    For example...
      rightBitShiftEquals(N, ctz((uint)N.lo));
    will have a non-constant ctz() throughout the execution.
    Also, for these "#define" functions, be sure to pass
    the correct type (either ulong or uint128_t).

    I did this in C (not C++) to not have to worry about compiler uncertainty.
    This is the same reason I prefer "#define" over an inline function.
    Also, C++ in OpenCL kernels is not yet widely supported.

*/

struct uint128_t{ ulong hi; ulong lo; };

// <
/* b should be uint128_t */
#define isLessThan(a,b) ((a.hi < b.hi) || ((a.hi == b.hi) && (a.lo < b.lo)))

// >
/* b should be uint128_t */
#define isGreaterThan(a,b) ((a.hi > b.hi) || ((a.hi == b.hi) && (a.lo > b.lo)))

// >=
/* b should be uint128_t */
#define isGreaterThanOrEqualTo(a,b) ((a.hi > b.hi) || ((a.hi == b.hi) && (a.lo >= b.lo)))

// >>=
/*
   0 < b < 64
   Actually, b==0 works if a.hi==0.
*/
#define rightBitShiftEquals(a,b) ({\
  a.lo = (a.lo >> b) | (a.hi << (64-b));\
  a.hi >>= b;\
})

// <<
/*
   0 < b < 64
   which requires that 0 < (TASK_SIZE_KERNEL2 - k) < 64
   and k not be larger than 63.
   k not being too large is the important restriction.
*/
static inline struct uint128_t leftBitShift( struct uint128_t a, int b){
  a.hi = (a.hi << b) | (a.lo >> (64-b));
  a.lo <<= b;
  return a;
}

// *=
/* b should be ulong */
#define mulEquals(a,b) ({\
  a.hi *= b;\
  a.hi += mul_hi(a.lo, b);\
  a.lo *= b;\
})

// *
static inline struct uint128_t mul( struct uint128_t a, ulong b){
  a.hi *= b;
  a.hi += mul_hi(a.lo, b);
  a.lo *= b;
  return a;
}

// -=
/* b should be ulong */
#define subEquals(a,b) ({\
  if(a.lo < b) a.hi--;\
  a.lo -= b;\
})

// +=
/* b should be ulong */
#define addEquals(a,b) ({\
  a.lo += b;\
  if(a.lo < b) a.hi++;\
})

// +=
/* b should be uint128_t */
#define addEquals128(a,b) ({\
  a.lo += b.lo;\
  a.hi += b.hi;\
  if(a.lo < b.lo) a.hi++;\
})

// +
static inline struct uint128_t add( struct uint128_t a, ulong b){
  a.lo += b;
  if(a.lo < b) a.hi++;
  return a;
}

// +
static inline struct uint128_t add128( struct uint128_t a, struct uint128_t b){
  a.lo += b.lo;
  a.hi += b.hi;
  if(a.lo < b.lo) a.hi++;
  return a;
}











__kernel void worker(
	__global ulong *indices,         // has a much shorter length than the rest of the arrays
	__global ulong *arrayLarge,      // actually 128-bit integers
	__global uchar *arrayIncreases,
	__global ulong *checksum_alpha,
	int chunk
)
{
	size_t id = get_global_id(0);
	ulong private_checksum_alpha = (ulong)0;

	__local uint lut[LUT_SIZE32];
	__local struct uint128_t maxNs[LUT_SIZE32];

	if (get_local_id(0) == 0) {
		for (size_t i = 0; i < LUT_SIZE32; ++i) {
			lut[i] = pow3(i);
			//maxNs[i] = UINT128_MAX / lut[i];
		}
		maxNs[0].hi = 18446744073709551615;
		maxNs[0].lo = 18446744073709551615;
		maxNs[1].hi = 6148914691236517205;
		maxNs[1].lo = 6148914691236517205;
		maxNs[2].hi = 2049638230412172401;
		maxNs[2].lo = 14347467612885206812;
		maxNs[3].hi = 683212743470724133;
		maxNs[3].lo = 17080318586768103348;
		maxNs[4].hi = 227737581156908044;
		maxNs[4].lo = 11842354220159218321;
		maxNs[5].hi = 75912527052302681;
		maxNs[5].lo = 10096366097956256645;
		maxNs[6].hi = 25304175684100893;
		maxNs[6].lo = 15663284748458453292;
		maxNs[7].hi = 8434725228033631;
		maxNs[7].lo = 5221094916152817764;
		maxNs[8].hi = 2811575076011210;
		maxNs[8].lo = 7889279663287456460;
		maxNs[9].hi = 937191692003736;
		maxNs[9].lo = 14927589270235519897;
		maxNs[10].hi = 312397230667912;
		maxNs[10].lo = 4975863090078506632;
		maxNs[11].hi = 104132410222637;
		maxNs[11].lo = 7807535721262686082;
		maxNs[12].hi = 34710803407545;
		maxNs[12].lo = 14900341289560596438;
		maxNs[13].hi = 11570267802515;
		maxNs[13].lo = 4966780429853532146;
		maxNs[14].hi = 3856755934171;
		maxNs[14].lo = 13953422859090878459;
		maxNs[15].hi = 1285585311390;
		maxNs[15].lo = 10800055644266810025;
		maxNs[16].hi = 428528437130;
		maxNs[16].lo = 3600018548088936675;
		maxNs[17].hi = 142842812376;
		maxNs[17].lo = 13497835565169346635;
		maxNs[18].hi = 47614270792;
		maxNs[18].lo = 4499278521723115545;
		maxNs[19].hi = 15871423597;
		maxNs[19].lo = 7648674198477555720;
		maxNs[20].hi = 5290474532;
		maxNs[20].lo = 8698472757395702445;
	}

	barrier(CLK_LOCAL_MEM_FENCE);

	// get h_minimum and h_supremum
	struct uint128_t h_minimum = {0, (TASK_ID_KERNEL2 + 0)};
	h_minimum = leftBitShift(mul(h_minimum, 9), TASK_SIZE_KERNEL2 - SIEVE_LOGSIZE);
	struct uint128_t h_step = {0, 9};
	h_step = leftBitShift(h_step, TASK_SIZE_KERNEL2 - SIEVE_LOGSIZE - CHUNKS_KERNEL2);
	struct uint128_t h_supremum = mul(h_step, (ulong)chunk);
	addEquals128( h_minimum, h_supremum );
	h_supremum = add128(h_minimum, h_step);

	//int cMod = ((uint128_t)1 << SIEVE_LOGSIZE) % 3;
	int cMod = (SIEVE_LOGSIZE & 1) ? 2 : 1 ;



	size_t index = indices[id];
	if (index == (~(ulong)0)) {
		checksum_alpha[id] = 0;
		return;
	}

	// deal with the 2^8 sieve that is used to compress the 2^k1 sieve
        //        L0 = TASK_ID * 2^TASK_SIZE  +  (index / 16) * 256  +  sieve8[index % 16]
	struct uint128_t L0 = {0, TASK_ID};
	L0 = add( leftBitShift(L0, TASK_SIZE), ((index >> 4) << 8) + sieve8[index & 0xf] );

	int aMod = 0;
	//int bMod = L0 % 3;

	// trick for mod 3 if L0 is uint128_t
	ulong r = 0;
	r += (uint)(L0.lo);
	r += (L0.lo >> 32);
	r += (uint)(L0.hi);
	r += (L0.hi >> 32);
	int bMod = r%3;


	/* precalculate */
	struct uint128_t L = {arrayLarge[2*index], arrayLarge[2*index + 1]} ;
	uint Salpha = (uint)arrayIncreases[index];


	/* iterate over highest bits */
	for (struct uint128_t h = h_minimum; isLessThan(h, h_supremum); addEquals(h,1)) {
		struct uint128_t N0 = add128( leftBitShift(h, SIEVE_LOGSIZE) , L0 );
		int N0Mod = (aMod * cMod + bMod) % 3;
		aMod++;
		if (aMod > 2) aMod -= 3;

		while (N0Mod == 2) {
			addEquals(h,1);
			N0 = add128( leftBitShift(h, SIEVE_LOGSIZE) , L0 );
			N0Mod = (aMod * cMod + bMod) % 3;
			aMod++;
			if (aMod > 2) aMod -= 3;
		}
		if (isGreaterThanOrEqualTo(h, h_supremum)) {
			break;
		}


		/* find the N after the first k steps */
		struct uint128_t N = h;
		uint Salpha0 = Salpha;
		do {
			uint alpha = min(Salpha0, (uint)LUT_SIZE32 - 1);
			mulEquals(N, (ulong)lut[alpha]);
			Salpha0 -= alpha;
		} while (Salpha0 > 0);
		addEquals128(N, L);


		int count = Salpha;
		int stop = 0;
		do {

			/* a "do while" loop won't work because rightBitShiftEquals(N, 0) isn't defined */
			while (!(N.lo & 1)) {
				uint beta = ctz((uint)N.lo);
				rightBitShiftEquals(N, beta);
			}
			if (isLessThan(N, N0)) {
				break;
			}

			addEquals(N,1);
			do {
				uint alpha = (uint)ctz((uint)N.lo);
				alpha = min(alpha, (uint)LUT_SIZE32 - 1);

				//if(!alpha) break;
				rightBitShiftEquals(N, alpha);

				if (isGreaterThan(N, maxNs[alpha])) {
					printf("  OVERFLOW: (%lu << 64) | %lu\n", N0.hi, N0.lo);
					stop = 1;
					break;
				}
				mulEquals(N, (ulong)lut[alpha]);

				count += alpha;
			} while (!(N.lo & 1));
			if (stop) break;
			//subEquals(N,1);
			N.lo--;    // N is odd

			/* I think watchdog timers require this check ?? */
			if (count > 2048){
				printf("  Too many steps: (%lu << 64) | %lu\n", N0.hi, N0.lo);
				break;
			}
		} while (1);
		private_checksum_alpha += (ulong)count;

	}

	checksum_alpha[id] = private_checksum_alpha;

}
