#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_FILES 100
#define MAX_WORD_LENGTH 50
#define MAX_BUFFER_SIZE 256
#define NUM_THREADS 4

typedef struct {
    char *file_path;
    int word_counts[MAX_FILES];
    char **words;
    int total_words;
} MapperOutput;

MapperOutput mapperOutputs[MAX_FILES];
int mapperCount = 0;
pthread_mutex_t lock;

void read_and_print_file(char *file_path) {
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "../checker/%s", file_path);
    FILE *file = fopen(full_path, "r");
    if (!file) {
        perror("Error opening file");
        return;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), file)) {
        printf("%s", buffer);
    }

    fclose(file);
}
// Function to convert string to lowercase
void to_lowercase(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower(str[i]);
    }
}

// Function to check if a word is valid
bool is_valid_word(char *word) {
    for (int i = 0; word[i]; i++) {
        if (!isalnum(word[i])) return false;
    }
    return true;
}

// Map function (processes a single file)
void *mapper(void *arg) {
    char *file_path = (char *)arg;
    FILE *file = fopen(file_path, "r");
    if (!file) {
        perror("Error opening file");
        return NULL;
    }

    MapperOutput output;
    output.file_path = file_path;
    output.total_words = 0;
    output.words = (char **)malloc(sizeof(char *) * MAX_BUFFER_SIZE);

    char buffer[MAX_BUFFER_SIZE];
    while (fgets(buffer, sizeof(buffer), file)) {
        char *token = strtok(buffer, " \t\n");
        while (token) {
            to_lowercase(token);
            if (is_valid_word(token)) {
                output.words[output.total_words] = strdup(token);
                output.total_words++;
            }
            token = strtok(NULL, " \t\n");
        }
    }
    fclose(file);

    pthread_mutex_lock(&lock);
    mapperOutputs[mapperCount++] = output;
    pthread_mutex_unlock(&lock);

    return NULL;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <file_name> <file_name> <file_name>\n", argv[0]);
        return 1;
    }

    

    // Open the inital file
    char path[256];
    snprintf(path, sizeof(path), "../checker/%s", argv[3]);
    printf("Reading file: %s\n", path);
    FILE *list_file = fopen(path, "r");
    if (!list_file) {
        perror("Error opening file");
        return 1;
    }

    // Read the files from test_in
    int num_files;
    fscanf(list_file, "%d\n", &num_files);

    // Read and print the file content
    char file_path[256];
    for (int i = 0; i < num_files; i++) {
        if (fgets(file_path, sizeof(file_path), list_file)) {
            // Remove newline character from the file path
            file_path[strcspn(file_path, "\n")] = '\0';
            // printf("Reading file: %s\n", file_path);
            read_and_print_file(file_path);
        }
    }
    // Close the file
    fclose(list_file);


    return 0;
}
