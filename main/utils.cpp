#include "utils.h"
#include <cstdio>

void printHexString(const uint8_t *buffer, size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        printf("%02X ", buffer[i]);
    }
    printf("\n");
}