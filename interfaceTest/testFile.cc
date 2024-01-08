#include <unistd.h>
#include <iostream>
#include <string>

using namespace std;

int main() {
    FILE* fp = fopen("./data/data.dat", "a");
    if (fp == nullptr) {
        cout << "PATH not EXIST\n";
        return 0;
    }
    // fwrite(fd, "hello\n", 6);
    // fwrite("hello\n", 6, fp);
    fputs("hello\n", fp);
    fclose(fp);
    return 0;
}