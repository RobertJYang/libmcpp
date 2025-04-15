# MC++ 反射系统使用指南

## 1. 概述

mc 反射系统提供了一种在运行时获取和操作类型信息的机制。通过反射系统，你可以：

- 获取类型名称
- 访问类的成员变量
- 序列化和反序列化对象
- 支持枚举类型的字符串转换

## 2. 基本用法

### 2.1 为类添加反射支持

使用 `MC_REFLECT` 宏来为类添加反射支持。示例：

```cpp
class Person {
public:
    std::string name;
    int age;
    bool is_male;

    Person() : name(""), age(0), is_male(false) {}
    Person(const std::string& n, int a, bool m)
        : name(n), age(a), is_male(m) {}
};

// 添加反射支持
MC_REFLECT(Person, (name)(age)(is_male))
```

### 2.2 为枚举添加反射支持

使用 `MC_REFLECT_ENUM` 宏来为枚举添加反射支持：

```cpp
enum class Color { RED, GREEN, BLUE };

// 添加反射支持
MC_REFLECT_ENUM(Color, (RED)(GREEN)(BLUE))
```

## 3. 反射API

### 3.5 部分对象更新
```cpp
// 部分更新对象成员
Person person("张三", 30, true);
mc::reflect::from_variant(mc::dict{{"age", 35}}, person);
// 仅更新age字段，其他字段保持不变
```

### 3.6 嵌套对象处理
```cpp
class Address {
public:
    std::string city;
    int zip_code;
};
MC_REFLECT(Address, (city)(zip_code));

class Company {
public:
    std::string name;
    Address address; 
};
MC_REFLECT(Company, (name)(address));

// 序列化嵌套对象
Company company{"Example", {"Beijing", 100080}};
mc::variant var = company;
// 反序列化时会自动处理嵌套结构
```

### 3.1 类型信息查询

```cpp
// 检查类型是否支持反射
bool supported = mc::reflect::is_reflectable<Person>();

// 检查类型是否为枚举
bool is_enum = mc::reflect::is_enum<Color>();

// 获取类型名称
const char* type_name = mc::reflect::get_type_name<Person>();
```

### 3.2 成员访问

```cpp
Person person("张三", 30, true);

// 访问所有成员
mc::reflect::visit_members<Person>([&](const char* name,
                                     auto getter,
                                     auto setter) {
    // name: 成员名称
    // getter: 获取成员值的函数
    // setter: 设置成员值的函数
    mc::variant value = getter(person);
    std::cout << name << ": " << value << std::endl;
});
```

### 3.3 序列化和反序列化

```cpp
// 序列化为变体
Person person("张三", 30, true);
mc::variant var(person);

// 检查序列化结果
const mc::dict& dict = var.as<mc::dict>();
assert(dict["name"] == "张三");
assert(dict["age"] == 30);
assert(dict["is_male"] == true);

// 反序列化
Person new_person = var.as<Person>();
```

#### 面向C语言开发者的详细解释

##### 什么是序列化和反序列化？

- **序列化**：将对象转换为可传输或存储的格式（例如字节流、字符串、数据结构）
- **反序列化**：将序列化的数据恢复为原始对象

在C语言中，通常需要手动编写代码来打包/解包结构体。在C++中，特别是使用反射系统，这个过程可以自动化。

##### 代码逐行解析：

1. **序列化部分**:
   ```cpp
   Person person("张三", 30, true);
   ```
   - 创建一个Person对象，相当于C语言中的结构体初始化
   - 使用了C++的构造函数语法，在C语言中可能是这样：
     ```c
     struct Person person;
     strcpy(person.name, "张三");
     person.age = 30;
     person.is_male = true;
     ```

   ```cpp
   mc::variant var(person);
   ```
   - 这行代码将Person对象序列化为variant容器
   - variant是一个通用容器，可以存储多种类型的数据（类似union但更强大）
   - 在C语言中可能需要手动实现：
     ```c
     // 手动填充序列化结构
     serialize_person(&person, &serialized_data);
     ```

2. **检查序列化结果**:
   ```cpp
   const mc::dict& dict = var.as<mc::dict>();
   ```
   - 这行代码是将variant中的内容提取为dict类型（键值对映射）
   - `const`：表示该引用指向的内容不可修改
   - `mc::dict&`：C++引用类型，类似指针但更安全，无需手动解引用
   - `var.as<mc::dict>()`：调用variant的模板方法as，指定要转换的目标类型为mc::dict
   - 在C语言中的类似操作：
     ```c
     struct Dictionary* dict_ptr = get_dict_from_variant(&var);
     if(dict_ptr == NULL) {
         // 处理错误
     }
     ```

   ```cpp
   assert(dict["name"] == "张三");
   assert(dict["age"] == 30);
   assert(dict["is_male"] == true);
   ```
   - 验证序列化结果是否正确
   - dict支持使用[]操作符按键访问（类似哈希表）
   - 在C语言中可能是：
     ```c
     assert(strcmp(get_string_from_dict(dict_ptr, "name"), "张三") == 0);
     assert(get_int_from_dict(dict_ptr, "age") == 30);
     assert(get_bool_from_dict(dict_ptr, "is_male") == true);
     ```

3. **反序列化部分**:
   ```cpp
   Person new_person = var.as<Person>();
   ```
   - 将variant转换回Person对象
   - 在C语言中可能需要：
     ```c
     struct Person new_person;
     deserialize_person(&serialized_data, &new_person);
     ```

##### mc::variant与mc::dict的关系

- **mc::variant**：通用数据容器，可以存储任意类型（整数、字符串、对象等）
- **mc::dict**：键值对映射，类似于C语言中的哈希表，但提供更高级的API

##### 实际应用场景

此代码在以下场景非常有用：
- 配置文件处理（加载/保存用户设置）
- 网络数据传输（API接口数据）
- 数据持久化（保存应用状态）
- 跨语言数据交换（如生成JSON数据）

### 3.4 枚举转换

```cpp
// 枚举转字符串
Color color = Color::GREEN;
mc::variant var(color);
assert(var == "GREEN");

// 字符串转枚举
Color new_color = var.as<Color>();
assert(new_color == Color::GREEN);

// 无效值会抛出异常
try {
    mc::variant invalid_var = "YELLOW";
    Color invalid_color = invalid_var.as<Color>();
} catch (const mc::reflect::bad_enum_cast& e) {
    std::cerr << e.what() << std::endl;
}
```

## 4. 测试覆盖

我们的测试用例验证了以下关键场景：

1. **基础功能验证**
   - 类/枚举的反射元数据正确性
   - 对象序列化/反序列化的完整性
   - 枚举类型的双向转换

2. **高级场景验证**
   - 嵌套对象的多层反射支持
   - STL容器(vector/map)的序列化
   - 对象的部分字段更新
   - 复杂类型系统的异常处理

3. **边界条件验证**
   - 无效枚举值的转换异常
   - 空对象的序列化处理
   - 包含特殊字符的字符串序列化

## 5. 最佳实践

1. 只反射需要序列化或运行时访问的成员
2. 使用枚举反射时，确保所有枚举值都已在 `MC_REFLECT_ENUM` 中声明
3. 处理反序列化时要考虑异常情况
4. 对于大型对象，建议使用引用传递避免不必要的拷贝

## 5. 注意事项

5. 支持以下STL容器类型：
   - vector
   - map
   - unordered_map
   - pair
   - tuple

   示例：
   ```cpp
   class Data {
   public:
       std::vector<int> scores;
       std::map<std::string, Address> locations;
   };
   MC_REFLECT(Data, (scores)(locations));

   // 支持复杂容器的序列化
   Data d{{"home", {}}, {"work", {}}};
   variant var = d;
   ```

1. 反射系统目前支持基本类型、STL容器和自定义类型
2. 反射的成员必须是公有的
3. 使用反射API时注意异常处理
4. 枚举反射不支持自定义值，只支持连续的枚举值

## 6. 完整示例

```cpp
#include <mc/reflect.h>
#include <iostream>

// 定义枚举
enum class Status { ACTIVE, INACTIVE, SUSPENDED };
MC_REFLECT_ENUM(Status, (ACTIVE)(INACTIVE)(SUSPENDED))

// 定义类
class User {
public:
    std::string username;
    int id;
    Status status;

    User() : username(""), id(0), status(Status::INACTIVE) {}
    User(const std::string& name, int uid, Status s)
        : username(name), id(uid), status(s) {}
};
MC_REFLECT(User, (username)(id)(status))

int main() {
    // 创建对象
    User user("admin", 1, Status::ACTIVE);

    // 序列化
    mc::variant var(user);
    std::cout << "Serialized: " << var << std::endl;

    // 修改序列化数据
    mc::mutable_dict dict = var.as<mc::dict>();
    dict["status"] = "SUSPENDED";

    // 反序列化
    User new_user = mc::variant(dict).as<User>();
    assert(new_user.status == Status::SUSPENDED);

    return 0;
}
```

这个示例展示了如何使用反射系统实现对象的序列化和反序列化，以及如何处理枚举类型。通过这个例子，你可以了解反射系统的主要功能和使用方法。