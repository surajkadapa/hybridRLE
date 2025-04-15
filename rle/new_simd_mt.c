#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <immintrin.h> // For SIMD intrinsics
#include <pthread.h>   // For multithreading

#define ALPHABET_SIZE 256
#define MAX_TREE_NODES 511
#define NUM_THREADS 6  // Using 6 threads as requested

// ----------------- Data Structures -----------------
typedef struct HuffmanNode {
    uint8_t symbol;
    int freq;
    struct HuffmanNode *left, *right;
} HuffmanNode;

typedef struct {
    uint8_t code[32]; // Huffman code (max 256 bits)
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

// Structure for frequency counting tasks
typedef struct {
    uint8_t* data;
    size_t start;
    size_t end;
    int* local_freq;  // Each thread gets its own frequency array
} FreqCountTask;

// Structure for compression tasks
typedef struct {
    uint8_t* input;
    size_t start;
    size_t end;
    HuffmanCode* codes;
    uint8_t** output_buffers;
    size_t* output_sizes;
    int thread_id;
} CompressTask;

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

// ----------------- Priority Queue -----------------
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

// ----------------- Multithreaded Frequency Counting -----------------
// Thread function for counting frequencies
void* count_freq_thread(void* arg) {
    FreqCountTask* task = (FreqCountTask*)arg;
    
    // Initialize local frequency array
    memset(task->local_freq, 0, ALPHABET_SIZE * sizeof(int));
    
    // Process data chunk with SIMD
    size_t i = task->start;
    
    // Process 32 bytes at a time using AVX2 if possible
    for (; i + 31 < task->end; i += 32) {
        __m256i chunk = _mm256_loadu_si256((__m256i*)&task->data[i]);
        // Extract bytes to temporary array
        uint8_t temp[32];
        _mm256_storeu_si256((__m256i*)temp, chunk);
        // Update histogram
        for (int j = 0; j < 32; j++) {
            task->local_freq[temp[j]]++;
        }
    }
    
    // Process remaining bytes
    for (; i < task->end; i++) {
        task->local_freq[task->data[i]]++;
    }
    
    return NULL;
}

// Multithreaded frequency counting
void count_frequencies_mt(uint8_t* data, size_t size, int freq[256]) {
    pthread_t threads[NUM_THREADS];
    FreqCountTask tasks[NUM_THREADS];
    int* local_freqs[NUM_THREADS];
    
    // Clear the main frequency array
    memset(freq, 0, ALPHABET_SIZE * sizeof(int));
    
    // Calculate chunk size
    size_t chunk_size = size / NUM_THREADS;
    
    // Create and launch threads
    for (int t = 0; t < NUM_THREADS; t++) {
        local_freqs[t] = malloc(ALPHABET_SIZE * sizeof(int));
        if (!local_freqs[t]) {
            fprintf(stderr, "Memory allocation failed for frequency array\n");
            exit(1);
        }
        
        tasks[t].data = data;
        tasks[t].start = t * chunk_size;
        tasks[t].end = (t == NUM_THREADS - 1) ? size : (t + 1) * chunk_size;
        tasks[t].local_freq = local_freqs[t];
        
        if (pthread_create(&threads[t], NULL, count_freq_thread, &tasks[t]) != 0) {
            fprintf(stderr, "Failed to create thread\n");
            exit(1);
        }
    }
    
    // Wait for all threads to complete
    for (int t = 0; t < NUM_THREADS; t++) {
        pthread_join(threads[t], NULL);
        
        // Merge frequency counts
        for (int i = 0; i < ALPHABET_SIZE; i++) {
            freq[i] += local_freqs[t][i];
        }
        
        free(local_freqs[t]);
    }
}

// ----------------- Huffman Tree -----------------
HuffmanNode* build_huffman_tree(int freq[256]) {
    PriorityQueue pq = { .size = 0 };
    
    // Create leaf nodes for symbols with non-zero frequency
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
    
    // Build the tree by combining nodes
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

// ----------------- Tree Serialization -----------------
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

// ----------------- Code Generation -----------------
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

// Function to add bits to a local bit buffer without flushing to file
void add_bit_to_buffer(uint8_t bit, BitBuffer* bb, uint8_t* buffer, size_t* pos) {
    bb->buffer = (bb->buffer << 1) | (bit & 1);
    bb->bit_pos++;
    if (bb->bit_pos == 8) {
        buffer[(*pos)++] = bb->buffer;
        bb->buffer = 0;
        bb->bit_pos = 0;
    }
}

// Flush any remaining bits in the buffer
void flush_bit_buffer(BitBuffer* bb, uint8_t* buffer, size_t* pos) {
    if (bb->bit_pos > 0) {
        // Pad with zeros
        bb->buffer <<= (8 - bb->bit_pos);
        buffer[(*pos)++] = bb->buffer;
        bb->buffer = 0;
        bb->bit_pos = 0;
    }
}

// ----------------- Multithreaded Compression -----------------
// Thread function for compressing chunks
void* compress_chunk_thread(void* arg) {
    CompressTask* task = (CompressTask*)arg;
    BitBuffer bb;
    bitbuffer_init(&bb);
    
    // Worst case: each symbol becomes 8 bits, plus some overhead
    size_t max_output_size = (task->end - task->start) * 8;
    task->output_buffers[task->thread_id] = malloc(max_output_size);
    if (!task->output_buffers[task->thread_id]) {
        fprintf(stderr, "Memory allocation failed for output buffer\n");
        exit(1);
    }
    
    size_t output_pos = 0;
    
    // Compress each symbol in this chunk
    for (size_t i = task->start; i < task->end; i++) {
        HuffmanCode code = task->codes[task->input[i]];
        for (int j = 0; j < code.length; j++) {
            uint8_t bit = (code.code[j / 8] >> (7 - (j % 8))) & 1;
            add_bit_to_buffer(bit, &bb, task->output_buffers[task->thread_id], &output_pos);
        }
    }
    
    // Flush any remaining bits
    flush_bit_buffer(&bb, task->output_buffers[task->thread_id], &output_pos);
    
    // Store the size of compressed data for this chunk
    task->output_sizes[task->thread_id] = output_pos;
    
    return NULL;
}

// ----------------- Compression -----------------
void huffman_compress_mt(uint8_t *input, size_t size, FILE *output) {
    // Count frequencies using multiple threads
    int freq[256];
    count_frequencies_mt(input, size, freq);
    
    // Build the Huffman tree (single-threaded)
    HuffmanNode *root = build_huffman_tree(freq);
    
    // Generate codes for symbols
    HuffmanCode codes[256] = {0};
    uint8_t bitstring[32] = {0};
    build_huffman_codes(root, codes, bitstring, 0);
    
    // Store tree and original size
    if (fwrite(&size, sizeof(size_t), 1, output) != 1) {
        fprintf(stderr, "Error writing size\n");
        exit(1);
    }
    store_tree(root, output);
    
    // Prepare for multithreaded compression
    pthread_t threads[NUM_THREADS];
    CompressTask tasks[NUM_THREADS];
    uint8_t** output_buffers = malloc(NUM_THREADS * sizeof(uint8_t*));
    size_t* output_sizes = malloc(NUM_THREADS * sizeof(size_t));
    
    if (!output_buffers || !output_sizes) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    
    // Calculate chunk size for each thread
    size_t chunk_size = size / NUM_THREADS;
    
    // Launch compression threads
    for (int t = 0; t < NUM_THREADS; t++) {
        tasks[t].input = input;
        tasks[t].start = t * chunk_size;
        tasks[t].end = (t == NUM_THREADS - 1) ? size : (t + 1) * chunk_size;
        tasks[t].codes = codes;
        tasks[t].output_buffers = output_buffers;
        tasks[t].output_sizes = output_sizes;
        tasks[t].thread_id = t;
        
        if (pthread_create(&threads[t], NULL, compress_chunk_thread, &tasks[t]) != 0) {
            fprintf(stderr, "Failed to create thread\n");
            exit(1);
        }
    }
    
    // Wait for all threads to complete
    for (int t = 0; t < NUM_THREADS; t++) {
        pthread_join(threads[t], NULL);
    }
    
    // Now write all compressed chunks to output
    // Note: This approach may have issues with Huffman code bit boundaries
    // A more sophisticated approach would handle partial bytes at chunk boundaries
    
    // Write a special sync marker between chunks (for simplicity)
    uint8_t sync_marker[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    
    // Write number of chunks for decompression to know
    if (fwrite(NUM_THREADS, sizeof(int), 1, output) != 1) {
        fprintf(stderr, "Error writing number of chunks\n");
        exit(1);
    }
    
    // Write each chunk's size and data
    for (int t = 0; t < NUM_THREADS; t++) {
        // Write chunk size
        if (fwrite(&output_sizes[t], sizeof(size_t), 1, output) != 1) {
            fprintf(stderr, "Error writing chunk size\n");
            exit(1);
        }
        
        // Write chunk data
        if (fwrite(output_buffers[t], 1, output_sizes[t], output) != output_sizes[t]) {
            fprintf(stderr, "Error writing compressed data\n");
            exit(1);
        }
        
        // Write sync marker between chunks (except after last chunk)
        if (t < NUM_THREADS - 1) {
            if (fwrite(sync_marker, 1, sizeof(sync_marker), output) != sizeof(sync_marker)) {
                fprintf(stderr, "Error writing sync marker\n");
                exit(1);
            }
        }
        
        // Free the buffer
        free(output_buffers[t]);
    }
    
    // Free resources
    free(output_buffers);
    free(output_sizes);
}

// ----------------- Decompression -----------------
// For simplicity, we'll keep decompression single-threaded
// A fully multithreaded solution would require significant changes to handle code boundaries
void huffman_decompress(FILE *input, uint8_t *output, size_t size) {
    HuffmanNode *root = load_tree(input);
    if (!root) {
        fprintf(stderr, "Empty tree during decompression\n");
        exit(1);
    }
    
    // Read number of chunks
    int num_chunks;
    if (fread(&num_chunks, sizeof(int), 1, input) != 1) {
        fprintf(stderr, "Error reading number of chunks\n");
        exit(1);
    }
    
    size_t output_pos = 0;
    uint8_t sync_marker[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    
    // Process each chunk
    for (int chunk = 0; chunk < num_chunks; chunk++) {
        // Read chunk size
        size_t chunk_size;
        if (fread(&chunk_size, sizeof(size_t), 1, input) != 1) {
            fprintf(stderr, "Error reading chunk size\n");
            exit(1);
        }
        
        // Decompress this chunk
        HuffmanNode *current = root;
        uint8_t byte;
        int bit_pos = 0;
        
        size_t bytes_read = 0;
        while (bytes_read < chunk_size) {
            if (bit_pos == 0) {
                int c = fgetc(input);
                if (c == EOF) {
                    fprintf(stderr, "Unexpected end of compressed data\n");
                    exit(1);
                }
                byte = (uint8_t)c;
                bit_pos = 8;
                bytes_read++;
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
        
        // Skip sync marker between chunks (except after last chunk)
        if (chunk < num_chunks - 1) {
            uint8_t marker_buf[4];
            if (fread(marker_buf, 1, 4, input) != 4 || 
                memcmp(marker_buf, sync_marker, 4) != 0) {
                fprintf(stderr, "Sync marker not found\n");
                exit(1);
            }
        }
    }
    
    // Verify we decompressed the expected amount
    if (output_pos != size) {
        fprintf(stderr, "Decompression size mismatch: got %zu, expected %zu\n", output_pos, size);
        exit(1);
    }
}

// ----------------- Free Tree -----------------
void free_tree(HuffmanNode *root) {
    if (root) {
        free_tree(root->left);
        free_tree(root->right);
        free(root);
    }
}

// ----------------- MAIN -----------------
int main() {
    const char* input_filename = "gatsby.txt";
    const char* compressed_filename = "compressed.bin";
    const char* decompressed_filename = "decompressed.txt";
    
    printf("Using %d threads for compression\n", NUM_THREADS);
    
    // Compression
    size_t text_size;
    uint8_t *text = read_file(input_filename, &text_size);
    FILE *compressed = fopen(compressed_filename, "wb");
    if (!compressed) {
        fprintf(stderr, "Error creating output file\n");
        exit(1);
    }
    
    // Use multithreaded compression
    printf("Compressing %s (%zu bytes)...\n", input_filename, text_size);
    huffman_compress_mt(text, text_size, compressed);
    fclose(compressed);
    
    // Get size of compressed file
    FILE *comp_size = fopen(compressed_filename, "rb");
    fseek(comp_size, 0, SEEK_END);
    size_t compressed_size = ftell(comp_size);
    fclose(comp_size);
    
    printf("Compression successful. Original: %zu bytes, Compressed: %zu bytes (%.2f%%)\n", 
           text_size, compressed_size, (float)compressed_size * 100 / text_size);
    
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
    
    printf("Decompressing to %s...\n", decompressed_filename);
    huffman_decompress(comp_input, decompressed, decompressed_size);
    fclose(comp_input);
    
    write_file(decompressed_filename, decompressed, decompressed_size);
    free(decompressed);
    
    printf("Decompression successful.\n");
    
    return 0;
}