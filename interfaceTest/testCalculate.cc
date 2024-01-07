#include <iostream>

using namespace std;

class test1 {
public:
    int a;
    int b;
    // test1() {
    //     cout << "test1 default constructor" << endl;
    // }
};

struct test2 {
    int a;
    int b;
};

int main() {
    test1 a = {1, 2};
    test2 b = {1, 2};
    return 0;
}