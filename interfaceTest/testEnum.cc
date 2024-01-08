#include <iostream>
#include <string>


using namespace std;

class Object {
public:
    enum level { DEBUG, INFO, WARN, ERROR };
};

int main() {
    cout << (Object::DEBUG < Object::ERROR) << endl;
    cout << (Object::INFO < 2) << endl;
    cout << (Object::INFO == 1) << endl;
    return 0;
}