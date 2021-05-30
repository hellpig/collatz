/* ******************************************

Find deltaN and count numbers to be tested for a 2^k sieve
  that is to me used to find maximum number of steps to 1.
For n = A 2^k + B, A>0 must be true when using this sieve.

Compile and run via something like...
  clang -O3 collatzSieve2toK_FindPatterns_GPU_reduceTo1.c -lOpenCL -fopenmp
  time ./a.out |tee -a log26.txt &
To remove any checkpoints once the code is fully completed, run...
  rm temp*

To configure OpenMP, you can change the argument of num_threads().
Just search this code for "num_threads".
Using more CPU threads is great when you have a fast GPU and large deltaN.
If you use too many CPU threads, the 0th thread will end up "busy waiting"
for the GPU to finish. But, with such large deltaN, this isn't really an issue!
I wouldn't do more threads than physical CPU cores
to prevent bottlenecks due to sharing resources.

k < 81 must be true.

You'll need a 64-bit computer with a not-ancient version of gcc
  in order for __uint128_t to work.




Collatz conjecture is the following...
Repeated application of following f eventually reduces any n>1 integer to 1.
  f(n) = (3*n+1)/2  if n%2 = 1
  f(n) = n/2        if n%2 = 0

This code will analyze a sieve of size 2^k to be used when reducing numbers to 1.
For n = A 2^k + B, A>0 must be true when using this sieve.



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
  The following are in log2...
  For the OpenCL kernel
    TASK_UNITS <= TASK_SIZE <= k
  If set incorrectly, they will be fixed by the code.
*/
int TASK_SIZE = 26;    // will use a bit more than 2^(TASK_SIZE + 4) bytes of RAM
int TASK_UNITS = 16;





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






// set kernel file
static const char *kernel = "kernel_reduceTo1.cl";





/*
  2^K is the number of numbers processed by each CPU thread
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

  if (TASK_SIZE > k) TASK_SIZE = k;
  if (TASK_UNITS > TASK_SIZE) TASK_UNITS = TASK_SIZE;
  if (K > TASK_SIZE) K = TASK_SIZE;



  const __uint128_t k2  = (__uint128_t)1 << k;   // 2^k
  const uint64_t bits = (uint64_t)1 << K;      // 2^K

  int j;
  __uint128_t count, n0start, maxM;





  printf("  k = %i\n", k);
  printf("  deltaN = ");
  print128(deltaN);
  printf("\n");
  fflush(stdout);





  // start timing
  gettimeofday(&tv1, NULL);



	/* setup OpenCL and start first kernel run */

	int g_ocl_ver1 = 0;
	int g_device_index = 0;

	cl_mem mem_obj_arrayLarge;
	cl_mem mem_obj_arrayIncreases;

	size_t arrayLargeCount = (size_t)1 << (TASK_SIZE + 1);   // two of these are needed per uint128
	size_t arrayIncreasesCount = (size_t)1 << TASK_SIZE;

	const size_t patternsPerArray = arrayIncreasesCount / bits;

	uint64_t *arrayLarge = malloc(sizeof(cl_ulong) * arrayLargeCount);
	uint8_t *arrayIncreases = malloc(sizeof(cl_uchar) * arrayIncreasesCount);

	if ( arrayLarge == NULL || arrayIncreases == NULL ) {
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

		ret = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)& mem_obj_arrayLarge);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clSetKernelArg(kernel, 1, sizeof(cl_mem), (void *)&mem_obj_arrayIncreases);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clSetKernelArg(kernel, 2, sizeof(cl_ulong), (void *)&task_id_run);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, &global_work_size, NULL, 0, NULL, NULL);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clEnqueueNDRangeKernel() failed with %" PRIi32 "\n", ret);
			return -1;
		}


	}













  //// Find max deltaN, and count numbers that need testing.

  fflush(stdout);

  n0start = loadCheckpoint * tasksPerSave * arrayIncreasesCount;
  count = 0;
  maxM = 0;  // see if deltaN is ever really reached
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

    sprintf(path, "temp%" PRIu64 "hold", loadCheckpoint);
    fp = fopen(path, "rb");
    fread(hold, sizeof(uint64_t), deltaN*2, fp);
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
  // I should use uint64_t instead ??
  uint32_t *collectCount = malloc(sizeof(uint32_t) * patternsPerArray);
  uint32_t *collectMaxM = malloc(sizeof(uint32_t) * patternsPerArray);


  // run task_id in groups and save after each group
  for (uint64_t task_id_group = loadCheckpoint; task_id_group < taskGroups; task_id_group++) {

  // loop over task_id to repeatedly call the kernel and analyze results
  for (uint64_t task_id = task_id_group * tasksPerSave; task_id < (task_id_group + 1) * tasksPerSave; task_id++) {





	/* wait for previous kernel to finish */
	clFlush(command_queue);
	clFinish(command_queue);
	//printf(".");

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
		clSetKernelArg(kernel, 2, sizeof(cl_ulong), (void *)&task_id_run);

		ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL, &global_work_size, NULL, 0, NULL, NULL);


		if (ret != CL_SUCCESS) {
			printf("[ERROR] clEnqueueNDRangeKernel() failed with %" PRIi32 "\n", ret);
			return -1;
		}
	} else {
		printf("\n");
	}














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
          size_t iStart = index*2;
          __uint128_t n = ( (__uint128_t)arrayLarge[iStart] << 64 ) | (__uint128_t)arrayLarge[iStart + 1] ;
          __uint128_t nm;

          // for seeing if the number of increases, c, equals the previous increases, cm
          int c = (int)arrayIncreases[index];
          int cm;

          __uint128_t lenList = ((deltaN+1) < (n0-1)) ? (deltaN+1) : (n0-1) ;   // get min(deltaN+1, n0-1)
          for(size_t m=1; m<lenList; m++) {

            if ( iStart >= 2*m ) {
              nm = ( (__uint128_t)arrayLarge[iStart - 2*m] << 64 ) | (__uint128_t)arrayLarge[iStart - 2*m + 1];
              cm = (int)arrayIncreases[index - m];
            } else {
              if (task_id == 0) break;
              size_t iAdjusted = 2*deltaN + iStart - 2*m;
              nm = ( (__uint128_t)hold[iAdjusted] << 64 ) | (__uint128_t)hold[iAdjusted + 1];
              size_t iAdjustedC = deltaN + index - m;
              cm = (int)holdC[iAdjustedC];
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



        if (temp) {
          countTiny++;
        }


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

      sprintf(path, "temp%" PRIu64 "hold", task_id_group + 1);
      fp = fopen(path, "wb");
      fwrite(hold, sizeof(uint64_t), deltaN*2, fp);
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

	ret = clReleaseKernel(kernel);
	ret = clReleaseProgram(program);
	ret = clReleaseCommandQueue(command_queue);
	ret = clReleaseContext(context);

	free(program_string);
	free(device_id);

  free(hold);
  free(holdC);
  free(collectCount);
  free(collectMaxM);
  free(arrayLarge);
  free(arrayIncreases);
  return 0;
}
