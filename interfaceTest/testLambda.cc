#include <iostream>
#include <functional>

using namespace std;

void Test01() {
    int val = 4;
    char ch = 'a';
    [lam_val = val, lam_ch = ch] {
        cout << lam_val << endl;
    }();
    function<void()> f = [] { cout << "Hello, world!\n"; };
    f();
}

template <typename T>
class Object {
public:
    Object(T val): val(val) {}
    void print() {
        [this] {
            cout << val << endl;
        }();
    }
private:
    T val;
};

void Test02() {
    Object<int> o(1);
    o.print();
}


int main() {
    Test02();
    return 0;
}