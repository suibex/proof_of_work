
/*

    @concept: Proof of Work ( Bitcoin )
                -hashing SHA1 (brute-forcing)
                +OpenCL support
    @author:nitrodegen
    @date: 13 Feb 2023.

*/
#define CL_USE_DEPRECATED_OPENCL_2_0_APIS
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <threads.h>
#include <iostream>
#include <math.h>
#include <openssl/sha.h>
#include <assert.h>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>
#include <random>
#include <sys/types.h>
#include <sys/mman.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <CL/cl.hpp>
#include <sstream>
#include <fstream>

//opencl defines
#define CL_DBG_MSGS true
#define NO_COMPATIBLE_OPENCL_DEVICES 0x00
#define SIZE 16
//ASCII coloring
#define GRN "\e[0;32m"
#define reset "\e[0m"
#define CRESET "\e[0m"
#define COLOR_RESET "\e[0m"
#define HASH_SIZE 24 // defines the actual size for SHA1 input ( SHA_DIGEST_LENGTH(20)) + missing bytes (MAX 4) - should be 24 at its peak
#define SHARED_PID_POOL_FD "miner_pid_pool"

/*Number of processes to be started, and number of threads to be started in those processes.*/
#define NUM_OF_PROCESSES 1
#define NUM_THREADS 1

const unsigned char  TRANSACTION_NUM[4] {0x44,0x32,0x31,0x56};

using namespace std;
using namespace cl;

void dbg_msg(const char* x){
    if(CL_DBG_MSGS){
        printf(" [ DEBUG ] %s\n",x);
    }
    
}

//will need to do this via OpenCL

unsigned char* create_sha_hash(unsigned char* a,int len ){
  
    unsigned char *hash= (  unsigned char*)malloc(SHA_DIGEST_LENGTH);
    const unsigned char *input = (const unsigned char*)a;
    SHA1(input,len,hash);

    return hash;
}


ssize_t get_padded_message_length(ssize_t message_length) { 
 
  int factor = ((message_length % 64) < 56) ? 1 : 2; // need 8 extra bytes minimum at the end
  return ((message_length / 64) + factor) * 64; 
}

class Accelerator{
    public:
        cl_uint platform_ids;
        Platform platform;
        Device dev;
        Program prog;
        Program::Sources src;
        Context cntx;
        CommandQueue cmdq;
        vector<string>defined_functions;
        Accelerator(){

            dbg_msg("initializing platform and devices..");
            //start opencl implementation here
            clGetPlatformIDs(0,NULL,&platform_ids);
            assert(platform_ids != NO_COMPATIBLE_OPENCL_DEVICES);

            vector<Platform>platforms;
            Platform::get(&platforms); 
            platform = platforms[0];

            vector<Device>devices;
            platform.getDevices(CL_DEVICE_TYPE_ALL,&devices);
            assert(devices.size() != NO_COMPATIBLE_OPENCL_DEVICES);

            dev= devices[0];

            dbg_msg(platform.getInfo<CL_PLATFORM_NAME>().c_str());
            dbg_msg(dev.getInfo<CL_DEVICE_NAME>().c_str());

            cl::Context context({dev});
            cntx = context;
            CommandQueue com_q(cntx,dev);
            cmdq= com_q;

            dbg_msg("context and platform initialized.");


                        
        } 
        int flip_byte(int byte){
            
            src.clear();


            Buffer byte_buf(cntx,CL_MEM_READ_WRITE,sizeof(int));
     
            cmdq.enqueueWriteBuffer(byte_buf,CL_TRUE,0,sizeof(int),&byte);
 
            fstream strm("./clsrc/flip.cl");
            stringstream ds;
            ds<<strm.rdbuf();
            string data(ds.str());

            src.push_back({data.c_str(),data.length()});

            Program prog(cntx,src);

            int ret_build = prog.build({dev});
            assert(ret_build == CL_SUCCESS);
            
            NDRange range(1);

            make_kernel<Buffer> flip_byte(Kernel(prog,"flip_byte"));
            flip_byte(EnqueueArgs(cmdq,range),byte_buf).wait();
            int ret =0;
          
            cmdq.enqueueReadBuffer(byte_buf,CL_TRUE,0,sizeof(int),&ret);
            return  ret;

        }
        unsigned char *sha_hash( char* x,unsigned int len ){
 
 
            int msg_len = get_padded_message_length(len);
 
            unsigned char compute[64];
            memcpy(compute,x,len);
            compute[len] = 0x80;


            int padded_len = msg_len-8-len;
            int start = len+1;
            for(int i = 1; i< padded_len;i++){
                compute[start] = 0x00;
                start++;
            }

            uint64_t val = len*8;
            uint64_t *ptr = &val;
            uint8_t *mad = (uint8_t*)ptr;
            
            int ln = start;
            for(int i = 7;i>=0;i--){
                compute[ln]= mad[i];
                ln++;
            }

            unsigned int X[16];
            int loaded = 0;
            for(int i = 0;i<64;i+=sizeof(unsigned int)){
                    unsigned int num = 0x000000;
                    unsigned int *ptr= &num;
                    memcpy(ptr,compute+i,sizeof(unsigned int));   
                    
                    //printf("\nX[%d] = %d",loaded,num);
                    X[loaded++] = num;
            }   
            //printf("\n\n");
            

            //Preparing done. Running OpenCL calculations...
 
            cl::Buffer input_digest(cntx,CL_MEM_READ_WRITE,16*sizeof(unsigned int));
            cl::Buffer hashed(cntx,CL_MEM_READ_WRITE,sizeof(uint32_t)*5);
            CommandQueue cmdq(cntx,dev);           
            cmdq.enqueueWriteBuffer(input_digest,CL_TRUE,0,16*sizeof(unsigned int),X);
            fstream strm("./clsrc/my_sha.cl");
            stringstream ss;
            ss<<strm.rdbuf();
            string data(ss.str());
    
            src.push_back({data.c_str(),data.length()});
            Program prog(cntx,src);

            int ret = prog.build({dev});
            if(ret != CL_SUCCESS){
                string log= prog.getBuildInfo<CL_PROGRAM_BUILD_LOG>(dev);
                cout<<log<<endl;
                exit(1);
            }
            dbg_msg("sha1_compute kernel succesfully built.");

            make_kernel<Buffer,Buffer> sha1_compute(Kernel(prog,"sha1_compute"));
            NDRange globl(1);
            sha1_compute(EnqueueArgs(cmdq,globl),input_digest,hashed).wait();
       
            unsigned char *out = (unsigned char*)malloc(SHA_DIGEST_LENGTH);
            memset(out,0x00,sizeof(unsigned int)*5);
            cmdq.enqueueReadBuffer(hashed,CL_TRUE,0,sizeof(unsigned int)*5,out);
            src.clear();
            cmdq.finish();
            
            return out;

            
        }
};

/* this hash is created on the actual blockchain, and is VERIFIED by the worker (miner) */
unsigned char* create_target_hash(){

    Accelerator*accel = new Accelerator();
    string TARGET_TRANSACTION;

    for(int i = 0; i < SHA_DIGEST_LENGTH;i++){
        TARGET_TRANSACTION+="\x41";
    }

    TARGET_TRANSACTION+=TRANSACTION_NUM[0];
    TARGET_TRANSACTION+=TRANSACTION_NUM[1];
    TARGET_TRANSACTION+=TRANSACTION_NUM[2];
    TARGET_TRANSACTION+=TRANSACTION_NUM[3];

    unsigned char  input[HASH_SIZE];
    for(int i =0;i<HASH_SIZE;i++){
        input[i] = TARGET_TRANSACTION[i];
    }
    unsigned char* TARGET_HASH=  accel->sha_hash((char*)input,HASH_SIZE);
    
    
    return TARGET_HASH;

}

pid_t *shared_pids;
int shared_mem_fd;

void* mine_sha1(void *vargp){
    Accelerator *accel = new Accelerator();
    unsigned char* unknwn_hash  = create_target_hash();

    for(int i = 0;i<20;i++){
        printf("%x",unknwn_hash[i]);
    }


    srand(time(NULL));
    random_device dev;
    int attempts=0;
    mt19937 rng(dev());
    uniform_int_distribution<std::mt19937::result_type> hashd(1,255);
    while(1){

        clock_t begin = clock();
        unsigned char b1 = hashd(rng);
        unsigned char b2 = hashd(rng);
        unsigned char b3 = hashd(rng);
        unsigned char b4 = hashd(rng);

        unsigned char *guess= (unsigned char *)malloc(HASH_SIZE);
        for(int i = 0 ;i <SHA_DIGEST_LENGTH;i++  ){ 
            guess[i]='\x41';
        }
        guess[20]=b1;
        guess[21] = b2;
        guess[22] =b3;
        guess[23] = b4;
        int matches =0;
        unsigned char* hashed = accel->sha_hash((char*)guess,HASH_SIZE);
        for(int i = 0 ; i<SHA_DIGEST_LENGTH;i++){
            if(hashed[i] == unknwn_hash[i]){
                matches++;
            }   
        }

        if(matches == SHA_DIGEST_LENGTH){

          printf("%s\n** Brute-forced bytes: 0x%hhx 0x%hhx 0x%hhx 0x%hhx%s\n",GRN,b1,b2,b3,b4,reset);
            printf("\n%s *** Transaction verified successfully. *** %s\n",GRN,reset);
            printf("** Stopping mining processes...\n");
            for(int i = 0 ;i < NUM_OF_PROCESSES;i++){
                if(shared_pids[i] != getpid()){
                    kill(shared_pids[i],SIGKILL);
                }
            }

            exit(1);
            return NULL;
        }
        clock_t end= clock();
    
        printf("$  PID: %d Matches:%d time:%lf secs 0x%hhx 0x%hhx 0x%hhx 0x%hhx\n",getpid(),matches,(end-begin)/1000000.0,b1,b2,b3,b4);
        free(guess);
        guess=NULL;
        free(hashed);
        hashed=NULL;
        attempts++;
    }
}

int main(){

    shared_mem_fd = memfd_create(SHARED_PID_POOL_FD,0);
    assert(shared_mem_fd != -1 );
    ftruncate(shared_mem_fd,NUM_OF_PROCESSES*sizeof(pid_t));
    char *vd = (char*)mmap(NULL,NUM_OF_PROCESSES*sizeof(pid_t),PROT_READ|PROT_WRITE,MAP_SHARED | MAP_ANONYMOUS,shared_mem_fd,0);
    assert(vd != MAP_FAILED);
    memset(vd,0x00,NUM_OF_PROCESSES*sizeof(pid_t));
    shared_pids = (pid_t*)vd;
    int counter = 0;

    pid_t pids[NUM_OF_PROCESSES];
   
    for(int i = 0;i < NUM_OF_PROCESSES;i++){

        pid_t cur  = fork();
        if(cur == 0 ){
            
            //create 20 threads here.
            vector<pthread_t> threads;
            for(int j = 0 ; j < NUM_THREADS;j++){
                pthread_t thread;
                pthread_create(&thread,NULL,mine_sha1,0);
                threads.push_back(thread);
            }
            int d = 1 ; 
            for(pthread_t const & th :threads){
                printf("\nStarted thread: %d",d);
                pthread_join(th,NULL);
                d++;
            }
            
            
        }
        else{
            pids[i] = cur;
            shared_pids[counter] = cur;
            counter++;
        }
    }
    for(int i =0 ;i < NUM_OF_PROCESSES;i++){
        int stat = wait(&pids[i]);
        printf("\nProcess %d exited with status (%d)\n",stat,pids[i]);
    }

    
    printf("\nClosing shared memory file descriptor...\n");
    close(shared_mem_fd);
    

}