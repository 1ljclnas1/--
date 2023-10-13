//
// Created by ljc on 2023/10/11.
//

// ========= 寻找平衡数 =========

// 左边等于右边，前缀和

#include "balance_number.h"

using namespace std;

pair<int, int> find_balance_num(vector<int> arr){
    auto sum = [](vector<int>::iterator begin, vector<int>::iterator end){
        int res{0};
        while(begin< end){
            res+=*begin;
            begin++;
        }
        return res;
    };
    int all = sum(arr.begin(), arr.end());
    for(int i = 1; i < arr.size();i++){
        if (all - arr[i-1] - arr[i] == arr[i-1]){ return {i, arr[i]}; }
        arr[i] += arr[i-1];
    }
    return {-1, -1};
}