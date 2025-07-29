#!/bin/bash

# 覆盖率模式演示脚本
# 展示快速模式与完整模式的性能对比

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${PURPLE}================================${NC}"
    echo -e "${PURPLE} $1${NC}"
    echo -e "${PURPLE}================================${NC}"
}

print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

print_timing() {
    echo -e "${YELLOW}[TIME]${NC} $1"
}

# 检查是否在项目根目录
if [ ! -f "meson.build" ]; then
    echo -e "${RED}[ERROR]${NC} 请在项目根目录执行此脚本"
    exit 1
fi

print_header "libmcpp 覆盖率模式演示"

print_info "此脚本将演示两种覆盖率模式的性能差异："
echo "  1. 快速模式：仅生成 coverage_summary.txt"
echo "  2. 完整模式：生成 HTML 报告和所有统计文件"
echo ""

# 确保有已编译的项目
print_step "检查编译状态..."
if [ ! -d "builddir" ] || [ ! -f "builddir/compile_commands.json" ]; then
    print_info "需要先编译项目（启用覆盖率）..."
    ./scripts/smart_build.sh --coverage
fi

# 检查是否有覆盖率数据
if ! find builddir -name "*.gcda" | grep -q .; then
    print_info "运行测试以生成覆盖率数据..."
    meson test -C builddir
fi

print_header "模式1: 快速覆盖率模式"
print_info "目标：仅生成 coverage_summary.txt 文件"

# 清理之前的覆盖率文件
rm -rf coverage_fast

start_time=$(date +%s.%N)
print_step "执行快速模式..."
./scripts/coverage.sh --fast --output coverage_fast
end_time=$(date +%s.%N)

fast_duration=$(echo "$end_time - $start_time" | bc)
print_timing "快速模式用时: ${fast_duration}秒"

# 检查生成的文件
print_info "生成的文件:"
ls -la coverage_fast/ | grep -E '\.(txt|info)$' || echo "  仅 coverage_summary.txt"

echo ""
print_header "模式2: 完整覆盖率模式"
print_info "目标：生成完整的 HTML 报告和所有统计文件"

# 清理之前的覆盖率文件
rm -rf coverage_full

start_time=$(date +%s.%N)
print_step "执行完整模式..."
./scripts/coverage.sh --output coverage_full
end_time=$(date +%s.%N)

full_duration=$(echo "$end_time - $start_time" | bc)
print_timing "完整模式用时: ${full_duration}秒"

# 检查生成的文件
print_info "生成的文件:"
find coverage_full -type f | sort

echo ""
print_header "性能对比结果"

# 计算加速比
speedup=$(echo "scale=1; $full_duration / $fast_duration" | bc)

echo "| 模式 | 执行时间 | 生成文件数 | 适用场景 |"
echo "|------|----------|------------|----------|"
echo "| 快速模式 | ${fast_duration}s | $(find coverage_fast -type f | wc -l) | CI/CD、快速检查 |"
echo "| 完整模式 | ${full_duration}s | $(find coverage_full -type f | wc -l) | 详细分析、代码审查 |"
echo ""
print_timing "快速模式比完整模式快 ${speedup}x 倍！"

echo ""
print_header "覆盖率数据对比"
print_info "快速模式结果："
cat coverage_fast/coverage_summary.txt | grep -E "(lines|functions|branches)"

print_info "完整模式结果："
cat coverage_full/coverage_summary.txt | grep -E "(lines|functions|branches)"

echo ""
print_header "使用建议"
echo "🚀 日常开发和 CI/CD："
echo "   ./scripts/coverage.sh --fast"
echo ""
echo "🔍 详细分析和代码审查："
echo "   ./scripts/coverage.sh --clean --open"
echo ""
echo "⚡ 一键编译 + 快速覆盖率："
echo "   ./scripts/smart_build.sh --fast-coverage"

print_info "演示完成！生成的覆盖率文件保存在 coverage_fast/ 和 coverage_full/ 目录中" 