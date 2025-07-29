#!/bin/bash

# 代码覆盖率报告生成脚本
# 日期：2025-01-25
# 版本：1.0
# 说明：本脚本用于生成 libmcpp 项目的代码覆盖率报告
# 使用方法：在项目根目录下执行 ./scripts/coverage.sh
# 注意事项：本脚本需要先运行带覆盖率的编译和测试

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 打印带颜色的消息
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

# 显示使用方法
show_usage() {
    echo "使用方法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  --clean          清理并重新编译，然后生成覆盖率报告"
    echo "  --output DIR     指定覆盖率报告输出目录（默认: coverage）"
    echo "  --build-dir DIR  指定构建目录（默认: builddir）"
    echo "  --open           生成报告后自动打开 HTML 报告"
    echo "  --fast           快速模式，仅生成 coverage_summary.txt"
    echo "  --help           显示此帮助信息"
    echo ""
    echo "示例:"
    echo "  $0                              # 使用现有编译结果生成覆盖率报告"
    echo "  $0 --clean                     # 清理并重新编译，然后生成覆盖率报告"
    echo "  $0 --output my_coverage --open # 生成报告到 my_coverage 目录并打开"
    echo "  $0 --fast                      # 快速生成汇总文件（约10秒内完成）"
}

# 检查依赖工具
check_dependencies() {
    print_step "检查依赖工具..."
    
    local missing_tools=()
    
    if ! command -v gcov >/dev/null 2>&1; then
        missing_tools+=("gcov (包含在gcc中)")
    fi
    
    if ! command -v lcov >/dev/null 2>&1; then
        missing_tools+=("lcov")
    fi
    
    if ! command -v genhtml >/dev/null 2>&1; then
        missing_tools+=("genhtml")
    fi
    
    if [ ${#missing_tools[@]} -gt 0 ]; then
        print_error "缺少必需的工具: ${missing_tools[*]}"
        print_info "请运行以下命令安装："
        echo "  sudo apt-get update"
        echo "  sudo apt-get install lcov gcc g++"
        return 1
    fi
    
    print_info "所有依赖工具已安装"
    return 0
}

# 生成覆盖率报告
generate_coverage_report() {
    local build_dir=$1
    local output_dir=$2
    
    print_step "生成覆盖率报告..."
    
    # 检查构建目录是否存在
    if [ ! -d "$build_dir" ]; then
        print_error "构建目录 $build_dir 不存在"
        print_info "请先运行编译，或使用 --clean 选项"
        return 1
    fi
    
    # 检查是否有覆盖率数据文件
    if ! find "$build_dir" -name "*.gcda" | grep -q .; then
        print_error "在构建目录中没有找到覆盖率数据文件 (*.gcda)"
        print_info "请确保编译时启用了覆盖率选项，并且已经运行了测试"
        print_info "建议使用: ./scripts/smart_build.sh --coverage"
        return 1
    fi
    
    # 创建输出目录
    mkdir -p "$output_dir"
    
    # 收集覆盖率数据
    print_info "收集覆盖率数据..."
    lcov --capture --directory "$build_dir" --output-file "$output_dir/coverage.info" \
        --rc lcov_branch_coverage=1 \
        --rc geninfo_unexecuted_blocks=1 \
        --ignore-errors mismatch,gcov,negative
    
    if [ $? -ne 0 ]; then
        print_error "收集覆盖率数据失败"
        return 1
    fi
    
    # 过滤掉系统头文件和测试文件
    print_info "过滤覆盖率数据..."
    lcov --remove "$output_dir/coverage.info" \
        '/usr/*' \
        '*/tests/*' \
        '*/test/*' \
        '*_test.cpp' \
        '*_test.h' \
        '*/gtest/*' \
        '*/boost/*' \
        '*/builddir/*' \
        --output-file "$output_dir/coverage_filtered.info" \
        --rc lcov_branch_coverage=1 \
        --ignore-errors mismatch,negative,unused
    
    if [ $? -ne 0 ]; then
        print_error "过滤覆盖率数据失败"
        return 1
    fi
    
    # 生成 HTML 报告
    print_info "生成 HTML 覆盖率报告..."
    genhtml "$output_dir/coverage_filtered.info" \
        --output-directory "$output_dir/html" \
        --title "libmcpp 代码覆盖率报告" \
        --show-details \
        --legend \
        --branch-coverage \
        --function-coverage \
        --ignore-errors mismatch,negative
    
    if [ $? -ne 0 ]; then
        print_error "生成 HTML 报告失败"
        return 1
    fi
    
    # 生成覆盖率汇总
    print_info "生成覆盖率汇总..."
    lcov --summary "$output_dir/coverage_filtered.info" \
        --rc lcov_branch_coverage=1 \
        --ignore-errors mismatch,negative > "$output_dir/coverage_summary.txt"
    
    # 生成简化的覆盖率统计
    echo "=== libmcpp 代码覆盖率统计 ===" > "$output_dir/coverage_stats.txt"
    echo "生成时间: $(date)" >> "$output_dir/coverage_stats.txt"
    echo "" >> "$output_dir/coverage_stats.txt"
    
    # 提取关键统计信息
    local line_coverage=$(grep "lines......:" "$output_dir/coverage_summary.txt" | grep -o '[0-9.]*%' | head -1)
    local function_coverage=$(grep "functions..:" "$output_dir/coverage_summary.txt" | grep -o '[0-9.]*%' | head -1)
    local branch_coverage=$(grep "branches...:" "$output_dir/coverage_summary.txt" | grep -o '[0-9.]*%' | head -1)
    
    echo "行覆盖率:     ${line_coverage:-N/A}" >> "$output_dir/coverage_stats.txt"
    echo "函数覆盖率:   ${function_coverage:-N/A}" >> "$output_dir/coverage_stats.txt"
    echo "分支覆盖率:   ${branch_coverage:-N/A}" >> "$output_dir/coverage_stats.txt"
    echo "" >> "$output_dir/coverage_stats.txt"
    echo "详细报告: $output_dir/html/index.html" >> "$output_dir/coverage_stats.txt"
    
    # 显示结果
    print_info "覆盖率统计结果:"
    echo ""
    cat "$output_dir/coverage_stats.txt"
    echo ""
    
    print_info "覆盖率报告已生成到: $(realpath $output_dir)"
    print_info "HTML 报告位置: $(realpath $output_dir/html/index.html)"
    
    return 0
}

# 快速模式：仅生成 coverage_summary.txt
generate_quick_summary() {
    local build_dir=$1
    local output_dir=$2
    
    print_step "快速模式：生成覆盖率汇总..."
    
    # 检查构建目录是否存在
    if [ ! -d "$build_dir" ]; then
        print_error "构建目录 $build_dir 不存在"
        return 1
    fi
    
    # 检查是否有覆盖率数据文件
    if ! find "$build_dir" -name "*.gcda" | grep -q .; then
        print_error "在构建目录中没有找到覆盖率数据文件 (*.gcda)"
        print_info "请确保编译时启用了覆盖率选项，并且已经运行了测试"
        return 1
    fi
    
    # 创建输出目录
    mkdir -p "$output_dir"
    
    # 直接收集并生成汇总，跳过复杂的过滤步骤
    print_info "收集覆盖率数据..."
    lcov --capture --directory "$build_dir" --output-file "$output_dir/coverage_raw.info" \
        --rc lcov_branch_coverage=1 \
        --ignore-errors mismatch,gcov,negative --quiet > /dev/null 2>&1
    
    if [ $? -ne 0 ]; then
        print_error "收集覆盖率数据失败"
        return 1
    fi
    
    # 生成汇总（直接从原始数据，跳过过滤以节省时间）
    print_info "生成覆盖率汇总..."
    lcov --summary "$output_dir/coverage_raw.info" \
        --rc lcov_branch_coverage=1 \
        --ignore-errors mismatch,negative > "$output_dir/coverage_summary.txt" 2>/dev/null
    
    if [ $? -ne 0 ]; then
        print_error "生成覆盖率汇总失败"
        return 1
    fi
    
    # 清理临时文件
    rm -f "$output_dir/coverage_raw.info"
    
    # 显示结果
    print_info "快速汇总完成！"
    print_info "文件位置: $(realpath $output_dir/coverage_summary.txt)"
    echo ""
    cat "$output_dir/coverage_summary.txt"
    
    return 0
}

# 主函数
main() {
    local clean_build=false
    local output_dir="coverage"
    local build_dir="builddir"
    local open_report=false
    local fast_mode=false
    
    # 解析参数
    while [[ $# -gt 0 ]]; do
        case $1 in
            --clean)
                clean_build=true
                shift
                ;;
            --output)
                output_dir="$2"
                shift 2
                ;;
            --build-dir)
                build_dir="$2"
                shift 2
                ;;
            --open)
                open_report=true
                shift
                ;;
            --fast)
                fast_mode=true
                shift
                ;;
            --help)
                show_usage
                exit 0
                ;;
            *)
                print_error "未知参数: $1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    # 检查是否在项目根目录
    if [ ! -f "meson.build" ]; then
        print_error "请在项目根目录执行此脚本"
        exit 1
    fi
    
    print_info "libmcpp 代码覆盖率报告生成器"
    print_info "输出目录: $output_dir"
    print_info "构建目录: $build_dir"
    
    # 快速模式处理
    if [ "$fast_mode" = true ]; then
        print_info "模式: 快速汇总"
        # 快速模式只需要 lcov，不需要 genhtml
        if ! command -v lcov >/dev/null 2>&1; then
            print_error "lcov 未安装，请运行: sudo apt-get install lcov"
            exit 1
        fi
        
        if ! generate_quick_summary "$build_dir" "$output_dir"; then
            exit 1
        fi
        
        print_info "快速模式完成！"
        exit 0
    fi
    
    print_info "模式: 完整报告"
    
    # 检查依赖工具
    if ! check_dependencies; then
        exit 1
    fi
    
    # 如果需要清理重新编译
    if [ "$clean_build" = true ]; then
        print_step "清理并重新编译..."
        if [ -f "scripts/smart_build.sh" ]; then
            ./scripts/smart_build.sh --clean --coverage --coverage-output "$output_dir"
            if [ $? -ne 0 ]; then
                print_error "编译失败"
                exit 1
            fi
        else
            print_error "找不到 smart_build.sh 脚本"
            exit 1
        fi
    else
        # 生成覆盖率报告
        if ! generate_coverage_report "$build_dir" "$output_dir"; then
            exit 1
        fi
    fi
    
    # 如果需要打开报告
    if [ "$open_report" = true ]; then
        local html_file="$output_dir/html/index.html"
        if [ -f "$html_file" ]; then
            if command -v xdg-open >/dev/null 2>&1; then
                print_info "正在打开 HTML 报告..."
                xdg-open "$html_file"
            elif command -v open >/dev/null 2>&1; then
                print_info "正在打开 HTML 报告..."
                open "$html_file"
            else
                print_warning "无法自动打开浏览器，请手动打开: $html_file"
            fi
        else
            print_error "HTML 报告文件不存在: $html_file"
        fi
    fi
    
    print_info "覆盖率报告生成完成！"
}

# 如果脚本被直接执行
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi 