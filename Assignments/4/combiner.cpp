#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <pthread.h>
#include <string.h>
#include <vector>
#include <regex>
#include <iostream>

#define MAX_LINE_LEN 32

using namespace std;

typedef struct {
    int consumer_id, depth, idx;
    pthread_mutex_t lock;
    pthread_cond_t full, empty;
    char (*buffer)[MAX_LINE_LEN];  // Pointer to shared memory buffer
    int buffer_count;
} reducer_t;

void thread_buffer_push(reducer_t *red, const string &buf) {
    pthread_mutex_lock(&red->lock);

    // Wait if the buffer is full
    while (red->buffer_count == red->depth) {
        pthread_cond_wait(&red->full, &red->lock);
    }

    // Push new element into the buffer and increment the buffer counter
    strncpy(red->buffer[red->buffer_count], buf.c_str(), MAX_LINE_LEN - 1);
    red->buffer[red->buffer_count][MAX_LINE_LEN - 1] = '\0';
    red->buffer_count++;

    pthread_mutex_unlock(&red->lock);

    // Signal that the buffer is no longer empty
    pthread_cond_signal(&red->empty);
}

void thread_buffer_pop(reducer_t *red, string &buf) {
    pthread_mutex_lock(&red->lock);

    // Wait if the buffer is empty
    while (red->buffer_count == 0) {
        pthread_cond_wait(&red->empty, &red->lock);
    }

    // Retrieve and remove the first element in the buffer and decrement the buffer counter
    buf = red->buffer[0];
    for (int i = 1; i < red->buffer_count; i++) {
        strcpy(red->buffer[i - 1], red->buffer[i]);
    }
    red->buffer_count--;

    pthread_mutex_unlock(&red->lock);

    // Signal that the buffer is no longer full
    pthread_cond_signal(&red->full);
}

void printUserdata(const string &id, const vector<pair<string, int>> &data) {
    for (const auto &entry : data) {
        printf("(%04d,%s,%d)\n", stoi(id), entry.first.c_str(), entry.second);
    }
}

static void *reducer(void *args) {
    string buf;
    reducer_t *red = (reducer_t *)args;
    vector<pair<string, int>> userdata;
    regex pattern(R"(\((\d{4}),([a-zA-Z]+),(-?\d+)\))");
    smatch matches;

    while (true) {
        buf.clear();
        thread_buffer_pop(red, buf);

        if (buf == "\n") break;

        string id, topic;
        int score;

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
    int num_processes = atoi(argv[2]);

    // do the mmap. Doing it this way allows us to map the reducers and the buffers all together, which is convienient for munmap later
    size_t reducer_size = num_processes * sizeof(reducer_t);
    size_t buffer_size = num_processes * buffer_depth * MAX_LINE_LEN * sizeof(char);
    size_t total_size = reducer_size + buffer_size;
    reducer_t *reducers = (reducer_t *)mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (reducers == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_processes; i++) {
        // setup the shared process attributes
        pthread_mutexattr_t mattr;
        pthread_condattr_t cattr;
        pthread_mutexattr_init(&mattr);
        pthread_condattr_init(&cattr);
        pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
        pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);

        // initialize the mutex and conds as process shared
        pthread_mutex_init(&reducers[i].lock, &mattr);
        pthread_cond_init(&reducers[i].full, &cattr);
        pthread_cond_init(&reducers[i].empty, &cattr);

        // initialize each reducer thread, and give the buffer it's own memory region
        reducers[i].consumer_id = -1;
        reducers[i].depth = buffer_depth;
        reducers[i].idx = 0;
        reducers[i].buffer_count = 0;
        reducers[i].buffer = (char (*)[MAX_LINE_LEN])((char *)(reducers + num_processes) + (i * buffer_depth * MAX_LINE_LEN));
    }

    for (int i = 0; i < num_processes; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            reducer(&reducers[i]);
            exit(0);
        }
    }

    // do mapper in main function
    string buf, id, topic;
    int score;

    while (1) {
        getline(cin, buf);

        // If out of data, send the end signal to all consumers
        if (buf.empty()) {
            for (int i = 0; i < num_processes; i++) {
                thread_buffer_push(&reducers[i], "\n");
            }
            break;
        }

        // Generate the string to send to the reducer
        id = buf.substr(1, 4);
        topic = buf.substr(8);
        topic.pop_back();

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
                continue;
        }

        buf.clear();
        string mapper_output = "(" + id + "," + topic + "," + to_string(score) + ")";

        // Find the reducer thread to send data to based on user id        
        int reducer_idx = 0;
        while (reducer_idx < num_processes) {
            if (reducers[reducer_idx].consumer_id == atoi(id.c_str())) {
                break;
            }
            reducer_idx++;
        }

        // Make a new consumer id if necessary
        if (reducer_idx == num_processes) {
            for (int i = 0; i < num_processes; i++) {
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

    // call wait for each process
    for (int i = 0; i < num_processes; i++) {
        wait(NULL);
    }

    // clean up
    for (int i = 0; i < num_processes; i++) {
        pthread_mutex_destroy(&reducers[i].lock);
        pthread_cond_destroy(&reducers[i].full);
        pthread_cond_destroy(&reducers[i].empty);
    }
    munmap(reducers, total_size);

    return 0;
}

