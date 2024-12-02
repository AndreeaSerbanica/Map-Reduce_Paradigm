#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <algorithm>
#include <cctype>
#include <unordered_set>

using namespace std;

pthread_mutex_t mutex;

struct MapperThread {
    vector<string> file_paths;
    int start_index;
    int end_index;
    unordered_map<string, unordered_map<int, int>> word_counts;   // Fiecare cuvant are index file si de cat ori apare
};

struct ReducerThread {
    vector<pair<string, unordered_set<int>>> word_file_map; // Cuvânt -> Set de ID-uri de fișiere
    int start_index;
    int end_index;
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

ReducerThread init_ReducerThread(int id, int num_reducers, const vector<MapperThread> &mapper_threads) {
    ReducerThread rt;
    rt.start_index = id * (double)26 / num_reducers;
    rt.end_index = min((id + 1) * (double)26 / num_reducers, 26);

    for (int i = 0; i < mapper_threads.size(); i++) {
        for (const auto &entry : mapper_threads[i].word_counts) {
            const string &word = entry.first;
            const unordered_map<int, int> &file_counts = entry.second;
            if (rt.start_index <= word[0] - 'a' && word[0] - 'a' < rt.end_index) {
                rt.word_file_map.push_back({word, unordered_set<int>()});
                for (const auto &file_count : file_counts) {
                    rt.word_file_map.back().second.insert(file_count.first);
                }
            }
        }
    }

    return rt;
}

void print_word_counts(const unordered_map<string, unordered_map<int, int>> &word_counts) {
    // Extract keys (words) from the map
    vector<string> words;
    for (const auto &entry : word_counts) {
        words.push_back(entry.first);
    }

    // Sort the keys alphabetically
    sort(words.begin(), words.end());

    // Print words and their counts in alphabetical order
    for (const auto &word : words) {
        cout << word << ": ";
        for (const auto &file_count : word_counts.at(word)) {
            cout << "{" << file_count.first << ": " << file_count.second << " times} ";
        }
        cout << endl;
    }
}




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
string remove_punctuation(const string &word) {
    string result;
    for (char c : word) {
        if (isalpha(c)) {
            result += c;
        }
    }
    return result;
}

// Function to check if a word is valid after removing punctuation
bool is_valid_word(string word) {
    return !word.empty() && all_of(word.begin(), word.end(), [](char c) {
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
                word = remove_punctuation(word);
                if (is_valid_word(word)) {
                    mt->word_counts[word][file_id]++;
                }
            }
        }
        file.close();
    }

    return nullptr;
}

bool mySort(const pair<string, pair<unordered_set<int>, int>> &a,
            const pair<string, pair<unordered_set<int>, int>> &b) {
    // Sort by number of appearances (descending)
    if (a.second.second != b.second.second) {
        return a.second.second > b.second.second;
    }
    // Then alphabetically (ascending)
    return a.first < b.first;
}


void *reducer(void *arg) {
    // Cast the argument back to ReducerThread pointer
    ReducerThread *rt = (ReducerThread *)arg;

    // Iterate over the range assigned to this reducer
    for (int i = rt->start_index; i < rt->end_index; ++i) {
        char letter = 'a' + i; // Corresponding letter for this index
        string file_name = string(1, letter) + ".txt";

        ofstream output_file(file_name);
        if (!output_file.is_open()) {
            cerr << "Error opening file for writing: " << file_name << endl;
            continue;
        }

        // Map to store aggregated file IDs for each word
        unordered_map<string, unordered_set<int>> aggregated_word_map;

        // Merge all file IDs for words starting with the current letter
        for (const auto &entry : rt->word_file_map) {
            const string &word = entry.first;
            const unordered_set<int> &file_ids = entry.second;

            if (tolower(word[0]) == letter) {
                aggregated_word_map[word].insert(file_ids.begin(), file_ids.end());
            }
        }

        // Convert map to vector for sorting
        vector<pair<string, pair<unordered_set<int>, int>>> word_list;
        for (const auto &entry : aggregated_word_map) {
            word_list.push_back({entry.first, {entry.second, static_cast<int>(entry.second.size())}});
        }

        // Sort using the mySort function
        sort(word_list.begin(), word_list.end(), mySort);

        // Write the sorted results to the file
        for (const auto &entry : word_list) {
            const string &word = entry.first;
            const unordered_set<int> &file_ids = entry.second.first;

            output_file << word << ":[";
            
            // Convert set to sorted vector for consistent ordering
            vector<int> sorted_file_ids(file_ids.begin(), file_ids.end());
            sort(sorted_file_ids.begin(), sorted_file_ids.end());

            for (size_t i = 0; i < sorted_file_ids.size(); ++i) {
                if (i > 0) {
                    output_file << " ";
                }
                output_file << sorted_file_ids[i];
            }
            output_file << "]" << endl;
        }

        output_file.close();
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


    for (int i = 0; i < num_mappers; ++i) {
        mapper_threads_data[i] = init_MapperThread(file_paths, i, num_mappers);
        pthread_create(&threads_mapper[i], NULL, mapper, &mapper_threads_data[i]);
    }

    for (int i = 0; i < num_mappers; ++i) {
        pthread_join(threads_mapper[i], NULL);
    }



    //Reducer Part
    int num_reducers = stoi(argv[2]);
    pthread_t threads_reducer[num_reducers];
    vector<ReducerThread> reducer_threads_data(num_reducers);

    for (int i = 0; i < num_reducers; ++i) {
        reducer_threads_data[i] = init_ReducerThread(i, num_reducers, mapper_threads_data);
        pthread_create(&threads_reducer[i], NULL, reducer, &reducer_threads_data[i]);
    }

    for (int i = 0; i < num_mappers; ++i) {
        pthread_join(threads_reducer[i], NULL);
    }

    // for (int i = 0; i < num_mappers; ++i) {
    //     print_word_counts(mapper_threads_data[i].word_counts);
    // }

    return 0;
}
