// Author: Tristan Barber 

#include <stdio.h>

#include <iostream>
#include <string>
#include <map>
#include <regex>

using namespace std;

void printUserdata(const string &id, const vector<pair<string, int>> &data) {
    for (const auto &entry : data) {
        printf("(%s,%s,%d)\n", id.c_str(), entry.first.c_str(), entry.second);
    }
}

int main() {
    string buf;
    string prevID = "";
    vector<pair<string, int>> userdata;

    regex pattern(R"(\((\d{4}),([a-zA-Z]+),(\d+)\))");
    smatch matches;

    while (getline(cin, buf)) {
        string id, topic;
        int score;

        if (!buf.empty()) {
            if (regex_match(buf, matches, pattern)) {
                id = matches[1]; 
                topic = matches[2];
                score = stoi(matches[3]); 
            } else {
                perror("Incorrect format");
                continue;
            }

            // Output previous user's results if id changes
            if (!prevID.empty() && prevID != id) {
                printUserdata(prevID, userdata);
                userdata.clear();
            }

            // attempt to find the topic, and add to the score if possible
            bool found = false;
            for (auto &elem : userdata) {
                if (elem.first == topic) {
                    elem.second += score;
                    found = true;
                    break;
                }
            }

            // otherwise, add new topic
            if (!found) {
                userdata.push_back({topic, score});
            }

            prevID = id;
        }
    }

    // Print data for the last user
    if (!prevID.empty()) {
        printUserdata(prevID, userdata);
    }

    return 0;
}
