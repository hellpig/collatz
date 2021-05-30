/* ******************************************

Count numbers in a 2^k sieve meant for reducing to 1.
Also find deltaN.

Compile and run via something like...
  clang -O3 collatzSieve2toK_FindPatterns_reduceTo1_Aequals0.c -fopenmp
  time ./a.out >> log.txt

To configure OpenMP, you can change the argument of num_threads().
Just search this code for "num_threads".
I wouldn't do more threads than physical CPU cores
to prevent bottlenecks due to sharing resources.

Since reducing to 1 in k steps is rare, there might be a faster way of doing this.

(c) 2021 Bradley Knockel

****************************************** */


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include <sys/time.h>
struct timeval tv1, tv2;



/* This code uses 64-bit integers, so k < 41 */
const int k = 17;


const uint64_t deltaN = 21845;



/*
  2^K is the number of numbers processed by each CPU thread
  K <= k
  K should ideally be a good amount less than k
  To use less RAM, K shouldn't be TOO much less than k
*/
int32_t K = 12;




int main(void) {


  const uint64_t k2  = (uint64_t)1 << k;   // 2^k
  const uint64_t collectLength = (uint64_t)1 << (k-K);
  const uint64_t b0perThread = (uint64_t)1 << K;


  printf("  k = %i\n", k);
  printf("  deltaN = %" PRIu64 "\n", deltaN);
  fflush(stdout);




  // start timing
  gettimeofday(&tv1, NULL);



  // for OpenMP threads to write to
  uint64_t *collectCount = malloc(sizeof(uint64_t) * collectLength);
  uint64_t *collectMaxM = malloc(sizeof(uint64_t) * collectLength);



  //// find max deltaN and count numbers that need testing

  #pragma omp parallel for schedule(guided) num_threads(6)
  for (size_t index=0; index<collectLength; index++){

    uint64_t countTiny = 0;
    uint64_t maxMtiny = 0;

    for (uint64_t b0 = index*b0perThread; b0 < (index+1)*b0perThread; b0++) {

      int temp = 1;  // acts as a boolean

      if (b0 != 0) {

        // lists for b0
        uint64_t b0steps[k+1];
        int b0c[k+1];
        b0steps[0] = b0;
        b0c[0] = 0;
        for (size_t i=1; i<=k; i++){
          if (b0steps[i-1] & 1) {                 // bitwise test for odd
            b0steps[i] = 3*(b0steps[i-1]/2) + 2;  // note that b0steps[i-1] is odd
            b0c[i] = b0c[i-1]+1;
          } else {
            b0steps[i] = (b0steps[i-1] >> 1);
            b0c[i] = b0c[i-1];
          }
        }

        uint64_t lenList = ((deltaN+1) < (b0-1)) ? (deltaN+1) : (b0-1) ;   // get min(deltaN+1, b0-1)
        for(uint64_t m=1; m<lenList; m++) {      // loop over m
          uint64_t n = b0-m;
          int c = 0;

          for(size_t j=1; j<=k; j++) {         // steps

            // take step
            if (n & 1) {          // bitwise test for odd
              n = 3*(n/2) + 2;    // note that n is odd
              c++;
            } else {
              n >>= 1;
              if ( n == 1 ) {n = 0;}
            }

            // check against b0 lists
            if ( n == b0steps[j] && c == b0c[j] ) {

              if(m > maxMtiny){
                maxMtiny = m;
              }

              temp = 0;
              m = lenList;   // break out of the m loop
              break;
            }
          }

        }
      }

      if (temp) countTiny++;

    }

    collectCount[index] = countTiny;
    collectMaxM[index] = maxMtiny;

  }



  uint64_t count = 0;
  uint64_t maxM = 0;  // see if deltaN is ever really reached
  for (size_t index=0; index<collectLength; index++){
    count += collectCount[index];

    uint64_t maxMtiny = collectMaxM[index];
    if (maxMtiny > maxM) maxM = maxMtiny;

  }





  gettimeofday(&tv2, NULL);
  printf("  Elapsed wall time is %e seconds\n",
      (double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec) ); 





  printf("%" PRIu64 " out of %" PRIu64 " need testing, so %f\n", count, k2, (double)count / (double)k2);
  if (maxM > 0)
    printf(" max deltaN = %" PRIu64 "\n", maxM);
  printf("\n");




  return 0;
}
