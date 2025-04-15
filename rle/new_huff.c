#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ALPHABET_SIZE 256

// ----------------- Read File into Memory -----------------
uint8_t* read_file(const char *filename, size_t *size) {
    FILE *file = fopen(filename, "rb");
    if (!file) { printf("Error opening file: %s\n", filename); exit(1); }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    rewind(file);

    uint8_t *data = malloc(*size);
    if (!data) { printf("Memory allocation failed\n"); exit(1); }

    fread(data, 1, *size, file);
    fclose(file);
    return data;
}

// ----------------- Write File -----------------
void write_file(const char *filename, uint8_t *data, size_t size) {
    FILE *file = fopen(filename, "wb");
    if (!file) { printf("Error writing file: %s\n", filename); exit(1); }

    fwrite(data, 1, size, file);
    fclose(file);
}

// ----------------- Huffman Compression -----------------
typedef struct HuffmanNode {
    uint8_t symbol;
    int freq;
    struct HuffmanNode *left, *right;
} HuffmanNode;

typedef struct {
    uint8_t code[32];  // Huffman code
    int length;
} HuffmanCode;

#define MAX_TREE_NODES 511  // Huffman tree nodes limit

// ----------------- Priority Queue -----------------
typedef struct {
    HuffmanNode *nodes[MAX_TREE_NODES];
    int size;
} PriorityQueue;

// ----------------- Priority Queue Functions -----------------
void pq_push(PriorityQueue *pq, HuffmanNode *node) {
    int i = pq->size++;
    while (i > 0 && node->freq < pq->nodes[(i - 1) / 2]->freq) {
        pq->nodes[i] = pq->nodes[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    pq->nodes[i] = node;
}

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

// ----------------- Build Huffman Tree -----------------
HuffmanNode* build_huffman_tree(uint8_t *data, size_t size) {
    int freq[256] = {0};
    for (size_t i = 0; i < size; i++) freq[data[i]]++;

    PriorityQueue pq = { .size = 0 };
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            HuffmanNode *node = malloc(sizeof(HuffmanNode));
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
        parent->symbol = 0;
        parent->freq = left->freq + right->freq;
        parent->left = left;
        parent->right = right;
        pq_push(&pq, parent);
    }
    return pq_pop(&pq);
}

// ----------------- Store & Reconstruct Huffman Tree -----------------
void store_tree(HuffmanNode *root, FILE *output) {
    if (!root) return;
    if (!root->left && !root->right) {
        fputc(1, output);
        fputc(root->symbol, output);
    } else {
        fputc(0, output);
    }
    store_tree(root->left, output);
    store_tree(root->right, output);
}

HuffmanNode* load_tree(FILE *input) {
    int flag = fgetc(input);
    if (flag == 1) {
        HuffmanNode *node = malloc(sizeof(HuffmanNode));
        node->symbol = fgetc(input);
        node->left = node->right = NULL;
        return node;
    }
    HuffmanNode *node = malloc(sizeof(HuffmanNode));
    node->left = load_tree(input);
    node->right = load_tree(input);
    return node;
}

// ----------------- Build Huffman Encoding Table -----------------
void build_huffman_codes(HuffmanNode *root, HuffmanCode codes[], uint8_t *bitstring, int depth) {
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

// ----------------- Huffman Compression -----------------
void huffman_compress(uint8_t *input, size_t size, FILE *output) {
    HuffmanNode *root = build_huffman_tree(input, size);
    HuffmanCode codes[256] = {0};
    uint8_t bitstring[32] = {0};
    build_huffman_codes(root, codes, bitstring, 0);

    store_tree(root, output);  // Save tree

    uint8_t buffer = 0, bit_count = 0;
    for (size_t i = 0; i < size; i++) {
        HuffmanCode code = codes[input[i]];
        for (int j = 0; j < code.length; j++) {
            buffer <<= 1;
            buffer |= (code.code[j / 8] >> (7 - (j % 8))) & 1;
            bit_count++;
            if (bit_count == 8) {
                fwrite(&buffer, 1, 1, output);
                bit_count = 0;
            }
        }
    }
    if (bit_count) {
        buffer <<= (8 - bit_count);
        fwrite(&buffer, 1, 1, output);
    }
}

// ----------------- Huffman Decompression -----------------
void huffman_decompress(FILE *input, uint8_t *output, size_t size) {
    HuffmanNode *root = load_tree(input);  // Load Huffman tree
    HuffmanNode *current = root;

    uint8_t buffer;
    int bit_count = 0;
    size_t i = 0;

    while (i < size) {
        if (bit_count == 0) {
            fread(&buffer, 1, 1, input);
            bit_count = 8;
        }

        for (int j = 7; j >= 0 && i < size; j--) {
            current = (buffer & (1 << j)) ? current->right : current->left;

            if (!current->left && !current->right) {  // Found a symbol
                output[i++] = current->symbol;
                current = root;  // Reset to root for next symbol
            }
        }

        bit_count = 0;  // Reset bit count after processing the byte
    }
}

// ----------------- MAIN -----------------
int main() {
    size_t text_size;
    uint8_t *text = read_file("gatsby.txt", &text_size);

    // Compress directly with Huffman
    FILE *compressed = fopen("compressed_huffman_gatsby.bin", "wb");
    fwrite(&text_size, sizeof(size_t), 1, compressed);  // Original size
    huffman_compress(text, text_size, compressed);
    fclose(compressed);

    printf("Compression successful.\n");

    // ----------------- Decompression -----------------
    FILE *comp_input = fopen("compressed_huffman_gatsby.bin", "rb");
    fread(&text_size, sizeof(size_t), 1, comp_input);

    // Decompress Huffman data directly
    uint8_t *final_output = malloc(text_size);
    huffman_decompress(comp_input, final_output, text_size);
    fclose(comp_input);

    write_file("decompressed_gatsby_final.txt", final_output, text_size);
    free(final_output);
    free(text);

    printf("Decompression successful.\n");
    return 0;
}