
#define CL_USE_DEPRECATED_OPENCL_2_0_APIS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <CL/cl.hpp>
#include <unistd.h>
#include <assert.h>
#include <iostream>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <vector>
#include <math.h>

#define NO_COMPATIBLE_OPENCL_DEVICES 0x00
#define SIZE 16

using namespace std;
using namespace cl;

/*

	New concepts I currently need to understand:

		cl::Context - thing that hold all of the cool objects like buffers,devices etc..
		cl::Program::Sources - source of our opcodes and shellcode stuff.
		command queues - api to talk to DEVICE
		GPU buffers - memory in the actual device

*/
int main () { 
	


	printf("hello %d\n",getpid());	 

	//getting all platform ids and checking if there are actually any opencl compatible devices.	
	cl_uint plat_id = 0 ;
	clGetPlatformIDs(0,nullptr,&plat_id);
	assert(plat_id != NO_COMPATIBLE_OPENCL_DEVICES);

	//getting all available platforms..
	vector<Platform> platforms;
	Platform::get(&platforms);

	cl::Platform def =platforms[0];
	cout<<"Using platform:\t"<<def.getInfo<CL_PLATFORM_NAME>()<<endl;

	//finding available devices;
	vector<cl::Device>devices;
	def.getDevices(CL_DEVICE_TYPE_ALL,&devices);
	assert(devices.size() != NO_COMPATIBLE_OPENCL_DEVICES);


	cl::Device dev = devices[0];
	cout<<"Using device (CPU/GPU):\t"<<dev.getInfo<CL_DEVICE_NAME>()<<endl;
	/*
		Creating Contexts.
		What are contexts?
			they are used to manage objects such as cmd queues and mem.
	*/

	cl::Context context({dev});
	//also as we know, we are pushing cmds to the queue with instructions.
	//that means, CREATE PROGRAM SOURCE

	cl::Program::Sources sources;

	//preparing random 2 arrays.

	double * A =  (double*)malloc(SIZE*sizeof(double));
	double * B =  (double*)malloc(SIZE*sizeof(double));

	for(int i = 0; i<SIZE;i++){
		A[i] = i*i;
		B[i] = sqrt(i*i*i);

	}

	double *C = (double*)malloc(SIZE*sizeof(double));
	memset(C,0x00,sizeof(double)*SIZE);

	// What should gpu do?  C = A @ B 
	//What will it do ? i dont have any idea.


	//3 buffers, W+R allowed.
	Buffer a_dev(context,CL_MEM_READ_WRITE,sizeof(double)*SIZE);
	Buffer b_dev(context,CL_MEM_READ_WRITE,sizeof(double)*SIZE);
	Buffer c_dev(context,CL_MEM_READ_WRITE,sizeof(double)*SIZE);


	//Once the CommandQueue queue is created, it is possible to execute commands on the device side.
	cl::CommandQueue cmdq(context,dev);
	
	//enqueueWriteBuffer - CMD QUEUE TO WRITE TO DEVICES BUFFER (WRITE SOME DATA BRO! SWAG)
	cmdq.enqueueWriteBuffer(a_dev,CL_TRUE,0,sizeof(double)*SIZE,A);
	cmdq.enqueueWriteBuffer(b_dev,CL_TRUE,0,sizeof(double)*SIZE,B);


	//kernel code?? is this really that bad, like , cant they just like make a constructor or something, what the fuck is this?
	string kern_code = 
	"	void kernel a_mul_basic(global const double* A, global const double* B, global double* C){"
	"		C[get_global_id(0)] = A[get_global_id(0)] + B[get_global_id(0)];"
	"	}";
	;


	sources.push_back({kern_code.c_str(),kern_code.length()});
	cl::Program prog(context,sources);

	int ret = prog.build({dev});
	assert(ret == CL_SUCCESS);

	//launch devices kernel on device
	//creating a kernel for the function that we have (our input args and name of the function with Program respectively.)

	make_kernel<Buffer,Buffer,Buffer> a_mul_basic(Kernel(prog,"a_mul_basic"));
	cl::NDRange global(SIZE); // get_global_id loop times N (times SIZE);
	a_mul_basic(EnqueueArgs(cmdq,global),a_dev,b_dev,c_dev).wait();

	cmdq.enqueueReadBuffer(c_dev,CL_TRUE,0,sizeof(double)*SIZE,C);
	for(int  i =0 ;i < SIZE;i++){
		printf("\n%lf",C[i]);
	}



}
