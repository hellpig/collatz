/* ******************************************

Sieves of size 2^k are used, where k can be very large!
Not a huge amount of RAM is used.
Storage drive not used.

You will need two OpenCL kernel files to run this code.

On Linux, compile via something like...
  clang -O3 collatzSieveless_npp_GPU.c -lOpenCL

You'll need a 64-bit computer with a not-ancient version of gcc
  in order for __uint128_t to work.

Note that I use the "long long" function strtoull when reading in the arguments.

k < 81 must be true.





Parts of this file are modified from...
  https://github.com/xbarin02/collatz/blob/master/src/gpuworker/gpuworker.c
The parts modified from the above project use tabs to indent
  whereas my original code has no tabs.
This applies to the kernel files too, which came from modifying...
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
Feel free to get rid of the 9 when h_minimum and h_supremum are defined in kernel2.cl
You'll also want to get rid of the division by 9 when this host program
  checks if TASK_ID_KERNEL2 will cause certain overflow.
If TASK_ID_KERNEL2 > 0, you'll also want to fix the line in kernel2...
  aMod = 0



I have assumed that your GPU has...
  CL_DEVICE_ADDRESS_BITS == 64
If not, this will limit various run parameters.

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
  k < 81
*/
const int k = 51;



/*
  The following are in log2...
  For kernel1, which starts making the sieve...
    TASK_UNITS <= TASK_SIZE <= k
  Will use more than 2^(TASK_SIZE + 4) bytes of RAM
  Due to the limits of strtoull(), k - TASK_SIZE < 64
  2^TASK_SIZE should be much larger than deltaN
  2^TASK_UNITS should be much larger than the number of CUDA Cores you have
  (fraction of sieve not excluded) * 2^TASK_SIZE should be much larger than the number of CUDA Cores you have
  My CPU-only code prefers a smaller TASK_SIZE so that the task finishes in a reasonable time
*/
const int TASK_SIZE = 24;
const int TASK_UNITS = 16;



/*
  The following is in log2...
  TASK_SIZE_KERNEL2 - k should ideally be at least 16 for k=51 and deltaN_max = 222
  TASK_SIZE_KERNEL2 - k should be larger for larger k or for larger deltaN_max
  For kernel2, which tests numbers given a sieve...
  9 * 2^(TASK_SIZE_KERNEL2 + TASK_SIZE - k) numbers will be run by each process
  9 * 2^TASK_SIZE_KERNEL2 numbers will be run total by all processes
*/
const int TASK_SIZE_KERNEL2 = 67;






__uint128_t deltaN_max = 222;     // don't let deltaN be larger than this





// set kernel files
static const char *kernel1 = "kernel1.cl";
static const char *kernel2 = "kernel2_npp.cl";







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






char *load_source(size_t *size, int num)
{
	FILE *fp;
	char *str;

	if ( num == 1 )
		fp = fopen(kernel1, "r");
	else
		fp = fopen(kernel2, "r");

	if (fp == NULL)
		return NULL;

	str = malloc(1<<20);

	if (str == NULL)
		return NULL;

	*size = fread(str, 1, 1<<20, fp);

	fclose(fp);

	return str;
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
    printf("Aborting. TASK_ID must be less than ");
    print128( maxTaskID );
    printf("\n");
    return 0;
  }

  /* Check for 100%-certain overflow.
     This check prevents having to check when doing any pre-calculation when interlacing
     Note that after k steps, for A * 2^k + B...
       B = 2^k - 1 will become 3^k - 1
       and A*2^k will become A*3^k
  */
  __uint128_t temp = 1;  // will equal 3^k
  __uint128_t UINT128_MAX = (~(__uint128_t)0);
  for (int i=0; i < k; i++) { temp *= 3; }
  if ( (__uint128_t)TASK_ID_KERNEL2 > ((UINT128_MAX - temp) >> (TASK_SIZE_KERNEL2 - k)) / 9 / temp - 1 ) {
    printf("  Well, aren't you ambitious!\n");
    return 0;
  }

  printf("TASK_ID_KERNEL2 = ");
  print128(TASK_ID_KERNEL2);
  printf("\nTASK_ID = ");
  print128(TASK_ID);
  printf("\nTASK_ID must be less than ");
  print128(maxTaskID);
  printf("\nTASK_SIZE = ");
  print128(TASK_SIZE);
  printf("\nTASK_SIZE_KERNEL2 = ");
  print128(TASK_SIZE_KERNEL2);
  printf("\n");
  fflush(stdout);




  int j;
  __uint128_t deltaN, n0start;




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

  if ( deltaN > deltaN_max ) deltaN = deltaN_max;

  printf("  k = %i\n", k);
  printf("  deltaN = ");
  print128(deltaN);
  printf("\n");
  fflush(stdout);




  // start timing
  gettimeofday(&tv1, NULL);



	/* setup OpenCL */

	int g_ocl_ver1 = 0;
	int g_device_index = 0;

	cl_mem mem_obj_arraySmall;
	cl_mem mem_obj_arrayLarge;
	cl_mem mem_obj_arrayIncreases;
	cl_mem mem_obj_indices;

	size_t arraySmallCount = (size_t)1 << (TASK_SIZE - 2);   // only every 4th is needed
	size_t arrayLargeCount = (size_t)1 << (TASK_SIZE + 1);   // two of these are needed per uint128
	size_t arrayIncreasesCount = (size_t)1 << TASK_SIZE;
	size_t indicesCount = (size_t)1 << (TASK_SIZE - 2);    // not all of this will be used

	uint8_t *arraySmall = malloc(sizeof(cl_uchar) * arraySmallCount);
	uint64_t *arrayLarge = malloc(sizeof(cl_ulong) * arrayLargeCount);
	uint8_t *arrayIncreases = malloc(sizeof(cl_uchar) * arrayIncreasesCount);
	uint64_t *indices = malloc(sizeof(cl_ulong) *indicesCount);

	if ( arraySmall == NULL || arrayLarge == NULL || arrayIncreases == NULL || indices == NULL ) {
		return -1;
	}

	cl_int ret;
	cl_platform_id platform_id[64];
	cl_uint num_platforms;

	cl_device_id *device_id = NULL;
	cl_uint num_devices;

	cl_context context;

	cl_command_queue command_queue;

	char *program_string1;
	char *program_string2;
	size_t program_length1;
	size_t program_length2;

	cl_program program1;
	cl_program program2;

	cl_kernel kernel1;
	cl_kernel kernel2;

	size_t global_work_size1;
	size_t global_work_size2;

	int platform_index = 0;
	int device_index = g_device_index;

	char options1[4096];
	char options2[4096];

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

		mem_obj_arrayLarge = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(cl_ulong) * arrayLargeCount, NULL, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateBuffer failed\n");
			return -1;
		}

		mem_obj_arrayIncreases = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(cl_uchar) * arrayIncreasesCount, NULL, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateBuffer failed\n");
			return -1;
		}

		mem_obj_indices = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(cl_ulong) * indicesCount, NULL, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateBuffer failed\n");
			return -1;
		}





		program_string1 = load_source(&program_length1, 1);

		if (program_string1 == NULL) {
			printf("[ERROR] load_source failed\n");
			return -1;
		}

		program1 = clCreateProgramWithSource(context, 1, (const char **)&program_string1, (const size_t *)&program_length1, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateProgramWithSource failed\n");
			return -1;
		}

		sprintf(options1, "%s -D SIEVE_LOGSIZE=%i -D TASK_ID=%" PRIu64 " -D TASK_SIZE=%i -D TASK_UNITS=%i",
			g_ocl_ver1 ? "" : "-cl-std=CL2.0",
			k,
			TASK_ID,
			TASK_SIZE,
			TASK_UNITS
		);

		printf("[DEBUG] clBuildProgram options1: %s\n", options1);

		ret = clBuildProgram(program1, 1, &device_id[device_index], options1, NULL, NULL);

		if (ret == CL_BUILD_PROGRAM_FAILURE) {
			size_t log_size;
			char *log;

			clGetProgramBuildInfo(program1, device_id[device_index], CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);

			log = malloc(log_size);

			clGetProgramBuildInfo(program1, device_id[device_index], CL_PROGRAM_BUILD_LOG, log_size, log, NULL);

			printf("%s\n", log);
		}

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clBuildProgram failed with %" PRIi32 "\n", ret);
			return -1;
		}

		kernel1 = clCreateKernel(program1, "worker", &ret);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		global_work_size1 = (size_t)1 << TASK_UNITS;

		printf("[DEBUG] global_work_size1 = %zu\n", global_work_size1);

		ret = clSetKernelArg(kernel1, 0, sizeof(cl_mem), (void *)&mem_obj_arraySmall);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clSetKernelArg(kernel1, 1, sizeof(cl_mem), (void *)& mem_obj_arrayLarge);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clSetKernelArg(kernel1, 2, sizeof(cl_mem), (void *)&mem_obj_arrayIncreases);

		if (ret != CL_SUCCESS) {
			return -1;
		}






		program_string2 = load_source(&program_length2, 2);

		if (program_string2 == NULL) {
			printf("[ERROR] load_source failed\n");
			return -1;
		}

		program2 = clCreateProgramWithSource(context, 1, (const char **)&program_string2, (const size_t *)&program_length2, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateProgramWithSource failed\n");
			return -1;
		}

		sprintf(options2, "%s -D SIEVE_LOGSIZE=%i -D TASK_SIZE=%i -D TASK_ID=%" PRIu64 " -D TASK_SIZE_KERNEL2=%i -D TASK_ID_KERNEL2=%" PRIu64,
			g_ocl_ver1 ? "" : "-cl-std=CL2.0",
			k,
			TASK_SIZE,
			TASK_ID,
			TASK_SIZE_KERNEL2,
			TASK_ID_KERNEL2
		);

		printf("[DEBUG] clBuildProgram options2: %s\n", options2);

		ret = clBuildProgram(program2, 1, &device_id[device_index], options2, NULL, NULL);

		if (ret == CL_BUILD_PROGRAM_FAILURE) {
			size_t log_size;
			char *log;

			clGetProgramBuildInfo(program2, device_id[device_index], CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);

			log = malloc(log_size);

			clGetProgramBuildInfo(program2, device_id[device_index], CL_PROGRAM_BUILD_LOG, log_size, log, NULL);

			printf("%s\n", log);
		}

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clBuildProgram failed with %" PRIi32 "\n", ret);
			return -1;
		}

		kernel2 = clCreateKernel(program2, "worker", &ret);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clSetKernelArg(kernel2, 0, sizeof(cl_mem), (void *)&mem_obj_indices);

		if (ret != CL_SUCCESS) {
			return -1;
		}


		ret = clSetKernelArg(kernel2, 1, sizeof(cl_mem), (void *)& mem_obj_arrayLarge);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clSetKernelArg(kernel2, 2, sizeof(cl_mem), (void *)&mem_obj_arrayIncreases);

		if (ret != CL_SUCCESS) {
			return -1;
		}


	}






	gettimeofday(&tv2, NULL);
	printf("  kernel1 is starting: %e seconds elapsed\n",
		(double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec));
	fflush(stdout);

        /* start kernel1 */
	ret = clEnqueueNDRangeKernel(command_queue, kernel1, 1, NULL, &global_work_size1, NULL, 0, NULL, NULL);

	if (ret != CL_SUCCESS) {
		printf("[ERROR] clEnqueueNDRangeKernel() failed with %" PRIi32 "\n", ret);
		return -1;
	}








  n0start = (__uint128_t)TASK_ID * arrayIncreasesCount + 3;  // for analyzing the results of kernel1

  global_work_size2 = 0;  // for seeing how much work kernel2 has to do

  // find the last deltaN 128-bit integers of previous TASK_ID (also hold on to the last deltaN 8-bit increases)
  __uint128_t *hold = malloc(sizeof(__uint128_t) * deltaN);
  uint8_t *holdC = malloc(sizeof(uint8_t) * deltaN);

  if (TASK_ID > 0) {
    for (size_t i = 0; i < deltaN; i++) {

      __uint128_t n = n0start - 3 - deltaN + i;

      int c = 0;

      for (int j=0; j<k; j++) {   // steps
        if (n & 1) {              // bitwise test for odd
          n = 3*(n/2) + 2;        // note that n is odd so n/2 is rounded down
          c++;
        } else {
          n >>= 1;
        }
      }

      hold[i] = n;
      holdC[i] = (uint8_t)c;
    }


    gettimeofday(&tv2, NULL);
    printf("  CPU has finished making hold[] and holdC[]: %e seconds elapsed\n",
      (double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec));
    fflush(stdout);

  }






	/* wait for kernel1 to finish */
	clFlush(command_queue);
	clFinish(command_queue);

	gettimeofday(&tv2, NULL);
	printf("  kernel1 is finished: %e seconds elapsed\n",
		(double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec));
	fflush(stdout);


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




  //// Only look at n%4 == 3, so 1/4 of the numbers.
  for (size_t index = 0; index < arraySmallCount; index++) {

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
              if (TASK_ID == 0) break;
              size_t iAdjustedC = deltaN + iStartC - m;
              nm = hold[iAdjustedC];
              cm = (int)holdC[iAdjustedC];
            }

            if ( nm == n ) {

              if ( cm == c ) {
                temp = 0;
                break;
              }

              // I'm curious if the following will ever happen!
              printf(" n0 and m are "); print128(n0); printf(" and "); print128(m); printf("\n");
              printf(" cm and c are %i and %i\n", cm, c);
              printf(" nm and n are "); print128(nm); printf(" and "); print128(n); printf("\n");
              fflush(stdout);

            }



          }
        }

        if (temp) {
          indices[global_work_size2] = 4*index + 3;
          //printf(" %llu\n", (unsigned long long)(4*index + 3));
          global_work_size2++;
        }

  }




  printf("Numbers in sieve segment that need testing = %zu\n", global_work_size2);

  /* pad indices[] to make global_work_size2 a multiple of 32 */
  for (int j = 0; j < (global_work_size2 % 32); j++) {
    indices[global_work_size2] = 0;
    global_work_size2++;
  }











	/* run kernel 2 */

	ret = clEnqueueWriteBuffer(command_queue, mem_obj_indices, CL_TRUE, 0, sizeof(uint64_t) * global_work_size2, indices, 0, NULL, NULL);

	if (ret != CL_SUCCESS) {
		printf("[ERROR] clEnqueueWriteBuffer() failed with %" PRIi32 "\n", ret);
		return -1;
	}

	gettimeofday(&tv2, NULL);
	printf("  kernel2 is starting: %e seconds elapsed\n",
		(double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec));
	fflush(stdout);

	ret = clEnqueueNDRangeKernel(command_queue, kernel2, 1, NULL, &global_work_size2, NULL, 0, NULL, NULL);

	if (ret != CL_SUCCESS) {
		printf("[ERROR] clEnqueueNDRangeKernel() failed with %" PRIi32 "\n", ret);
		return -1;
	}


	/* wait for kernel2 to finish */
	clFlush(command_queue);
	clFinish(command_queue);






	ret = clReleaseKernel(kernel1);
	ret = clReleaseProgram(program1);
	ret = clReleaseKernel(kernel2);
	ret = clReleaseProgram(program2);
	ret = clReleaseCommandQueue(command_queue);
	ret = clReleaseContext(context);

	free(program_string1);
	free(program_string2);
	free(device_id);




  gettimeofday(&tv2, NULL);
  printf("  Elapsed wall time is %e seconds\n\n",
      (double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec)); 

  free(arraySmall);
  free(arrayLarge);
  free(arrayIncreases);
  free(indices);
  free(hold);
  free(holdC);
  return 0;
}
