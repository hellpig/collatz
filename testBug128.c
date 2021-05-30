/*

Does the following work correctly in OpenCL ?
  unsigned __int128

It's shorter to write
  __uint128_t

I avoid 64-bit numbers in my kernel here since I know they have arithmetic issues (see testBug64.c).

Windows and macOS have problems using AMD and Intel GPUs.
Ubuntu (using beignet-dev package) doesn't seem to support 128-bit integers.
NVIDIA (using Ubuntu's nvidia-cuda-toolkit package) works!

On various devices (including NVIDIA on Ubuntu)...
128-bit seems to work for everything but passing variables into kernel.
To test this, uncomment everything having to do with variable c (4 places in code).

On my Intel GPUs (not on Linux)...
128-bit numbers won't add (error when kernel compiles).
If changing n1, n2, and n to unsigned int, it "works" but then there is an arithmetic error.

*/


#include <stdlib.h>
#include <stdio.h>


// Sets version to OpenCL 1.2 (my Windows setup uses this)
//#define CL_TARGET_OPENCL_VERSION 120

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif




// kernel needs at least one argument to compile
const char *kernelString =
    "__kernel void kernelFunc(const unsigned int a1, const unsigned int a2 /*, const __uint128_t c */ ) {    \n"
    "    __uint128_t n0 = (__uint128_t)1 << 127;   \n"   // 2^127
    "    __uint128_t n1 =  (__uint128_t)a2 << 23;  \n"
    "    __uint128_t n2 = (__uint128_t)a1;         \n"
    "    __uint128_t n = (n1+n2)*3;                \n"   // this step crashes ??
    "    __uint128_t mask = (__uint128_t)1 << 127;         \n"  // start printing n
    "    for (int i=0; i<128; i++) {                       \n"
    "      if (n & mask) { printf(\"1\"); }                \n"
    "      else { printf(\"0\"); }                         \n"
    "      mask >>= 1;                                     \n"
    "    }                                                 \n"
    "    printf(\"\\n\");                                  \n"
    "    mask = (__uint128_t)1 << 127;                     \n"  // start printing n0
    "    for (int i=0; i<128; i++) {                       \n"
    "      if (n0 & mask) { printf(\"1\"); }               \n"
    "      else { printf(\"0\"); }                         \n"
    "      mask >>= 1;                                     \n"
    "    }                                                 \n"
    "    printf(\"\\n\");                                  \n"
/*    "    mask = (__uint128_t)1 << 127;                     \n"  // start printing c
    "    for (int i=0; i<128; i++) {                       \n"
    "      if (c & mask) { printf(\"1\"); }                \n"
    "      else { printf(\"0\"); }                         \n"
    "      mask >>= 1;                                     \n"
    "    }                                                 \n"
    "    printf(\"\\n\");                                  \n"  */
    "}";



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




int main(void) {

    uint32_t a = 3;
    uint64_t b = 45210249140;
    printBinary128(3*(b+a), 128);

    uint32_t a1 = 4040631;  // least significant bits of 45210249143
    uint32_t a2 = 5389;     // most significant bits of 45210249143

    /* __uint128_t c = (__uint128_t)1 << 127; */

    // set these to match your setup
    cl_platform_id platform = 0;
    cl_device_id device = 0;

    // Configure the OpenCL environment
    clGetPlatformIDs(1, &platform, NULL);
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    char deviceName[1024];
    clGetDeviceInfo(device, CL_DEVICE_NAME, 1024, deviceName, NULL);
    printf(">>> %s\n", deviceName);
    cl_context context = clCreateContext(NULL, 1, &device, NULL, NULL, NULL);
    cl_command_queue queue = clCreateCommandQueue(context, device, 0, NULL);

    // Compile the kernel
    cl_program program = clCreateProgramWithSource(context, 1, &kernelString, NULL, NULL);
    clBuildProgram(program, 0, NULL, "", NULL, NULL);

    // Check for compilation errors
    size_t logSize;
    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &logSize);
    char* messages = (char*)malloc((1+logSize)*sizeof(char));
    clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, logSize, messages, NULL);
    messages[logSize] = '\0';
    if (logSize > 10) { printf(">>> Compiler message: %s\n", messages); }
    free(messages);

    // Run the kernel
    cl_kernel kernel = clCreateKernel(program, "kernelFunc", NULL);
    clSetKernelArg(kernel, 0, sizeof(uint32_t), (void*)&a1);
    clSetKernelArg(kernel, 1, sizeof(uint32_t), (void*)&a2);
    /* clSetKernelArg(kernel, 2, sizeof(__uint128_t), (void*)&c); */
    const size_t local = 1;
    const size_t global = 1;
    cl_event event = NULL;
    clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global, &local, 0, NULL, &event);
    clWaitForEvents(1, &event);

    // Clean-up OpenCL
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);

    return 0;
}