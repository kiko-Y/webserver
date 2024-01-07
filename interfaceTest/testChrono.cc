#include <chrono>
#include <iostream>

using namespace std;

using Clock = std::chrono::high_resolution_clock;
using TimeStamp = Clock::time_point;

long fibnacci(int n) {
    if (n < 2) return n;
    return fibnacci(n - 1) + fibnacci(n - 2);
}

void TestCalcCost() {
    TimeStamp start = Clock::now();
    fibnacci(42);
    TimeStamp end = Clock::now();
    // std::chrono::duration<double> elapsed = end - start;
    auto elapsed = end - start;
    cout << elapsed.count() << "ns" << endl;
    cout << chrono::duration_cast<chrono::duration<double>>(elapsed).count() << "s" << endl;
    cout << chrono::duration_cast<chrono::milliseconds>(elapsed).count() << "ms" << endl;
}

int main() {
    TestCalcCost();

    return 0;
}