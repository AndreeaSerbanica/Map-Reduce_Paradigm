#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <algorithm>
#include <cctype>

using namespace std;


struct MapperThread {
    vector<string> file_paths;
    int start_index;
    int end_index;
    unordered_map<string, unordered_map<int, int>> word_counts;   // Fiecare cuvant are index file si de cat ori apare
};

int min(int a, int b) {
    return a < b ? a : b;
}

MapperThread init_MapperThread(vector<string> file_paths, int id, int num_mappers) {
    MapperThread mt;
    mt.file_paths = file_paths;
    mt.start_index = id * (double)file_paths.size() / num_mappers;
    mt.end_index = min((id + 1) * (double)file_paths.size() / num_mappers, file_paths.size());
    return mt;
}

void print_MapperThread(MapperThread mt) {
    cout << "MapperThread: " << endl;
    cout << "Start index: " << mt.start_index << endl;
    cout << "End index: " << mt.end_index << endl;
    cout << "File paths: " << endl;
    for (auto &file_path : mt.file_paths) {
        cout << file_path << endl;
    }
}

void print_word_counts(unordered_map<string, unordered_map<int, int>> word_counts) {
    for (auto &word_count : word_counts) {
        cout << word_count.first << ": ";
        for (auto &file_count : word_count.second) {
            cout << "{" << file_count.first << ": " << file_count.second << " times}" << endl;
        }
    }
}

mutex mtx;

// Function to read and print a file
void read_and_print_file(const string &file_path) {
    ifstream file("../checker/" + file_path);
    if (!file.is_open()) {
        cerr << "Error opening file: " << file_path << endl;
        return;
    }

    string line;
    while (getline(file, line)) {
        cout << line << endl;
    }

    file.close();
}

// Function to convert string to lowercase
void to_lowercase(string &str) {
    transform(str.begin(), str.end(), str.begin(), ::tolower);
}

// Function to check if a word is valid
bool is_valid_word(const string &word) {
    return all_of(word.begin(), word.end(), [](char c) {
        return isalnum(c);
    });
}

void *mapper(void *arg) {
    // Cast the argument back to MapperThread pointer
    MapperThread *mt = (MapperThread *)arg;

    // Iterate over the range of files assigned to this mapper
    for (int i = mt->start_index; i < mt->end_index; ++i) {
        string file_path = "../checker/" + mt->file_paths[i];


        ifstream file(file_path);
        if (!file.is_open()) {
            cerr << "Error opening file: " << mt->file_paths[i] << endl;
            continue;
        }

        string line, word;
        int file_id = i + 1; // Assign file ID based on index
        while (getline(file, line)) {
            istringstream iss(line);
            while (iss >> word) {
                // Normalize word to lowercase
                to_lowercase(word);

                // Check if the word is valid
                if (is_valid_word(word)) {
                    // Increment the word count for this file
                    mt->word_counts[word][file_id]++;
                }
            }
        }
        file.close();
    }

    return nullptr;
}


int main(int argc, char **argv) {

    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <file_name> <file_name> <file_name>" << endl;
        return 1;
    }

    // Open the initial file
    string path = "../checker/" + string(argv[3]);
    ifstream list_file(path);
    if (!list_file.is_open()) {
        cerr << "Error opening file: " << path << endl;
        return 1;
    }

    // Read the number of files
    int num_files;
    list_file >> num_files;
    list_file.ignore(numeric_limits<streamsize>::max(), '\n');

    // Read and process the file paths
    vector<string> file_paths;
    string file_path;
    for (int i = 0; i < num_files; ++i) {
        if (getline(list_file, file_path)) {
            file_path.erase(file_path.find_last_not_of("\n\r") + 1); // Remove newline character
            file_paths.push_back(file_path);
            // read_and_print_file(file_path);
        }
    }
    list_file.close();

    int num_mappers = stoi(argv[1]);
    pthread_t threads_mapper[num_mappers];

    vector<MapperThread> mapper_threads_data(num_mappers);

    int num_reducers = stoi(argv[2]);


    // cout << "Number of mappers: " << num_mappers << endl;
    for (int i = 0; i < num_mappers; ++i) {
        // cout << "Creating mapper thread: " << i << endl;
        mapper_threads_data[i] = init_MapperThread(file_paths, i, num_mappers);
        pthread_create(&threads_mapper[i], NULL, mapper, &mapper_threads_data[i]);
    }

    for (int i = 0; i < num_mappers; ++i) {
        pthread_join(threads_mapper[i], NULL);
    }

    cout << "Mapper threads finished" << endl;

    // print the word counts
    for (int i = 0; i < num_mappers; ++i) {
        print_word_counts(mapper_threads_data[i].word_counts);
    }


    return 0;
}
