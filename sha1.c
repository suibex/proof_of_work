#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#define SHA_DIGEST_LEN 20 //160 bits.
#define INTEGER_BIT_LENGTH 32
#define H0 0x67452301
#define H1 0xEFCDAB89
#define H2 0x98BADCFE
#define H3 0x10325476
#define H4 0xC3D2E1F0

uint32_t rotl32 (uint32_t value, unsigned int count) {
    const unsigned int mask = CHAR_BIT * sizeof(value) - 1;
    count &= mask;
    return (value << count) | (value >> (-count & mask));
}


size_t get_padded_message_length(size_t message_length) { 
  // Returns the message length rounded up to the nearest multiple of 64 (so that it rounds up to a multiple of 512 bitwise).
  int factor = ((message_length % 64) < 56) ? 1 : 2; // need 8 extra bytes minimum at the end
  return ((message_length / 64) + factor) * 64; 
}
unsigned int parse_integer(unsigned char*  location) {
  // Converts a chunk of four byte at *location into a big-endian integer
  unsigned int n = *((unsigned int*) location); // first convert the memory at location into an unsigned integer
  return ( (n        & 0xff) << 24) |
       (((n >> 8 ) & 0xff) << 16) |
       (((n >> 16) & 0xff) << 8 ) |
       ( (n >> 24) & 0xff       );  // then flip the endianness of the four bytes
}

unsigned char compute_sha1(unsigned char*x ,uint8_t len ){ 
  
  uint32_t h0 = H0;
  uint32_t h1 = H1;
  uint32_t h2 = H2;
  uint32_t h3 = H3;
  uint32_t h4 = H4;



  size_t msg_len = get_padded_message_length(len);
  //printf("\nmsg_len:%d before:%d",msg_len,len);
  //exit(1);
  //448 bits + 64 bits  = 512
  unsigned char compute[msg_len];
  memcpy(compute,x,len);
  
  compute[len] = 0x80;
  int lnt = msg_len-len-8; 
 
 // exit(1);
  int lnd = len+1;
  for(int i = 1;i<lnt;i++){
    compute[lnd] = 0x00;
    lnd++;
  }

  uint64_t val = len*8;
  uint64_t *ptr = &val;
  uint8_t *mad = ptr;
  
  uint8_t mask = 0b11111111;

  int ln = lnd;
 
  for(int i = 7;i>=0;i--){
   
    compute[ln] = mad[i];
    ln++;
 
  }
  /* This code above will be done before actual OpenCL hashing ... */

  // **** SHA1 STARTS HERE **** - everything else is just preparation of the input to our hashfunc 
  // input -> hash -> Y
  
  printf("\n");
  for(int z =0;z<msg_len;z+=64){
   
    //breaking chunk into 32bit chunks big-endian 16 max.
    uint32_t w[80];
    int loaded_nums = 0 ; 
    for(int j = 0 ;j < 16;j++ ){ 
        uint32_t result = parse_integer(compute+j*sizeof(unsigned int));
        w[loaded_nums]= result;
        loaded_nums++;
    }


    for(int i = 16;i<80;i++){
      w[i] = left_rotate((w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16]),1);
    } 
    
    uint32_t a = H0;
    uint32_t b = H1;
    uint32_t c = H2;
    uint32_t d = H3;
    uint32_t e = H4;


    for(int i = 0 ;i<80;i++){
      uint32_t f=0;
      uint32_t k = 0; 
      if(i>=0 && i<=19){
        f = (b & c ) ^ ((~b) & d);
        k = 0x5A827999;
      }
      else if(i>=20 && i<=39){
        f= (b^c^d);
        k = 0x6ED9EBA1;

      }
      else if(i>=40 && i<=59){
        f =(b&c) ^ (b & d ) ^ (c & d);
        k = 0x8F1BBCDC;

      }
      else if(i>=60 && i<=79){

        f= b^c^d;
        k = 0xCA62C1D6;

      }
      uint32_t temp = rotl32(a,5) + f + e + k + w[i];

      e = d;
      d = c;
      c = rotl32(b,30);
      b = a;
      a = temp;

    }
    h0=h0+a;
    h1=h1+b;
    h2=h2+c;
    h3=h3+d;
    h4=h4+e;
    
  }

  uint32_t h[5];
  h[0] = h0;
  h[1] = h1;
  h[2] = h2;
  h[3] = h3;
  h[4] = h4;

  printf("\n");
  for(int i = 0 ;i < 5;i++){
    printf("%08x",h[i]);
  }
  printf("\n");




 
  return NULL;
}

/*
  https://en.wikipedia.org/wiki/Circular_shift

  Basically shifting numbers from back to front right?

*/

int main (){
    
    unsigned char input[40];
    memset(input,0xff,40);
 
   compute_sha1(input,40);

}
