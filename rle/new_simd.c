#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>  // For SIMD intrinsics

#define ALPHABET_SIZE 256
#define MAX_TREE_NODES 511

typedef struct HuffmanNode {
    uint8_t symbol;
    int freq;
    struct HuffmanNode *left, *right;
} HuffmanNode;

typedef struct {
    uint8_t code[32];  // Huffman code (max 256 bits)
    int length;
} HuffmanCode;

typedef struct {
    HuffmanNode *nodes[MAX_TREE_NODES];
    int size;
} PriorityQueue;

typedef struct {
    uint8_t buffer;
    int bit_pos;
} BitBuffer;

// ----------------- File I/O -----------------
uint8_t* read_file(const char *filename, size_t *size) {
    FILE *file = fopen(filename, "rb");
    if (!file) { 
        fprintf(stderr, "Error opening file: %s\n", filename); 
        exit(1); 
    }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    rewind(file);

    uint8_t *data = malloc(*size);
    if (!data) { 
        fprintf(stderr, "Memory allocation failed\n"); 
        exit(1); 
    }

    if (fread(data, 1, *size, file) != *size) {
        fprintf(stderr, "Error reading file\n");
        exit(1);
    }
    fclose(file);
    return data;
}

void write_file(const char *filename, uint8_t *data, size_t size) {
    FILE *file = fopen(filename, "wb");
    if (!file) { 
        fprintf(stderr, "Error writing file: %s\n", filename); 
        exit(1); 
    }

    if (fwrite(data, 1, size, file) != size) {
        fprintf(stderr, "Error writing file\n");
        exit(1);
    }
    fclose(file);
}

//implements the sift up property in a heap
void pq_push(PriorityQueue *pq, HuffmanNode *node) {
    int i = pq->size++;
    while (i > 0 && node->freq < pq->nodes[(i - 1) / 2]->freq) { //checking if the new nodes freq is lesser than the parents freq(if node is "i" then the parent is always "(i-1)/2")
        pq->nodes[i] = pq->nodes[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    pq->nodes[i] = node;
}

/*
pops the highest element in the heap(lowest freq) and heapify again
*/
HuffmanNode* pq_pop(PriorityQueue *pq) {
    HuffmanNode *top = pq->nodes[0];
    HuffmanNode *last = pq->nodes[--pq->size];

    int i = 0;
    while (2 * i + 1 < pq->size) {
        int j = 2 * i + 1;
        if (j + 1 < pq->size && pq->nodes[j + 1]->freq < pq->nodes[j]->freq) j++;
        if (last->freq <= pq->nodes[j]->freq) break;
        pq->nodes[i] = pq->nodes[j];
        i = j;
    }
    pq->nodes[i] = last;
    return top;
}

/*
calculates the freq of the byte values, and creates a freq table for the huffmann process
*/
void count_frequencies_simd(uint8_t* data, size_t size, int freq[256]) {
    // Initialize frequency counts to zero
    memset(freq, 0, 256 * sizeof(int));

    // Process 32 bytes at a time using AVX2
    size_t i = 0;
    for (; i + 31 < size; i += 32) {
        __m256i chunk = _mm256_loadu_si256((__m256i*)&data[i]);
        
        // Extract bytes to temporary array
        uint8_t temp[32];
        _mm256_storeu_si256((__m256i*)temp, chunk);
        
        // Update histogram
        for (int j = 0; j < 32; j++) {
            freq[temp[j]]++;
        }
    }

    // Process remaining bytes
    for (; i < size; i++) {
        freq[data[i]]++;
    }
}

//building the huffman tree
HuffmanNode* build_huffman_tree(uint8_t *data, size_t size) {
    int freq[256];
    count_frequencies_simd(data, size, freq);

    PriorityQueue pq = { .size = 0 };
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            HuffmanNode *node = malloc(sizeof(HuffmanNode));
            if (!node) {
                fprintf(stderr, "Memory allocation failed\n");
                exit(1);
            }
            node->symbol = i;
            node->freq = freq[i];
            node->left = node->right = NULL;
            pq_push(&pq, node);
        }
    }

    while (pq.size > 1) {
        HuffmanNode *left = pq_pop(&pq);
        HuffmanNode *right = pq_pop(&pq);
        HuffmanNode *parent = malloc(sizeof(HuffmanNode));
        if (!parent) {
            fprintf(stderr, "Memory allocation failed\n");
            exit(1);
        }
        parent->symbol = 0;
        parent->freq = left->freq + right->freq;
        parent->left = left;
        parent->right = right;
        pq_push(&pq, parent);
    }
    return pq.size > 0 ? pq_pop(&pq) : NULL;
}

//this is to store the huffman tree
void store_tree(HuffmanNode *root, FILE *output) {
    if (!root) return;
    
    if (!root->left && !root->right) {
        if (fputc(1, output) == EOF || fputc(root->symbol, output) == EOF) {
            fprintf(stderr, "Error writing tree\n");
            exit(1);
        }
    } else {
        if (fputc(0, output) == EOF) {
            fprintf(stderr, "Error writing tree\n");
            exit(1);
        }
        store_tree(root->left, output);
        store_tree(root->right, output);
    }
}

HuffmanNode* load_tree(FILE *input) {
    int flag = fgetc(input);
    if (flag == EOF) {
        fprintf(stderr, "Error reading tree\n");
        exit(1);
    }
    
    if (flag == 1) {
        int symbol = fgetc(input);
        if (symbol == EOF) {
            fprintf(stderr, "Error reading tree\n");
            exit(1);
        }
        HuffmanNode *node = malloc(sizeof(HuffmanNode));
        if (!node) {
            fprintf(stderr, "Memory allocation failed\n");
            exit(1);
        }
        node->symbol = (uint8_t)symbol;
        node->left = node->right = NULL;
        return node;
    } else {
        HuffmanNode *node = malloc(sizeof(HuffmanNode));
        if (!node) {
            fprintf(stderr, "Memory allocation failed\n");
            exit(1);
        }
        node->left = load_tree(input);
        node->right = load_tree(input);
        return node;
    }
}

void build_huffman_codes(HuffmanNode *root, HuffmanCode codes[], uint8_t *bitstring, int depth) {
    if (!root) return;
    
    if (!root->left && !root->right) {
        codes[root->symbol].length = depth;
        memcpy(codes[root->symbol].code, bitstring, (depth + 7) / 8);
        return;
    }
    
    if (root->left) {
        bitstring[depth / 8] &= ~(1 << (7 - depth % 8));
        build_huffman_codes(root->left, codes, bitstring, depth + 1);
    }
    
    if (root->right) {
        bitstring[depth / 8] |= (1 << (7 - depth % 8));
        build_huffman_codes(root->right, codes, bitstring, depth + 1);
    }
}

// ----------------- Bit Buffer -----------------
void bitbuffer_init(BitBuffer* bb) {
    bb->buffer = 0;
    bb->bit_pos = 0;
}

void bitbuffer_flush(BitBuffer* bb, FILE* output) {
    if (bb->bit_pos > 0) {
        if (fputc(bb->buffer, output) == EOF) {
            fprintf(stderr, "Error writing output\n");
            exit(1);
        }
        bitbuffer_init(bb);
    }
}

void bitbuffer_put(BitBuffer* bb, uint8_t bit, FILE* output) {
    bb->buffer = (bb->buffer << 1) | (bit & 1);
    bb->bit_pos++;
    
    if (bb->bit_pos == 8) {
        bitbuffer_flush(bb, output);
    }
}

// ----------------- Compression -----------------
void huffman_compress(uint8_t *input, size_t size, FILE *output) {
    HuffmanNode *root = build_huffman_tree(input, size);
    HuffmanCode codes[256] = {0};
    uint8_t bitstring[32] = {0};
    build_huffman_codes(root, codes, bitstring, 0);

    // Store tree and original size
    if (fwrite(&size, sizeof(size_t), 1, output) != 1) {
        fprintf(stderr, "Error writing size\n");
        exit(1);
    }
    store_tree(root, output);

    // Compress data
    BitBuffer bb;
    bitbuffer_init(&bb);
    
    for (size_t i = 0; i < size; i++) {
        HuffmanCode code = codes[input[i]];
        for (int j = 0; j < code.length; j++) {
            uint8_t bit = (code.code[j / 8] >> (7 - (j % 8))) & 1;
            bitbuffer_put(&bb, bit, output);
        }
    }
    bitbuffer_flush(&bb, output); // Flush any remaining bits
}

// ----------------- Decompression -----------------
void huffman_decompress(FILE *input, uint8_t *output, size_t size) {
    HuffmanNode *root = load_tree(input);
    if (!root) {
        fprintf(stderr, "Empty tree during decompression\n");
        exit(1);
    }

    HuffmanNode *current = root;
    uint8_t byte;
    int bit_pos = 0;
    size_t output_pos = 0;

    while (output_pos < size) {
        if (bit_pos == 0) {
            int c = fgetc(input);
            if (c == EOF) {
                fprintf(stderr, "Unexpected end of compressed data\n");
                exit(1);
            }
            byte = (uint8_t)c;
            bit_pos = 8;
        }

        uint8_t bit = (byte >> 7) & 1;
        byte <<= 1;
        bit_pos--;

        current = bit ? current->right : current->left;
        
        if (!current) {
            fprintf(stderr, "Invalid compressed data\n");
            exit(1);
        }

        if (!current->left && !current->right) {
            if (output_pos >= size) {
                fprintf(stderr, "Output buffer overflow\n");
                exit(1);
            }
            output[output_pos++] = current->symbol;
            current = root;
        }
    }
}

// ----------------- MAIN -----------------
int main() {
    const char* input_filename = "gatsby.txt";
    const char* compressed_filename = "compressed.bin";
    const char* decompressed_filename = "decompressed.txt";

    // Compression
    size_t text_size;
    uint8_t *text = read_file(input_filename, &text_size);

    FILE *compressed = fopen(compressed_filename, "wb");
    if (!compressed) {
        fprintf(stderr, "Error creating output file\n");
        exit(1);
    }

    huffman_compress(text, text_size, compressed);
    fclose(compressed);
    printf("Compression successful.\n");
    free(text);

    // Decompression
    FILE *comp_input = fopen(compressed_filename, "rb");
    if (!comp_input) {
        fprintf(stderr, "Error opening compressed file\n");
        exit(1);
    }

    size_t decompressed_size;
    if (fread(&decompressed_size, sizeof(size_t), 1, comp_input) != 1) {
        fprintf(stderr, "Error reading size\n");
        exit(1);
    }

    uint8_t *decompressed = malloc(decompressed_size);
    if (!decompressed) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    huffman_decompress(comp_input, decompressed, decompressed_size);
    fclose(comp_input);

    write_file(decompressed_filename, decompressed, decompressed_size);
    free(decompressed);

    printf("Decompression successful.\n");
    return 0;
}