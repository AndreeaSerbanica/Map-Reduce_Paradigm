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
#include <unordered_map>

using namespace std;

// Shared data between threads
struct SharedData {
    pthread_mutex_t counter_mutex;
    int completed_mappers;
};

// Mapper thread data
struct MapperThread {
    vector<string> file_paths;
    int start_index;
    int end_index;
    unordered_map<string, unordered_map<int, int>> word_counts; 
    SharedData *shared_data;
};

// Reducer thread data
struct ReducerThread {
    vector<pair<string, unordered_set<int>>> word_file_map; // Cuvânt -> Set de ID-uri de fișiere
    int start_index;
    int end_index;
};


int min(int a, int b) {
    return a < b ? a : b;
}

MapperThread init_MapperThread(vector<string> file_paths, int id, int num_mappers, SharedData *shared_data) {
    MapperThread mt;
    mt.file_paths = file_paths;
    mt.start_index = id * (double)file_paths.size() / num_mappers;
    mt.end_index = min((id + 1) * (double)file_paths.size() / num_mappers, file_paths.size());
    mt.shared_data = shared_data;
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

// Convert string to lowercase
void to_lowercase(string &str) {
    transform(str.begin(), str.end(), str.begin(), ::tolower);
}

// Remove punctuation from a word
string remove_punctuation(const string &word) {
    string result;
    for (char c : word) {
        if (isalpha(c)) {
            result += c;
        }
    }
    return result;
}

// Check if a word is valid after removing punctuation
bool is_valid_word(string word) {
    return !word.empty() && all_of(word.begin(), word.end(), [](char c) {
        return isalnum(c);
    });
}

// Mapper function
void *mapper(void *arg) {
    MapperThread *mt = (MapperThread *)arg;

    for (int i = mt->start_index; i < mt->end_index; ++i) {
        string file_path = "../checker/" + mt->file_paths[i];


        ifstream file(file_path);
        if (!file.is_open()) {
            cerr << "Error opening file: " << mt->file_paths[i] << endl;
            continue;
        }

        string line, word;
        int file_id = i + 1;
        while (getline(file, line)) {
            istringstream iss(line);
            while (iss >> word) {
                to_lowercase(word);

                word = remove_punctuation(word);
                if (is_valid_word(word)) {
                    mt->word_counts[word][file_id]++;
                }
            }
        }
        file.close();
    }
    

    // Mutex lock to update the counter
    pthread_mutex_lock(&mt->shared_data->counter_mutex);
    mt->shared_data->completed_mappers++;
    pthread_mutex_unlock(&mt->shared_data->counter_mutex);


    return nullptr;
}

bool mySort(const pair<string, pair<unordered_set<int>, int>> &a,
            const pair<string, pair<unordered_set<int>, int>> &b) {
    // Appearances
    if (a.second.second != b.second.second) {
        return a.second.second > b.second.second;
    }
    // Alphabetically
    return a.first < b.first;
}

// Reducer function
void *reducer(void *arg) {
    ReducerThread *rt = (ReducerThread *)arg;

    for (int i = rt->start_index; i < rt->end_index; ++i) {
        char letter = 'a' + i;
        string file_name = string(1, letter) + ".txt";

        ofstream output_file(file_name);
        if (!output_file.is_open()) {
            cerr << "Error opening file for writing: " << file_name << endl;
            continue;
        }

        unordered_map<string, unordered_set<int>> aggregated_word_map;

        for (const auto &entry : rt->word_file_map) {
            const string &word = entry.first;
            const unordered_set<int> &file_ids = entry.second;

            if (tolower(word[0]) == letter) {
                aggregated_word_map[word].insert(file_ids.begin(), file_ids.end());
            }
        }

        vector<pair<string, pair<unordered_set<int>, int>>> word_list;
        for (const auto &entry : aggregated_word_map) {
            word_list.push_back({entry.first, {entry.second, static_cast<int>(entry.second.size())}});
        }

        sort(word_list.begin(), word_list.end(), mySort);

        for (const auto &entry : word_list) {
            const string &word = entry.first;
            const unordered_set<int> &file_ids = entry.second.first;

            output_file << word << ":[";
            
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
        }
    }
    list_file.close();

    int num_mappers = stoi(argv[1]);
    pthread_t threads_mapper[num_mappers];
    vector<MapperThread> mapper_threads_data(num_mappers);


    //Reducer Part
    int num_reducers = stoi(argv[2]);
    pthread_t threads_reducer[num_reducers];
    vector<ReducerThread> reducer_threads_data(num_reducers);
    

    int total_threads = num_mappers + num_reducers;
    pthread_t threads[total_threads];

    // Initialize shared data
    SharedData shared_data;
    pthread_mutex_init(&shared_data.counter_mutex, NULL);
    shared_data.completed_mappers = 0;


    for (int i = 0; i < num_mappers; ++i) {
        mapper_threads_data[i] = init_MapperThread(file_paths, i, num_mappers, &shared_data);
        pthread_create(&threads[i], NULL, mapper, &mapper_threads_data[i]);
    }

    // Let all the mapper threads finish their execution
    while (true) {
        pthread_mutex_lock(&shared_data.counter_mutex);
        if (shared_data.completed_mappers == num_mappers) {
            pthread_mutex_unlock(&shared_data.counter_mutex);
            break;
        }
        pthread_mutex_unlock(&shared_data.counter_mutex);
    }

    for (int i = 0; i < num_reducers; ++i) {
        reducer_threads_data[i] = init_ReducerThread(i, num_reducers, mapper_threads_data);
        pthread_create(&threads[i + num_mappers], NULL, reducer, &reducer_threads_data[i]);
    }

    for (int i = 0; i < total_threads; ++i) {
        pthread_join(threads[i], NULL);
    }

    // Clean up mutex
    pthread_mutex_destroy(&shared_data.counter_mutex);
    return 0;
}