hugo@hugo: g++ ./TesTRBT.cpp -o a.out

hugo@hugo: ./a.out

插入元素: 11->7, 9->3, 7->18, 6->10, 41->22, 23->8, 89->11, 1->26
中序遍历: key: 1 value: 26(R)   key: 6 value: 10(B)   key: 7 value: 18(R)   key: 9 value: 3(B)   key: 11 value: 7(B)   key: 23 value: 8(R)   key: 41 value: 22(B)   key: 89 value: 11(R)   

层序遍历:
key: 9 value: 3(B)   
key: 6 value: 10(B)   key: 23 value: 8(R)   
key: 1 value: 26(R)   key: 7 value: 18(R)   key: 11 value: 7(B)   key: 41 value: 22(B)   
key: 89 value: 11(R)   

验证红黑树性质: 通过

查找元素:
查找 9: 3
查找 7: 18

最小key: 1
最大key: 89

删除元素 23
中序遍历: key: 1 value: 26(R)   key: 6 value: 10(B)   key: 7 value: 18(R)   key: 9 value: 3(B)   key: 11 value: 7(B)   key: 41 value: 22(R)   key: 89 value: 11(B)   

修改元素 1->26 为 1->21
中序遍历: key: 1 value: 21(R)   key: 6 value: 10(B)   key: 7 value: 18(R)   key: 9 value: 3(B)   key: 11 value: 7(B)   key: 41 value: 22(R)   key: 89 value: 11(B)   

验证红黑树性质: 通过
