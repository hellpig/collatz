/* ******************************************

Sieves of size 2^k are used, where k can be very large!
Not a huge amount of RAM is used.

On Linux, compile via something like...
  nvcc collatzPartiallySieveless_npp_CUDA.cu

You'll need a 64-bit CPU and GPU.

Note that I use the "long long" function strtoull when reading in the arguments.

Currently loads in a sieve file, which must must match the k1 value set in this code.

Currently requires my cuda_uint128.h

k < 81 must be true.





Parts of this file are modified from...
  https://github.com/xbarin02/collatz/blob/master/src/gpuworker/kernel32-precalc.cl



This code requires two arguments...
  ./a.out  TASK_ID_KERNEL2  TASK_ID

Starting at 0, increase TASK_ID by 1 each run until its max
  ( 2^(k - TASK_SIZE) - 1 )
Then, reset TASK_ID and increase TASK_ID_KERNEL2 by 1 (starting at 0).
Don't skip!

Here is a neat way to run this code...
  seq -f %1.0f 0 1048575 | xargs -L 1 -P 1 ./a.out 0 |tee -a log.txt &
To pick up where you left off, change the start (and stop) value of seq.
k, TASK_SIZE, and TASK_SIZE_KERNEL2 should not change between runs.

For each TASK_ID_KERNEL2, 9 * 2 ^ TASK_SIZE_KERNEL2 numbers will be tested,
  but only after each TASK_ID is run from 0 to ( 2^(k - TASK_SIZE) - 1 )
Why the 9? I thought it might help my GPU code, but it only does EXTREMELY SLIGHTLY.
Feel free to get rid of the 9 when h_minimum and h_supremum are defined in kernel2.
You'll also want to get rid of the division by 9 when this host program
  checks if TASK_ID_KERNEL2 will cause certain overflow.
If TASK_ID_KERNEL2 > 0, you'll also want to fix the line in kernel2...
  aMod = 0




This code is not compatible with a watchdog timer.
I recommend that you play around with your device to see if it has a GPU watchdog timer.
It has one if the kernel always stops after a certain amount of time (like 5 seconds).

I would expect that, if a bunch of OVERFLOW messages are printed by kernel2
  within a short period of time, some might be lost due to buffer sizes ??

55247846101001863167 will overflow 128 bits.
This number is over 64-bits, so it cannot be made into an integer literal.
To test 128-bit overflow and using printf() in kernel2,
calculate the following using Python 3, then put it as your TASK_ID_KERNEL2...
  TASK_ID_KERNEL2 = 55247846101001863167 // (9 << TASK_SIZE_KERNEL2)
Then set the following as your TASK_ID...
  remainder = (55247846101001863167 % (9 << TASK_SIZE_KERNEL2))
  TASK_ID = (remainder % (1 << k)) // (1 << TASK_SIZE)
where I am using a double slash, //, for integer division.



(c) 2021 Bradley Knockel

****************************************** */


#include "cuda_uint128.h"

#include <unistd.h>   // for usleep()

#include <sys/time.h>
struct timeval tv1, tv2;





/*
  k < 81
*/

#define SIEVE_LOGSIZE 51
const int k = SIEVE_LOGSIZE;



/*
  The following are in log2...
  For kernel1, which starts making the sieve...
    TASK_UNITS + 8 <= TASK_SIZE <= k
  Will use more than 2^TASK_SIZE bytes of RAM
  Due to the limits of strtoull(), k - TASK_SIZE < 64
  2^TASK_UNITS should be much larger than the number of CUDA Cores you have
  (fraction of 2^k sieve not excluded) * 2^TASK_SIZE should be much larger than the number of CUDA Cores you have
  My CPU-only code prefers a smaller TASK_SIZE so that the task finishes in a reasonable time
*/

#define TASK_SIZE 24
#define TASK_UNITS 16



/*
  The following is in log2...
  TASK_SIZE_KERNEL2 - k should ideally be at least 10
  For kernel2, which tests numbers given a sieve...
  9 * 2^(TASK_SIZE_KERNEL2 + TASK_SIZE - k) numbers will be run by each process
  9 * 2^TASK_SIZE_KERNEL2 numbers will be run total by all processes
*/

#define TASK_SIZE_KERNEL2 67






// For the 2^k1 sieve that speeds up the calculation of the 2^k sieve...
//   TASK_SIZE <= k1 <= k
// The only con of a larger k1 is that the sieve file is harder to create and store,
//   but, once you have the sieve file, use it!
// Especially good values are 32, 35, 37, 40, ...

const int k1 = 37;

const char file[10] = "sieve37";







// integer that is 2^(non-negative integer), so 1, 2, 4, 8, ...
// For setting the number of CUDA threads per block
// Currently, this controls all three kernels
// Note that 2^TASK_UNITS must be at least (multipleOf32 * 2^5), but this shouldn't be a problem
// Note that (2^TASK_UNITS / (multipleOf32 * 2^5) ) cannot exceed the max unsigned int
//   because blockIdx.x is an unsigned int (again, not a problem)

#define multipleOf32 1









/*
  just for kernels
*/

#define LUT_SIZE32 21

#define UINT128_MAX (~(uint128_t)0)

__device__ uint32_t pow3(size_t n)   // returns 3^n
{
	uint32_t r = 1;
	uint32_t b = 3;

	while (n) {
		if (n & 1) {
			r *= b;
		}
		b *= b;
		n >>= 1;
	}

	return r;
}

/* 2^8 sieve */
__constant__ uint8_t sieve8[16];
const uint8_t sieve8_host[16] = {27, 31, 47, 71, 91, 103, 111, 127, 155, 159, 167, 191, 231, 239, 251, 255};










/*
  kernel1 makes 2^k sieve
*/


__global__ void kernel1(
	uint16_t *arraySmall,    // array length is 2^(TASK_SIZE - 8)
	uint64_t *arrayLarge,    // array length is 2^(TASK_SIZE - 3)
	uint8_t *arrayIncreases, // array length is 2^(TASK_SIZE - 4)
	uint64_t TASK_ID
)
{
	size_t id = (size_t)blockIdx.x * (size_t)blockDim.x + (size_t)threadIdx.x;


	__shared__ uint32_t lut[LUT_SIZE32];

	if (threadIdx.x == 0) {
		for (size_t i = 0; i < LUT_SIZE32; ++i) {
			lut[i] = pow3(i);
		}
	}

	__syncthreads();


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

		uint128_t L0 = ((uint128_t)TASK_ID << TASK_SIZE) + (pattern * 256 + sieve8[bit]);

		uint128_t L = L0;
		uint32_t Salpha = 0;    /* sum of alpha, which are the number of increases */

		uint128_t nextL = L0 - 1;
		uint32_t nextSalpha = 0;

		int R = SIEVE_LOGSIZE;  /* counter */
		do {
			/* note that L starts as odd, so we start by increasing */
			++L;
			do {
				//uint32_t alpha = (uint32_t)ctz((uint32_t)L.lo);
				/* note that __ffs(0) returns 0, and 0 - 1 is caught by min() later */
				uint32_t alpha = (__ffs((uint32_t)L.lo) - 1);

				alpha = min(alpha, (uint32_t)LUT_SIZE32 - 1);
				alpha = min(alpha, (uint32_t)R);
				R -= alpha;
				L >>= alpha;
				L *= lut[alpha];
				Salpha += alpha;
				if (R == 0) {
					--L;
					goto next1;
				}
			} while (!(L.lo & 1));
			L.lo--;    // L is odd
			do {
				//size_t beta = ctz((uint32_t)L.lo);
				/* note that __ffs(0) returns 0, and 0 - 1 is caught by min() later */
				uint32_t beta = __ffs((uint32_t)L.lo) - 1;

				beta = min(beta, (uint32_t)R);
				R -= beta;
				L >>= beta;
				if (L < L0) goto next3;
				if (R == 0) goto next1;
			} while (!(L.lo & 1));
		} while (1);

next1:

		/* 
		  Try something else.
		  See if L0 - 1 joins paths in SIEVE_LOGSIZE steps
		  Is it worth doing this ???
		*/

		R = SIEVE_LOGSIZE;
		do {
			/* note that nextL starts as even, so we start by decreasing */
			do {
				//size_t beta = ctz((uint32_t)nextL.lo);
				/* note that __ffs(0) returns 0, and 0 - 1 is caught by min() later */
				uint32_t beta = __ffs((uint32_t)nextL.lo) - 1;

				beta = min(beta, (uint32_t)R);
				R -= beta;
				nextL >>= beta;
				if (R == 0) goto next2;
			} while (!(nextL.lo & 1));
			++nextL;
			do {
				//uint32_t alpha = (size_t)ctz((uint32_t)nextL.lo);
				/* note that __ffs(0) returns 0, and 0 - 1 is caught by min() later */
				uint32_t alpha = __ffs((uint32_t)nextL.lo) - 1;

				alpha = min(alpha, (uint32_t)LUT_SIZE32 - 1);
				alpha = min(alpha, (uint32_t)R);
				R -= alpha;
				nextL >>= alpha;
				nextL *= lut[alpha];
				nextSalpha += alpha;
				if (R == 0) {
					--nextL;
					goto next2;
				}
			} while (!(nextL.lo & 1));
			nextL.lo--;    // nextL is odd
		} while (1);


next2:

		/* only write to RAM if number needs to be tested */
		if ( L == nextL && Salpha == nextSalpha) goto next3;

		arrayLarge[2*index] = L.hi;
		arrayLarge[2*index + 1] = L.lo;
		arrayIncreases[index] = (uint8_t)Salpha;

		//printf(" %llu\n", L0.lo);

next3:

		if (bit < 15)
			bit++;
		else {
			bit = 0;
			pattern++;
		}

	}

}







/*
  kernel2 uses the sieves
*/

__global__ void kernel2(
	uint64_t *indices,         // has a much shorter length than the rest of the arrays
	uint64_t *arrayLarge,      // actually 128-bit integers
	uint8_t *arrayIncreases,
	uint64_t TASK_ID,
	uint64_t TASK_ID_KERNEL2

/*
  index = indices[id] is the only time indices[] will be used
  Only arrayLarge[index*2] and arrayLarge[index*2 + 1] will be used
  Only arrayIncreases[index] will be used
*/

)
{
	size_t id = (size_t)blockIdx.x * (size_t)blockDim.x + (size_t)threadIdx.x;

	__shared__ uint32_t lut[LUT_SIZE32];
	__shared__ uint128_t maxNs[LUT_SIZE32];

	if (threadIdx.x == 0) {
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

	__syncthreads();

	uint128_t h_minimum = ((uint128_t)(TASK_ID_KERNEL2 + 0)*9 << (TASK_SIZE_KERNEL2 - SIEVE_LOGSIZE));
	uint128_t h_supremum = ((uint128_t)(TASK_ID_KERNEL2 + 1)*9 << (TASK_SIZE_KERNEL2 - SIEVE_LOGSIZE));

	//int cMod = ((uint128_t)1 << SIEVE_LOGSIZE) % 3;
	int cMod = (SIEVE_LOGSIZE & 1) ? 2 : 1 ;



	size_t index = indices[id];
	if (index == (~(uint64_t)0) ) return;

	// deal with the 2^8 sieve that is used to compress the 2^k1 sieve
        //        L0 = TASK_ID * 2^TASK_SIZE             + (index / 16) * 256   + sieve8[index % 16]
	uint128_t L0 = ((uint128_t)TASK_ID << TASK_SIZE) + (((index >> 4) << 8) + sieve8[index & 0xf]);

	int aMod = 0;
	//int bMod = L0 % 3;

	// trick for mod 3 if L0 is uint128_t
	uint64_t r = 0;
	r += (uint32_t)(L0.lo);
	r += (L0.lo >> 32);
	r += (uint32_t)(L0.hi);
	r += (L0.hi >> 32);
        int bMod = r%3;


	/* precalculate */
	uint128_t L;
	L.hi = arrayLarge[2*index];
        L.lo = arrayLarge[2*index + 1];
	uint32_t Salpha = (uint32_t)arrayIncreases[index];


	/* iterate over highest bits */
	for (uint128_t h = h_minimum; h < h_supremum; ++h) {
		uint128_t N0 = (h << SIEVE_LOGSIZE) + L0;
		int N0Mod = (aMod * cMod + bMod) % 3;
		aMod++;
		if (aMod > 2) aMod -= 3;

		while (N0Mod == 2) {
			++h;
			N0 = (h << SIEVE_LOGSIZE) + L0;
			N0Mod = (aMod * cMod + bMod) % 3;
			aMod++;
			if (aMod > 2) aMod -= 3;
		}
		if (h >= h_supremum) {
			break;
		}



		uint128_t N = h;
		uint32_t Salpha0 = Salpha;
		do {
			uint32_t alpha = min(Salpha0, (uint32_t)LUT_SIZE32 - 1);
			N *= lut[alpha];
			Salpha0 -= alpha;
		} while (Salpha0 > 0);
		N += L;


		do {

			/* a "do while" loop won't work because "N >>= 0" isn't defined */
			while (!(N.lo & 1)) {
				//N >>= ctz((uint32_t)N.lo);
				/* note that __ffs(0) returns 0, and 0 - 1 very large */
				uint32_t beta = __ffs((uint32_t)N.lo) - 1;
				if (beta > 32) beta=32;
				N >>= beta;
				
			}
			if (N < N0) {
				goto next;
			}

			++N;
			do {
				//uint32_t alpha = (size_t)ctz((uint32_t)N.lo);
				/* note that __ffs(0) returns 0, and 0 - 1 is caught by min() later */
				uint32_t alpha = (__ffs((uint32_t)N.lo) - 1);

				alpha = min(alpha, (uint32_t)LUT_SIZE32 - 1);
				N >>= alpha;
				if (N > maxNs[alpha]) {
					printf("  OVERFLOW: (%llu << 64) | %llu\n", N0.hi, N0.lo);
					goto next;
				}
				N *= lut[alpha];
			} while (!(N.lo & 1));
			N.lo--;   // N is odd

		} while (1);
next:
		;
	}


}









int main(int argc, char *argv[]) {


  if( argc < 3 ) {
    printf("Too few arguments. Aborting.\n");
    return 0;
  }

  uint64_t TASK_ID_KERNEL2 = (uint64_t)strtoull(argv[1], NULL, 10);

  uint64_t TASK_ID         = (uint64_t)strtoull(argv[2], NULL, 10);

  uint64_t maxTaskID = ((uint64_t)1 << (k - TASK_SIZE));
  if ( TASK_ID >= maxTaskID ) {
    printf("Aborting. TASK_ID must be less than %" PRIu64 "\n", maxTaskID);
    return 0;
  }


  /* Check for 100%-certain overflow.
     This check prevents having to check when doing any pre-calculation when interlacing
     Note that after k steps, for A * 2^k + B...
       B = 2^k - 1 will become 3^k - 1
       and A*2^k will become A*3^k
  */
  __uint128_t temp = 1;  // will equal 3^k
  __uint128_t UINT128_MAX2 = (~(__uint128_t)0);
  for (int i=0; i < k; i++) { temp *= 3; }
  if ( (__uint128_t)TASK_ID_KERNEL2 > ((UINT128_MAX2 - temp) >> (TASK_SIZE_KERNEL2 - k)) / 9 / temp - 1 ) {
    printf("  Well, aren't you ambitious!\n");
    return 0;
  }



  printf("TASK_ID_KERNEL2 = %" PRIu64 "\n", TASK_ID_KERNEL2);
  printf("TASK_ID = %" PRIu64 "\n", TASK_ID);
  printf("TASK_ID must be less than %" PRIu64 "\n", maxTaskID);
  printf("TASK_SIZE_KERNEL2 = %i\n", TASK_SIZE_KERNEL2);
  printf("TASK_SIZE = %i\n", TASK_SIZE);
  printf("  k = %i\n", k);
  printf("  k1 = %i\n", k1);
  printf("\n");
  fflush(stdout);





  // start timing
  gettimeofday(&tv1, NULL);



	/* setup kernel stuff */

	cudaError_t ret;

	size_t arraySmallCount = ((size_t)1 << (TASK_SIZE - 8)) + 1;   // each element is 2^8 numbers; add 1 to prevent kern1 from reading too far
	size_t arrayLargeCount = (size_t)1 << (TASK_SIZE - 3);     // two of these are needed per uint128
	size_t arrayIncreasesCount = (size_t)1 << (TASK_SIZE - 4); // one for each bit in portion of 2^k1 sieve
	size_t indicesCount = (size_t)1 << (TASK_SIZE - 4);        // not all of this will be used

	uint16_t *arraySmall = (uint16_t *)malloc(sizeof(uint16_t) * arraySmallCount);
	uint64_t *arrayLarge = (uint64_t *)malloc(sizeof(uint64_t) * arrayLargeCount);
	uint8_t *arrayIncreases = (uint8_t *)malloc(sizeof(uint8_t) * arrayIncreasesCount);
	uint64_t *indices = (uint64_t *)malloc(sizeof(uint64_t) *indicesCount);

	if ( arraySmall == NULL || arrayLarge == NULL || arrayIncreases == NULL || indices == NULL ) {
		return -1;
	}

	// will be in GPU RAM
	uint16_t *d_arraySmall;
	uint64_t *d_arrayLarge;
	uint8_t *d_arrayIncreases;
	uint64_t *d_indices;

	ret = cudaMalloc(&d_arraySmall, sizeof(uint16_t) * arraySmallCount);
	if ( ret != cudaSuccess ){ printf("cudaMalloc Error: %s\n", cudaGetErrorString(ret)); return -1; }
	ret = cudaMalloc(&d_arrayLarge, sizeof(uint64_t) * arrayLargeCount);
	if ( ret != cudaSuccess ){ printf("cudaMalloc Error: %s\n", cudaGetErrorString(ret)); return -1; }
	ret = cudaMalloc(&d_arrayIncreases, sizeof(uint8_t) * arrayIncreasesCount);
	if ( ret != cudaSuccess ){ printf("cudaMalloc Error: %s\n", cudaGetErrorString(ret)); return -1; }
	ret = cudaMalloc(&d_indices, sizeof(uint64_t) * indicesCount);
	if ( ret != cudaSuccess ){ printf("cudaMalloc Error: %s\n", cudaGetErrorString(ret)); return -1; }

	size_t global_work_size1;
	size_t global_work_size2;

	global_work_size1 = (size_t)1 << TASK_UNITS;

	// copy constant data to GPU
	cudaMemcpyToSymbol(sieve8, sieve8_host, sizeof(uint8_t)*16);





  /* open the 2^k1 sieve file */

  FILE* fp;
  size_t file_size;

  fp = fopen(file, "rb");

  // Check file size
  // Bytes in sieve file are 2^(k1 - 7)
  fseek(fp, 0, SEEK_END);
  file_size = ftell(fp);
  if ( file_size != ((size_t)1 << (k1 - 7)) ) {
    printf("  error: wrong sieve file!\n");
    return 0;
  }

  /*
    Seek to necessary part of the file
    Note that ((((uint64_t)1 << k1) - 1) & bStart) equals bStart % ((uint64_t)1 << k1)
  */
  __uint128_t bStart = ( (__uint128_t)TASK_ID << TASK_SIZE );
  fseek(fp, ((((uint64_t)1 << k1) - 1) & bStart) >> 7, SEEK_SET);







	/*
	 fill arraySmall[] with part of 2^k1 array, and initialize and arrayLarge[] and arrayIncreases[] to 0,
	 then send to GPU
	*/

	fread(arraySmall, sizeof(uint16_t), arraySmallCount - 1, fp);
	arraySmall[arraySmallCount - 1] = 0xffff;    // to stop kern1 from reading too far
	for (size_t i = 0; i < arrayLargeCount; i++) arrayLarge[i] = 0;
	for (size_t i = 0; i < arrayIncreasesCount; i++) arrayIncreases[i] = 0;

	ret = cudaMemcpy(d_arraySmall, arraySmall, sizeof(uint16_t) * arraySmallCount, cudaMemcpyHostToDevice);
	if ( ret != cudaSuccess ){ printf("cudaMemcpy Error: %s\n", cudaGetErrorString(ret)); return -1; }
	ret = cudaMemcpy(d_arrayLarge, arrayLarge, sizeof(uint64_t) * arrayLargeCount, cudaMemcpyHostToDevice);
	if ( ret != cudaSuccess ){ printf("cudaMemcpy Error: %s\n", cudaGetErrorString(ret)); return -1; }
	ret = cudaMemcpy(d_arrayIncreases, arrayIncreases, sizeof(uint8_t) * arrayIncreasesCount, cudaMemcpyHostToDevice);
	if ( ret != cudaSuccess ){ printf("cudaMemcpy Error: %s\n", cudaGetErrorString(ret)); return -1; }








	gettimeofday(&tv2, NULL);
	printf("  kernel1 is starting: %e seconds elapsed\n",
		(double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec));
	fflush(stdout);

        /* start kernel1 */
	kernel1<<<global_work_size1/(32*multipleOf32), 32*multipleOf32>>>(d_arraySmall, d_arrayLarge, d_arrayIncreases, TASK_ID);










	/* wait for kernel1 to finish */
	cudaDeviceSynchronize();

	gettimeofday(&tv2, NULL);
	printf("  kernel1 is finished: %e seconds elapsed\n",
		(double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec));
	fflush(stdout);

	// read arrayIncreases from GPU
	ret = cudaMemcpy(arrayIncreases, d_arrayIncreases, sizeof(uint8_t) * arrayIncreasesCount, cudaMemcpyDeviceToHost);
	if ( ret != cudaSuccess ){ printf("cudaMemcpy read error: %s\n", cudaGetErrorString(ret)); return -1; }






  /* fill indices[] */

  global_work_size2 = 0;  // for seeing how much work kernel2 has to do

  for (size_t index = 0; index < arrayIncreasesCount; index++) {

        if (arrayIncreases[index]) {
          indices[global_work_size2] = index;
          global_work_size2++;
        }

  }

  printf("Numbers in sieve segment that need testing = %zu\n", global_work_size2);

  /* pad indices[] to make global_work_size2 a multiple of (32 * multipleOf32) */
  for (int j = 0; j < (global_work_size2 % (32 * multipleOf32)); j++) {
    indices[global_work_size2] = (~(uint64_t)0);        // I believe this requires TASK_SIZE < 64 + 4
    global_work_size2++;
  }











	/* run kernel 2 */

	ret = cudaMemcpy(d_indices, indices, sizeof(uint64_t) * global_work_size2, cudaMemcpyHostToDevice);
	if ( ret != cudaSuccess ){ printf("cudaMemcpy Error: %s\n", cudaGetErrorString(ret)); return -1; }

	gettimeofday(&tv2, NULL);
	printf("  kernel2 is starting: %e seconds elapsed\n",
		(double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec));
	fflush(stdout);

	cudaEvent_t event;
	cudaEventCreateWithFlags(&event, cudaEventDisableTiming);
	kernel2<<<global_work_size2/(32*multipleOf32), 32*multipleOf32>>>(d_indices, d_arrayLarge, d_arrayIncreases, TASK_ID, TASK_ID_KERNEL2);
	cudaEventRecord(event);


	/* wait for kernel2 to finish without busy waiting */
	//cudaDeviceSynchronize();
	while(cudaEventQuery(event) != cudaSuccess) {
		usleep(10000);  // sleep for 1/100 of a second
	}








  gettimeofday(&tv2, NULL);
  printf("  Elapsed wall time is %e seconds\n\n",
      (double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec));

  cudaFree(d_arraySmall);
  cudaFree(d_arrayLarge);
  cudaFree(d_arrayIncreases);
  cudaFree(d_indices);

  free(arraySmall);
  free(arrayLarge);
  free(arrayIncreases);
  free(indices);
  return 0;
}
