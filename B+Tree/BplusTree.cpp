#include <iostream>
#include <vector>
#include <queue>
#include <algorithm>
#include <memory>
#include <cassert>
#include <random>

template<typename K, typename V>
class BPlusTree {
private:
    static const int DEFAULT_DEGREE = 3;  // 默认度数（最小子节点数）
    const int degree;  // B+树的度数（最小子节点数）
    
    // B+树节点基类
    class Node {
    public:
        bool is_leaf;
        std::vector<K> keys;
        const int degree_ref;  // 引用外部B+树的degree
        
        Node(bool leaf, int deg) : is_leaf(leaf), degree_ref(deg) {}
        virtual ~Node() = default;
        
        virtual bool is_full() const = 0;
        virtual bool is_underflow() const = 0;
        virtual bool can_borrow() const = 0;
        virtual void insert_key(const K& key, const V& value) = 0;
        virtual void remove_key(const K& key) = 0;
        virtual bool contains(const K& key) const = 0;
        virtual V* find(const K& key) = 0;
        virtual std::vector<V> range_query(const K& start, const K& end) const = 0;
        virtual void print() const = 0;
    };
    
    // 内部节点
    class InternalNode : public Node {
    public:
        std::vector<std::shared_ptr<Node>> children;
        
        InternalNode(int deg) : Node(false, deg) {}
        
        bool is_full() const override {
            return Node::keys.size() >= 2 * Node::degree_ref - 1;
        }
        
        bool is_underflow() const override {
            // 根节点可以有最少1个键，其他内部节点至少需要degree-1个键
            return Node::keys.size() < Node::degree_ref - 1;
        }
        
        bool can_borrow() const override {
            return Node::keys.size() > Node::degree_ref - 1;
        }
        
        void insert_key(const K& key, const V& value) override {
            // 内部节点不存储值
            (void)value;
            (void)key;
        }
        
        void remove_key(const K& key) override {
            // 内部节点不直接存储值
            (void)key;
        }
        
        bool contains(const K& key) const override {
            return false;  // 内部节点不存储值
        }
        
        V* find(const K& key) override {
            return nullptr;  // 内部节点不存储值
        }
        
        std::vector<V> range_query(const K& start, const K& end) const override {
            return {};  // 内部节点不存储值
        }
        
        void print() const override {
            std::cout << "[Internal: ";
            for (size_t i = 0; i < Node::keys.size(); i++) {
                std::cout << Node::keys[i];
                if (i < Node::keys.size() - 1) std::cout << "|";
            }
            std::cout << "]";
        }
        
        // 找到应插入的子节点索引
        int find_child_index(const K& key) const {
            int idx = 0;
            while (idx < Node::keys.size() && key > Node::keys[idx]) {
                idx++;
            }
            return idx;
        }
        
        // 分裂节点
        std::shared_ptr<InternalNode> split() {
            auto new_right = std::make_shared<InternalNode>(Node::degree_ref);
            
            // 移动一半的键和子节点到新节点
            int mid = Node::keys.size() / 2;
            K mid_key = Node::keys[mid];
            
            // 新节点获取右半部分的键
            new_right->keys.assign(Node::keys.begin() + mid + 1, Node::keys.end());
            
            // 新节点获取右半部分的子节点
            new_right->children.assign(children.begin() + mid + 1, children.end());
            
            // 调整原节点
            Node::keys.resize(mid);
            children.resize(mid + 1);
            
            return new_right;
        }
        
        // 在指定位置插入键和子节点
        void insert_child(int idx, const K& key, std::shared_ptr<Node> child) {
            if (idx < Node::keys.size()) {
                Node::keys.insert(Node::keys.begin() + idx, key);
                children.insert(children.begin() + idx + 1, child);
            } else {
                Node::keys.push_back(key);
                children.push_back(child);
            }
        }
    };
    
    // 叶子节点
    class LeafNode : public Node {
    public:
        std::vector<V> values;
        std::shared_ptr<LeafNode> next;  // 指向下一个叶子节点（用于范围查询）
        
        LeafNode(int deg) : Node(true, deg), next(nullptr) {}
        
        bool is_full() const override {
            return Node::keys.size() >= 2 * Node::degree_ref - 1;
        }
        
        bool is_underflow() const override {
            return Node::keys.size() < Node::degree_ref - 1;
        }
        
        bool can_borrow() const override {
            return Node::keys.size() > Node::degree_ref - 1;
        }
        
        // 在叶子节点中插入键值对
        void insert_key(const K& key, const V& value) override {
            auto it = std::lower_bound(Node::keys.begin(), Node::keys.end(), key);
            int idx = it - Node::keys.begin();
            
            // 插入键和值
            Node::keys.insert(it, key);
            values.insert(values.begin() + idx, value);
        }
        
        // 从叶子节点删除键
        void remove_key(const K& key) override {
            auto it = std::lower_bound(Node::keys.begin(), Node::keys.end(), key);
            if (it != Node::keys.end() && *it == key) {
                int idx = it - Node::keys.begin();
                Node::keys.erase(it);
                values.erase(values.begin() + idx);
            }
        }
        
        bool contains(const K& key) const override {
            return std::binary_search(Node::keys.begin(), Node::keys.end(), key);
        }
        
        V* find(const K& key) override {
            auto it = std::lower_bound(Node::keys.begin(), Node::keys.end(), key);
            if (it != Node::keys.end() && *it == key) {
                int idx = it - Node::keys.begin();
                return &values[idx];
            }
            return nullptr;
        }
        
        std::vector<V> range_query(const K& start, const K& end) const override {
            std::vector<V> result;
            auto leaf = this;
            
            while (leaf) {
                for (size_t i = 0; i < leaf->keys.size(); i++) {
                    if (leaf->keys[i] >= start && leaf->keys[i] <= end) {
                        result.push_back(leaf->values[i]);
                    } else if (leaf->keys[i] > end) {
                        return result;  // 超过范围，提前返回
                    }
                }
                leaf = leaf->next.get();
            }
            
            return result;
        }
        
        void print() const override {
            std::cout << "[Leaf: ";
            for (size_t i = 0; i < Node::keys.size(); i++) {
                std::cout << Node::keys[i];
                if (i < Node::keys.size() - 1) std::cout << ",";
            }
            std::cout << "]";
        }
        
        // 分裂叶子节点
        std::shared_ptr<LeafNode> split() {
            auto new_leaf = std::make_shared<LeafNode>(Node::degree_ref);
            
            // 移动一半的键值对到新节点
            int mid = Node::keys.size() / 2;
            new_leaf->keys.assign(Node::keys.begin() + mid, Node::keys.end());
            new_leaf->values.assign(values.begin() + mid, values.end());
            
            // 调整原节点
            Node::keys.resize(mid);
            values.resize(mid);
            
            // 更新链表指针
            new_leaf->next = next;
            next = new_leaf;
            
            return new_leaf;
        }
    };
    
    std::shared_ptr<Node> root;
    std::shared_ptr<LeafNode> first_leaf;  // 指向第一个叶子节点
    
    // 插入辅助函数
    void insert_nonfull(std::shared_ptr<Node> node, const K& key, const V& value) {
        if (node->is_leaf) {
            auto leaf = std::dynamic_pointer_cast<LeafNode>(node);
            leaf->insert_key(key, value);
        } else {
            auto internal = std::dynamic_pointer_cast<InternalNode>(node);
            int idx = internal->find_child_index(key);
            
            // 如果子节点已满，先分裂
            if (internal->children[idx]->is_full()) {
                if (internal->children[idx]->is_leaf) {
                    auto leaf = std::dynamic_pointer_cast<LeafNode>(internal->children[idx]);
                    auto new_leaf = leaf->split();
                    K promote_key = new_leaf->keys[0];
                    
                    // 将提升的键和新的子节点插入到当前内部节点
                    internal->insert_child(idx, promote_key, new_leaf);
                } else {
                    auto child_internal = std::dynamic_pointer_cast<InternalNode>(internal->children[idx]);
                    auto new_internal = child_internal->split();
                    K promote_key = child_internal->keys[child_internal->keys.size()]; // 中间键
                    
                    // 将提升的键和新的子节点插入到当前内部节点
                    internal->insert_child(idx, promote_key, new_internal);
                }
                
                // 重新确定插入位置
                if (key > internal->keys[idx]) {
                    idx++;
                }
            }
            
            insert_nonfull(internal->children[idx], key, value);
        }
    }
    
    // 删除辅助函数
    bool remove_recursive(std::shared_ptr<Node> node, const K& key) {
        if (node->is_leaf) {
            auto leaf = std::dynamic_pointer_cast<LeafNode>(node);
            bool had_key = leaf->contains(key);
            leaf->remove_key(key);
            return had_key && leaf->is_underflow();
        } else {
            auto internal = std::dynamic_pointer_cast<InternalNode>(node);
            int idx = internal->find_child_index(key);
            
            bool child_underflow = remove_recursive(internal->children[idx], key);
            
            if (child_underflow) {
                // 简化的处理：如果子节点下溢，尝试合并或借用
                // 这里只实现简单的删除，不处理复杂的合并操作
                if (internal->children[idx]->is_leaf) {
                    auto leaf = std::dynamic_pointer_cast<LeafNode>(internal->children[idx]);
                    if (leaf->keys.empty()) {
                        // 删除空的叶子节点
                        internal->children.erase(internal->children.begin() + idx);
                        if (idx > 0) {
                            internal->keys.erase(internal->keys.begin() + idx - 1);
                        } else if (!internal->keys.empty()) {
                            internal->keys.erase(internal->keys.begin());
                        }
                    }
                }
            }
            return internal->is_underflow();
        }
    }
    
    // 查找叶子节点
    std::shared_ptr<LeafNode> find_leaf(const K& key) const {
        if (!root) return nullptr;
        
        auto node = root;
        while (!node->is_leaf) {
            auto internal = std::dynamic_pointer_cast<InternalNode>(node);
            int idx = internal->find_child_index(key);
            if (idx >= internal->children.size()) {
                return nullptr;  // 不应该发生
            }
            node = internal->children[idx];
        }
        return std::dynamic_pointer_cast<LeafNode>(node);
    }
    
    // 验证树结构
    bool validate_node(std::shared_ptr<Node> node, int level, K& min_key, K& max_key, bool& first) const {
        if (!node) return true;
        
        // 检查节点大小
        if (node != root) {
            if (node->is_underflow()) {
                std::cout << "Error: Node underflow at level " << level << std::endl;
                return false;
            }
            if (node->is_full()) {
                std::cout << "Warning: Node full at level " << level << std::endl;
            }
        }
        
        if (node->is_leaf) {
            auto leaf = std::dynamic_pointer_cast<LeafNode>(node);
            if (leaf->keys.empty()) {
                std::cout << "Error: Empty leaf node at level " << level << std::endl;
                return false;
            }
            
            // 检查键的顺序
            for (size_t i = 1; i < leaf->keys.size(); i++) {
                if (leaf->keys[i] <= leaf->keys[i-1]) {
                    std::cout << "Error: Leaf keys not sorted at level " << level << std::endl;
                    return false;
                }
            }
            
            // 更新最小值和最大值
            if (first) {
                min_key = leaf->keys.front();
                max_key = leaf->keys.back();
                first = false;
            } else {
                if (leaf->keys.front() <= max_key) {
                    std::cout << "Error: Leaf key range overlap at level " << level << std::endl;
                    return false;
                }
                max_key = leaf->keys.back();
            }
            
            return true;
        } else {
            auto internal = std::dynamic_pointer_cast<InternalNode>(node);
            if (internal->children.empty()) {
                std::cout << "Error: Internal node has no children at level " << level << std::endl;
                return false;
            }
            
            if (internal->keys.size() != internal->children.size() - 1) {
                std::cout << "Error: Key count mismatch at internal node level " << level << std::endl;
                return false;
            }
            
            // 递归验证每个子树
            K child_min, child_max;
            bool child_first = true;
            
            for (size_t i = 0; i < internal->children.size(); i++) {
                if (!validate_node(internal->children[i], level + 1, child_min, child_max, child_first)) {
                    return false;
                }
                
                if (i < internal->keys.size()) {
                    if (child_max > internal->keys[i]) {
                        std::cout << "Error: Child max key > split key at level " << level << std::endl;
                        return false;
                    }
                }
                if (i > 0) {
                    if (child_min <= internal->keys[i-1]) {
                        std::cout << "Error: Child min key <= previous split key at level " << level << std::endl;
                        return false;
                    }
                }
            }
            
            return true;
        }
    }
    
public:
    BPlusTree(int deg = DEFAULT_DEGREE) : degree(std::max(2, deg)), root(nullptr), first_leaf(nullptr) {}
    
    // 插入键值对
    void insert(const K& key, const V& value) {
        if (!root) {
            auto leaf = std::make_shared<LeafNode>(degree);
            leaf->insert_key(key, value);
            root = leaf;
            first_leaf = leaf;
            return;
        }
        
        // 如果根节点已满，需要分裂根节点
        if (root->is_full()) {
            auto new_root = std::make_shared<InternalNode>(degree);
            
            if (root->is_leaf) {
                auto old_leaf = std::dynamic_pointer_cast<LeafNode>(root);
                auto new_leaf = old_leaf->split();
                K promote_key = new_leaf->keys[0];
                
                new_root->keys.push_back(promote_key);
                new_root->children.push_back(old_leaf);
                new_root->children.push_back(new_leaf);
            } else {
                auto old_internal = std::dynamic_pointer_cast<InternalNode>(root);
                auto new_internal = old_internal->split();
                K promote_key = old_internal->keys[old_internal->keys.size() / 2];
                
                // 移除提升的键
                old_internal->keys.erase(old_internal->keys.begin() + old_internal->keys.size() / 2);
                
                new_root->keys.push_back(promote_key);
                new_root->children.push_back(old_internal);
                new_root->children.push_back(new_internal);
            }
            
            root = new_root;
        }
        
        insert_nonfull(root, key, value);
    }
    
    // 删除键
    void remove(const K& key) {
        if (!root) return;
        
        remove_recursive(root, key);
        
        // 如果根节点变成叶子节点且为空，则清空树
        if (root->is_leaf) {
            auto leaf = std::dynamic_pointer_cast<LeafNode>(root);
            if (leaf->keys.empty()) {
                root = nullptr;
                first_leaf = nullptr;
            }
        }
        // 如果根节点只有一个子节点，则降低树的高度
        else if (root->keys.empty()) {
            auto internal = std::dynamic_pointer_cast<InternalNode>(root);
            if (internal->children.size() == 1) {
                root = internal->children[0];
            }
        }
    }
    
    // 查找键
    V* find(const K& key) {
        auto leaf = find_leaf(key);
        if (leaf) {
            return leaf->find(key);
        }
        return nullptr;
    }
    
    // 范围查询
    std::vector<V> range_query(const K& start, const K& end) {
        if (!root) return {};
        
        auto leaf = find_leaf(start);
        if (leaf) {
            return leaf->range_query(start, end);
        }
        return {};
    }
    
    // 检查键是否存在
    bool contains(const K& key) {
        return find(key) != nullptr;
    }
    
    // 验证树结构
    bool validate() const {
        if (!root) return true;
        
        K min_key, max_key;
        bool first = true;
        return validate_node(root, 0, min_key, max_key, first);
    }
    
    // 打印B+树（层次遍历）
    void print() const {
        if (!root) {
            std::cout << "Empty tree" << std::endl;
            return;
        }
        
        std::queue<std::pair<std::shared_ptr<Node>, int>> q;
        q.push({root, 0});
        
        int current_level = -1;
        while (!q.empty()) {
            auto [node, level] = q.front();
            q.pop();
            
            if (level != current_level) {
                if (current_level != -1) std::cout << std::endl;
                std::cout << "Level " << level << ": ";
                current_level = level;
            }
            
            node->print();
            std::cout << " ";
            
            if (!node->is_leaf) {
                auto internal = std::dynamic_pointer_cast<InternalNode>(node);
                for (auto& child : internal->children) {
                    q.push({child, level + 1});
                }
            }
        }
        std::cout << std::endl;
    }
    
    // 打印所有叶子节点（按顺序）
    void print_leaves() const {
        std::cout << "Leaf nodes (in order):" << std::endl;
        auto leaf = first_leaf;
        int leaf_count = 0;
        
        while (leaf) {
            std::cout << "Leaf " << leaf_count++ << ": ";
            leaf->print();
            std::cout << std::endl;
            leaf = leaf->next;
        }
    }
    
    // 获取树的高度
    int height() const {
        int h = 0;
        auto node = root;
        while (node && !node->is_leaf) {
            auto internal = std::dynamic_pointer_cast<InternalNode>(node);
            if (!internal->children.empty()) {
                node = internal->children[0];
                h++;
            } else {
                break;
            }
        }
        return h + 1;  // +1 for leaf level
    }
    
    // 获取树的大小（键的数量）
    size_t size() const {
        size_t count = 0;
        auto leaf = first_leaf;
        while (leaf) {
            count += leaf->keys.size();
            leaf = leaf->next;
        }
        return count;
    }
};

// 测试函数
void test_bplustree() {
    std::cout << "=== B+ Tree Test ===\n" << std::endl;
    
    // 测试1：创建B+树并插入数据
    std::cout << "Test 1: Insertion" << std::endl;
    BPlusTree<int, std::string> tree(3);
    
    // 插入一些数据
    std::vector<std::pair<int, std::string>> data = {
        {10, "Alice"},
        {20, "Bob"},
        {5, "Charlie"},
        {15, "David"},
        {25, "Eve"},
        {3, "Frank"},
        {7, "Grace"},
        {12, "Henry"},
        {17, "Ivy"},
        {22, "Jack"}
    };
    
    for (const auto& pair : data) {
        tree.insert(pair.first, pair.second);
    }
    
    tree.print();
    std::cout << "Tree height: " << tree.height() << std::endl;
    std::cout << "Tree size: " << tree.size() << std::endl;
    
    if (tree.validate()) {
        std::cout << "Tree validation: PASSED" << std::endl;
    } else {
        std::cout << "Tree validation: FAILED" << std::endl;
    }
    
    std::cout << std::endl;
    
    // 测试2：查找操作
    std::cout << "Test 2: Lookup" << std::endl;
    for (const auto& pair : data) {
        auto* value = tree.find(pair.first);
        if (value) {
            std::cout << "Found key " << pair.first << ": " << *value << std::endl;
        } else {
            std::cout << "Key " << pair.first << " not found!" << std::endl;
        }
    }
    
    // 查找不存在的键
    auto* not_found = tree.find(100);
    if (!not_found) {
        std::cout << "Key 100 not found (as expected)" << std::endl;
    }
    std::cout << std::endl;
    
    // 测试3：范围查询
    std::cout << "Test 3: Range Query [10, 20]" << std::endl;
    auto results = tree.range_query(10, 20);
    std::cout << "Results (" << results.size() << "): ";
    for (const auto& val : results) {
        std::cout << val << " ";
    }
    std::cout << std::endl << std::endl;
    
    // 测试4：删除操作
    std::cout << "Test 4: Deletion" << std::endl;
    std::cout << "Before deleting key 15:" << std::endl;
    tree.print_leaves();
    std::cout << std::endl;
    
    tree.remove(15);
    std::cout << "After deleting key 15:" << std::endl;
    tree.print_leaves();
    std::cout << std::endl;
    
    // 验证删除
    if (!tree.contains(15)) {
        std::cout << "Key 15 successfully deleted" << std::endl;
    }
    std::cout << std::endl;
    
    // 测试5：批量操作
    std::cout << "Test 5: Bulk Operations" << std::endl;
    BPlusTree<int, int> tree2(4);
    
    // 插入100个键值对
    std::cout << "Inserting 100 keys..." << std::endl;
    for (int i = 0; i < 100; i++) {
        tree2.insert(i, i * 10);
        if ((i + 1) % 20 == 0) {
            std::cout << "Inserted " << (i + 1) << " keys, tree height: " << tree2.height() << std::endl;
        }
    }
    
    std::cout << "Final tree height: " << tree2.height() << std::endl;
    std::cout << "Final tree size: " << tree2.size() << std::endl;
    
    // 验证所有插入的值
    std::cout << "Verifying all 100 keys..." << std::endl;
    bool all_found = true;
    for (int i = 0; i < 100; i++) {
        auto* val = tree2.find(i);
        if (!val || *val != i * 10) {
            std::cout << "Error: Key " << i << " not found or incorrect value" << std::endl;
            all_found = false;
            break;
        }
    }
    
    if (all_found) {
        std::cout << "All 100 keys found correctly" << std::endl;
    }
    
    // 范围查询测试
    auto range_results = tree2.range_query(25, 35);
    std::cout << "Range [25, 35] has " << range_results.size() << " elements" << std::endl;
    
    // 验证树结构
    if (tree2.validate()) {
        std::cout << "Tree validation after bulk insert: PASSED" << std::endl;
    } else {
        std::cout << "Tree validation after bulk insert: FAILED" << std::endl;
    }
    
    std::cout << std::endl;
    
    // 测试6：随机操作
    std::cout << "Test 6: Random Operations" << std::endl;
    BPlusTree<int, int> tree3(5);
    std::vector<int> keys;
    std::mt19937 rng(42);  // 固定种子以便重现
    
    // 随机插入
    std::cout << "Random insertions..." << std::endl;
    for (int i = 0; i < 50; i++) {
        int key = rng() % 1000;
        tree3.insert(key, key * 2);
        keys.push_back(key);
        
        if (!tree3.validate()) {
            std::cout << "Validation failed after inserting key " << key << std::endl;
            break;
        }
    }
    
    std::cout << "Tree size after random insertions: " << tree3.size() << std::endl;
    std::cout << "Tree height after random insertions: " << tree3.height() << std::endl;
    
    // 随机查找
    std::cout << "Random lookups..." << std::endl;
    for (int i = 0; i < 20; i++) {
        int key = keys[rng() % keys.size()];
        auto* val = tree3.find(key);
        if (val && *val == key * 2) {
            std::cout << "Key " << key << " found correctly" << std::endl;
        } else {
            std::cout << "Key " << key << " not found or incorrect value" << std::endl;
        }
    }
    
    // 随机删除
    std::cout << "Random deletions..." << std::endl;
    for (int i = 0; i < 10; i++) {
        if (keys.empty()) break;
        
        int idx = rng() % keys.size();
        int key = keys[idx];
        tree3.remove(key);
        keys.erase(keys.begin() + idx);
        
        if (!tree3.validate()) {
            std::cout << "Validation failed after deleting key " << key << std::endl;
            break;
        }
    }
    
    std::cout << "Tree size after random deletions: " << tree3.size() << std::endl;
    
    std::cout << "\n=== All Tests Completed ===" << std::endl;
}

int main() {
    try {
        test_bplustree();
    } catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}