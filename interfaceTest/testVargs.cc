#include <iostream>
#include <cstdarg>

using namespace std;

void func1(int, ...);
void func2(int, ...);

void func1(int n, ...) {
    cout << "in func1\n";
    va_list args;
    va_start(args, n);
    func2(n, args);
    va_end(args);
}

void func2(int n, ...) {
    cout << "in func2\n";
    va_list args;
    va_start(args, n);
    for (int i = 0; i < n; i++) {
        int x = va_arg(args, int);
        cout << x << endl;
    }
    va_end(args);
}

int main() {
    func1(3, 2, 42, 66);
    return 0;
}