#include <iostream>
#include <memory>
#include <queue>
#include <vector>
#include <algorithm>
#include <cassert>
#include <string>
#include <cmath>

enum class Color { RED, BLACK };

template<typename K, typename V>
class RedBlackTree {
private:
    // 红黑树节点结构
    struct Node {
        K key;
        V value;
        Color color;
        std::shared_ptr<Node> left;
        std::shared_ptr<Node> right;
        std::weak_ptr<Node> parent;
        
        Node(const K& k, const V& v, Color c = Color::RED)
            : key(k), value(v), color(c), left(nullptr), right(nullptr) {}
        
        // 判断是否是左子节点
        bool is_left_child() const {
            auto p = parent.lock();
            return p && p->left.get() == this;
        }
        
        // 获取兄弟节点
        std::shared_ptr<Node> sibling() const {
            auto p = parent.lock();
            if (!p) return nullptr;
            return is_left_child() ? p->right : p->left;
        }
        
        // 获取叔叔节点
        std::shared_ptr<Node> uncle() const {
            auto p = parent.lock();
            if (!p) return nullptr;
            auto gp = p->parent.lock();
            if (!gp) return nullptr;
            return p->is_left_child() ? gp->right : gp->left;
        }
        
        // 获取祖父节点
        std::shared_ptr<Node> grandparent() const {
            auto p = parent.lock();
            if (!p) return nullptr;
            return p->parent.lock();
        }
    };
    
    std::shared_ptr<Node> root;
    size_t count;
    
    // 左旋
    void left_rotate(std::shared_ptr<Node> x) {
        auto y = x->right;
        if (!y) return;  // 安全检查
        
        x->right = y->left;
        
        if (y->left) {
            y->left->parent = x;
        }
        
        y->parent = x->parent;
        
        if (!x->parent.lock()) {
            root = y;
        } else if (x->is_left_child()) {
            x->parent.lock()->left = y;
        } else {
            x->parent.lock()->right = y;
        }
        
        y->left = x;
        x->parent = y;
    }
    
    // 右旋
    void right_rotate(std::shared_ptr<Node> y) {
        auto x = y->left;
        if (!x) return;  // 安全检查
        
        y->left = x->right;
        
        if (x->right) {
            x->right->parent = y;
        }
        
        x->parent = y->parent;
        
        if (!y->parent.lock()) {
            root = x;
        } else if (y->is_left_child()) {
            y->parent.lock()->left = x;
        } else {
            y->parent.lock()->right = x;
        }
        
        x->right = y;
        y->parent = x;
    }
    
    // 插入修复
    void fix_insert(std::shared_ptr<Node> node) {
        while (node != root && node->parent.lock()->color == Color::RED) {
            auto parent = node->parent.lock();
            auto grandparent = parent->parent.lock();
            if (!grandparent) break;
            
            if (parent->is_left_child()) {  // 父节点是左子节点
                auto uncle = parent->sibling();
                
                if (uncle && uncle->color == Color::RED) {
                    // 情况1：叔叔节点是红色
                    parent->color = Color::BLACK;
                    uncle->color = Color::BLACK;
                    grandparent->color = Color::RED;
                    node = grandparent;
                } else {
                    if (!node->is_left_child()) {
                        // 情况2：节点是右子节点
                        node = parent;
                        left_rotate(node);
                        parent = node->parent.lock();
                        grandparent = parent ? parent->parent.lock() : nullptr;
                        if (!grandparent) break;
                    }
                    
                    // 情况3：节点是左子节点
                    parent->color = Color::BLACK;
                    grandparent->color = Color::RED;
                    right_rotate(grandparent);
                }
            } else {  // 父节点是右子节点
                auto uncle = parent->sibling();
                
                if (uncle && uncle->color == Color::RED) {
                    // 情况1：叔叔节点是红色
                    parent->color = Color::BLACK;
                    uncle->color = Color::BLACK;
                    grandparent->color = Color::RED;
                    node = grandparent;
                } else {
                    if (node->is_left_child()) {
                        // 情况2：节点是左子节点
                        node = parent;
                        right_rotate(node);
                        parent = node->parent.lock();
                        grandparent = parent ? parent->parent.lock() : nullptr;
                        if (!grandparent) break;
                    }
                    
                    // 情况3：节点是右子节点
                    parent->color = Color::BLACK;
                    grandparent->color = Color::RED;
                    left_rotate(grandparent);
                }
            }
        }
        
        if (root) {
            root->color = Color::BLACK;
        }
    }
    
    // 查找最小节点
    std::shared_ptr<Node> minimum(std::shared_ptr<Node> node) const {
        if (!node) return nullptr;
        while (node->left) {
            node = node->left;
        }
        return node;
    }
    
    // 查找节点
    std::shared_ptr<Node> find_node(const K& key) const {
        auto current = root;
        while (current) {
            if (key < current->key) {
                current = current->left;
            } else if (key > current->key) {
                current = current->right;
            } else {
                return current;
            }
        }
        return nullptr;
    }
    
    // 移植节点（用v替换u）
    void transplant(std::shared_ptr<Node> u, std::shared_ptr<Node> v) {
        auto parent = u->parent.lock();
        if (!parent) {
            root = v;
        } else if (u->is_left_child()) {
            parent->left = v;
        } else {
            parent->right = v;
        }
        
        if (v) {
            v->parent = u->parent;
        }
    }
    
    // 删除修复
    void fix_delete(std::shared_ptr<Node> node, std::shared_ptr<Node> parent) {
        // 如果树为空，直接返回
        if (!root) return;
        
        std::shared_ptr<Node> sibling;
        
        while (node != root && (!node || node->color == Color::BLACK)) {
            if (!parent) break; // 父节点为空，退出循环
            
            if (node == parent->left) {
                sibling = parent->right;
                if (!sibling) break; // 兄弟节点为空，退出循环
                
                if (sibling->color == Color::RED) {
                    // 情况1：兄弟节点是红色
                    sibling->color = Color::BLACK;
                    parent->color = Color::RED;
                    left_rotate(parent);
                    sibling = parent->right;
                    if (!sibling) break;
                }
                
                if ((!sibling->left || sibling->left->color == Color::BLACK) &&
                    (!sibling->right || sibling->right->color == Color::BLACK)) {
                    // 情况2：兄弟节点的两个子节点都是黑色
                    sibling->color = Color::RED;
                    node = parent;
                    parent = node->parent.lock();
                } else {
                    if (!sibling->right || sibling->right->color == Color::BLACK) {
                        // 情况3：兄弟节点的右子节点是黑色，左子节点是红色
                        if (sibling->left) sibling->left->color = Color::BLACK;
                        sibling->color = Color::RED;
                        right_rotate(sibling);
                        sibling = parent->right;
                        if (!sibling) break;
                    }
                    
                    // 情况4：兄弟节点的右子节点是红色
                    sibling->color = parent->color;
                    parent->color = Color::BLACK;
                    if (sibling->right) sibling->right->color = Color::BLACK;
                    left_rotate(parent);
                    node = root;
                    break;
                }
            } else {
                sibling = parent->left;
                if (!sibling) break; // 兄弟节点为空，退出循环
                
                if (sibling->color == Color::RED) {
                    // 情况1：兄弟节点是红色（镜像）
                    sibling->color = Color::BLACK;
                    parent->color = Color::RED;
                    right_rotate(parent);
                    sibling = parent->left;
                    if (!sibling) break;
                }
                
                if ((!sibling->left || sibling->left->color == Color::BLACK) &&
                    (!sibling->right || sibling->right->color == Color::BLACK)) {
                    // 情况2：兄弟节点的两个子节点都是黑色（镜像）
                    sibling->color = Color::RED;
                    node = parent;
                    parent = node->parent.lock();
                } else {
                    if (!sibling->left || sibling->left->color == Color::BLACK) {
                        // 情况3：兄弟节点的左子节点是黑色，右子节点是红色（镜像）
                        if (sibling->right) sibling->right->color = Color::BLACK;
                        sibling->color = Color::RED;
                        left_rotate(sibling);
                        sibling = parent->left;
                        if (!sibling) break;
                    }
                    
                    // 情况4：兄弟节点的左子节点是红色（镜像）
                    sibling->color = parent->color;
                    parent->color = Color::BLACK;
                    if (sibling->left) sibling->left->color = Color::BLACK;
                    right_rotate(parent);
                    node = root;
                    break;
                }
            }
        }
        
        if (node) {
            node->color = Color::BLACK;
        }
    }
    
    // 中序遍历辅助函数
    void inorder_traversal(std::shared_ptr<Node> node, 
                          std::vector<std::pair<K, V>>& result) const {
        if (!node) return;
        inorder_traversal(node->left, result);
        result.emplace_back(node->key, node->value);
        inorder_traversal(node->right, result);
    }
    
    // 验证红黑树属性
    bool validate_rb(std::shared_ptr<Node> node, int black_count, int& path_black_count) const {
        if (!node) {
            if (path_black_count == -1) {
                path_black_count = black_count;
            }
            return black_count == path_black_count;
        }
        
        // 检查红色节点的子节点不能是红色
        if (node->color == Color::RED) {
            if ((node->left && node->left->color == Color::RED) ||
                (node->right && node->right->color == Color::RED)) {
                return false;
            }
        }
        
        int new_black_count = black_count + (node->color == Color::BLACK ? 1 : 0);
        
        return validate_rb(node->left, new_black_count, path_black_count) &&
               validate_rb(node->right, new_black_count, path_black_count);
    }
    
    // 计算树的高度
    int height(std::shared_ptr<Node> node) const {
        if (!node) return 0;
        return 1 + std::max(height(node->left), height(node->right));
    }
    
public:
    RedBlackTree() : root(nullptr), count(0) {}
    
    ~RedBlackTree() = default;
    
    // 插入键值对
    bool insert(const K& key, const V& value) {
        // 如果树为空，直接创建根节点
        if (!root) {
            root = std::make_shared<Node>(key, value, Color::BLACK);
            count = 1;
            return true;
        }
        
        // 查找插入位置
        auto current = root;
        std::shared_ptr<Node> parent = nullptr;
        
        while (current) {
            if (key < current->key) {
                parent = current;
                current = current->left;
            } else if (key > current->key) {
                parent = current;
                current = current->right;
            } else {
                // 键已存在，更新值
                current->value = value;
                return false;
            }
        }
        
        // 创建新节点
        auto new_node = std::make_shared<Node>(key, value, Color::RED);
        new_node->parent = parent;
        
        // 插入到正确位置
        if (key < parent->key) {
            parent->left = new_node;
        } else {
            parent->right = new_node;
        }
        
        // 修复红黑树属性
        fix_insert(new_node);
        count++;
        return true;
    }
    
    // 删除键
    bool remove(const K& key) {
        auto node = find_node(key);
        if (!node) return false;
        
        auto original_color = node->color;
        std::shared_ptr<Node> x = nullptr;
        std::shared_ptr<Node> parent = nullptr;
        
        if (!node->left) {
            // 只有右子节点或没有子节点
            x = node->right;
            parent = node->parent.lock();
            transplant(node, node->right);
        } else if (!node->right) {
            // 只有左子节点
            x = node->left;
            parent = node->parent.lock();
            transplant(node, node->left);
        } else {
            // 有两个子节点，找到后继节点
            auto successor = minimum(node->right);
            original_color = successor->color;
            x = successor->right;
            parent = successor->parent.lock();
            
            if (successor->parent.lock() != node) {
                transplant(successor, successor->right);
                successor->right = node->right;
                if (successor->right) {
                    successor->right->parent = successor;
                }
            } else {
                // 如果后继节点是node的直接右子节点
                parent = successor;
            }
            
            transplant(node, successor);
            successor->left = node->left;
            if (successor->left) {
                successor->left->parent = successor;
            }
            successor->color = node->color;
        }
        
        // 如果删除的是黑色节点，需要修复
        if (original_color == Color::BLACK) {
            if (x == root) {
                // 如果x是根节点，只需将其染黑
                if (x) x->color = Color::BLACK;
            } else if (parent) {
                // 只有当我们有有效的父节点时才修复
                fix_delete(x, parent);
            }
            // 否则，树已经为空或不需要修复
        }
        
        count--;
        return true;
    }
    
    // 查找键
    V* find(const K& key) const {
        auto node = find_node(key);
        if (node) {
            return &node->value;
        }
        return nullptr;
    }
    
    // 检查键是否存在
    bool contains(const K& key) const {
        return find_node(key) != nullptr;
    }
    
    // 获取元素个数
    size_t size() const {
        return count;
    }
    
    // 判断树是否为空
    bool empty() const {
        return count == 0;
    }
    
    // 清空树
    void clear() {
        root.reset();
        count = 0;
    }
    
    // 中序遍历（返回排序后的键值对）
    std::vector<std::pair<K, V>> inorder() const {
        std::vector<std::pair<K, V>> result;
        inorder_traversal(root, result);
        return result;
    }
    
    // 层序遍历（用于打印树结构）
    std::vector<std::vector<std::pair<K, Color>>> level_order() const {
        std::vector<std::vector<std::pair<K, Color>>> result;
        if (!root) return result;
        
        std::queue<std::shared_ptr<Node>> q;
        q.push(root);
        
        while (!q.empty()) {
            int level_size = q.size();
            std::vector<std::pair<K, Color>> level;
            
            for (int i = 0; i < level_size; i++) {
                auto node = q.front();
                q.pop();
                
                level.emplace_back(node->key, node->color);
                
                if (node->left) q.push(node->left);
                if (node->right) q.push(node->right);
            }
            
            result.push_back(level);
        }
        
        return result;
    }
    
    // 验证红黑树的所有属性
    bool validate() const {
        if (!root) return true;
        
        // 性质2：根节点必须是黑色
        if (root->color != Color::BLACK) {
            std::cout << "Violation: Root is not black" << std::endl;
            return false;
        }
        
        // 性质4和5：检查所有路径的黑色节点数量相同
        int path_black_count = -1;
        if (!validate_rb(root, 0, path_black_count)) {
            std::cout << "Violation: Different number of black nodes in paths" << std::endl;
            return false;
        }
        
        return true;
    }
    
    // 打印树结构（ASCII图形）
    void print_tree() const {
        if (!root) {
            std::cout << "Empty tree" << std::endl;
            return;
        }
        
        auto levels = level_order();
        int total_levels = levels.size();
        
        for (int i = 0; i < total_levels; i++) {
            std::cout << "Level " << i << ": ";
            for (const auto& node : levels[i]) {
                std::cout << node.first;
                if (node.second == Color::RED) {
                    std::cout << "(R)";
                } else {
                    std::cout << "(B)";
                }
                std::cout << " ";
            }
            std::cout << std::endl;
        }
    }
    
    // 获取树的高度
    int get_height() const {
        return height(root);
    }
};

// 测试函数
void test_redblack_tree() {
    std::cout << "=== Red-Black Tree Test ===\n" << std::endl;
    
    // 测试1：基本插入和查找
    std::cout << "Test 1: Basic Insertion and Lookup" << std::endl;
    RedBlackTree<int, std::string> tree;
    
    // 插入测试数据
    std::vector<std::pair<int, std::string>> test_data = {
        {10, "Alice"},
        {20, "Bob"},
        {30, "Charlie"},
        {15, "David"},
        {25, "Eve"},
        {5, "Frank"},
        {3, "Grace"},
        {8, "Henry"},
        {12, "Ivy"},
        {18, "Jack"}
    };
    
    for (const auto& p : test_data) {
        tree.insert(p.first, p.second);
    }
    
    // 验证所有插入的数据都能找到
    bool all_found = true;
    for (const auto& p : test_data) {
        if (!tree.contains(p.first)) {
            all_found = false;
            break;
        }
    }
    
    if (all_found) {
        std::cout << "✓ All " << tree.size() << " keys inserted and found" << std::endl;
    } else {
        std::cout << "✗ Some keys not found!" << std::endl;
    }
    
    // 测试2：验证红黑树属性
    std::cout << "\nTest 2: Red-Black Tree Properties" << std::endl;
    if (tree.validate()) {
        std::cout << "✓ All red-black tree properties satisfied" << std::endl;
    } else {
        std::cout << "✗ Red-black tree properties violated!" << std::endl;
    }
    
    // 打印树结构
    std::cout << "\nTree structure (level order):" << std::endl;
    tree.print_tree();
    
    // 测试3：中序遍历（应该是有序的）
    std::cout << "\nTest 3: Inorder Traversal (Should be sorted)" << std::endl;
    auto sorted = tree.inorder();
    bool is_sorted = std::is_sorted(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });
    
    if (is_sorted) {
        std::cout << "✓ Inorder traversal is sorted" << std::endl;
        std::cout << "Sorted keys: ";
        for (const auto& p : sorted) {
            std::cout << p.first << " ";
        }
        std::cout << std::endl;
    }
    
    // 测试4：删除操作
    std::cout << "\nTest 4: Deletion" << std::endl;
    int delete_key = 15;
    if (tree.contains(delete_key)) {
        tree.remove(delete_key);
        if (!tree.contains(delete_key)) {
            std::cout << "✓ Key " << delete_key << " successfully deleted" << std::endl;
        }
        
        // 验证删除后红黑树属性仍然保持
        if (tree.validate()) {
            std::cout << "✓ Red-black tree properties still satisfied after deletion" << std::endl;
        }
    }
    
    // 测试5：更新操作
    std::cout << "\nTest 5: Update Existing Key" << std::endl;
    int update_key = 10;
    std::string new_value = "Alice_Updated";
    bool inserted = tree.insert(update_key, new_value);
    auto* value_ptr = tree.find(update_key);
    
    if (!inserted && value_ptr && *value_ptr == new_value) {
        std::cout << "✓ Key " << update_key << " successfully updated" << std::endl;
    }
    
    // 测试6：边界情况
    std::cout << "\nTest 6: Edge Cases" << std::endl;
    RedBlackTree<int, int> empty_tree;
    
    // 空树操作
    if (empty_tree.empty() && empty_tree.size() == 0) {
        std::cout << "✓ Empty tree properties correct" << std::endl;
    }
    
    // 查找不存在的键
    if (!empty_tree.contains(999)) {
        std::cout << "✓ Non-existent key not found in empty tree" << std::endl;
    }
    
    // 测试7：大量数据插入
    std::cout << "\nTest 7: Bulk Operations" << std::endl;
    RedBlackTree<int, int> large_tree;
    const int NUM_KEYS = 1000;
    
    // 插入1000个键
    for (int i = 0; i < NUM_KEYS; i++) {
        large_tree.insert(i, i * 10);
    }
    
    // 验证所有键都能找到
    bool all_keys_found = true;
    for (int i = 0; i < NUM_KEYS; i++) {
        if (!large_tree.contains(i)) {
            all_keys_found = false;
            break;
        }
    }
    
    if (all_keys_found) {
        std::cout << "✓ All " << NUM_KEYS << " keys inserted and found" << std::endl;
    }
    
    // 验证红黑树属性
    if (large_tree.validate()) {
        std::cout << "✓ Red-black tree properties satisfied for large tree" << std::endl;
    }
    
    // 计算树的高度
    int height = large_tree.get_height();
    int max_height_upper_bound = 2 * static_cast<int>(std::log2(NUM_KEYS + 1));
    
    std::cout << "Tree height: " << height << std::endl;
    std::cout << "Theoretical maximum height for " << NUM_KEYS 
              << " nodes: 2*log2(n+1) ≈ " << max_height_upper_bound << std::endl;
    
    if (height <= max_height_upper_bound) {
        std::cout << "✓ Tree height is within theoretical bounds" << std::endl;
    }
    
    // 测试8：有序插入（最坏情况）
    std::cout << "\nTest 8: Sorted Insertion (Worst Case)" << std::endl;
    RedBlackTree<int, std::string> sorted_tree;
    
    for (int i = 1; i <= 20; i++) {
        sorted_tree.insert(i, "Value" + std::to_string(i));
    }
    
    if (sorted_tree.validate()) {
        std::cout << "✓ Red-black tree handles sorted insertion correctly" << std::endl;
        std::cout << "Tree height after sorted insertion: " << sorted_tree.get_height() << std::endl;
    }
    
    // 测试9：删除所有节点
    std::cout << "\nTest 9: Delete All Nodes" << std::endl;
    RedBlackTree<int, int> delete_test;
    
    // 插入一些数据
    for (int i = 0; i < 10; i++) {
        delete_test.insert(i, i);
    }
    
    // 删除所有数据
    for (int i = 0; i < 10; i++) {
        delete_test.remove(i);
        if (!delete_test.validate()) {
            std::cout << "✗ Red-black tree properties violated after deleting key " << i << std::endl;
            break;
        }
    }
    
    if (delete_test.empty()) {
        std::cout << "✓ All nodes successfully deleted, tree is empty" << std::endl;
    }
    
    std::cout << "\n=== All Tests Completed Successfully ===" << std::endl;
}

int main() {
    try {
        test_redblack_tree();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}