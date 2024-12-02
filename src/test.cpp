#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <thread>
#include <algorithm>
#include <cctype>
#include <sstream>

#define NUM_MAPPERS 2
#define NUM_REDUCERS 1

std::mutex mutex;

// Normalize a word (convert to lowercase and remove punctuation)
std::string normalize_word(const std::string& word) {
    std::string normalized;
    for (char ch : word) {
        if (std::isalpha(ch)) {
            normalized += std::tolower(ch);
        }
    }
    return normalized;
}

// Mapper function
void mapper(const std::vector<std::string>& data, int start, int end, std::map<std::string, int>& local_results) {
    for (int i = start; i < end; ++i) {
        std::istringstream stream(data[i]);
        std::string word;
        while (stream >> word) {
            word = normalize_word(word);
            if (!word.empty()) {
                local_results[word]++;
            }
        }
    }
}

// Reducer function
void reducer(const std::vector<std::map<std::string, int>>& intermediate_results, std::map<std::string, int>& final_results) {
    for (const auto& local_map : intermediate_results) {
        for (const auto& [word, count] : local_map) {
            std::lock_guard<std::mutex> lock(mutex);
            final_results[word] += count;
        }
    }
}

int main() {
    // Simulated input data
    std::vector<std::string> data = {
        "The quick brown fox jumps over the lazy dog",
        "The quick blue fox runs under the lazy cat",
        "The blue sky is clear and the sun is bright",
        "Bright sun and blue sky make a happy day"
    };

    int num_data = data.size();
    int chunk_size = (num_data + NUM_MAPPERS - 1) / NUM_MAPPERS;

    // Intermediate and final results
    std::vector<std::map<std::string, int>> intermediate_results(NUM_MAPPERS);
    std::map<std::string, int> final_results;

    // Mapper threads
    std::vector<std::thread> mapper_threads;
    for (int i = 0; i < NUM_MAPPERS; ++i) {
        int start = i * chunk_size;
        int end = std::min(start + chunk_size, num_data);
        mapper_threads.emplace_back(mapper, std::ref(data), start, end, std::ref(intermediate_results[i]));
    }

    // Join mapper threads
    for (auto& thread : mapper_threads) {
        thread.join();
    }

    // Reducer threads
    std::vector<std::thread> reducer_threads;
    for (int i = 0; i < NUM_REDUCERS; ++i) {
        reducer_threads.emplace_back(reducer, std::ref(intermediate_results), std::ref(final_results));
    }

    // Join reducer threads
    for (auto& thread : reducer_threads) {
        thread.join();
    }

    // Display final results
    for (const auto& [word, count] : final_results) {
        std::cout << word << ": " << count << std::endl;
    }

    return 0;
}
