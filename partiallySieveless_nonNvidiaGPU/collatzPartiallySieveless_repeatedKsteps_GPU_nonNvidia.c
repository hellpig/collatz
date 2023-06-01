/* ******************************************


On Linux, compile via something like...
  clang -O3 collatzPartiallySieveless_repeatedKsteps_GPU_nonNvidia.c -lOpenCL

On Windows with Cygwin installed...
  gcc -O3 collatzPartiallySieveless_repeatedKsteps_GPU_nonNvidia.c /cygdrive/c/Windows/System32/OpenCL.DLL

On macOS...
  clang -O3 collatzPartiallySieveless_repeatedKsteps_GPU_nonNvidia.c -framework opencl






Sieves of size 2^k are used, where k can be very large!

You will need three OpenCL kernel files to run this code.

You'll need a 64-bit computer with a not-ancient version of gcc
  in order for __uint128_t to work.

Note that I use the "long long" function strtoull when reading in the arguments.

Currently loads in a sieve file, which must must match the k1 value set in this code.

k < 81 must be true




Parts of this file are modified from...
  https://github.com/xbarin02/collatz/blob/master/src/gpuworker/gpuworker.c
The parts modified from the above project use tabs to indent
  whereas my original code has no tabs.
This applies to the kernel files too, which came from modifying...
  https://github.com/xbarin02/collatz/blob/master/src/gpuworker/kernel32-precalc.cl

I am using the idea of this paper...
  http://www.ijnc.org/index.php/ijnc/article/download/135/144



This code requires requires two arguments...
  ./a.out  TASK_ID_KERNEL2  TASK_ID

Starting at 0, increase TASK_ID by 1 each run until its max
  ( 2^(k - TASK_SIZE) - 1 )
Then, reset TASK_ID and increase TASK_ID_KERNEL2 by 1 (starting at 0).
Don't skip!

Here is a neat way to run this code...
  seq -f %1.0f 0 9 | xargs -L 1 -P 1 ./a.out 0 |tee -a log.txt &
To pick up where you left off, change the start and stop value of seq.
k and TASK_SIZE_KERNEL2 should not change between runs.

For each TASK_ID_KERNEL2, 9 * 2 ^ TASK_SIZE_KERNEL2 numbers will be tested,
  but only after each TASK_ID is run from 0 to ( 2^(k - TASK_SIZE) - 1 )
Why the 9? I thought it might help my GPU code, but it only does EXTREMELY SLIGHTLY.
Feel free to get rid of the 9s when h_minimum and h_supremum are defined in kernel2.
You'll also want to get rid of the division by 9 when this host program
  checks if TASK_ID_KERNEL2 will cause certain overflow.
If TASK_ID_KERNEL2 > 0, you'll also want to fix the line in kernel2...
  aMod = 0



I have assumed that your GPU has...
  CL_DEVICE_ADDRESS_BITS == 64
If not, this will limit various run parameters.

I recommend that you play around with your device to see if it has a GPU watchdog timer.
It has one if the kernel always stops after a certain amount of time (like 5 seconds).

I would expect that, if a bunch of OVERFLOW messages are printed by kernel2
  within a short period of time, some might be lost due to buffer sizes ??

274133054632352106267 will overflow 128 bits and, for not-crazy k2, should be
  noticed by this code!
This number is over 64-bits, so it cannot be made into an integer literal.
To test 128-bit overflow and using printf() in kernel2,
calculate the following using Python 3, then put it as your TASK_ID_KERNEL2...
  TASK_ID_KERNEL2 = 274133054632352106267 // (9 << TASK_SIZE_KERNEL2)
Then set the following as your TASK_ID...
  remainder = (274133054632352106267 % (9 << TASK_SIZE_KERNEL2))
  TASK_ID = (remainder % (1 << k)) // (1 << TASK_SIZE)
where I am using a double slash, //, for integer division.


(c) 2021 Bradley Knockel

****************************************** */


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include <unistd.h>   // for usleep()

#include <sys/time.h>
struct timeval tv1, tv2, tv3;



/*
  For a 2^k sieve
  k < 81
*/
const int k = 51;


/*
  For a 2^k2 lookup table to do k2 steps at a time after the initial k steps
  3 < k2 < 37, where k2 < 37 so that table fits in uint64_t
  Will use more than 2^(k2 + 3) bytes of RAM
  For my GPU, 18 is the best because it fits in GPU cache
*/
const int k2 = 18;



/*
  For kernel1 and kernel1_2, which make the lookup table...
    TASK_UNITS + 8 <= TASK_SIZE <= k
    TASK_UNITS <= k2
  Will use more than 2^TASK_SIZE bytes of RAM
  Due to the limits of strtoull(), k - TASK_SIZE < 64
  2^TASK_UNITS should be much larger than the number of CUDA Cores you have
  (fraction of 2^k sieve not excluded) * 2^TASK_SIZE should be much larger than the number of CUDA Cores you have
*/
const int TASK_SIZE = 24;
const int TASK_UNITS = 16;   // for OpenCL, global_work_size = 2^TASK_UNITS



/*
  TASK_SIZE_KERNEL2 - k should ideally be at least 10
  9 * 2^TASK_SIZE_KERNEL2 numbers will be run
  For kernel2, which tests numbers given a sieve...
  9 * 2^(TASK_SIZE_KERNEL2 + TASK_SIZE - k) numbers will be run by each process
  9 * 2^TASK_SIZE_KERNEL2 numbers will be run total by all processes
*/
const int TASK_SIZE_KERNEL2 = 67;






// For the 2^k1 sieve that speeds up the calculation of the 2^k sieve...
//   TASK_SIZE <= k1 <= k
// The only con of a larger k1 is that the sieve file is harder to create and store,
//   but, once you have the sieve file, use it!
// Especially good values are:   ..., 32, 35, 37, 40, ...

const int k1 = 27;

const char file[10] = "sieve27";







// set kernel files
static const char *kernel1   = "kern1_128byHand_nonNvidia.cl";        // for k1
static const char *kernel1_2 = "kern1_2_repeatedKsteps_nonNvidia.cl"; // for k2
static const char *kernel2   = "kern2_repeatedKsteps_128byHand_nonNvidia.cl";   // for actually using sieve and lookup table





// set to 1 if you don't want to use OpenCL 2.0 (else 0)
const int g_ocl_ver1 = 0;





/*
  If your GPU has a watchdog timer, make CHUNKS_KERNEL2 larger than 0,
  and have a small secondsKernel2.
    CHUNKS_KERNEL2 <= TASK_SIZE_KERNEL2 - k
  2^CHUNKS_KERNEL2 chunks will be run.
  Increase CHUNKS_KERNEL2 if secondsKernel2 time limit is being reached.
*/

const int CHUNKS_KERNEL2 = 7;

const double secondsKernel2 = 4.5;   // 1.0E32 is a good value if not watchdog timer




char *load_source(size_t *size, int num)
{
	FILE *fp;
	char *str;

	if ( num == 1 )
		fp = fopen(kernel1, "r");
	else if (num == 2)
		fp = fopen(kernel1_2, "r");
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
  __uint128_t UINT128_MAX = (~(__uint128_t)0);
  for (int i=0; i < k; i++) { temp *= 3; }
  if ( (__uint128_t)TASK_ID_KERNEL2 > ((UINT128_MAX - temp) >> (TASK_SIZE_KERNEL2 - k)) / 9 / temp - 1 ) {
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
  printf("  k2 = %i\n", k2);
  fflush(stdout);




  // start timing
  gettimeofday(&tv1, NULL);



	/* setup OpenCL */

	int g_device_index = 0;

	cl_mem mem_obj_arraySmall;
	cl_mem mem_obj_arrayLarge;
	cl_mem mem_obj_arrayLarge2;   // for k2 to store final value AND increases
	cl_mem mem_obj_arrayIncreases;
	cl_mem mem_obj_indices;

	size_t arraySmallCount = ((size_t)1 << (TASK_SIZE - 8)) + 1;   // each element is 2^8 numbers; add 1 to prevent kern1 from reading too far
	size_t arrayLargeCount = (size_t)1 << (TASK_SIZE - 3);     // two of these are needed per uint128
	size_t arrayLarge2Count = (size_t)1 << k2;                 // for k2
	size_t arrayIncreasesCount = (size_t)1 << (TASK_SIZE - 4); // one for each bit in portion of 2^k1 sieve
	size_t indicesCount = (size_t)1 << (TASK_SIZE - 4);        // not all of this will be used

	uint16_t *arraySmall = malloc(sizeof(cl_ushort) * arraySmallCount);
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
	char *program_string1_2;
	char *program_string2;
	size_t program_length1;
	size_t program_length1_2;
	size_t program_length2;

	cl_program program1;
	cl_program program1_2;
	cl_program program2;

	cl_kernel kernel1;
	cl_kernel kernel1_2;
	cl_kernel kernel2;

	size_t global_work_size1;
	size_t global_work_size1_2;
	size_t global_work_size2;

	int platform_index = 0;
	int device_index = g_device_index;

	char options1[4096];
	char options1_2[4096];
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




		mem_obj_arraySmall = clCreateBuffer(context, CL_MEM_READ_ONLY, sizeof(cl_ushort) * arraySmallCount, NULL, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateBuffer failed\n");
			return -1;
		}

		mem_obj_arrayLarge = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(cl_ulong) * arrayLargeCount, NULL, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateBuffer failed\n");
			return -1;
		}

		mem_obj_arrayLarge2 = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(cl_ulong) * arrayLarge2Count, NULL, &ret);

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








		program_string1_2 = load_source(&program_length1_2, 2);

		if (program_string1_2 == NULL) {
			printf("[ERROR] load_source failed\n");
			return -1;
		}

		program1_2 = clCreateProgramWithSource(context, 1, (const char **)&program_string1_2, (const size_t *)&program_length1_2, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateProgramWithSource failed\n");
			return -1;
		}

		sprintf(options1_2, "%s -D SIEVE_LOGSIZE2=%i -D TASK_UNITS=%i",
			g_ocl_ver1 ? "" : "-cl-std=CL2.0",
			k2,
			TASK_UNITS
		);

		printf("[DEBUG] clBuildProgram options1_2: %s\n", options1_2);

		ret = clBuildProgram(program1_2, 1, &device_id[device_index], options1_2, NULL, NULL);

		if (ret == CL_BUILD_PROGRAM_FAILURE) {
			size_t log_size;
			char *log;

			clGetProgramBuildInfo(program1_2, device_id[device_index], CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);

			log = malloc(log_size);

			clGetProgramBuildInfo(program1_2, device_id[device_index], CL_PROGRAM_BUILD_LOG, log_size, log, NULL);

			printf("%s\n", log);
		}

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clBuildProgram failed with %" PRIi32 "\n", ret);
			return -1;
		}

		kernel1_2 = clCreateKernel(program1_2, "worker", &ret);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		global_work_size1_2 = (size_t)1 << TASK_UNITS;

		printf("[DEBUG] global_work_size1_2 = %zu\n", global_work_size1_2);

		ret = clSetKernelArg(kernel1_2, 0, sizeof(cl_mem), (void *)& mem_obj_arrayLarge2);

		if (ret != CL_SUCCESS) {
			return -1;
		}






		program_string2 = load_source(&program_length2, 3);

		if (program_string2 == NULL) {
			printf("[ERROR] load_source failed\n");
			return -1;
		}

		program2 = clCreateProgramWithSource(context, 1, (const char **)&program_string2, (const size_t *)&program_length2, &ret);

		if (ret != CL_SUCCESS) {
			printf("[ERROR] clCreateProgramWithSource failed\n");
			return -1;
		}

		sprintf(options2, "%s -D SIEVE_LOGSIZE=%i -D SIEVE_LOGSIZE2=%i -D TASK_SIZE=%i -D TASK_ID=%" PRIu64 " -D TASK_SIZE_KERNEL2=%i -D TASK_ID_KERNEL2=%" PRIu64 " -D CHUNKS_KERNEL2=%i",
			g_ocl_ver1 ? "" : "-cl-std=CL2.0",
			k,
			k2,
			TASK_SIZE,
			TASK_ID,
			TASK_SIZE_KERNEL2,
			TASK_ID_KERNEL2,
			CHUNKS_KERNEL2
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

		ret = clSetKernelArg(kernel2, 2, sizeof(cl_mem), (void *)& mem_obj_arrayIncreases);

		if (ret != CL_SUCCESS) {
			return -1;
		}

		ret = clSetKernelArg(kernel2, 3, sizeof(cl_mem), (void *)& mem_obj_arrayLarge2);

		if (ret != CL_SUCCESS) {
			return -1;
		}



  /* test for 64-bit arithmetic errors */

  const uint64_t inputVal = 45210249143;

  const char *kernelString =
    "__kernel void kernelFunc(const ulong a, __global ulong *c) { c[0] = a+a+a;}";

  // Compile the kernelString
  cl_program programTest = clCreateProgramWithSource(context, 1, &kernelString, NULL, &ret);
  if (ret != CL_SUCCESS) {printf("[ERROR] clCreateProgramWithSource failed\n"); return -1;}
  char *optionsTest = g_ocl_ver1 ? "" : "-cl-std=CL2.0";
  ret = clBuildProgram(programTest, 1, &device_id[device_index], optionsTest, NULL, NULL);

  // Check for compilation errors
  if (ret == CL_BUILD_PROGRAM_FAILURE) {
    size_t logSize;
    clGetProgramBuildInfo(programTest, device_id[device_index], CL_PROGRAM_BUILD_LOG, 0, NULL, &logSize);
    char* messages = (char*)malloc((1+logSize)*sizeof(char));
    clGetProgramBuildInfo(programTest, device_id[device_index], CL_PROGRAM_BUILD_LOG, logSize, messages, NULL);
    messages[logSize] = '\0';
    if (logSize > 10) { printf(">>> Compiler message: %s\n", messages); }
    return -1;
  }
  if (ret != CL_SUCCESS) {
    printf("[ERROR] clBuildProgram failed with %" PRIi32 "\n", ret);
    return -1;
  }

  // Run the kernel
  cl_kernel kernelTest = clCreateKernel(programTest, "kernelFunc", NULL);
  clSetKernelArg(kernelTest, 0, sizeof(uint64_t), (void*)&inputVal);
  cl_mem mem_obj_output = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(cl_ulong), NULL, NULL);
  clSetKernelArg(kernelTest, 1, sizeof(cl_mem), (void *)&mem_obj_output);
  const size_t one = 1;
  clEnqueueNDRangeKernel(command_queue, kernelTest, 1, NULL, &one, &one, 0, NULL, NULL);
  clFlush(command_queue);
  clFinish(command_queue);

  // get the output
  uint64_t *c = malloc(sizeof(cl_ulong));
  ret = clEnqueueReadBuffer(command_queue, mem_obj_output, CL_TRUE, 0, sizeof(cl_ulong), c, 0, NULL, NULL);
  if (ret != CL_SUCCESS) { printf("  [ERROR]\n"); return -1; }

  // Clean-up OpenCL
  clReleaseKernel(kernelTest);
  clReleaseProgram(programTest);

  // is there an arithmetic error?
  if (c[0] != 135630747429) {
    printf( "UNFORGIVABLE ERROR: 64-bit arithmetic error! %" PRIu64 "\n", c[0]);
    return -1;
  }
  free(c);




	}





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

	ret = clEnqueueWriteBuffer(command_queue, mem_obj_arraySmall, CL_TRUE, 0, sizeof(cl_ushort) * arraySmallCount, arraySmall, 0, NULL, NULL);

	if (ret != CL_SUCCESS) {
		printf("[ERROR] clEnqueueWriteBuffer() failed with %" PRIi32 "\n", ret);
		return -1;
	}

	ret = clEnqueueWriteBuffer(command_queue, mem_obj_arrayLarge, CL_TRUE, 0, sizeof(cl_ulong) * arrayLargeCount, arrayLarge, 0, NULL, NULL);

	if (ret != CL_SUCCESS) {
		printf("[ERROR] clEnqueueWriteBuffer() failed with %" PRIi32 "\n", ret);
		return -1;
	}

	ret = clEnqueueWriteBuffer(command_queue, mem_obj_arrayIncreases, CL_TRUE, 0, sizeof(cl_uchar) * arrayIncreasesCount, arrayIncreases, 0, NULL, NULL);

	if (ret != CL_SUCCESS) {
		printf("[ERROR] clEnqueueWriteBuffer() failed with %" PRIi32 "\n", ret);
		return -1;
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










	/* wait for kernel1 to finish (not exactly necessary) */
	clFlush(command_queue);
	clFinish(command_queue);

	gettimeofday(&tv2, NULL);
	printf("  kernel1 is finished: %e seconds elapsed\n",
		(double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec));
	fflush(stdout);








	gettimeofday(&tv2, NULL);
	printf("  kernel1_2 is starting: %e seconds elapsed\n",
		(double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec));
	fflush(stdout);

        /* start kernel1_2 */
	ret = clEnqueueNDRangeKernel(command_queue, kernel1_2, 1, NULL, &global_work_size1_2, NULL, 0, NULL, NULL);

	if (ret != CL_SUCCESS) {
		printf("[ERROR] clEnqueueNDRangeKernel() failed with %" PRIi32 "\n", ret);
		return -1;
	}







	ret = clEnqueueReadBuffer(command_queue, mem_obj_arrayIncreases, CL_TRUE, 0, sizeof(uint8_t) * arrayIncreasesCount, arrayIncreases, 0, NULL, NULL);

	if (ret != CL_SUCCESS) {
		printf("[ERROR] clEnqueueReadBuffer failed with = %" PRIi32 "\n", ret);
		return -1;
	}







  /* fill indices[] */

  global_work_size2 = 0;  // for seeing how much work kernel2 has to do

  for (size_t index = 0; index < arrayIncreasesCount; index++) {

        if (arrayIncreases[index]) {
          indices[global_work_size2] = index;
          global_work_size2++;
        }

  }

  printf("Numbers in sieve segment that need testing = %zu\n", global_work_size2);

  /* pad indices[] to make global_work_size2 a multiple of 32 */
  for (int j = 0; j < (global_work_size2 % 32); j++) {
    indices[global_work_size2] = (~(uint64_t)0);        // I believe this requires TASK_SIZE < 64 + 4
    global_work_size2++;
  }






	/* write indices[] to GPU */
	ret = clEnqueueWriteBuffer(command_queue, mem_obj_indices, CL_TRUE, 0, sizeof(uint64_t) * global_work_size2, indices, 0, NULL, NULL);

	if (ret != CL_SUCCESS) {
		printf("[ERROR] clEnqueueWriteBuffer() failed with %" PRIi32 "\n", ret);
		return -1;
	}







	/* wait for kernel1_2 to finish */
	clFlush(command_queue);
	clFinish(command_queue);

	gettimeofday(&tv2, NULL);
	printf("  kernel1_2 is finished: %e seconds elapsed\n",
		(double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec));
	fflush(stdout);




  // checksum stuff
  cl_mem mem_obj_checksum_alpha = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(cl_ulong) * global_work_size2, NULL, &ret);
  uint64_t g_checksum_alpha = 0;
  clSetKernelArg(kernel2, 4, sizeof(cl_mem), (void *)&mem_obj_checksum_alpha);
  uint64_t *checksum_alpha = malloc(sizeof(uint64_t) * global_work_size2);



	gettimeofday(&tv2, NULL);
	printf("  kernel2 is starting: %e seconds elapsed\n",
		(double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec));
	fflush(stdout);


  for (int i = 0; i < (1 << CHUNKS_KERNEL2); i++) {


	/* run kernel 2 */

	clSetKernelArg(kernel2, 5, sizeof(cl_int), (void *)&i);

	cl_event kernelsDone;
	ret = clEnqueueNDRangeKernel(command_queue, kernel2, 1, NULL, &global_work_size2, NULL, 0, NULL, &kernelsDone);

	if (ret != CL_SUCCESS) {
		printf("[ERROR] clEnqueueNDRangeKernel() failed with %" PRIi32 "\n", ret);
		return -1;
	}

	clFlush(command_queue);




        /*
	   Wait for kernel2 to finish and enforce a time limit.
	   Basically, just use usleep() in a loop that checks if the kernel is done or if time limit is reached.
	   I would like to thank the developers at primegrid.com for sharing this idea with me!
	*/

	cl_int info = CL_QUEUED;   // arbitrary start
	while(info != CL_COMPLETE){
		usleep(1000);    // sleep for 1/1000 of a second
		ret = clGetEventInfo(kernelsDone, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &info, NULL);
		if ( ret != CL_SUCCESS ) { printf( "ERROR: clGetEventInfo\n" ); return -1; }

		gettimeofday(&tv3, NULL);
		if ((double)(tv3.tv_usec - tv2.tv_usec) / 1000000.0 + (double)(tv3.tv_sec - tv2.tv_sec) > secondsKernel2) {
			printf( "ERROR: time limit reached!\n" );
			return -1;
		}
	}
	clReleaseEvent(kernelsDone);




    // checksum stuff
    clEnqueueReadBuffer(command_queue, mem_obj_checksum_alpha, CL_TRUE, 0, sizeof(uint64_t) * global_work_size2, checksum_alpha, 0, NULL, NULL);
    for (long i = 0; i < global_work_size2; ++i) g_checksum_alpha += checksum_alpha[i];


    gettimeofday(&tv2, NULL);

  }

  // checksum stuff
  printf("CHECKSUM %" PRIu64 "\n", g_checksum_alpha);
  free(checksum_alpha);


	ret = clReleaseKernel(kernel1);
	ret = clReleaseProgram(program1);
	ret = clReleaseKernel(kernel1_2);
	ret = clReleaseProgram(program1_2);
	ret = clReleaseKernel(kernel2);
	ret = clReleaseProgram(program2);
	ret = clReleaseCommandQueue(command_queue);
	ret = clReleaseContext(context);

	free(program_string1);
	free(program_string1_2);
	free(program_string2);
	free(device_id);



  free(arraySmall);
  free(arrayLarge);
  free(arrayIncreases);
  free(indices);

  printf("  Elapsed wall time is %e seconds\n\n",
      (double)(tv2.tv_usec - tv1.tv_usec) / 1000000.0 + (double)(tv2.tv_sec - tv1.tv_sec));

  return 0;
}
