#include <stdio.h>
#include <string.h>
#include <zlib.h>

unsigned long calculate_crc32(const char *data) {
    return crc32(0L, (const Bytef *)data, strlen(data));
}

int main(int argc, char **argv) {
    const char *str = "hello";
    unsigned long crc = calculate_crc32(str);

    printf("crc: %#010lx\n", crc);
    return 0;
}