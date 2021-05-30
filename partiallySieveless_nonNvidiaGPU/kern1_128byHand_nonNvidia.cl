

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

// ==
/* b should be uint128_t */
#define isEqualTo(a,b) (a.lo == b.lo && a.hi == b.hi)

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

// --
#define subOne(a) ({\
  if(a.lo == 0) a.hi--;\
  a.lo--;\
})

// -
static inline struct uint128_t sub( struct uint128_t a, ulong b){
  if(a.lo < b) a.hi--;
  a.lo -= b;
  return a;
}

// ++
#define addOne(a) ({\
  a.lo++;\
  if(a.lo == 0) a.hi++;\
})

// +
static inline struct uint128_t add( struct uint128_t a, ulong b){
  a.lo += b;
  if(a.lo < b) a.hi++;
  return a;
}











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

		struct uint128_t L0 = {0, TASK_ID};
		L0 = add( leftBitShift(L0, TASK_SIZE), pattern * 256 + sieve8[bit]);

		struct uint128_t L = L0;

		int Salpha = 0;      /* sum of alpha */

		struct uint128_t nextL = sub(L0, 1);
		int nextSalpha = 0;

		int R = SIEVE_LOGSIZE;  /* counter */
		int go = 1;
		do {
			/* note that L starts as odd, so we start by increasing */
			addOne(L);
			do {
				int alpha = ctz((uint)L.lo);
				alpha = min(alpha, (int)LUT_SIZE32 - 1);
				alpha = min(alpha, (int)R);
				R -= alpha;
				rightBitShiftEquals(L, alpha);
				mulEquals(L, (ulong)lut[alpha]);
				Salpha += alpha;
			} while (!(L.lo & 1) && R);
			subOne(L);
			while (!(L.lo & 1) && R) {
				int beta = ctz((uint)L.lo);
				beta = min(beta, (int)R);
				R -= beta;
				rightBitShiftEquals(L, beta);
				if (isLessThan(L, L0)) go = 0;
			}
		} while (go && R);

		/* 
		  Try something else.
		  See if L0 - 1 joins paths in SIEVE_LOGSIZE steps
		  Is it worth doing this ???
		*/

		R = SIEVE_LOGSIZE;
		while (go && R) {
			/* note that nextL starts as even, so we start by decreasing */
			while (!(nextL.lo & 1) && R) {
				int beta = ctz((uint)nextL.lo);
				beta = min(beta, (int)R);
				R -= beta;
				rightBitShiftEquals(nextL, beta);
			}
			addOne(nextL);
			do {
				int alpha = ctz((uint)nextL.lo);
				alpha = min(alpha, (int)LUT_SIZE32 - 1);
				alpha = min(alpha, (int)R);
				R -= alpha;
				rightBitShiftEquals(nextL, alpha);
				mulEquals(nextL, (ulong)lut[alpha]);
				nextSalpha += alpha;
			} while (!(nextL.lo & 1) && R);
			subOne(nextL);
		}


		/* only write to RAM if number needs to be tested */
		if ( isEqualTo(L, nextL) && Salpha == nextSalpha) go = 0;

		if (go) {
			arrayLarge[2*index] = L.hi;
			arrayLarge[2*index + 1] = L.lo;
			arrayIncreases[index] = (uchar)Salpha;
		}


		if (bit < 15)
			bit++;
		else {
			bit = 0;
			pattern++;
		}

	}

}
