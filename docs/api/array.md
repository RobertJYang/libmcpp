#mc::array 类

##定义

```cpp namespace mc {
    template <typename T, typename Allocator = std::allocator<T>>
    class array;
}
```

    ##概述

`mc::array` 是一个引用语义的数组类，可以用来替换 `std::vector` 或 `mc::variants`。它内部使用 `mc::shared_ptr` 管理数据，采用写时共享（copy -
    on - write semantics）的设计，可以显著减少拷贝开销。

    ##核心特性

    ## #1. 引用语义

        与 `std::vector` 不同，`mc::array` 的拷贝操作会共享内部数据，而不是深拷贝：

```cpp        mc::array<int>
               arr1 = {1, 2, 3};
mc::array<int> arr2 = arr1; // 共享数据，不会复制

arr1[0] = 100;
// arr2[0] 也变成了 100，因为它们共享同一份数据
```

    ## #2. 内存高效

            由于使用了 `mc::shared_ptr`（8字节），而不是 `std::shared_ptr`（16字节），使得 `mc::array` 特别适合存储在 `mc::variant` 中：

```cpp
            mc::variant v = mc::array<int>{1, 2, 3, 4, 5};
// variant 内部只存储一个指针，非常高效
```

    ## #3. 完全兼容 std::vector 接口

`mc::array` 提供了与 `std::vector` 几乎完全一致的接口：

```cpp
                                mc::array<int>
                                arr;

// 添加元素
arr.push_back(1);
arr.emplace_back(2);

// 访问元素
int first = arr[0];
int last  = arr.back();

// 迭代
for (int val : arr) {
    // 处理 val
}

// 容量管理
arr.reserve(100);
arr.resize(50);
arr.clear();
```

    ##内部实现

`mc::array` 内部使用 `mc::detail::array_impl` 类，该类继承自：

    1. `mc::enable_shared_from_this<array_impl<T, Allocator>>` -
    支持 `mc::shared_ptr` 2. `std::vector<T, Allocator>` - 继承所有 vector 的功能

                                                               这种设计既保证了引用计数的正确性，又无缝继承了 `std::vector` 的所有功能。

    ##使用示例

    ## #基本用法

```cpp
#include <mc/array.h>

        // 默认构造
            mc::array<int>
            arr1;

// 指定大小
mc::array<int> arr2(10);

// 指定大小和初始值
mc::array<int> arr3(10, 42);

// 初始化列表
mc::array<int> arr4 = {1, 2, 3, 4, 5};

// 从 std::vector 构造
std::vector<int> vec = {1, 2, 3};
mc::array<int>   arr5(vec);
```

    ## #引用语义示例

```cpp
               mc::array<int>
               arr1 = {1, 2, 3};
mc::array<int> arr2 = arr1; // 共享数据
mc::array<int> arr3 = arr1; // 也共享同一份数据

// 修改任何一个都会影响其他
arr1[0] = 100;
// 此时 arr1[0], arr2[0], arr3[0] 都是 100
```

    ## #深拷贝

    如果需要独立的副本，使用 `deep_copy()` 方法：

```cpp
    mc::array<int> arr1 = {1, 2, 3};
mc::array<int> arr2     = arr1.deep_copy(); // 独立副本

arr1[0] = 100;
// arr2[0] 仍然是 1
```

    ## #与 mc::variant 配合使用

`mc::array` 可以完美替代 `mc::variants`：

```cpp
        // 使用 mc::array<mc::variant> 替代 mc::variants
            mc::array<mc::variant>
            arr;

arr.push_back(mc::variant(42));
arr.push_back(mc::variant("hello"));
arr.push_back(mc::variant(3.14));
arr.push_back(mc::variant(true));

// 访问元素
EXPECT_EQ(arr[0], 42);
EXPECT_EQ(arr[1], "hello");
EXPECT_EQ(arr[2], 3.14);
EXPECT_EQ(arr[3], true);
```

    ## #嵌套数组

```cpp
            mc::array<mc::array<int>>
            matrix;

matrix.push_back(mc::array<int>{1, 2, 3});
matrix.push_back(mc::array<int>{4, 5, 6});
matrix.push_back(mc::array<int>{7, 8, 9});

// 访问元素
int val = matrix[1][1]; // 5
```

    ## #转换为 std::vector

```cpp
                 mc::array<int>
                 arr = {1, 2, 3, 4, 5};
std::vector<int> vec = arr.to_vector(); // 深拷贝到 std::vector
```

    ##API 参考

    ## #构造函数

```cpp
    array();                            // 默认构造
array(size_type count);                 // 指定大小
array(size_type count, const T& value); // 指定大小和值
array(InputIt first, InputIt last);     // 迭代器范围
array(std::initializer_list<T> init);   // 初始化列表
array(const vector_type& vec);          // 从 std::vector
array(vector_type&& vec);               // 移动 std::vector
```

    ## #元素访问

```cpp
        reference
                operator[](size_type pos);
const_reference operator[](size_type pos) const;
reference       at(size_type pos);
const_reference at(size_type pos) const;
reference       front();
const_reference front() const;
reference       back();
const_reference back() const;
T*              data() noexcept;
const T*        data() const noexcept;
```

    ## #迭代器

```cpp
        iterator
                       begin();
const_iterator         begin() const;
const_iterator         cbegin() const;
iterator               end();
const_iterator         end() const;
const_iterator         cend() const;
reverse_iterator       rbegin();
const_reverse_iterator rbegin() const;
reverse_iterator       rend();
const_reverse_iterator rend() const;
```

    ## #容量

```cpp bool
          empty() const noexcept;
size_type size() const noexcept;
size_type max_size() const noexcept;
void      reserve(size_type new_cap);
size_type capacity() const noexcept;
void      shrink_to_fit();
```

    ## #修改器

```cpp void
          clear();
iterator  insert(const_iterator pos, const T& value);
iterator  insert(const_iterator pos, T&& value);
iterator  insert(const_iterator pos, size_type count, const T& value);
iterator  insert(const_iterator pos, InputIt first, InputIt last);
iterator  insert(const_iterator pos, std::initializer_list<T> ilist);
iterator  emplace(const_iterator pos, Args&&... args);
iterator  erase(const_iterator pos);
iterator  erase(const_iterator first, const_iterator last);
void      push_back(const T& value);
void      push_back(T&& value);
reference emplace_back(Args&&... args);
void      pop_back();
void      resize(size_type count);
void      resize(size_type count, const value_type& value);
void      swap(array& other) noexcept;
```

    ## #比较运算符

```cpp bool
     operator==(const array& other) const;
bool operator!=(const array& other) const;
bool operator<(const array& other) const;
bool operator<=(const array& other) const;
bool operator>(const array& other) const;
bool operator>=(const array& other) const;
```

    ## #其他方法

```cpp
        array
                      deep_copy() const; // 深拷贝
vector_type           to_vector() const; // 转换为 std::vector
shared_ptr<impl_type> get_data() const;  // 获取内部数据指针
```

        ##性能特点

        1. *
        *拷贝操作**：O(1) -
    只拷贝指针，不拷贝数据
        2. *
        *修改操作**：与 `std::vector` 相同
        3. *
        *内存开销**：对象本身只占 8 字节（一个指针）

        ##适用场景

    - 需要频繁拷贝数组但不需要修改的场景
    - 需要在多个地方共享同一份数据的场景
    - 存储在 `mc::variant` 中作为数组类型
    - 替换 `mc::variants` 以获得更好的性能

          ##注意事项

          1. *
          *共享语义**：拷贝后的对象共享数据，修改一个会影响另一个 2. * *深拷贝**：如需独立副本，使用 `deep_copy()` 方法 3. * *线程安全**：多线程访问同一份数据需要额外的同步措施
