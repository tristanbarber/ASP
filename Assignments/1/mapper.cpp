// Author: Tristan Barber 

#include <stdio.h>

#include <iostream>
#include <string>

using namespace std;

int main() {
    string buf, id, topic;
    int score;

    while (getline(cin, buf)) {
        if (!buf.empty()) {
            id = buf.substr(1,4);
            topic = buf.substr(8);
            topic.pop_back(); // Remove the parenthesis

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
            printf("(%s,%s,%d)\n", id.c_str(), topic.c_str(), score);
        }
    }    
}
