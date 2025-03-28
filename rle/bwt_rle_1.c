#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <divsufsort.h>

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

// ----------------- Burrows-Wheeler Transform (BWT) -----------------
void bwt_transform(const uint8_t *input, uint8_t *bwt_out, int *orig_index, size_t size) {
    // Add sentinel (0x00) to input
    uint8_t *modified_input = malloc(size + 1);
    memcpy(modified_input, input, size);
    modified_input[size] = 0x00;

    // Compute suffix array
    int *suffix_array = malloc((size + 1) * sizeof(int));
    divsufsort(modified_input, suffix_array, size + 1);

    // Generate BWT output (size + 1 bytes)
    for (size_t i = 0; i <= size; i++) {
        int sa_entry = suffix_array[i];
        if (sa_entry == 0) *orig_index = i; // Sentinel position
        bwt_out[i] = modified_input[(sa_entry + size) % (size + 1)]; // Wrap correctly
    }

    free(suffix_array);
    free(modified_input);
}
// ----------------- Inverse BWT -----------------
// ----------------- Corrected Inverse BWT -----------------
void inverse_bwt(const uint8_t *bwt_data, uint8_t *output, int orig_index, size_t size) {
    const size_t bwt_size = size + 1; // Includes sentinel
    int count[ALPHABET_SIZE] = {0};
    int *rank = malloc(bwt_size * sizeof(int));

    // Compute frequency of each character
    for (size_t i = 0; i < bwt_size; i++) count[bwt_data[i]]++;

    // Compute cumulative counts (starting indices)
    int sum = 0;
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        int temp = count[i];
        count[i] = sum;
        sum += temp;
    }

    // Compute rank (number of occurrences up to current index)
    int curr_counts[ALPHABET_SIZE] = {0};
    for (size_t i = 0; i < bwt_size; i++) {
        uint8_t c = bwt_data[i];
        rank[i] = curr_counts[c]++;
    }

    // Reconstruct original data in reverse order
    int idx = orig_index;
    for (int i = size; i >= 0; i--) { // Iterate over original_size + 1
        output[i] = bwt_data[idx];
        uint8_t c = bwt_data[idx];
        idx = count[c] + rank[idx]; // LF mapping
    }

    free(rank);
}
// ----------------- Move-to-Front (MTF) Encoding -----------------
void mtf_encode(uint8_t *input, uint8_t *output, size_t size) {
    uint8_t alphabet[ALPHABET_SIZE];
    for (int i = 0; i < ALPHABET_SIZE; i++) alphabet[i] = i;

    for (size_t i = 0; i < size; i++) {
        uint8_t symbol = input[i];
        int index = 0;
        while (alphabet[index] != symbol) index++;

        output[i] = index;

        while (index > 0) {  // Move to front
            alphabet[index] = alphabet[index - 1];
            index--;
        }
        alphabet[0] = symbol;
    }
}

// ----------------- Move-to-Front (MTF) Decoding -----------------
void mtf_decode(uint8_t *input, uint8_t *output, size_t size) {
    uint8_t alphabet[ALPHABET_SIZE];
    for (int i = 0; i < ALPHABET_SIZE; i++) alphabet[i] = i;

    for (size_t i = 0; i < size; i++) {
        uint8_t index = input[i];
        uint8_t symbol = alphabet[index];

        output[i] = symbol;

        while (index > 0) {  // Move to front
            alphabet[index] = alphabet[index - 1];
            index--;
        }
        alphabet[0] = symbol;
    }
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

HuffmanNode *build_huffman_tree(uint8_t *data, size_t size);
void build_huffman_codes(HuffmanNode *root, HuffmanCode codes[], uint8_t *bitstring, int depth);
void huffman_compress(uint8_t *input, size_t size, FILE *output);
void huffman_decompress(FILE *input, uint8_t *output, size_t size);


#define MAX_TREE_NODES 511  // Huffman tree nodes limit

// ----------------- Huffman Node Definition -----------------

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
    uint8_t *text = read_file("frank.txt", &text_size);

    // BWT output includes sentinel (size + 1)
    uint8_t *bwt_data = malloc(text_size + 1);
    int orig_index;
    bwt_transform(text, bwt_data, &orig_index, text_size);

    // MTF encode BWT data (size + 1)
    uint8_t *mtf_data = malloc(text_size + 1);
    mtf_encode(bwt_data, mtf_data, text_size + 1);
    free(bwt_data);

    // Compress MTF data (size + 1)
    FILE *compressed = fopen("compressed_huffmann_frank.bin", "wb");
    fwrite(&text_size, sizeof(size_t), 1, compressed);  // Original size
    fwrite(&orig_index, sizeof(int), 1, compressed);    // BWT index
    huffman_compress(mtf_data, text_size + 1, compressed); // Include sentinel
    fclose(compressed);
    free(mtf_data);

    printf("Compression successful.\n");

    // ----------------- Decompression -----------------
  FILE *comp_input = fopen("compressed_huffmann_frank.bin", "rb");
    fread(&text_size, sizeof(size_t), 1, comp_input);
    fread(&orig_index, sizeof(int), 1, comp_input);

    // Decompress Huffman data (size + 1 bytes)
    uint8_t *huff_output = malloc(text_size + 1);
    huffman_decompress(comp_input, huff_output, text_size + 1);
    fclose(comp_input);

    // MTF decode (size + 1)
    uint8_t *mtf_decoded = malloc(text_size + 1);
    mtf_decode(huff_output, mtf_decoded, text_size + 1);
    free(huff_output);

    // Inverse BWT (output has size + 1, sentinel at the end)
    uint8_t *final_output = malloc(text_size + 1);
    inverse_bwt(mtf_decoded, final_output, orig_index, text_size);
    free(mtf_decoded);

    // Trim sentinel (write original_size bytes)
    write_file("decompressed_frank.txt", final_output, text_size);
    free(final_output);
    free(text);

    printf("Decompression successful.\n");
    return 0;
}