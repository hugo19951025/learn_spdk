#ifndef RBTREE_HPP
#define RBTREE_HPP

#include <iostream>
#include <memory>
#include <functional>
#include <stdexcept>

#include <queue>
#include <stack>
#include <vector>
#include <initializer_list>


enum class Color {RED, BLACK};

template <typename T>
class RBTree {
private:
    struct Node {
        T key;
        T value;
        Color color;
        std::shared_ptr<Node> left;
        std::shared_ptr<Node> right;
        std::weak_ptr<Node> parent;

        explicit Node (const T& key, const T& value) : key(key), value(value), color(Color::RED), left(nullptr), right(nullptr) {}
    };

    std::shared_ptr<Node> root;
    std::shared_ptr<Node> nil; // 哨兵节点 空节点
    
    std::shared_ptr<Node> createNil(); // 创建哨兵节点
    void leftRotate(std::shared_ptr<Node> x); // 左旋
    void rightRotate(std::shared_ptr<Node> y); // 右旋
    void insertFixup(std::shared_ptr<Node> z); // 插入修复
    void deleteFixup(std::shared_ptr<Node> x); // 删除修复
    std::shared_ptr<Node> minimum(std::shared_ptr<Node> node); // 找最小节点
    std::shared_ptr<Node> maximum(std::shared_ptr<Node> node); // 找最大节点
    void transplant(std::shared_ptr<Node> u, std::shared_ptr<Node> v); // 移植替换节点
    std::shared_ptr<Node> searchNode(const T& key); // 查找节点
    void clear(std::shared_ptr<Node> node); // 递归删除子树

public:
    RBTree() {
        nil = createNil();
        root = nil;
    }

    ~RBTree() {
        clear(root);
    }

    void insert(const T& key, const T& value); // 插入元素
    void remove(const T& key); // 删除元素
    T get(const T& key); // 获得value
    void modify(const T& key, const T& value); // 修改元素

    T min(); // 找最小值
    T max(); // 找最大值

    void inOrder(); // 中序遍历
    void levelOrder(); // 层序遍历
    bool verifyRBProperties(); // 验证红黑树属性
    int height(); // 获取树高度
    
    // 判断树空
    bool empty() {
        return root == nil;
    } 

    // 清空整棵树所有节点 将树恢复为初始状态
    void cleanTree() {
        clear(root);
        root = nil;
    }
};

template <typename T>
std::shared_ptr<typename RBTree<T>::Node> RBTree<T>::createNil() {
    auto node = std::make_shared<Node>(T(), T());
    node->color = Color::BLACK;
    node->left = nullptr;    // 应该指向自身或nullptr
    node->right = nullptr;   // 应该指向自身或nullptr
    node->parent.reset();    // 显式重置parent
    return node;
}

template <typename T>
void RBTree<T>::leftRotate(std::shared_ptr<typename RBTree<T>::Node> x) {
    auto y = x->right;

    x->right = y->left;
    if (y->left != nil) {
        y->left->parent = x;
    }

    y->parent = x->parent;
    if (x->parent.lock() == nullptr) {
        root = y;
    } else if (x == x->parent.lock()->left) {
        x->parent.lock()->left = y;
    } else {
        x->parent.lock()->right = y;
    }

    y->left = x;
    x->parent = y;
}

template <typename T>
void RBTree<T>::rightRotate(std::shared_ptr<typename RBTree<T>::Node> y) {
    auto x = y->left;

    y->left = x->right;
    if (x->right != nil) {
        x->right->parent = y;
    }

    x->parent = y->parent;
    if (y->parent.lock() == nullptr) {
        root = x;
    } else if (y == y->parent.lock()->left) {
        y->parent.lock()->left = x;
    } else {
        y->parent.lock()->right = x;
    }

    x->right = y;
    y->parent = x;
}

template <typename T>
void RBTree<T>::insertFixup(std::shared_ptr<typename RBTree<T>::Node> z) {
    while (z->parent.lock() != nullptr && z->parent.lock()->color == Color::RED) {
        // 插入节点z的父节点是爷爷节点的左孩子
        if (z->parent.lock() == z->parent.lock()->parent.lock()->left) {
            // 叔叔节点是y
            auto y = z->parent.lock()->parent.lock()->right;

            if (y != nil && y->color == Color::RED) {
                // case1 叔叔节点是红色 
                // 不在乎z是左右孩子 只要求父亲这一行变黑 爷爷节点变红
                z->parent.lock()->color = Color::BLACK;
                y->color = Color::BLACK;
                z->parent.lock()->parent.lock()->color = Color::RED;
                z = z->parent.lock()->parent.lock();
            } else {
                // 叔叔节点是黑色
                // case2 z是右孩子 先左旋父节点 再右旋
                 if (z == z->parent.lock()->right) {
                    z = z->parent.lock();
                    leftRotate(z);
                 }
                 // case3 z是左孩子
                 z->parent.lock()->color = Color::BLACK;
                 z->parent.lock()->parent.lock()->color = Color::RED;
                 rightRotate(z->parent.lock()->parent.lock());
            }
        } else {
            // 叔叔节点是y
            auto y = z->parent.lock()->parent.lock()->left;

            if (y != nil && y->color == Color::RED) {
                // case1 叔叔节点是红色
                z->parent.lock()->color = Color::BLACK;
                y->color = Color::BLACK;
                z->parent.lock()->parent.lock()->color = Color::RED;
                z = z->parent.lock()->parent.lock();
            } else {
                // 叔叔节点是黑色
                // Case 2: z是左孩子 先右旋父节点 再左旋
                if (z == z->parent.lock()->left) {
                    z = z->parent.lock();
                    rightRotate(z);
                }
                // Case 3: z是右孩子
                z->parent.lock()->color = Color::BLACK;
                z->parent.lock()->parent.lock()->color = Color::RED;
                leftRotate(z->parent.lock()->parent.lock());
            }
        }
        if (z == root) break;
    }
    root->color = Color::BLACK;
}

template <typename T>
void RBTree<T>::insert(const T& key, const T& value) {
    auto existing = searchNode(key);
    if (existing != nullptr && existing != nil) {
        return;
    }

    auto z = std::make_shared<typename RBTree<T>::Node>(key, value);
    z->left = nil;
    z->right = nil;

    std::shared_ptr<Node> y = nullptr;
    auto x = root;

    while (x != nil) {
        y = x;
        if (z->key < x->key) {
            x = x->left;
        } else if (z->key > x->key) {
            x = x->right;
        } else {
            return; // exist
        }
    }

    z->parent = y;
    if (y == nullptr) {
        root = z;
    } else if (z->key < y->key) {
        y->left = z;
    } else {
        y->right = z;
    }

    z->left = nil;
    z->right = nil;
    if (z->parent.lock() == nullptr) {
        z->color = Color::BLACK;
        return;
    }

    if (z->parent.lock()->parent.lock() == nullptr) {
        // 没有爷爷节点 不能调整
        return;
    }

    insertFixup(z);
}

template <typename T>
std::shared_ptr<typename RBTree<T>::Node> RBTree<T>::minimum(std::shared_ptr<typename RBTree<T>::Node> node) {
    while (node->left != nil) {
        node = node->left;
    }
    return node;
}

template <typename T>
std::shared_ptr<typename RBTree<T>::Node> RBTree<T>::maximum(std::shared_ptr<typename RBTree<T>::Node> node) {
    while (node->right != nil) {
        node = node->right;
    }
    return node;
}

template <typename T>
void RBTree<T>::transplant(std::shared_ptr<typename RBTree<T>::Node> u, std::shared_ptr<typename RBTree<T>::Node> v) {
    if (u->parent.lock() == nullptr) {
        root = v;
    } else if (u == u->parent.lock()->left) {
        u->parent.lock()->left = v;
    } else {
        u->parent.lock()->right = v;
    }
    
    // 即使v是nil，也要设置其parent，但避免修改nil的共享状态
    if (v != nullptr) {
        v->parent = u->parent;
    }
}

template <typename T>
std::shared_ptr<typename RBTree<T>::Node> RBTree<T>::searchNode(const T& key) {
    auto current = root;
    while (current != nil) {
        if (key == current->key) {
            return current;
        } else if (key < current->key) {
            current = current->left;
        } else {
            current = current->right;
        }
    }
    return nullptr; 
}

template <typename T>
T RBTree<T>::get(const T& key) {
    auto node = searchNode(key);
    if (node == nullptr) {
        throw std::runtime_error("Key not found");
    }
    return node->value;
}

template <typename T>
void RBTree<T>::modify(const T& key, const T& value) {
    auto node = searchNode(key);
    if (node) {
        node->value = value;
    }
    return;
}

template <typename T>
void RBTree<T>::deleteFixup(std::shared_ptr<typename RBTree<T>::Node> x) {
    // x指向替代被删除节点的节点，可能具有"双重黑色"属性
    while (x != root && x->color == Color::BLACK) {
        if (x == x->parent.lock()->left) {
            // 情况1: x是其父节点的左孩子
            std::shared_ptr<Node> w = x->parent.lock()->right;  // x的兄弟节点
            
            // Case 1: 兄弟节点w是红色
            // 目标：转换为兄弟节点为黑色的情况
            if (w->color == Color::RED) {
                w->color = Color::BLACK;
                x->parent.lock()->color = Color::RED;
                leftRotate(x->parent.lock());
                w = x->parent.lock()->right;  // 重新设置w，现在w是黑色
            }
            
            // Case 2: 兄弟节点w是黑色，且w的两个子节点都是黑色
            // 目标：将x上移一层
            if (w->left->color == Color::BLACK && w->right->color == Color::BLACK) {
                w->color = Color::RED;
                x = x->parent.lock();
            } else {
                // Case 3: 兄弟节点w是黑色，w的右孩子是黑色，左孩子是红色
                // 目标：转换为Case 4
                if (w->right->color == Color::BLACK) {
                    w->left->color = Color::BLACK;
                    w->color = Color::RED;
                    rightRotate(w);
                    w = x->parent.lock()->right;
                }
                
                // Case 4: 兄弟节点w是黑色，w的右孩子是红色
                // 目标：通过旋转和重新着色修复红黑树性质
                w->color = x->parent.lock()->color;
                x->parent.lock()->color = Color::BLACK;
                w->right->color = Color::BLACK;
                leftRotate(x->parent.lock());
                x = root;  // 修复完成，退出循环
            }
        } else {
            // 对称情况：x是其父节点的右孩子
            std::shared_ptr<Node> w = x->parent.lock()->left;  // x的兄弟节点
            
            // Case 1: 兄弟节点w是红色
            if (w->color == Color::RED) {
                w->color = Color::BLACK;
                x->parent.lock()->color = Color::RED;
                rightRotate(x->parent.lock());
                w = x->parent.lock()->left;  // 重新设置w，现在w是黑色
            }
            
            // Case 2: 兄弟节点w是黑色，且w的两个子节点都是黑色
            if (w->right->color == Color::BLACK && w->left->color == Color::BLACK) {
                w->color = Color::RED;
                x = x->parent.lock();
            } else {
                // Case 3: 兄弟节点w是黑色，w的左孩子是黑色，右孩子是红色
                if (w->left->color == Color::BLACK) {
                    w->right->color = Color::BLACK;
                    w->color = Color::RED;
                    leftRotate(w);
                    w = x->parent.lock()->left;
                }
                
                // Case 4: 兄弟节点w是黑色，w的左孩子是红色
                w->color = x->parent.lock()->color;
                x->parent.lock()->color = Color::BLACK;
                w->left->color = Color::BLACK;
                rightRotate(x->parent.lock());
                x = root;  // 修复完成，退出循环
            }
        }
    }
    
    // 最终将x着色为黑色，确保性质1（节点是红色或黑色）和性质2（根节点是黑色）
    x->color = Color::BLACK;
}

template <typename T>
void RBTree<T>::remove(const T& key) {
    auto z = searchNode(key);
    if (z == nullptr || z == nil) return;

    auto y = z;
    auto y_original_color = y->color;
    std::shared_ptr<Node> x;

    if (z->left == nil) {
        x = z->right;
        transplant(z, z->right);
    } else if (z->right == nil) {
        x = z->left;
        transplant(z, z->left);
    } else {
        y = minimum(z->right);
        y_original_color = y->color;
        x = y->right;

        if (y->parent.lock() == z) {
            // 确保x的parent正确指向y
            x->parent = y;
        } else {
            transplant(y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }

        transplant(z, y);
        y->left = z->left;
        y->left->parent = y;
        y->color = z->color;
    }

    if (y_original_color == Color::BLACK) {
        deleteFixup(x);
    }
    
    // 确保nil的parent是weak_ptr默认状态，防止循环引用
    nil->parent.reset();
}

template <typename T>
void RBTree<T>::clear(std::shared_ptr<typename RBTree<T>::Node> node) {
    if (node != nil) {
        clear(node->left);
        clear(node->right);
    }
}

template <typename T>
T RBTree<T>::min() {
    if (root == nil) {
        throw std::runtime_error("Tree is empty");
    }
    return minimum(root)->key;
}

template <typename T>
T RBTree<T>::max() {
    if (root == nil) {
        throw std::runtime_error("Tree is empty");
    }
    return maximum(root)->key;
}

template <typename T>
void RBTree<T>::inOrder() {
    std::stack<std::shared_ptr<Node>> s;
    auto current = root;
    
    while (current != nil || !s.empty()) {
        while (current != nil) {
            s.push(current);
            current = current->left;
        }
        
        current = s.top();
        s.pop();
        
        std::cout << "key: " << current->key << " value: " << current->value << "(" << (current->color == Color::RED ? "R" : "B") << ")   ";
        current = current->right;
    }
    std::cout << std::endl;
}

template <typename T>
void RBTree<T>::levelOrder() {
    if (root == nil) return;
    
    std::queue<std::shared_ptr<Node>> q;
    q.push(root);
    
    while (!q.empty()) {
        int levelSize = q.size();
        
        for (int i = 0; i < levelSize; ++i) {
            auto node = q.front();
            q.pop();
            
            if (node != nil) {
                std::cout << "key: " << node->key << " value: " << node->value << "(" << (node->color == Color::RED ? "R" : "B") << ")   ";
                if (node->left != nil) q.push(node->left);
                if (node->right != nil) q.push(node->right);
            }
        }
        std::cout << std::endl;
    }
}

template <typename T>
bool RBTree<T>::verifyRBProperties() {
    if (root == nil) return true;
    
    // 性质2: 根节点必须是黑色
    if (root->color != Color::BLACK) {
        std::cout << "Violation: Root is not black" << std::endl;
        return false;
    }
    
    // 性质4: 红色节点的子节点必须是黑色
    std::function<bool(std::shared_ptr<Node>)> checkRedProperty;
    checkRedProperty = [&](std::shared_ptr<Node> node) -> bool {
        if (node == nil) return true;
        
        if (node->color == Color::RED) {
            if (node->left->color == Color::RED || node->right->color == Color::RED) {
                std::cout << "Violation: Red node has red child" << std::endl;
                return false;
            }
        }
        
        return checkRedProperty(node->left) && checkRedProperty(node->right);
    };
    
    if (!checkRedProperty(root)) return false;
    
    // 性质5: 从任一节点到其每个叶子的所有路径都包含相同数目的黑色节点
    std::function<int(std::shared_ptr<Node>, int, std::vector<int>&)> checkBlackHeight;
    checkBlackHeight = [&](std::shared_ptr<Node> node, int blackCount, 
                            std::vector<int>& leafBlackCounts) -> int {
        if (node == nil) {
            leafBlackCounts.push_back(blackCount);
            return blackCount;
        }
        
        if (node->color == Color::BLACK) {
            blackCount++;
        }
        
        checkBlackHeight(node->left, blackCount, leafBlackCounts);
        checkBlackHeight(node->right, blackCount, leafBlackCounts);
        
        return blackCount;
    };
    
    std::vector<int> leafBlackCounts;
    checkBlackHeight(root, 0, leafBlackCounts);
    
    int first = leafBlackCounts[0];
    for (size_t i = 1; i < leafBlackCounts.size(); ++i) {
        if (leafBlackCounts[i] != first) {
            std::cout << "Violation: Different black heights in paths" << std::endl;
            return false;
        }
    }
    
    return true;
}

template <typename T>
int RBTree<T>::height() {
    std::function<int(std::shared_ptr<Node>)> getHeight;
    getHeight = [&](std::shared_ptr<Node> node) -> int {
        if (node == nil) return 0;
        return 1 + std::max(getHeight(node->left), getHeight(node->right));
    };
    return getHeight(root);
}

#endif