//
// Created by ljc on 2023/10/11.
//

#ifndef TEARCODE_LRU_H
#define TEARCODE_LRU_H

/*
 * LRU定义
 * 初始化接收int参数 为缓存大小
 * put 放入缓存
 * get获取缓存
 *
 * 目标：操作实现O(1)时间复杂度
 * */

# include <unordered_map>
# include <list>

using std::unordered_map;

struct Node{
    int key;
    int value;
    Node* next = nullptr;
    Node* prev = nullptr;
    Node(int key, int value):key(key), value(value){}
};

class LRU{
public:
    LRU(int size):max_size(size){}
    ~LRU();

    void put(int key, int value);
    int get(int key);

private:
    void update(int key);
    // 用于查询O(1)
    unordered_map<int, Node*> my_map;
    // 其顺序就是最近使用的，用于替换
    Node* my_root;
    Node* my_tail;
    int my_size{0};
    int max_size;
};
#endif //TEARCODE_LRU_H
