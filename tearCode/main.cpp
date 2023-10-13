#include <iostream>
#include "LRU.h"
#include "balance_number.h"
using namespace std;

int main() {
    cout << "======= tearCode ========" << endl;

    cout << "======= LRU Test ========" << endl;
    LRU test(2);
    test.put(1, 1);
    test.put(2, 2);
    cout << test.get(1) << endl;
    test.put(3, 3);
    cout << test.get(2) << endl;
    test.put(4, 4);
    cout << test.get(1) << endl;
    cout << test.get(3) << endl;
    cout << test.get(4) << endl;

    cout << "======== balance number Test =========" << endl;
    vector<int> arr{1,3,2,4,3,3};
    auto res = find_balance_num(arr);
    cout << " index: " << res.first << " number: " <<  res.second << endl;
    return 0;
}
