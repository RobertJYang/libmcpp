#include <mc/engine/utils.h>
#include <mc/string.h>

namespace mc::engine::detail {

bool path_starts_with(std::string_view path, std::string_view prefix) {
    if (!mc::string::starts_with(path, prefix)) {
        return false;
    }

    // 是根路径
    if (prefix.size() == 1 && prefix[0] == '/') {
        return true;
    }

    // 否则必须判断末尾是 / 分隔符
    return path.size() > prefix.size() && path[prefix.size()] == '/';
}

} // namespace mc::engine::detail
