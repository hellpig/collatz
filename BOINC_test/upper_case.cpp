// A wrapper for sticking my code into the BOINC server's example_app
// Modified from apps/upper_case.cpp to run Collatz code!
// No Graphics
// No command line options

#ifdef _WIN32
#include "boinc_win.h"
#else
#include "config.h"
#include <cstdio>
#include <cctype>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <unistd.h>
#endif

#include "str_util.h"
#include "util.h"
#include "filesys.h"
#include "boinc_api.h"
#include "mfile.h"
#include "graphics2.h"

using std::string;

#define CHECKPOINT_FILE "collatz_state"
#define INPUT_FILENAME "in"
#define OUTPUT_FILENAME "out"

MFILE out;






/*put your global code here*/


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <cinttypes>



/*
  The following variables are in log2
  TASK_SIZE0 - k should ideally be at least 10
  TASK_SIZE should at least be 10 to give each process something to run!
     (minimum TASK_SIZE is 8)
  Due to the limits of strtoull(), k - TASK_SIZE < 64
  9 * 2^(TASK_SIZE0 + TASK_SIZE - k) numbers will be run by each process
*/
const int k = 51;
const int TASK_SIZE0 = 67;       // 9 * 2^TASK_SIZE0 numbers will be run total
const int TASK_SIZE = 20;        // TASK_SIZE <= k



/*
  For a smaller 2^k2 sieve to do k2 steps at a time after the initial k steps
  3 < k2 < 37
  Will use more than 2^(k2 + 3) bytes of RAM
  For my CPU, 11 is the best because it fits in cache
*/
const int k2 = 11;





// For the 2^k1 sieve that speeds up the calculation of the 2^k sieve...
//   TASK_SIZE <= k1 <= k
// The only con of a larger k1 is that the sieve file is harder to create and store,
//   but, once you have the sieve file, use it!
// Especially good values are 32, 35, 37, 40, ...

const int k1 = 37;

const char file[10] = "sieve37";






// 2^8 sieve
const uint8_t sieveSmall[16] = {27, 31, 47, 71, 91, 103, 111, 127, 155, 159, 167, 191, 231, 239, 251, 255};





// Code will test aStart <= a < aStart + aSteps
// Set this to 2^0 = 1 to have get an idea of how long processing the sieve takes.
// Ideally, I like to set this to at least 2^10.
const __uint128_t aSteps = (__uint128_t)9 << (TASK_SIZE0 - k);



// Prints __uint128_t numbers since printf("%llu\n", x) doesn't work
//   since "long long" is only 64-bit in gcc.
// This function works for any non-negative integer less than 128 bits.
void print128(__uint128_t n) {
  char a[40] = { '\0' };
  char *p = a + 39;
  if (n==0) { *--p = (char)('0'); }
  else { for (; n != 0; n /= 10) *--p = (char)('0' + n % 10); }
  out.printf("%s\n", p);
  fprintf(stderr, "%s\n", p);
  fflush(stderr);
  out.flush();
}


int do_checkpoint(uint64_t a, uint64_t b, uint64_t c) {
    // print a,b,c to CHECKPOINT_FILE

    int retval;
    string resolved_name;

    FILE* f = fopen("temp", "w");
    if (!f) return 1;
    fprintf(f, "%" PRIu64 " %" PRIu64 " %" PRIu64, a, b, c);
    fclose(f);

    boinc_resolve_filename_s(CHECKPOINT_FILE, resolved_name);
    retval = boinc_rename("temp", resolved_name.c_str());
    if (retval) return retval;

    return 0;
}










double cpu_time = 20;

int main(int argc, char **argv) {
    int retval;
    double fsize;
    char input_path[512], output_path[512], buf[256];
    FILE* infile;

    retval = boinc_init();
    if (retval) {
        fprintf(stderr, "%s boinc_init returned %d\n",
            boinc_msg_prefix(buf, sizeof(buf)), retval
        );
        exit(retval);
    }

    // open the input file (resolve logical name first)
    //
    boinc_resolve_filename(INPUT_FILENAME, input_path, sizeof(input_path));
    infile = boinc_fopen(input_path, "r");
    if (!infile) {
        fprintf(stderr,
            "%s Couldn't find input file, resolved name %s.\n",
            boinc_msg_prefix(buf, sizeof(buf)), input_path
        );
        exit(-1);
    }
    char idInput[25];
    fscanf(infile, "%s", idInput);
    rewind(infile);

    // get size of input file (used to compute fraction done)
    //
    file_size(input_path, fsize);

    boinc_resolve_filename(OUTPUT_FILENAME, output_path, sizeof(output_path));

    boinc_fraction_done(0);











  /*put your main() code here*/

  uint64_t task_id0 = 0;
  uint64_t task_id  = (uint64_t)strtoull(idInput, NULL, 10);

  // checkpoint variables
  uint64_t checkp = 0;
  uint64_t countB = 0;    // to count the numbers that need testing in segment of 2^k sieve
  uint64_t checksum_alpha = 0;

  uint64_t maxTaskID = ((uint64_t)1 << (k - TASK_SIZE));
  if ( task_id >= maxTaskID ) {
    out.printf("Aborting. task_id must be less than ");
    fprintf(stderr, "Aborting. task_id must be less than ");
    print128( maxTaskID );
    exit(-1);
  }



  // See if there's a valid checkpoint file.
  //
  char chkpt_path[512];
  FILE* state;
  boinc_resolve_filename(CHECKPOINT_FILE, chkpt_path, sizeof(chkpt_path));
  state = boinc_fopen(chkpt_path, "r");
  if (state) {
      retval = out.open(output_path, "ab");
  } else {
      retval = out.open(output_path, "wb");
  }
  if (retval) {
      fprintf(stderr, "%s APP: output open failed:\n",
          boinc_msg_prefix(buf, sizeof(buf))
      );
      fprintf(stderr, "%s resolved name %s, retval %d\n",
          boinc_msg_prefix(buf, sizeof(buf)), output_path, retval
      );
      perror("open");
      exit(1);
  }
  if (state) {
      fscanf(state, "%" PRIu64 " %" PRIu64 " %" PRIu64, &checkp, &countB, &checksum_alpha);
      fclose(state);

      fprintf(stderr, "  restoring from checkpoint: %" PRIu64 " %" PRIu64 " %" PRIu64 "\n", checkp, countB, checksum_alpha);
      fflush(stderr);
  } else {

      out.printf("task_id0 = ");
      fprintf(stderr, "task_id0 = ");
      print128(task_id0);
      out.printf("task_id = ");
      fprintf(stderr, "task_id = ");
      print128(task_id);
      out.printf("task_id must be less than ");
      fprintf(stderr, "task_id must be less than ");
      print128(maxTaskID);
      out.printf("TASK_SIZE = ");
      fprintf(stderr, "TASK_SIZE = ");
      print128(TASK_SIZE);
      out.printf("TASK_SIZE0 = ");
      fprintf(stderr, "TASK_SIZE0 = ");
      print128(TASK_SIZE0);
      out.printf("  k = %i\n", k);
      fprintf(stderr, "  k = %i\n", k);
      out.printf("  k1 = %i\n", k1);
      fprintf(stderr, "  k1 = %i\n", k1);
      out.printf("  k2 = %i\n", k2);
      fprintf(stderr, "  k2 = %i\n", k2);
      out.flush();
      fflush(stderr);

  }


  const __uint128_t kk = (__uint128_t)1 << k;       // 2^k
  const __uint128_t UINTmax = -1;                   // trick to get all bits to be 1


  if ( task_id0 > ( (UINTmax / 9) >> (TASK_SIZE0 - k) ) - 1 ) {
    out.printf("Error: Overflow!\n");
    fprintf(stderr, "Error: Overflow!\n");
    exit(-1);
  }
  const __uint128_t aStart = (__uint128_t)task_id0*9 << (TASK_SIZE0 - k);
  const __uint128_t aEnd = aStart + aSteps;



  // 3 or 9 (the actual code must be changed to match, so keep it at 9)
  const int modNum = 9;

  // 3^c = 2^(log2(3)*c) = 2^(1.585*c),
  //    so c=80 is the max to fit in 128-bit numbers.
  // Note that c3[0] = 3^0
  //const int lenC3 = 81;  // for 128-bit numbers
  const int lenC3 = k+1;




  __uint128_t n, nStart, a;
  int aMod, nMod, bMod, j;


  //const int kkMod = kk % modNum;
  //const int kkMod = (k & 1) ? 2 : 1 ;   // trick for modNum == 3
  uint64_t r = 0;    // trick for modNum == 9
  r += (uint64_t)(kk)        & 0xfffffffffffffff;
  r += (uint64_t)(kk >>  60) & 0xfffffffffffffff;
  r += (uint64_t)(kk >> 120);
  const int kkMod = r%9;



  // calculate lookup table for c3, which is 3^c
  __uint128_t* c3 = (__uint128_t*)malloc(lenC3*sizeof(__uint128_t));
  c3[0] = 1;
  for (j=1; j<lenC3; j++) { c3[j] = c3[j-1]*3; }


  // defining max values is very important to detect overflow
  // calculate lookup table for maxNs
  __uint128_t* maxNs = (__uint128_t*)malloc(lenC3*sizeof(__uint128_t));
  for (j=0; j<lenC3; j++) { maxNs[j] = UINTmax / c3[j]; }


  // Test a for overflow
  // Note that after k steps for B = 2^k - 1, B will become 3^k - 1
  //   and A*2^k will become A*3^k
  const __uint128_t maxA = (UINTmax - c3[k] + 1) / c3[k];     // max a
  if (aEnd - 1 > maxA) {
    out.printf("Error: a*2^k + (2^k - 1) will overflow after k steps!\n");
    fprintf(stderr, "Error: a*2^k + (2^k - 1) will overflow after k steps!\n");
    exit(-1);
  }





  __uint128_t bStart = ( (__uint128_t)1 << TASK_SIZE )*task_id;





  /* open the 2^k1 sieve file */

  FILE* fp;
  size_t file_size;

  // make the following smaller if you want more BOINC checkpoints to occur
  const uint64_t bufferBytes = (uint64_t)1 << 10;    // 2^1 bytes = 1 kiB

  // I will load only a single chunk of 2^k1 sieve into RAM at a time
  char input_path_sieve[512];
  boinc_resolve_filename(file, input_path_sieve, sizeof(input_path_sieve));
  fp = boinc_fopen(input_path_sieve, "rb");
  uint16_t* data = (uint16_t*)malloc(bufferBytes);

  // Check file size
  // Bytes in sieve file are 2^(k1 - 7)
  fseek(fp, 0, SEEK_END);
  file_size = ftell(fp);
  if ( file_size != ((size_t)1 << (k1 - 7)) ) {
    out.printf("  error: wrong sieve file!\n");
    fprintf(stderr, "  error: wrong sieve file!\n");
    exit(-1);
  }

  // for handling the buffer...
  uint64_t bufferStepMax = bufferBytes >> 1;   // since each pattern is 2 bytes
  uint64_t bufferStep = bufferStepMax;
  uint16_t bytes;    // the current 2 bytes

  /*
    Seek to necessary part of the file
    Note that ((((uint64_t)1 << k1) - 1) & bStart) equals bStart % ((uint64_t)1 << k1)
  */
  fseek(fp, ((((uint64_t)1 << k1) - 1) & bStart) >> 7, SEEK_SET);
  // take care of checkpoint
  fseek(fp, 2 * checkp, SEEK_CUR);






  ////////////////////////////////////////////////////////////////
  //////// create arrayk2[] for the 2^k2 lookup table
  ////////////////////////////////////////////////////////////////

#define min(a,b) (((a)<(b))?(a):(b))

  uint64_t *arrayk2 = (uint64_t*)malloc(sizeof(uint64_t) * ((size_t)1 << k2));

  for (size_t index = 0; index < ((size_t)1 << k2); ++index) {

    uint64_t L = index;   // index is the initial L
    size_t Salpha = 0;   // sum of alpha
    int R;   // counter
    if (L == 0) goto next;

    R = k2;
    size_t alpha, beta;

    do {
        L++;
        do {
            if ((uint64_t)L == 0) alpha = 64;  // __builtin_ctzll(0) is undefined
            else alpha = __builtin_ctzll(L);

            alpha = min(alpha, (size_t)R);
            R -= alpha;
            L >>= alpha;
            L *= c3[alpha];
            Salpha += alpha;
            if (R == 0) {
              L--;
              goto next;
            }
        } while (!(L & 1));
        L--;
        do {
            if ((uint64_t)L == 0) beta = 64;  // __builtin_ctzll(0) is undefined
            else beta = __builtin_ctzll(L);

            beta = min(beta, (size_t)R);
            R -= beta;
            L >>= beta;
            if (R == 0) goto next;
        } while (!(L & 1));
    } while (1);

next:

    /* stores both L and Salpha */
    arrayk2[index] = L + ((uint64_t)Salpha << 58);

  }





  ////////////////////////////////////////////////////////////////
  //////// test integers that aren't excluded by certain rules
  ////////////////////////////////////////////////////////////////

  uint64_t patternEnd = ((uint64_t)1 << (TASK_SIZE - 8));
  double patternEndDouble = (double)patternEnd;

  for (uint64_t pattern = checkp; pattern < patternEnd; pattern++) {

    // get bytes
    if (bufferStep >= bufferStepMax) {    // do we need to refresh buffer?

        // first, do some BOINC stuff
        boinc_fraction_done((double)pattern/patternEndDouble);
        if (boinc_time_to_checkpoint()) {
            retval = do_checkpoint(pattern, countB, checksum_alpha);
            if (retval) {
                fprintf(stderr, "%s APP: upper_case checkpoint failed %d\n",
                    boinc_msg_prefix(buf, sizeof(buf)), retval
                );
                exit(retval);
            }
            boinc_checkpoint_completed();
        }

        // read from 2^k1 sieve
        fread(data, sizeof(uint16_t), bufferBytes / sizeof(uint16_t), fp);
        bufferStep = 0;
    }
    bytes = data[bufferStep];
    bufferStep++;

    for (int bit = 0; bit < 16; bit ++) {      // loop over 16 bits in pattern

      // allow the 2^k1 sieve to help!
      if ( !( bytes & (1 << bit) ) ) continue;
      //countB++;



      __uint128_t b = bStart + pattern * 256 + sieveSmall[bit];

      int go = 1;          // acts as a boolean
      __uint128_t bb = b;  // will become fk(b)
      int c = 0;           // number of increases experienced when calculated fk(b)

      // check to see if 2^k*N + b is reduced in no more than k steps
      for (j=0; j<k; j++) {   // step
        if (bb & 1) {         // bitwise test for odd
          bb = 3*(bb/2) + 2;  // note that bb is odd
          c++;
        } else {
          bb >>= 1;
          if (bb < b) {      // if 2^k*N + b is reduced to a*N + bb
            go = 0;
            break;
          }
        }
      }
      if ( go == 0 ) continue;




      // Try another method to rule out the b...
      //   Starting at b-1, compare the final b and c after k steps
      // This will take more time, and will only rule out a few more b values

      __uint128_t bm = b - 1;
      int cm = 0;

      // take k steps to get bm and cm
      for(j=0; j<k; j++) {
        if (bm & 1) {
          bm = 3*(bm/2) + 2;
          cm++;
        } else {
          bm >>= 1;
        }
      }

      // check bm and cm against bb and c
      if ( bm == bb && cm == c ) {
        continue;
        //print128(b);
        break;
      }





      countB++;

      aMod = 0;

      // a trick for modNum == 9 to get b%9 faster
      uint64_t r = 0;
      r += (uint64_t)(b)        & 0xfffffffffffffff;
      r += (uint64_t)(b >>  60) & 0xfffffffffffffff;
      r += (uint64_t)(b >> 120);
      bMod = r%9;

      for (a=aStart; a<aEnd; a++) {    // loop over a before next b (interlacing for speed)



          // do a 3^2 sieve

          nMod = (aMod * kkMod + bMod) % modNum;

          // iterate before the following possible continue
          aMod++;
          if (aMod == modNum) aMod = 0;

          //if (nMod == 2) continue;    // modNum = 3
          if (nMod == 2 || nMod == 4 || nMod == 5 || nMod == 8) {   // modNum = 9
            continue;
          }


          checksum_alpha += c;

          nStart = (a<<k) + b;

          n = a*c3[c] + bb;


          /* do k2 steps at a time */
          do {
              size_t index = n & ( ((uint64_t)1<<k2) - 1 );  // get lowest k2 bits of n
              uint64_t newB = arrayk2[index];
              size_t newC = newB >> 58;    // just 6 bits gives c
              newB &= 0x3ffffffffffffff;   // rest of bits gives b

              /* find the new n */
              //n = (n >> k2)*c3[newC] + newB;
              n >>= k2;
              if (n > maxNs[newC]) {
                  out.printf("Overflow! nStart = ");
                  fprintf(stderr, "Overflow! nStart = ");
                  print128(nStart);
                  break;
              }
              n *= c3[newC];
              if (n > UINTmax - newB) {
                  out.printf("Overflow! nStart = ");
                  fprintf(stderr, "Overflow! nStart = ");
                  print128(nStart);
                  break;
              }
              n += newB;
              checksum_alpha += k2;

              if (n < nStart) break;
          } while (1);

      }

    }
  }

  out.printf("  Numbers in sieve segment that needed testing = ");
  fprintf(stderr, "  Numbers in sieve segment that needed testing = ");
  print128(countB);
  out.printf("  Checksum = ");
  fprintf(stderr, "  Checksum = ");
  print128(checksum_alpha);


















    boinc_fraction_done(1);
    boinc_finish(0);
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR Args, int WinMode) {
    LPSTR command_line;
    char* argv[100];
    int argc;

    command_line = GetCommandLine();
    argc = parse_command_line( command_line, argv );
    return main(argc, argv);
}
#endif
