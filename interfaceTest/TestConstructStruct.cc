#include <iostream>
#include <vector>

using namespace std;

struct Object {
    int a;
    int b;
    // Object() {
    //     cout << "default\n";
    // }
    // Object(const Object& rhs) {
    //     cout << "copy\n";
    //     a = rhs.a;
    //     b = rhs.b;
    // }
    // Object(Object&& rhs) {
    //     cout << "move\n";
    //     a = rhs.a;
    //     b = rhs.b;
    // }
};

int main() {
    vector<Object> vec;
    vec.push_back({1, 2});
    return 0;
}