
void kernel flip_byte( global  int *byte){
    *byte = ((*byte) >> 8) | ((*byte) << 8 );
}