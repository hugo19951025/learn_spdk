#include "RBTree.hpp"
#include <iostream>

void testRBTree() {
    RBTree<int> tree;
    
    // 测试插入
    std::cout << "插入元素: 11->7, 9->3, 7->18, 6->10, 41->22, 23->8, 89->11, 1->26" << std::endl;
    std::vector<int> keys = {11, 9, 7, 6, 41, 23, 89, 1};
    std::vector<int> values = {7, 3, 18, 10, 22, 8, 11, 26};
    int i = 0;
    for (int val : values) {
        tree.insert(keys[i], val);
        i++;
    }
    
    // 中序遍历
    std::cout << "中序遍历: ";
    tree.inOrder();
    
    // 层序遍历
    std::cout << "层序遍历:" << std::endl;
    tree.levelOrder();
    
    // 验证红黑树性质
    std::cout << "\n验证红黑树性质: " 
              << (tree.verifyRBProperties() ? "通过" : "失败") << std::endl;
    
    // 测试查找
    std::cout << "\n查找元素:" << std::endl;
    std::cout << "查找 9: " << tree.get(9)  << std::endl;
    std::cout << "查找 7: " << tree.get(7) << std::endl;
    
    // 测试最值
    std::cout << "\n最小key: " << tree.min() << std::endl;
    std::cout << "最大key: " << tree.max() << std::endl;
    
    // 测试删除
    std::cout << "\n删除元素 23" << std::endl;
    tree.remove(23);
    std::cout << "中序遍历: ";
    tree.inOrder();
    

    // 测试修改
    std::cout << "\n修改元素 1->26 为 1->21" << std::endl;
    tree.modify(1, 21);
    std::cout << "中序遍历: ";
    tree.inOrder();

    // 验证删除后的性质
    std::cout << "\n验证红黑树性质: " 
              << (tree.verifyRBProperties() ? "通过" : "失败") << std::endl;
}

int main() {
    testRBTree();
    return 0;
}