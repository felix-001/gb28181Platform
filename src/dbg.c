#include "public.h"

void dbg_dump_buf(uint8_t *buf, size_t size)
{
    for (int i=0; i<size; i++) {
        printf("0x%02x ", buf[i]);
        if (!((i+1)%16)) {
            printf(" [%03d]\n", i+1);
        }
    }
    printf("\n");
}