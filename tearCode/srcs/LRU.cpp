//
// Created by ljc on 2023/10/11.
//

// =========== LRU 实现 ===========

#include "LRU.h"
#include <iostream>

using namespace std;


LRU::~LRU() {
    delete my_root;
    delete my_tail;
    my_root = nullptr;
    my_tail = nullptr;

    for(auto i:my_map){
        delete i.second;
        i.second = nullptr;
    }
}

void LRU::update(int key) {
    // 如果是尾节点就更新尾节点
    if(my_tail==my_map[key] && my_tail->prev){
        my_tail = my_tail->prev;
        my_tail->next = nullptr;
    }
    //更新list如果不是首节点，则更新
    if(my_map[key]->prev){
        my_map[key]->prev->next = my_map[key]->next;
        if(my_map[key]->next) my_map[key]->next->prev = my_map[key]->prev;
        my_map[key]->next = my_root;
        my_root->prev = my_map[key];
        my_root = my_map[key];
        my_map[key]->prev = nullptr;
    }
}
void LRU::put(int key, int value) {

    // 如果已经在缓存里面了，就更新
    if(my_map.count(key) > 0){
        my_map[key]->value = value;
        update(key);
    }
    //如果不在缓存里，就直接放进去,再检查是不是超过max_size，超过就删除尾节点
    else{
        Node* cur = new Node(key, value);
        // size为0，特殊处理
        if(my_size==0){
            my_root = cur;
            my_tail = cur;
            my_map[key] = cur;
            my_size++;
            return;
        }

        cur->next = my_root;
        // 更新list
        my_root->prev = cur;
        my_root = cur;

        // 更新map
        my_map[key] = cur;
        my_size++;
        // 如果数量超过上限了就把最后一个删掉
        if(my_size > max_size){
            my_map.erase(my_tail->key);
            my_tail->prev->next = nullptr;
            my_tail = my_tail->prev;
            my_size--;
        }
        return;
    }
}

int LRU::get(int key) {
    // 如果不存在返回-1
    if(my_map.count(key)==0) return -1;
    // 如果存在就更新

    update(key);
    return my_map[key]->value;
}