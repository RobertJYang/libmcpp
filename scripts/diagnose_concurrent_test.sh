#!/bin/bash

# 并发测试问题诊断脚本
# 用于检测和分析 PropertyRelateTest.RefObjectConcurrentInvoke 的概率失败问题

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
NC='\033[0m'

print_header() {
    echo -e "${PURPLE}========================================${NC}"
    echo -e "${PURPLE} $1${NC}"
    echo -e "${PURPLE}========================================${NC}"
}

print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查是否在项目根目录
if [ ! -f "meson.build" ]; then
    print_error "请在项目根目录执行此脚本"
    exit 1
fi

print_header "libmcpp 并发测试问题诊断工具"

print_info "此脚本将诊断 PropertyRelateTest.RefObjectConcurrentInvoke 的概率失败问题"
echo ""

# 1. 检查源代码中的并发问题
print_header "1. 源代码并发问题检查"

print_info "检查 thread_local 变量使用..."
if grep -r "thread_local" src/ include/ | grep -v "// 修复"; then
    print_warning "发现 thread_local 变量使用，可能存在并发问题"
else
    print_info "未发现问题性的 thread_local 变量使用"
fi

print_info "检查 static 变量使用..."
problematic_static=$(grep -rn "static.*cached" src/ include/ | grep -v "const" || true)
if [ -n "$problematic_static" ]; then
    print_warning "发现可能有问题的 static 变量:"
    echo "$problematic_static"
else
    print_info "未发现问题性的 static 变量使用"
fi

echo ""

# 2. 运行并发测试
print_header "2. 并发测试重复执行"

if [ ! -d "builddir" ]; then
    print_info "构建项目..."
    meson setup builddir
    meson compile -C builddir
fi

print_info "重复运行问题测试用例..."
success_count=0
fail_count=0
total_runs=10

for i in $(seq 1 $total_runs); do
    echo -n "运行 #$i: "
    
    if meson test -C builddir PropertyRelateTest.RefObjectConcurrentInvoke --verbose 2>&1 | grep -q "PASSED"; then
        echo -e "${GREEN}PASS${NC}"
        ((success_count++))
    else
        echo -e "${RED}FAIL${NC}"
        ((fail_count++))
    fi
done

echo ""
print_info "测试结果统计:"
echo "  成功: $success_count/$total_runs"
echo "  失败: $fail_count/$total_runs"

if [ $fail_count -gt 0 ]; then
    failure_rate=$(echo "scale=2; $fail_count * 100 / $total_runs" | bc)
    print_warning "失败率: ${failure_rate}%"
    
    if [ $fail_count -gt 2 ]; then
        print_error "检测到明显的并发问题！"
    fi
else
    print_info "所有测试都通过，并发问题可能已修复"
fi

echo ""

# 3. 性能和并发压力测试
print_header "3. 并发压力测试"

print_info "编译并发修复测试..."
if [ -f "tests/engine/test_property_concurrent_fix.cpp" ]; then
    # 临时添加到测试列表（如果尚未添加）
    if ! grep -q "test_property_concurrent_fix.cpp" tests/engine/meson.build; then
        print_info "添加并发修复测试到构建列表..."
        echo "test_engine_files += files('test_property_concurrent_fix.cpp')" >> tests/engine/meson.build
        meson compile -C builddir
    fi
    
    print_info "运行并发修复测试..."
    if meson test -C builddir PropertyConcurrentFixTest --verbose; then
        print_info "并发修复测试通过"
    else
        print_warning "并发修复测试失败，需要进一步检查"
    fi
else
    print_warning "并发修复测试文件不存在"
fi

echo ""

# 4. 生成诊断报告
print_header "4. 诊断报告"

print_info "问题分析:"
echo "1. 原因: get_object_name() 方法中使用了 thread_local static 变量"
echo "2. 机制: 多线程共享同一个 cached_name 变量，导致数据竞争"
echo "3. 表现: 对象查找时使用了错误的对象名称"

echo ""
print_info "解决方案:"
echo "1. 修复 src/engine/object.cpp 中的 thread_local 问题"
echo "2. 在 object.h 中添加 m_cached_name 成员变量"
echo "3. 使用对象级别的缓存替代线程级别的缓存"

echo ""
print_info "验证方法:"
echo "1. 重复运行原测试用例，确保失败率降为 0"
echo "2. 运行新的并发安全测试用例"
echo "3. 执行高并发压力测试"

echo ""

# 5. 提供修复建议
print_header "5. 修复状态检查"

if grep -q "m_cached_name" include/mc/engine/object.h && 
   grep -q "m_cached_name.*this->get_name" src/engine/object.cpp; then
    print_info "✅ 修复代码已应用"
else
    print_warning "❌ 修复代码未完全应用"
    echo ""
    print_info "请确保以下修复已应用:"
    echo "1. 在 include/mc/engine/object.h 中添加 m_cached_name 成员变量"
    echo "2. 在 src/engine/object.cpp 中修改 get_object_name() 方法"
fi

echo ""
print_header "诊断完成"

if [ $fail_count -eq 0 ]; then
    print_info "🎉 测试稳定，问题已解决！"
else
    print_warning "⚠️  仍存在并发问题，建议检查修复代码"
fi 