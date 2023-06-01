/* ******************************************

Find deltaN and count numbers to be tested for a 2^k sieve
  that is to be used to find maximum number of steps to 1.
For n = A 2^k + B, A>0 must be true when using this sieve.

Compile and run via something like...
  clang -O3 collatzSieve2toK_FindPatterns_reduceTo1.c -fopenmp
  time ./a.out |tee -a log26.txt &
To remove any checkpoints once the code is fully completed, run...
  rm temp*

To configure OpenMP, you can change the argument of num_threads().
Just search this code for "num_threads" (appears twice).
I wouldn't do more threads than physical CPU cores
to prevent bottlenecks due to sharing resources.

k < 81 must be true.

You'll need a 64-bit computer with a not-ancient version of gcc
  in order for __uint128_t to work.




Collatz conjecture is the following...
Repeated application of following f eventually reduces any n>1 integer to 1.
  f(n) = (3*n+1)/2  if n%2 = 1
  f(n) = n/2        if n%2 = 0



(c) 2021 Bradley Knockel

****************************************** */


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include <sys/time.h>
struct timeval tv1, tv2;


/*
  To use less RAM, break job into tasks
  Will use a bit more than 2^(TASK_SIZE + 4) bytes of RAM
  TASK_SIZE <= k
  If you set this larger than k, TASK_SIZE = k will be used
*/
int TASK_SIZE = 26;





const int k = 23;

/*
  If TASK_SIZE < k, deltaN must not be more than 2^TASK_SIZE
  This is because hold[] and holdC[] cannot be longer than the arrays.
*/
const __uint128_t deltaN = 1398101;





// how many checkpoints to save out ?
// will save (2^log2saves - 1) times
// log2saves must be less than or equal to k - TASK_SIZE

const int log2saves = 0;


// which checkpoint to load? 0 if none.
// If loading, don't change any parameters of this code except this one!!
// If loading, will load the files named temp#____, where loadCheckpoint is the #.

const uint64_t loadCheckpoint = 0;






/*
  2^K is the number of numbers processed by each CPU thread
    for the SECOND use of OpenMP
  K <= TASK_SIZE
  K should ideally be a good amount less than TASK_SIZE
*/
int32_t K = 6;





// Prints __uint128_t numbers since printf("%llu\n", x) doesn't work
//   since "long long" is only 64-bit in gcc.
// This function works for any non-negative integer less than 128 bits.
void print128(__uint128_t n) {
  char a[40] = { '\0' };
  char *p = a + 39;
  if (n==0) { *--p = (char)('0'); }
  else { for (; n != 0; n /= 10) *--p = (char)('0' + n % 10); }
  printf("%s", p);
}







uint32_t pow3(size_t n)   // returns 3^n
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

#define LUT_SIZE32 21

uint32_t lut[LUT_SIZE32];



#define min(a,b) (((a)<(b))?(a):(b))

/*
  for updating arrayLarge[] and arrayIncreases[]
  I used code from...
    https://github.com/xbarin02/collatz/blob/master/src/gpuworker/kernel32-precalc.cl
*/
void kSteps(__uint128_t arrayLarge[], uint8_t arrayIncreases[], uint64_t task_id) {


	#pragma omp parallel for schedule(guided) num_threads(6)
	for (size_t index = 0; index < ((size_t)1 << TASK_SIZE); ++index) {

		__uint128_t L0 = index + (task_id << TASK_SIZE);
		__uint128_t L = L0;

		uint32_t R = k;  /* counter */

		size_t Salpha = 0; /* sum of alpha */

		if (L == 0) goto next;

		do {
			L++;
			do {
				size_t alpha;
				if ((uint64_t)L == 0) alpha = 64;  // __builtin_ctzll(0) is undefined
				else alpha = __builtin_ctzll(L);
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
				size_t beta;
				if ((uint64_t)L == 0) beta = 64;  // __builtin_ctzll(0) is undefined
				else beta = __builtin_ctzll(L);
				beta = min(beta, (size_t)R);
				R -= beta;
				L >>= beta;
				if (R == 0) goto next;
			} while (!(L & 1));
		} while (1);

next:

		arrayLarge[index] = L;
		arrayIncreases[index] = (uint8_t)Salpha;

	}


}







int main(void) {

  if (TASK_SIZE > k) TASK_SIZE = k;
  if (K > TASK_SIZE) K = TASK_SIZE;



  int j;
  __uint128_t count, n0start, maxM;


  // for doing stuff for finding unique patterns
  int32_t nn, mm, oo;



  const __uint128_t k2  = (__uint128_t)1 << k;   // 2^k
  const uint64_t bits = (uint64_t)1 << K;      // 2^K



  printf("  k = %i\n", k);
  printf("  deltaN = ");
  print128(deltaN);
  printf("\n");
  fflush(stdout);





  // start timing
  gettimeofday(&tv1, NULL);





	/* setup */

	size_t arrayLargeCount = (size_t)1 << TASK_SIZE;
	size_t arrayIncreasesCount = (size_t)1 << TASK_SIZE;

	const size_t patternsPerArray = arrayIncreasesCount / bits;

	__uint128_t *arrayLarge = malloc(sizeof(__uint128_t) * arrayLargeCount);
	uint8_t *arrayIncreases = malloc(sizeof(uint8_t) * arrayIncreasesCount);

	if ( arrayLarge == NULL || arrayIncreases == NULL ) {
		return -1;
	}

	uint64_t taskIDmax = ((uint64_t)1 << (k - TASK_SIZE));  // the number of tasks run
	uint64_t tasksPerSave = taskIDmax / ((uint64_t)1 << log2saves);
	uint64_t taskGroups = taskIDmax / tasksPerSave;

	for (size_t i = 0; i < LUT_SIZE32; ++i) lut[i] = pow3(i);









  //// Look for unique patterns, find max deltaN, and
  ////   count numbers that need testing.

  fflush(stdout);

  n0start = loadCheckpoint * tasksPerSave * arrayIncreasesCount;
  count = 0;
  maxM = 0;  // see if deltaN is ever really reached
  double timePrior = 0;   // from before loaded checkpoint

  // temporarily hold the last deltaN 128-bit integers of old task (also hold the last deltaN 8-bit increases)
  __uint128_t *hold = malloc(sizeof(__uint128_t) * deltaN);
  uint8_t *holdC = malloc(sizeof(uint8_t) * deltaN);

  if (loadCheckpoint) {

    FILE *fp;
    char path[4096];

    sprintf(path, "temp%" PRIu64 "count", loadCheckpoint);
    fp = fopen(path, "rb");
    fread(&count, sizeof(__uint128_t), 1, fp);
    fclose(fp);

    sprintf(path, "temp%" PRIu64 "maxM", loadCheckpoint);
    fp = fopen(path, "rb");
    fread(&maxM, sizeof(__uint128_t), 1, fp);
    fclose(fp);

    sprintf(path, "temp%" PRIu64 "hold", loadCheckpoint);
    fp = fopen(path, "rb");
    fread(hold, sizeof(__uint128_t), deltaN, fp);
    fclose(fp);

    sprintf(path, "temp%" PRIu64 "holdC", loadCheckpoint);
    fp = fopen(path, "rb");
    fread(holdC, sizeof(uint8_t), deltaN, fp);
    fclose(fp);

    sprintf(path, "temp%" PRIu64 "timeElapsed", loadCheckpoint);
    fp = fopen(path, "rb");
    fread(&timePrior, sizeof(double), 1, fp);
    fclose(fp);

    printf("  Checkpoint %" PRIu64 " loaded\n", loadCheckpoint);

  }

  // for OpenMP threads to write to
  // I probably should use uint64_t instead ??
  uint32_t *collectCount = malloc(sizeof(uint32_t) * patternsPerArray);
  uint32_t *collectMaxM = malloc(sizeof(uint32_t) * patternsPerArray);




  // run task_id in groups and save after each group
  for (uint64_t task_id_group = loadCheckpoint; task_id_group < taskGroups; task_id_group++)
  {

  // loop over task_id to repeatedly call kSteps() and analyze results
  for (uint64_t task_id = task_id_group * tasksPerSave; task_id < (task_id_group + 1) * tasksPerSave; task_id++)
  {







	/* run kSteps() to update arrayLarge[] and arrayIncreases[] */
	kSteps(arrayLarge, arrayIncreases, task_id);







    #pragma omp parallel for schedule(guided) num_threads(6)
    for (size_t iPattern = 0; iPattern < patternsPerArray; iPattern++) {

      uint32_t countTiny = 0;
      uint32_t maxMtiny = maxM;

      for (size_t index = iPattern*bits; index < (iPattern + 1)*bits; index++) {

        int temp = 1;

        // the number being tested
        __uint128_t n0 = n0start + index;


	if (n0 > 0) {

          // for seeing if n joins the previous number, nm
          __uint128_t n = arrayLarge[index];
          __uint128_t nm;

          // for seeing if the number of increases, c, equals the previous increases, cm
          int c = (int)arrayIncreases[index];
          int cm;

          __uint128_t lenList = ((deltaN+1) < (n0-1)) ? (deltaN+1) : (n0-1) ;   // get min(deltaN+1, n0-1)
          for(size_t m=1; m<lenList; m++) {

            if ( index >= m ) {
              nm = arrayLarge[index - m];
              cm = (int)arrayIncreases[index - m];
            } else {
              if (task_id == 0) break;
              size_t iAdjusted = deltaN + index - m;
              nm = hold[iAdjusted];
              cm = (int)holdC[iAdjusted];
            }

            if ( nm == n && cm == c ) {

                if(m > maxMtiny){
                  maxMtiny = m;
                }

                temp = 0;
                break;

            }



          }
        }

        if (temp) countTiny++;

      }

      collectCount[iPattern] = countTiny;
      collectMaxM[iPattern] = maxMtiny;
    }













    n0start += arrayIncreasesCount;


    for (size_t iPattern = 0; iPattern < patternsPerArray; iPattern++) {

      count += collectCount[iPattern];

      uint32_t maxMtiny = collectMaxM[iPattern];
      if (maxMtiny > maxM) maxM = maxMtiny;

    }


    // fill hold[] and holdC[] with arrayLarge[] and arrayIncreases[], respectively
    if ( task_id < taskIDmax - 1 ) {

      for (size_t i=0; i < deltaN; i++)
        hold[i] = arrayLarge[arrayLargeCount - deltaN + i];

      for (size_t i=0; i < deltaN; i++)
        holdC[i] = arrayIncreases[arrayIncreasesCount - deltaN + i];

    }









  }

    // save everything out!
    if ( task_id_group < taskGroups - 1 ) {

      FILE *fp;
      char path[4096];

      sprintf(path, "temp%" PRIu64 "count", task_id_group + 1);
      fp = fopen(path, "wb");
      fwrite(&count, sizeof(__uint128_t), 1, fp);
      fclose(fp);

      sprintf(path, "temp%" PRIu64 "maxM", task_id_group + 1);
      fp = fopen(path, "wb");
      fwrite(&maxM, sizeof(__uint128_t), 1, fp);
      fclose(fp);

      sprintf(path, "temp%" PRIu64 "hold", task_id_group + 1);
      fp = fopen(path, "wb");
      fwrite(hold, sizeof(__uint128_t), deltaN, fp);
      fclose(fp);

      sprintf(path, "temp%" PRIu64 "holdC", task_id_group + 1);
      fp = fopen(path, "wb");
      fwrite(holdC, sizeof(uint8_t), deltaN, fp);
      fclose(fp);

      gettimeofday(&tv2, NULL);
      double timeElapsed = (double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec);
      timeElapsed += timePrior;
      sprintf(path, "temp%" PRIu64 "timeElapsed", task_id_group + 1);
      fp = fopen(path, "wb");
      fwrite(&timeElapsed, sizeof(double), 1, fp);
      fclose(fp);

    }

  }











  gettimeofday(&tv2, NULL);
  printf("  Elapsed wall time is %e seconds\n",
      (double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec) + timePrior ); 



  print128(count);
  printf(" out of ");
  print128(k2);
  printf(" need testing, so %f\n", (double)count / (double)k2);
  if (maxM > 0) {
    printf(" max deltaN = ");
    print128(maxM);
    printf("\n");
  }
  printf("\n");



  free(hold);
  free(holdC);
  free(collectCount);
  free(collectMaxM);
  free(arrayLarge);
  free(arrayIncreases);
  return 0;
}
