#include <cstring>
#include <iostream>

using namespace std;


void TestNprintf() {
    char buf[8] = {0};
    snprintf(buf, 8, "hello g");
    cout << buf << endl;
    cout << strlen(buf) << endl;
}

int main() {
    TestNprintf();

    return 0;
}