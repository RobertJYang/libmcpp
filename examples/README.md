# 插件动态加载示例

本目录包含了一个示例插件，演示如何创建可以被动态加载的插件。

## 插件开发

要创建一个可以被动态加载的插件，需要遵循以下步骤：

1. 创建一个继承自 `appbase::plugin` 的插件类
2. 实现所有必要的虚函数：`name()`, `initialize()`, `startup()`, `shutdown()`
3. 导出一个名为 `create_plugin` 的 C 函数，用于创建插件实例

示例代码如下：

```cpp
#include "appbase/plugin.h"

class my_plugin : public appbase::plugin {
public:
    std::string name() const override { return "my_plugin"; }
    bool initialize() override { return true; }
    void startup() override {}
    void shutdown() override {}
};

extern "C" appbase::plugin* create_plugin() {
    return new my_plugin();
}
```

## 编译插件

使用 meson 构建系统编译插件：

```bash
meson setup builddir
cd builddir
meson compile
```

编译后的插件将生成为 `libmy_plugin.so`（Linux）或 `libmy_plugin.dylib`（macOS）。

## 加载插件

在应用程序中加载插件：

```cpp
#include "appbase/application.h"

int main(int argc, char** argv) {
    appbase::application& app = appbase::application::instance();
    
    // 设置插件目录
    app.set_plugin_dir("/path/to/plugins");
    
    // 初始化应用程序，解析命令行参数
    app.initialize(argc, argv);
    
    // 启动应用程序
    app.startup();
    
    // 应用程序主循环
    // ...
    
    // 关闭应用程序
    app.shutdown();
    
    return 0;
}
```

也可以通过命令行参数加载插件：

```bash
./my_app --plugin-dir=/path/to/plugins --plugin=my_plugin,another_plugin
```

## 示例插件

本目录中的示例插件展示了一个基本的插件实现。查看 `example_plugin.cpp` 了解更多细节。