#include <iostream>
#include <memory>
#include <string>
#include <ctime>

using namespace std;


int main() {
    time_t curtime = time(nullptr);
    cout << "1900 年到目前的秒数: " << curtime << endl;
    cout << "本地日期和时间: " << ctime(&curtime) << endl;
    tm* nowtime = localtime(&curtime);
    cout << "年: " << 1900 + nowtime->tm_year << endl;
    cout << "月: " << nowtime->tm_mon + 1 << endl;
    cout << "日: " << nowtime->tm_mday << endl;
    cout << "时: " << nowtime->tm_hour << endl;
    cout << "分: " << nowtime->tm_min << endl;
    cout << "秒: " << nowtime->tm_sec << endl;

    return 0;
}
