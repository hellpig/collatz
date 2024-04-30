/*

45210249143 * 3 = 135630747429

But OpenCL can give the wrong answer for 64-bit 45210249143 when 3 is an integer literal!
This is probably because the compiler actually does n+n+n ???

Windows and macOS have this problem using AMD and Intel GPUs.
Ubuntu (using beignet-dev package) works fine.
NVIDIA (using Ubuntu's nvidia-cuda-toolkit package) works!

*/


#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>


// Sets version to OpenCL 1.2 (my Windows setup uses this)
//#define CL_TARGET_OPENCL_VERSION 120

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif




// kernel needs at least one argument to compile
const char *kernelString =
    "__kernel void kernelFunc(const unsigned long a, const unsigned long b) {"
    "    unsigned long n = a*3;"
    "    unsigned long fix = (unsigned long)1 << 32;"    // 2^32 
    "    printf(\"%llu\\n\", n);"    // bug
    "    printf(\"%llu\\n\", a+a+a);"    // I think this is the real bug ???
    "    printf(\"%llu\\n\", a*3);"  // bug
    "    printf(\"fix: %llu\\n\", n + fix);"
    "    printf(\"%llu\\n\", 3*45210249143);"
    "    unsigned long n2 = b*45210249143;"
    "    printf(\"%llu\\n\", n2);"
    "    printf(\"%llu\\n\", a*b);"
    "}";




int main(void) {

    uint64_t a = 45210249143;
    uint64_t b = 3;
    uint64_t n = b*a;
    printf("CPU: %" PRIu64 "\n", n);

    // set these to match your setup
    int platformID = 0;
    int deviceID = 0;

    // Configure the OpenCL environment
    cl_platform_id platforms[100];
    cl_device_id devices[100];
    clGetPlatformIDs(100, platforms, NULL);
    clGetDeviceIDs(platforms[platformID], CL_DEVICE_TYPE_GPU, 100, devices, NULL);
    char deviceName[1024];
    clGetDeviceInfo(devices[deviceID], CL_DEVICE_NAME, 1024, deviceName, NULL);
    printf(">>> %s\n", deviceName);
    cl_context context = clCreateContext(NULL, deviceID+1, devices, NULL, NULL, NULL);
    cl_command_queue queue = clCreateCommandQueue(context, devices[deviceID], 0, NULL);

    // Compile the kernel
    cl_program program = clCreateProgramWithSource(context, 1, &kernelString, NULL, NULL);
    clBuildProgram(program, 0, NULL, "", NULL, NULL);

    // Check for compilation errors
    size_t logSize;
    clGetProgramBuildInfo(program, devices[deviceID], CL_PROGRAM_BUILD_LOG, 0, NULL, &logSize);
    char* messages = (char*)malloc((1+logSize)*sizeof(char));
    clGetProgramBuildInfo(program, devices[deviceID], CL_PROGRAM_BUILD_LOG, logSize, messages, NULL);
    messages[logSize] = '\0';
    if (logSize > 10) { printf(">>> Compiler message: %s\n", messages); }
    free(messages);

    // Run the kernel
    cl_kernel kernel = clCreateKernel(program, "kernelFunc", NULL);
    clSetKernelArg(kernel, 0, sizeof(uint64_t), (void*)&a);
    clSetKernelArg(kernel, 1, sizeof(uint64_t), (void*)&b);
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