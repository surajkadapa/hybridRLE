#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void rle_compress(const char *input, char *output) {
    int count, i, j = 0;
    int len = strlen(input);

    for (i = 0; i < len; i++) {
        count = 1;
        while (i + 1 < len && input[i] == input[i + 1]) {
            count++;
            i++;
        }
        output[j++] = input[i];
        j += sprintf(&output[j], "%d", count);
    }
    output[j] = '\0';
}

void rle_decompress(const char *input, char *output) {
    int i, j = 0, count;
    char ch;
    for (i = 0; input[i] != '\0'; i++) {
        ch = input[i];
        count = 0;

        while (input[i + 1] >= '0' && input[i + 1] <= '9') {
            count = count * 10 + (input[i + 1] - '0');
            i++;
        }

        while (count--) {
            output[j++] = ch;
        }
    }
    output[j] = '\0';
}

int main() {
    char input[] = "AAAABBBCCDAAAA";
    char compressed[100], decompressed[100];

    rle_compress(input, compressed);
    printf("Compressed: %s\n", compressed);

    rle_decompress(compressed, decompressed);
    printf("Decompressed: %s\n", decompressed);

    return 0;
}
