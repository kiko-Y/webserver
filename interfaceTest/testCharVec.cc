#include <vector>
#include <string>
#include <string_view>
#include <iostream>
#include <assert.h>

using namespace std;

void TestCharVector1() {
    vector<char> vec(4, '\0');
    vec[0] = 'a';
    vec[1] = 'b';
    char* ptr = &vec[0];
    ptr[2] = 'H';
    ptr[3] = 'D';
    cout << ptr << endl;
}

void TestCharVector2() {
    vector<char> vec;
    cout << (vec.data() == 0) << endl;
    vec.push_back('a');
    vec.push_back('b');
    assert(vec.data() == &*vec.begin());
}

int main() {
    // TestCharVector1();
    TestCharVector2();
    return 0;
}