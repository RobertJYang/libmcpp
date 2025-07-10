/*
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef MC_DBUS_SERIALIZE_H
#define MC_DBUS_SERIALIZE_H

#include <mc/dbus/message.h>
#include <mc/dict.h>
#include <mc/variant.h>

namespace mc::dbus::serialize {
constexpr size_t      MAX_COOKIE           = 32;
constexpr size_t      BLOCK_SIZE           = 128;
constexpr size_t      MAX_DEPTH            = 32;
constexpr uint8_t     COOKIE_NUMBER_ZERO   = 0;
constexpr uint8_t     COOKIE_NUMBER_BYTE   = 1;
constexpr uint8_t     COOKIE_NUMBER_WORD   = 2;
constexpr uint8_t     COOKIE_NUMBER_DWORD  = 4;
constexpr uint8_t     COOKIE_NUMBER_QWORD  = 6;
constexpr uint8_t     COOKIE_NUMBER_REAL   = 8;
constexpr const char* ERROR_INVALID_FORMAT = "invalid format";
constexpr uint8_t     TYPE_MASK            = 7;
constexpr uint8_t     VALUE_SHIFT          = 3;

std::string pack(const variants& args);

variants unpack(std::string_view msg);

enum class data_type : uint8_t {
    nil          = 0,
    boolean      = 1,
    number       = 2,
    userdata     = 3,
    short_string = 4,
    long_string  = 5,
    table        = 6,
    gvariant     = 7,
    tail         = 8
};

struct data_block {
    data_block()
        : next(nullptr), buf(BLOCK_SIZE) {
    }

    data_block*          next;
    std::vector<uint8_t> buf;
};

class write_buffer {
public:
    write_buffer();
    ~write_buffer();
    void        write_arg(const variant& arg, int depth = 0);
    void        write_arg_with_signature(signature_iterator it, const variant& arg, int depth = 0);
    void        write_array(signature_iterator it, const variants& args, int depth = 0);
    void        write_variant_elements(signature_iterator it, const variants& args, int depth = 0);
    std::string to_string() const;

private:
    void write_array_or_dict(signature_iterator it, const variant& arg, int depth);
    void write_inner(const uint8_t* input, size_t size);
    void write_nil();
    void write_boolean(bool value);
    void write_number(const variant& arg);
    void write_double(double value);
    void write_integer(int64_t value);
    void write_string(std::string_view value);
    void write_array(const variants& args, int depth);
    void write_dict(const dict& args, int depth);
    void write_dict(signature_iterator it, const dict& args, int depth);
    void write_gvariant(const variant& arg);

    data_block* m_head;
    data_block* m_current;
    size_t      m_len;
    size_t      m_offset;
};

class read_buffer {
public:
    read_buffer(std::string_view msg);
    const char* read(size_t size);
    variant     read_value(uint8_t type, uint8_t cookie);

private:
    variant     read_inner();
    int64_t     read_integer(uint8_t cookie);
    double      read_double();
    void*       read_pointer();
    std::string read_string(size_t len);
    variant     read_table(int64_t array_size);
    variant     read_gvariant(size_t len);

    std::string_view m_buf;
    size_t           m_offset;
};

} // namespace mc::dbus::serialize

#endif // MC_DBUS_SERIALIZE_H