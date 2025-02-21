#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>

#include <fstream>
#include <iostream>
#include <string>
#include <map>
#include <regex>
#include <vector>
#include <sstream>

#define DEPTH 5
#define MAX_LINE_LEN 64

using namespace std;

typedef struct operator_t {
    int depth, idx;
    pthread_mutex_t lock;
    pthread_cond_t full, empty;
    vector<std::string> buffer;
} operator_t;

typedef struct account_t {
    int account_id, balance;
    sem_t sem;
} account_t;

// must be global to be accessed by worker funct
vector<account_t*> accounts;

// code borrowed from assignment 2, renamed reducers to operators
void thread_buffer_push(operator_t *op, const string &buf) {
    pthread_mutex_lock(&op->lock);

    // Wait if the buffer is full
    while (op->buffer.size() == op->depth) {
        pthread_cond_wait(&op->full, &op->lock);
    }

    // Push new element into the buffer
    op->buffer.push_back(buf);

    pthread_mutex_unlock(&op->lock);

    // Signal that the buffer is no longer empty
    pthread_cond_signal(&op->empty);
}

void thread_buffer_pop(operator_t *op, string &buf) {
    pthread_mutex_lock(&op->lock);

    // Wait if the buffer is empty
    while (op->buffer.empty()) {
        pthread_cond_wait(&op->empty, &op->lock);
    }

    // Retrieve and remove the first element in the buffer
    buf = op->buffer.front();
    op->buffer.erase(op->buffer.begin());

    pthread_mutex_unlock(&op->lock);

    // Signal that the buffer is no longer full
    pthread_cond_signal(&op->full);
}

void init_operators(operator_t *op, int depth) {
    pthread_mutex_init(&op->lock, NULL);
    pthread_cond_init(&op->full, NULL);
    pthread_cond_init(&op->empty, NULL);

    op->depth = depth;
    op->idx = 0;
    op->buffer.reserve(depth);
}

static void *worker(void *args) {
    string buf;
    operator_t *op = (operator_t *)args;

    regex pattern(R"(^\s*Transfer\s+(\d+)\s+(\d+)\s+(\d+)\s*$)");
    smatch matches;

    while (1) {
        buf = "";
        thread_buffer_pop(op, buf);
        int acc1, acc2, amount;

        if (buf == "\n") {
            break;
        }

        if (regex_match(buf, matches, pattern)) {
            acc1 = stoi(matches[1]);
            acc2 = stoi(matches[2]);
            amount = stoi(matches[3]); 
        } else {
            perror("Incorrect format");
            continue;
        }

        if (acc1 == acc2) {
            continue;
        }

        // obtain the smaller acc id sem first (breaking circularity)
        if (acc1 < acc2) {
            sem_wait(&accounts[acc1 - 1]->sem);
            sem_wait(&accounts[acc2 - 1]->sem);
        } else {
            sem_wait(&accounts[acc2 - 1]->sem);
            sem_wait(&accounts[acc1 - 1]->sem);
        }

        accounts[acc1 - 1]->balance -= amount;
        accounts[acc2 - 1]->balance += amount;

        // release in reverse order
        if (acc1 > acc2) {
            sem_post(&accounts[acc1 - 1]->sem);
            sem_post(&accounts[acc2 - 1]->sem);
        } else {
            sem_post(&accounts[acc2 - 1]->sem);
            sem_post(&accounts[acc1 - 1]->sem);
        }
    }

    pthread_exit(0);
}

int main(int argc, char **argv) {
    ifstream inputFile(argv[1]);
    int num_threads = atoi(argv[2]);
    string buf;

    // initialize threads
    pthread_t *threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    operator_t *operators = (operator_t *)malloc(num_threads * sizeof(operator_t));

    if (!threads || !operators) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    
    for (int i = 0; i < num_threads; i++) {
        init_operators(&operators[i], DEPTH);
        if (pthread_create(&threads[i], NULL, worker, &operators[i]) != 0) {
            perror("pthread_create failed");
            exit(EXIT_FAILURE);
        }
    }

    // now init accounts and send transfers to operators
    int i = 0;
    while (getline(inputFile, buf)) {
        if (buf.empty()) {
            continue;
        }

        if (buf[0] != 'T') {
            istringstream delimiter(buf);
            int acc_id, init_bal;
            delimiter >> acc_id >> init_bal;

            account_t *acc = (account_t *)malloc(sizeof(account_t));
            acc->account_id = acc_id;
            acc->balance = init_bal;
            sem_init(&acc->sem, 0, 1);

            accounts.push_back(acc);
        } else {
            thread_buffer_push(&operators[i], buf);
            i = (i + 1) % num_threads;
        }
    }

    // send termination char to all operators after all transfers have been sent
    for (int i = 0; i < num_threads; i++) {
        thread_buffer_push(&operators[i], "\n");
    }

    // Join threads to wait for operator completion
    for (int i = 0; i < num_threads; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("pthread_join failed");
            exit(EXIT_FAILURE);
        }

        // Clean up operator[i] after pthread_join returns because it's no longer used
        pthread_cond_destroy(&operators[i].full);
        pthread_cond_destroy(&operators[i].empty);
        pthread_mutex_destroy(&operators[i].lock);
    }

    // Finish cleanup
    free(threads);
    free(operators);
    for(int i = 0 ; i < accounts.size(); i++) {
        printf("%d %d\n", accounts[i]->account_id, accounts[i]->balance);
        free(accounts[i]);
    }

    exit(EXIT_SUCCESS);
}
