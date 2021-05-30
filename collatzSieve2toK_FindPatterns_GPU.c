/* ******************************************

Find deltaN, count numbers to be tested,
  and find unique patterns for 2^k sieve.
Patterns are compressed 4-to-1 by only looking at n%4==3.
Not a huge amount of RAM is used.

Compile and run via something like...
  clang -O3 collatzSieve2toK_FindPatterns_GPU.c -lOpenCL -fopenmp
  time ./a.out |tee -a log47.txt &
To remove any checkpoints once the code is fully completed, run...
  rm temp*

To configure OpenMP, you can change the argument of num_threads().
Just search this code for "num_threads".
Using more CPU threads is great when you have a fast GPU and large deltaN.
If you use too many CPU threads, the 0th thread will end up "busy waiting"
for the GPU to finish. I wouldn't do more threads than physical CPU cores
to prevent bottlenecks due to sharing resources.

Something like the following can eventually be useful
after putting all k < 47 into log.txt...
  sort log.txt | uniq > logSorted.txt
  sort log47.txt > log47Sorted.txt
  comm -23 logSorted.txt log47Sorted.txt
Ideally nothing will be output!
But, if there is output, just combine it with log47!
Will the following return anything?
  comm -23 log47Sorted.txt logSorted.txt

k < 81 must be true.

You'll need a 64-bit computer with a not-ancient version of gcc
  in order for __uint128_t to work.




Collatz conjecture is the following...
Repeated application of following f eventually reduces any n>1 integer to 1.
  f(n) = (3*n+1)/2  if n%2 = 1
  f(n) = n/2        if n%2 = 0

This code will analyze a sieve of size 2^k.
If n = 2^k*N + i always reduces after no more than k steps for all N,
  there is no need to ever test these numbers.
This code tests all 0 <= i < 2^k to see if this reduction occurs.

This file does an extra search.
For n0 belonging to the numbers less than 2^k that remain to be excluded,
  see if n0 joins the path of any numbers n0 - deltaN <= n < n0
  in the same j<=k steps having undergone the same c increases.
It can be shown that...
    deltaN <= 2^(k - minC)
The above is derived using minC > k / log2(3) being the lowest c that does not
  give already-ruled-out n0. You want to think about the lowest c since
  it gives you the greatest deltaN. After k steps with c increases...
    n0 -> 3^c / 2^k * n0 + something
  where something does not depend on n0 (only the order of increases and decreases).
  Then define...
    deltaSomething = (something2 from k-c decreases then c increases)
                    - (something1 from c increases then k-c decreases)
  where it is easiest to calculate these somethings using n0=0.
  Anyway...
    deltaN <= deltaSomething / (3^c / 2^k) <= (3/2)^c / (3^c / 2^k)
    deltaN <= 2^(k-c) <= 2^(k-minc)
  Here is some Python code for calculating a something2...
----
n = 0.0
for i in range(c):
  n = (3*n+1) / 2
something2 = n     # something2 < (3/2)^c
----
  To get the something1, divide something2 by 2^(k-c).
Using the same derivation idea above, you may find a tighter limit...
    deltaN <= something2 * (2^k - 2^minC) / 3^minC
I bet a clever person could restrict deltaN more since deltaN must be an integer.
  In fact, I succeeded! Here are some of the results that take no more
  than a couple hours to find...
    k = 41: deltaN <= 1215
    k = 42: deltaN <= 1647
    k = 43: deltaN <= 1647
    k = 44: deltaN <= 4207
    k = 45: deltaN <= 5231
    k = 46: deltaN <= 5231
  I still need to run these k values to get the actual experimental deltaN,
    but the above numbers GREATLY speed this up!



Parts of this file are modified from...
  https://github.com/xbarin02/collatz/blob/master/src/gpuworker/gpuworker.c
The parts modified from the above project use tabs to indent
  whereas my original code has no tabs.
This applies to the kernel file too, which came from modifying...
  https://github.com/xbarin02/collatz/blob/master/src/gpuworker/kernel32-precalc.cl



(c) 2021 Bradley Knockel

****************************************** */


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <CL/cl.h>

#include <sys/time.h>
struct timeval tv1, tv2;


/*
  For the OpenCL kernel
    TASK_UNITS <= TASK_SIZE <= k
  The following are in log2...
*/
const int TASK_SIZE = 26;    // will use a bit more than 2^(TASK_SIZE + 4) bytes of RAM
const int TASK_UNITS = 16;



//  k = 36 would take a couple hours to run on CPU
//    but it takes about 5.5 minutes on an Nvidia Quadro P4000
//  k = 40 would take over a day to run on a CPU core,
//    but it takes about 85 minutes on an Nvidia Quadro P4000.

const int k = 46;


/*
  The following is only used if code sets deltaN to be larger than this
  If you are only interested in deltaN=1 sieves, set this to 1
*/
const uint64_t deltaN_max = 1000000;






// how many checkpoints to save out ?
// will save (2^log2saves - 1) times
// log2saves must be less than or equal to k - TASK_SIZE

const int log2saves = 6;


// which checkpoint to load? 0 if none.
// If loading, don't change any parameters of this code except this one!!
// If loading, will load the files named temp#____, where loadCheckpoint is the #.

const uint64_t loadCheckpoint = 0;






// set kernel file
static const char *kernel = "kernel.cl";




// Find patterns of size 2^K, where K <= TASK_SIZE.
// 2 <= K <= 9 is hardcoded into the analysis.
const int K = 8;






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



// prints all m bits, where m <= 128
void printBinary128(__uint128_t n, int m) {
  __uint128_t mask = (__uint128_t)1 << (m-1);
  for (int i=0; i<m; i++) {
    if (n & mask) { printf("1"); }
    else { printf("0"); }
    mask >>= 1;
  }
  printf("\n");
  fflush(stdout);
}




__uint128_t* patterns;

// Binary search
// But, if not found, return location of where it should go.
// I found it useful to, by hand, run through {0,1,2,3,4,5,6,7,8,9}
//   to look for 2.5 and 3.5. Also try -0.5 and 9.5.
// For an empty patterns[] array, l=0 and r=-1, so it works!
int32_t binarySearch(int32_t l, int32_t r, __uint128_t bytes) {

  if (r >= l) {
    int mid = l + (r - l) / 2;
    if (patterns[mid] == bytes) return mid;
    if (patterns[mid] > bytes) return binarySearch(l, mid - 1, bytes);
    return binarySearch(mid + 1, r, bytes);
  }

  return l; 
}




char *load_source(size_t *size)
{
	FILE *fp;
	char *str;

	printf("KERNEL %s\n", kernel);

	fp = fopen(kernel, "r");

	if (fp == NULL) {
		return NULL;
	}

	str = malloc(1<<20);

	if (str == NULL) {
		return NULL;
	}

	*size = fread(str, 1, 1<<20, fp);

	fclose(fp);

	return str;
}




int main(void) {



  int j;
  __uint128_t count, deltaN, n0start, maxM;


  // for doing stuff for finding unique patterns
  int32_t nn, mm, oo;



  const __uint128_t k2  = (__uint128_t)1 << k;   // 2^k
  const int32_t  bits  = (__uint128_t)1 << (K-2);   // bits needed for each pattern



  // lookup deltaN (else calculate it)
  if (k<=5) deltaN = 0;
  else if (k<=18) deltaN = 1;
  else if (k<=24) deltaN = 6;
  else if (k<=27) deltaN = 12;
  else if (k<=29) deltaN = 25;
  else if (k<=32) deltaN = 34;
  else if (k<=33) deltaN = 37;
  else if (k<=35) deltaN = 46;
  else if (k<=37) deltaN = 88;
  else if (k<=40) deltaN = 120;
  else if (k<=43) deltaN = 208;
  else if (k<=45) deltaN = 222;
  else if (k<=46) deltaN = 5231;  // needs experimental reduction
  else if (k<=47) deltaN = 6015;  // needs experimental reduction
  else {
    int minC = 0.6309297535714574371 * k + 1.0;  // add 1 to get ceiling
    double minC3 = 1.0;     // 3^minC
    for (j=0; j<minC; j++) minC3 *= 3.0;
    double deltaNtemp = 0.0;
    for (j=0; j<minC; j++) deltaNtemp = (3.0 * deltaNtemp + 1.0) / 2.0;
    deltaNtemp = deltaNtemp * (((__uint128_t)1<<k) - ((__uint128_t)1<<minC)) / minC3;
    deltaN = deltaNtemp + 1.0;    // add 1 to get ceiling
  }

  if (deltaN > deltaN_max) deltaN = deltaN_max;

  printf("  k = %i\n", k);
  printf("  deltaN = ");
  print128(deltaN);
  printf("\n");
  fflush(stdout);


  __uint128_t deltaNequals1count = 0;
  __uint128_t totalDeltaNcount = 0;



  // Array of unique patterns.
  // I can prove that length = 2^11 is for sure enough for K=7.
  // I can prove that length = 2^16 is for sure enough for K=8.
  // I can prove that length = 2^31 is for sure enough for K=9.
  int32_t length;
  if (K == 9) length = 1048576;   // 16 MiB of RAM for 128-bit int (enough ???)
  else length = 32768;            // 0.5 MiB of RAM for 128-bit int
  patterns = (__uint128_t*)malloc(length*sizeof(__uint128_t));
  for (int32_t i=0; i<length; i++) { patterns[i] = 0; }



  // start timing
  gettimeofday(&tv1, NULL);



	/* setup OpenCL and start first kernel run */

	int g_ocl_ver1 = 0;
	int g_device_index = 0;

	cl_mem mem_obj_arraySmall;
	cl_mem mem_obj_arrayLarge;
	cl_mem mem_obj_arrayIncreases;

	size_t arraySmallCount = (size_t)1 << (TASK_SIZE - 2);   // only every 4th is needed
	size_t arrayLargeCount = (size_t)1 << (TASK_SIZE + 1);   // two of these are needed per uint128
	size_t arrayIncreasesCount = (size_t)1 << TASK_SIZE;

	const size_t patternsPerArraySmall = arraySmallCount / bits;

	uint8_t *arraySmall = malloc(sizeof(cl_uchar) * arraySmallCount);
	uint64_t *arrayLarge = malloc(sizeof(cl_ulong) * arrayLargeCount);
	uint8_t *arrayIncreases = malloc(sizeof(cl_uchar) * arrayIncreasesCount);

	if ( arraySmall == NULL || arrayLarge == NULL || arrayIncreases == NULL ) {
		return -1;
	}

	uint64_t taskIDmax = ((uint64_t)1 << (k - TASK_SIZE));  // the number of kernel runs
	uint64_t tasksPerSave = taskIDmax / ((uint64_t)1 << log2saves);
	uint64_t taskGroups = taskIDmax / tasksPerSave;
	uint64_t task_id_run = loadCheckpoint * tasksPerSave;   // the kernel run will be 1 ahead of the CPU analysis

	cl_int ret;
	cl_platform_id platform_id[64];
	cl_uint num_platforms;

	cl_device_id *device_id = NULL;
	cl_uint num_devices;

	cl_context context;

	cl_command_queue command_queue;

	char *program_string;
	size_t program_length;

	cl_program program;

	cl_kernel kernel;

	size_t global_work_size;

	int platform_index = 0;
	int device_index = g_device_index;

	char options[4096];

	ret = clGetPlatformIDs(0, NULL, &num_platforms);

	if (ret != CL_SUCCESS) {
		printf("[ERROR] clGetPlarformIDs failed with = %" PRIi32 "\n", ret);
		return -1;
	}

	printf("[DEBUG] num_platforms = %u\n", (unsigned)num_platforms);

	if (num_platforms == 0) {
		printf("[ERROR] no platform\n");
		return -1;
	}

	ret = clGetPlatformIDs(num_platforms, &platform_id[0], NULL);

	if (ret != CL_SUCCESS) {
		printf("[ERROR] clGetPlarformIDs failed\n");
		return -1;
	}

next_platform:
	printf("[DEBUG] platform = %i\n", platform_index);

	num_devices = 0;

	ret = clGetDeviceIDs(platform_id[platform_index], CL_DEVICE_TYPE_GPU, 0, NULL, &num_devices);

	if (ret == CL_DEVICE_NOT_FOUND) {
		if ((cl_uint)platform_index + 1 < num_platforms) {
			platform_index++;
			goto next_platform;
		}
	}

	if (ret != CL_SUCCESS) {
		printf("[ERROR] clGetDeviceIDs failed with %" PRIi32 "\n", ret);
		return -1;
	}

	printf("[DEBUG] num_devices = %u\n", num_devices);

	device_id = malloc(sizeof(cl_device_id) * num_devices);

	if (device_id == NULL) {
		return -1;
	}

	ret = clGetDeviceIDs(platform_id[platform_index], CL_DEVICE_TYPE_GPU, num_devices, &device_id[0], NULL);

	if (ret != CL_SUCCESS) {
		return -1;
	}

	for (; (cl_uint)device_index < num_devices; ++device_index) {
		printf("[DEBUG] device_index = %i...\n", device_index);

		context = clCreateContext(NULL, 1, &device_id[device_index], NULL, NULL, &ret);

		if (ret == CL_INVALID_DEVICE) {
			continue;
		}

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateContext failed with %" PRIi32 "\n", ret);
			return -1;
		}

		printf("[DEBUG] context created @ device_index = %i\n", device_index);


		command_queue = clCreateCommandQueue(context, device_id[device_index], 0, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateCommandQueue failed\n");
			return -1;
		}

		mem_obj_arraySmall = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(cl_uchar) * arraySmallCount, NULL, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateBuffer failed\n");
			return -1;
		}

		mem_obj_arrayLarge = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(cl_ulong) * arrayLargeCount, NULL, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateBuffer failed\n");
			return -1;
		}

		mem_obj_arrayIncreases = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(cl_uchar) * arrayIncreasesCount, NULL, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateBuffer failed\n");
			return -1;
		}

		program_string = load_source(&program_length);

		if (program_string == NULL) {
			printf("[ERROR] load_source failed\n");
			return -1;
		}

		program = clCreateProgramWithSource(context, 1, (const char **)&program_string, (const size_t *)&program_length, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateProgramWithSource failed\n");
			return -1;
		}

		sprintf(options, "%s -D SIEVE_LOGSIZE=%i -D TASK_SIZE=%i -D TASK_UNITS=%i",
			g_ocl_ver1 ? "" : "-cl-std=CL2.0",
			k,
			TASK_SIZE,
			TASK_UNITS
		);

		printf("[DEBUG] clBuildProgram options: %s\n", options);

		ret = clBuildProgram(program, 1, &device_id[device_index], options, NULL, NULL);

		if (ret == CL_BUILD_PROGRAM_FAILURE) {
			size_t log_size;
			char *log;

			clGetProgramBuildInfo(program, device_id[device_index], CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);

			log = malloc(log_size);

			clGetProgramBuildInfo(program, device_id[device_index], CL_PROGRAM_BUILD_LOG, log_size, log, NULL);

			printf("%s\n", log);
		}

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clBuildProgram failed with %" PRIi32 "\n", ret);
			return -1;
		}

		kernel = clCreateKernel(program, "worker", &ret);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		global_work_size = (size_t)1 << TASK_UNITS;

		printf("[DEBUG] global_work_size = %zu\n", global_work_size);

		ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&mem_obj_arraySmall);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)& mem_obj_arrayLarge);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void *)&mem_obj_arrayIncreases);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clSetKernelArg(kernel, 3, sizeof(cl_ulong), (void *)&task_id_run);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, &global_work_size, NULL, 0, NULL, NULL);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clEnqueueNDRangeKernel() failed with %" PRIi32 "\n", ret);
			return -1;
		}


	}













  //// Look for unique patterns, find max deltaN, and
  ////   count numbers that need testing.
  //// Only look at n%4 == 3, so 1/4 of the numbers.

  fflush(stdout);

  n0start = 3 + loadCheckpoint * tasksPerSave * arrayIncreasesCount;
  count = 0;
  maxM = 0;  // see if deltaN is ever really reached
  nn = 0;    // counter for patterns array
  double timePrior = 0;   // from before loaded checkpoint

  // temporarily hold the last deltaN 128-bit integers of old kernel run (also hold the last deltaN 8-bit increases)
  uint64_t *hold = malloc(sizeof(cl_ulong) * deltaN * 2);
  uint8_t *holdC = malloc(sizeof(cl_uchar) * deltaN);

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

    sprintf(path, "temp%" PRIu64 "nn", loadCheckpoint);
    fp = fopen(path, "rb");
    fread(&nn, sizeof(int32_t), 1, fp);
    fclose(fp);

    sprintf(path, "temp%" PRIu64 "hold", loadCheckpoint);
    fp = fopen(path, "rb");
    fread(hold, sizeof(uint64_t), deltaN*2, fp);
    fclose(fp);

    sprintf(path, "temp%" PRIu64 "holdC", loadCheckpoint);
    fp = fopen(path, "rb");
    fread(holdC, sizeof(uint8_t), deltaN, fp);
    fclose(fp);

    sprintf(path, "temp%" PRIu64 "patterns", loadCheckpoint);
    fp = fopen(path, "rb");
    fread(patterns, sizeof(__uint128_t), nn, fp);
    fclose(fp);

    sprintf(path, "temp%" PRIu64 "timeElapsed", loadCheckpoint);
    fp = fopen(path, "rb");
    fread(&timePrior, sizeof(double), 1, fp);
    fclose(fp);

    printf("  Checkpoint %" PRIu64 " loaded\n", loadCheckpoint);

  }

  // for OpenMP threads to write to
  // I probably should use uint64_t and uint8_t instead ??
  __uint128_t *collect = malloc(sizeof(__uint128_t) * patternsPerArraySmall);
  uint32_t *collectCount = malloc(sizeof(uint32_t) * patternsPerArraySmall);
  uint32_t *collectMaxM = malloc(sizeof(uint32_t) * patternsPerArraySmall);

  uint32_t *collectDeltaNcount = malloc(sizeof(uint32_t) * patternsPerArraySmall);
  uint32_t *collectDeltaNequals1count = malloc(sizeof(uint32_t) * patternsPerArraySmall);


  // run task_id in groups and save after each group
  for (uint64_t task_id_group = loadCheckpoint; task_id_group < taskGroups; task_id_group++) {

  // loop over task_id to repeatedly call the kernel and analyze results
  for (uint64_t task_id = task_id_group * tasksPerSave; task_id < (task_id_group + 1) * tasksPerSave; task_id++) {






	/* wait for previous kernel to finish */
	clFlush(command_queue);
	clFinish(command_queue);
	//printf(".");

	ret = clEnqueueReadBuffer(command_queue, mem_obj_arraySmall, CL_TRUE, 0, sizeof(uint8_t) * arraySmallCount, arraySmall, 0, NULL, NULL);

	if (ret != CL_SUCCESS) {
		printf("[ERROR] clEnqueueReadBuffer failed with = %" PRIi32 "\n", ret);
		return -1;
	}

	ret = clEnqueueReadBuffer(command_queue, mem_obj_arrayLarge, CL_TRUE, 0, sizeof(uint64_t) * arrayLargeCount, arrayLarge, 0, NULL, NULL);

	if (ret != CL_SUCCESS) {
		printf("[ERROR] clEnqueueReadBuffer failed with = %" PRIi32 "\n", ret);
		return -1;
	}

	ret = clEnqueueReadBuffer(command_queue, mem_obj_arrayIncreases, CL_TRUE, 0, sizeof(uint8_t) * arrayIncreasesCount, arrayIncreases, 0, NULL, NULL);

	if (ret != CL_SUCCESS) {
		printf("[ERROR] clEnqueueReadBuffer failed with = %" PRIi32 "\n", ret);
		return -1;
	}

	/* run next kernel */
	if ( task_id < taskIDmax - 1 ) {

		task_id_run = task_id + 1;
		clSetKernelArg(kernel, 3, sizeof(cl_ulong), (void *)&task_id_run);

		ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, &global_work_size, NULL, 0, NULL, NULL);


		if (ret != CL_SUCCESS) {
			printf("[ERROR] clEnqueueNDRangeKernel() failed with %" PRIi32 "\n", ret);
			return -1;
		}
	} else {
		printf("\n");
	}














    #pragma omp parallel for schedule(guided) num_threads(6)
    for (size_t iPattern = 0; iPattern < patternsPerArraySmall; iPattern++) {

      __uint128_t aa = 0;    // stores the pattern
      int32_t bb = 0;        // counter for indexing bits of pattern
      uint32_t countTiny = 0;
      uint32_t maxMtiny = maxM;

      uint32_t countDeltaNTiny = 0;
      uint32_t countDeltaNequals1Tiny = 0;

      for (size_t index = iPattern*bits; index < (iPattern + 1)*bits; index++) {

        // check to see if if 2^k*N + n0 is reduced in no more than k steps
        int temp = arraySmall[index];

        // if temp=1, use another method to try to get temp=0
        if (temp) {

          // for seeing if n joins the previous number, nm
          size_t iStart = 8*index + 6;         // (4*index + 3) * 2
          __uint128_t n = ( (__uint128_t)arrayLarge[iStart] << 64 ) | (__uint128_t)arrayLarge[iStart + 1] ;
          __uint128_t nm;

          // for seeing if the number of increases, c, equals the previous increases, cm
          size_t iStartC = 4*index + 3;
          int c = (int)arrayIncreases[iStartC];
          int cm;

          // the number being tested
          __uint128_t n0 = n0start + index * 4;

          __uint128_t lenList = ((deltaN+1) < (n0-1)) ? (deltaN+1) : (n0-1) ;   // get min(deltaN+1, n0-1)
          for(size_t m=1; m<lenList; m++) {

            if ( iStart >= 2*m ) {
              nm = ( (__uint128_t)arrayLarge[iStart - 2*m] << 64 ) | (__uint128_t)arrayLarge[iStart - 2*m + 1];
              cm = (int)arrayIncreases[iStartC - m];
            } else {
              if (task_id == 0) break;
              size_t iAdjusted = 2*deltaN + iStart - 2*m;
              nm = ( (__uint128_t)hold[iAdjusted] << 64 ) | (__uint128_t)hold[iAdjusted + 1];
              size_t iAdjustedC = deltaN + iStartC - m;
              cm = (int)holdC[iAdjustedC];
            }

            if ( nm == n ) {

              if ( cm == c ) {

                if(m > maxMtiny){
                  maxMtiny = m;
                }

                countDeltaNTiny++;
                if (m==1) countDeltaNequals1Tiny++;

                temp = 0;
                break;
              }

              // I'm very curious if the following will ever happen! If it never does, arrayIncreases[], Salpha, and holdC[] are not needed!
              printf(" n0 and m are "); print128(n0); printf(" and "); print128(m); printf("\n");
              printf(" cm and c are %i and %i\n", cm, c);
              printf(" nm and n are "); print128(nm); printf(" and "); print128(n); printf("\n");
              fflush(stdout);

            }


      
          }
        }

        if (temp) {
          aa += (__uint128_t)1 << bb;
          countTiny++;
        }

        bb++;

      }

      collect[iPattern] = aa;
      collectCount[iPattern] = countTiny;
      collectMaxM[iPattern] = maxMtiny;

      collectDeltaNcount[iPattern] = countDeltaNTiny;
      collectDeltaNequals1count[iPattern] = countDeltaNequals1Tiny;
    }













    n0start += arrayIncreasesCount;


    for (size_t iPattern = 0; iPattern < patternsPerArraySmall; iPattern++) {

      __uint128_t aa = collect[iPattern];
      count += collectCount[iPattern];
      uint32_t maxMtiny = collectMaxM[iPattern];

      totalDeltaNcount += collectDeltaNcount[iPattern];
      deltaNequals1count += collectDeltaNequals1count[iPattern];

      // add aa to patterns if not already in patterns (binary search; sorted)
      mm = binarySearch(0, nn-1, aa);
      if (patterns[mm] != aa) {       // add into a sorted patterns[]
        for (oo = nn; oo > mm; oo--)  // shift things over by 1 spot
          patterns[oo] = patterns[oo - 1];
        patterns[mm] = aa;
        nn++;
      }

      if (maxMtiny > maxM) maxM = maxMtiny;

    }


    // fill hold[] and holdC[] with arrayLarge[] and arrayIncreases[], respectively
    if ( task_id < taskIDmax - 1 ) {

      for (size_t i=0; i < 2*deltaN; i++)
        hold[i] = arrayLarge[arrayLargeCount - 2*deltaN + i];

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

      sprintf(path, "temp%" PRIu64 "nn", task_id_group + 1);
      fp = fopen(path, "wb");
      fwrite(&nn, sizeof(int32_t), 1, fp);
      fclose(fp);

      sprintf(path, "temp%" PRIu64 "hold", task_id_group + 1);
      fp = fopen(path, "wb");
      fwrite(hold, sizeof(uint64_t), deltaN*2, fp);
      fclose(fp);

      sprintf(path, "temp%" PRIu64 "holdC", task_id_group + 1);
      fp = fopen(path, "wb");
      fwrite(holdC, sizeof(uint8_t), deltaN, fp);
      fclose(fp);

      // This is the largest one (though hold[] can be large too)
      // to be sure this fully saves, don't save it last
      sprintf(path, "temp%" PRIu64 "patterns", task_id_group + 1);
      fp = fopen(path, "wb");
      fwrite(patterns, sizeof(__uint128_t), nn, fp);
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


  const __uint128_t K2    = (__uint128_t)1 << K;       // bit-shift trick to get 2^K
  const __uint128_t len2  = (__uint128_t)1 << (k-K);   // number of chunks


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



  printf("There are ");
  print128(nn);
  printf(" unique ");
  print128(K2);
  printf("-bit patterns in ");
  print128(len2);
  printf(" ");
  print128(K2);
  printf("-bit chunks of ");
  print128(k2);
  printf(" values...\n");

  for (mm=0; mm<nn; mm++) { printBinary128(patterns[mm], bits); }
  printf("\n");



  printf(" need testing without any deltaN: ");
  print128(count + totalDeltaNcount);
  printf("\n need testing with only deltaN = 1: ");
  print128(count + totalDeltaNcount - deltaNequals1count);

  printf("\n total deltaN count = ");
  print128(totalDeltaNcount);
  printf("\n deltaN=1 count: ");
  print128(deltaNequals1count);
  printf("\n");



	ret = clReleaseKernel(kernel);
	ret = clReleaseProgram(program);
	ret = clReleaseCommandQueue(command_queue);
	ret = clReleaseContext(context);

	free(program_string);
	free(device_id);

  free(patterns);
  free(hold);
  free(holdC);
  free(collect);
  free(collectCount);
  free(collectMaxM);
  free(collectDeltaNcount);
  free(collectDeltaNequals1count);
  free(arraySmall);
  free(arrayLarge);
  free(arrayIncreases);
  return 0;
}
