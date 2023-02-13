
/*
    author:@nitrodegen
    title: really bad sha1 implementation for OpenCL
    date: 13 Feb. 2023.
*/
#define SHA_DIGEST_LEN 20 //160 bits.
#define INTEGER_BIT_LENGTH 32

#define H0 0x67452301
#define H1 0xEFCDAB89
#define H2 0x98BADCFE
#define H3 0x10325476
#define H4 0xC3D2E1F0

uint rotl32 (uint number, uint n) {
   return (uint) ((number << n) | (number >> (INTEGER_BIT_LENGTH - n)));
}

unsigned int parse_integer(uint nd) {
  // Converts a chunk of four byte atn*location into a big-endian integer
    unsigned int  n= nd;
    return ( (n        & 0xff) << 24) |
       (((n >> 8 ) & 0xff) << 16) |
       (((n >> 16) & 0xff) << 8 ) |
       ( (n >> 24) & 0xff       );  // then flip the endianness of the four bytes
}

void __kernel sha1_compute(const global uint* input,  global unsigned char* out){

    uint h0 = H0;
    uint h1 = H1;
    uint h2 = H2;
    uint h3 = H3;
    uint h4 = H4;

        uint w[80];
        int ld = 0 ;
        for(int j = 0 ;j < 16;j++)
        {
            unsigned int res = parse_integer(input[j]);
            w[ld] = res;
            ld++;
        }
        w[ld] = 320;


        for(int j = 16;j<80;j++){
            w[j] = rotl32((w[j-3] ^ w[j-8] ^ w[j-14] ^ w[j-16]),1);
        }
        uint a = h0;
        uint b = h1;
        uint c = h2;
        uint d = h3;
        uint e = h4;
        for(int j = 0 ;j < 80 ;j++){
            uint k = 0 ;
            uint f = 0 ;
             if(j>=0 && j<=19){
                f = (b & c ) ^ ((~b) & d);
                k = 0x5A827999;
            }
            else if(j>=20 && j<=39){
                f= (b^c^d);
                k = 0x6ED9EBA1;

            }
            else if(j>=40 && j<=59){
                f =(b&c) ^ (b & d ) ^ (c & d);
                k = 0x8F1BBCDC;

            }
            else if(j>=60 && j<=79){

                f= b^c^d;
                k = 0xCA62C1D6;

            }
           // printf("\nK:%x",k);
            uint temp = rotl32(a,5) + f + e + k + w[j];
            e = d;
            d = c;
            c = rotl32(b,30);
            b = a;
            a = temp;
        }
        h0 +=a;
        h1 +=b;
        h2 +=c;
        h3 +=d;
        h4 +=e;

    
    uint h[5];
    h[0] = h0;
    h[1] = h1;
    h[2] = h2;
    h[3] = h3;
    h[4] = h4;  

    unsigned char *m =(unsigned char*)h;
    int loaded_bytes = 0 ;
    for(int i = 0;i <20;i+=4){
        for(int j = 3;j>=0;j--){
            out[loaded_bytes++] = m[i+j];
        }
    }


}