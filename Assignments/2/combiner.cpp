#include <stdio.h>
#include <pthread.h>
#include <string.h>

#include <iostream>
#include <string>
#include <map>
#include <regex>
#include <vector>

#define MAX_LINE_LEN 32

using namespace std;

typedef struct reducer_t {
    int consumer_id, depth, idx;
    pthread_mutex_t lock;
    pthread_cond_t full, empty;
    vector<string> buffer;
} reducer_t;

void thread_buffer_push(reducer_t *red, const string &buf) {
    pthread_mutex_lock(&red->lock);

    // Wait if the buffer is full
    while (red->buffer.size() == red->depth) {
        pthread_cond_wait(&red->full, &red->lock);
    }

    // Push new element into the buffer
    red->buffer.push_back(buf);

    pthread_mutex_unlock(&red->lock);

    // Signal that the buffer is no longer empty
    pthread_cond_signal(&red->empty);
}

void thread_buffer_pop(reducer_t *red, string &buf) {
    pthread_mutex_lock(&red->lock);

    // Wait if the buffer is empty
    while (red->buffer.empty()) {
        pthread_cond_wait(&red->empty, &red->lock);
    }

    // Retrieve and remove the first element in the buffer
    buf = red->buffer.front();
    red->buffer.erase(red->buffer.begin());

    pthread_mutex_unlock(&red->lock);

    // Signal that the buffer is no longer full
    pthread_cond_signal(&red->full);
}

void printUserdata(const string &id, const vector<pair<string, int>> &data) {
    for (const auto &entry : data) {
        printf("(%04d,%s,%d)\n", std::stoi(id), entry.first.c_str(), entry.second);
    }
}

static void *reducer(void *args) {
    string buf;
    reducer_t *red = (reducer_t *)args;
    vector<pair<string, int>> userdata;

    regex pattern(R"(\((\d{4}),([a-zA-Z]+),(-?\d+)\))");
    smatch matches;

    while (1) {
        buf = "";
        thread_buffer_pop(red, buf);
        string id, topic;
        int score;

        if (buf == "\n") {
            break;
        }

        if (regex_match(buf, matches, pattern)) {
            id = matches[1]; 
            topic = matches[2];
            score = stoi(matches[3]); 
        } else {
            perror("Incorrect format");
            continue;
        }

        // Attempt to find the topic and add to the score if possible
        bool found = false;
        for (auto &elem : userdata) {
            if (elem.first == topic) {
                elem.second += score;
                found = true;
                break;
            }
        }

        // Otherwise, add a new topic
        if (!found) {
            userdata.push_back({topic, score});
        }
    }

    printUserdata(to_string(red->consumer_id), userdata);
    pthread_exit(0);
}

int main(int argc, char **argv) {
    int buffer_depth = atoi(argv[1]);
    int num_threads = atoi(argv[2]);

    pthread_t *threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    reducer_t *reducers = (reducer_t *)malloc(num_threads * sizeof(reducer_t));

    if (!threads || !reducers) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize reducer structs and threads
    for (int i = 0; i < num_threads; i++) {
        pthread_mutex_init(&reducers[i].lock, NULL);
        pthread_cond_init(&reducers[i].full, NULL);
        pthread_cond_init(&reducers[i].empty, NULL);

        reducers[i].consumer_id = -1;
        reducers[i].depth = buffer_depth;
        reducers[i].idx = 0;
        reducers[i].buffer.reserve(buffer_depth);

        if (pthread_create(&threads[i], NULL, reducer, &reducers[i]) != 0) {
            perror("pthread_create failed");
            exit(EXIT_FAILURE);
        }
    }

    // Do mapper in main function
    string buf, id, topic;
    int score;

    while (1) {
        getline(cin, buf);

        // If out of data, send the end signal to all consumers
        if (buf.empty()) {
            for (int i = 0; i < num_threads; i++) {
                thread_buffer_push(&reducers[i], "\n");
            }
            break;
        }

        // Generate the string to send to the reducer
        id = buf.substr(1, 4);
        topic = buf.substr(8);
        topic.pop_back();  // Remove the parenthesis

        switch (buf[6]) {
            case 'P':
                score = 50;
                break;
            case 'L':
                score = 20;
                break;
            case 'D':
                score = -10;
                break;
            case 'C':
                score = 30;
                break;
            case 'S':
                score = 40;
                break;
            default:
                perror("Not an option");
                break;
        }

        buf = "";
        string mapper_output = "(" + id + "," + topic + "," + to_string(score) + ")";

        // Find the reducer thread to send data to based on user id
        int reducer_idx = 0;
        while (reducer_idx < num_threads) {
            if (reducers[reducer_idx].consumer_id == atoi(id.c_str())) {
                break;
            }
            reducer_idx++;
        }

        // Make a new consumer id if necessary
        if (reducer_idx == num_threads) {
            for (int i = 0; i < num_threads; i++) {
                if (reducers[i].consumer_id == -1) {
                    reducers[i].consumer_id = atoi(id.c_str());
                    reducer_idx = i;
                    break;
                }
            }
        }

        // Send data to reducer
        thread_buffer_push(&reducers[reducer_idx], mapper_output);
    }

    // Join threads to wait for reducer completion
    for (int i = 0; i < num_threads; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("pthread_join failed");
            exit(EXIT_FAILURE);
        }

        // Clean up reducer[i] after pthread_join returns because it's no longer used
        pthread_cond_destroy(&reducers[i].full);
        pthread_cond_destroy(&reducers[i].empty);
        pthread_mutex_destroy(&reducers[i].lock);
    }

    // Finish cleanup
    free(threads);
    free(reducers);

    exit(EXIT_SUCCESS);
}
