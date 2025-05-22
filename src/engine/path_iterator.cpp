#include "mc/engine/path_iterator.h"

namespace mc::engine {

path_iterator::path_iterator(std::string_view path)
    : m_path(path), m_start(0), m_end(0), m_is_initialized(false) {
    // 去除尾部斜杠
    if (!m_path.empty() && m_path.back() == IMMUTABLE_PATH_SEP) {
        m_path = m_path.substr(0, m_path.length() - 1);
    }
}

void path_iterator::reset() {
    m_start          = 0;
    m_end            = 0;
    m_is_initialized = false;
}

bool path_iterator::is_empty_or_root_path() const {
    return m_path.empty() || m_path == "/";
}

size_t path_iterator::skip_separators_forward(size_t pos) const {
    while (pos < m_path.length() && m_path[pos] == IMMUTABLE_PATH_SEP) {
        pos++;
    }
    return pos;
}

size_t path_iterator::skip_separators_backward(size_t pos) const {
    while (pos > 0 && m_path[pos - 1] == IMMUTABLE_PATH_SEP) {
        pos--;
    }
    return pos;
}

size_t path_iterator::find_segment_end(size_t start_pos) const {
    size_t end = start_pos;
    while (end < m_path.length() && m_path[end] != IMMUTABLE_PATH_SEP) {
        end++;
    }
    return end;
}

size_t path_iterator::find_prev_segment_start(size_t pos) const {
    // 找到前一个分隔符的位置
    while (pos > 0 && m_path[pos - 1] != IMMUTABLE_PATH_SEP) {
        pos--;
    }
    return pos;
}

bool path_iterator::find_next_segment(size_t& start, size_t& end) const {
    // 空路径或根路径没有段
    if (is_empty_or_root_path()) {
        return false;
    }

    // 从起始位置开始查找下一个段
    start = end;

    // 跳过分隔符
    start = skip_separators_forward(start);

    // 已经到达路径末尾
    if (start >= m_path.length()) {
        return false;
    }

    // 找到段的结束位置
    end = find_segment_end(start);

    return true;
}

bool path_iterator::find_prev_segment(size_t& start, size_t& end) const {
    // 无法向前移动的情况
    if (is_empty_or_root_path() || m_start == 0) {
        return false;
    }

    // 找到当前段前的最后一个分隔符
    size_t sep_pos = m_start;
    sep_pos        = skip_separators_backward(sep_pos);

    // 如果已经到开头，没有前一个段
    if (sep_pos == 0) {
        return false;
    }

    // 找到前一个段的结束位置
    end = sep_pos;

    // 找到前一个段的开始位置
    start = find_prev_segment_start(end);

    return true;
}

bool path_iterator::to_next() {
    // 第一次调用 to_next()
    if (!m_is_initialized) {
        bool has_segment = find_next_segment(m_start, m_end);
        m_is_initialized = true;
        return has_segment;
    }

    // 尝试移动到下一个段
    size_t new_start = m_end, new_end = m_end;
    if (!find_next_segment(new_start, new_end)) {
        return false; // 已经是最后一个段
    }

    // 更新位置
    m_start = new_start;
    m_end   = new_end;
    return true;
}

bool path_iterator::to_prev() {
    // 还没有初始化或已在第一个段
    if (!m_is_initialized) {
        m_start          = m_path.length();
        bool has_segment = find_prev_segment(m_start, m_end);
        m_is_initialized = true;
        return has_segment;
    }

    // 尝试查找前一个段
    size_t new_start = 0, new_end = 0;
    if (!find_prev_segment(new_start, new_end)) {
        return false; // 已经是第一个段
    }

    // 更新位置
    m_start = new_start;
    m_end   = new_end;
    return true;
}

std::string_view path_iterator::current() const {
    if (!m_is_initialized || m_start >= m_path.length()) {
        return {};
    }
    return m_path.substr(m_start, m_end - m_start);
}

std::string_view path_iterator::parent_path() const {
    if (!m_is_initialized || m_start == 0) {
        return {};
    }

    if (m_start == 1) {
        return "/";
    }

    return m_path.substr(0, m_start - 1);
}

} // namespace mc::engine
