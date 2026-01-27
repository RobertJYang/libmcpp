--[[
Copyright (c) 2026 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.
]]

local lvalidate = require("lvalidate")
local lu = require("luaunit")

TestValidateRanges = {}

function TestValidateRanges:testValidRanges()
    -- integers and floats within range
    local ok = pcall(lvalidate.ranges, "Number", 1.5, 0, 2, false)
    lu.assertTrue(ok, "float within range should pass")

    ok = pcall(lvalidate.ranges, "Number", 0, 0, 10, false)
    lu.assertTrue(ok, "lower boundary should pass")

    ok = pcall(lvalidate.ranges, "Number", 10, 0, 10, false)
    lu.assertTrue(ok, "upper boundary should pass")
end

function TestValidateRanges:testRangesTypeError()
    local ok, err = pcall(lvalidate.ranges, "Number", "1.5", 0, 2, false)
    lu.assertTrue(not ok, "non-number should raise type error")
    lu.assertNotNil(string.find(err, "PropertyValueTypeError"), "should be type error")
end

function TestValidateRanges:testRangesOutOfRange()
    local ok, err = pcall(lvalidate.ranges, "Number", -1, 0, 10, false)
    lu.assertTrue(not ok, "below min should raise range error")
    lu.assertNotNil(string.find(err, "PropertyValueOutOfRange"), "should be range error")

    ok, err = pcall(lvalidate.ranges, "Number", 11, 0, 10, false)
    lu.assertTrue(not ok, "above max should raise range error")
    lu.assertNotNil(string.find(err, "PropertyValueOutOfRange"), "should be range error")
end

function TestValidateRanges:testRangeOrNone()
    local ok, err
    ok = pcall(lvalidate.range_or_none, "Number", nil, 0, 10, false)
    lu.assertTrue(ok, "nil should skip validation")

    ok = pcall(lvalidate.range_or_none, "Number", 5, 0, 10, false)
    lu.assertTrue(ok, "value within range should pass")

    ok, err = pcall(lvalidate.range_or_none, "Number", "5", 0, 10, false)
    lu.assertTrue(not ok, "non-number should raise type error")
    lu.assertNotNil(string.find(err, "PropertyValueTypeError"), "should be type error")

    ok, err = pcall(lvalidate.range_or_none, "Number", 20, 0, 10, false)
    lu.assertTrue(not ok, "value out of range should raise range error")
    lu.assertNotNil(string.find(err, "PropertyValueOutOfRange"), "should be range error")
end

TestValidateLength = {}

function TestValidateLength:testLensValid()
    local ok = pcall(lvalidate.lens, "Name", "abc", 1, 5, false)
    lu.assertTrue(ok, "length within range should pass")

    ok = pcall(lvalidate.lens, "Name", "", 0, 5, false)
    lu.assertTrue(ok, "min length should pass")

    ok = pcall(lvalidate.lens, "Name", "abcde", 0, 5, false)
    lu.assertTrue(ok, "max length should pass")
end

function TestValidateLength:testLensTypeError()
    local ok, err = pcall(lvalidate.lens, "Name", 123, 0, 5, false)
    lu.assertTrue(not ok, "non-string should raise type error")
    lu.assertNotNil(string.find(err, "PropertyValueTypeError"), "should be type error")
end

function TestValidateLength:testLensTooShortOrTooLong()
    local ok, err = pcall(lvalidate.lens, "Name", "", 1, 5, false)
    lu.assertTrue(not ok, "length below min should raise error")
    lu.assertNotNil(string.find(err, "StringValueTooShort"), "should be length too short error")

    ok, err = pcall(lvalidate.lens, "Name", "abcdef", 0, 5, false)
    lu.assertTrue(not ok, "length above max should raise error")
    lu.assertNotNil(string.find(err, "StringValueTooLong"), "should be length too long error")
end

function TestValidateLength:testLenOrNone()
    local ok, err
    ok = pcall(lvalidate.len_or_none, "Name", nil, 1, 5, false)
    lu.assertTrue(ok, "nil should skip validation")

    ok = pcall(lvalidate.len_or_none, "Name", "abc", 1, 5, false)
    lu.assertTrue(ok, "length within range should pass")

    ok, err = pcall(lvalidate.len_or_none, "Name", 123, 1, 5, false)
    lu.assertTrue(not ok, "non-string should raise type error")
    lu.assertNotNil(string.find(err, "PropertyValueTypeError"), "should be type error")

    ok, err = pcall(lvalidate.len_or_none, "Name", "", 1, 5, false)
    lu.assertTrue(not ok, "length below min should raise error")
    lu.assertNotNil(string.find(err, "StringValueTooShort"), "should be length too short error")
end

TestValidateRegex = {}

function TestValidateRegex:testRegexValid()
    -- simple email regex (standard regex, not Lua pattern)
    local ok = pcall(
        lvalidate.regex,
        "Email",
        "user@example.com",
        "^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]+$",
        false
    )
    lu.assertTrue(ok, "valid email should pass")
end

function TestValidateRegex:testRegexTypeError()
    local ok, err = pcall(lvalidate.regex, "Email", 12345, "^[0-9]+$", false)
    lu.assertTrue(not ok, "non-string should raise type error")
    lu.assertNotNil(string.find(err, "PropertyValueTypeError"), "should be type error")
end

function TestValidateRegex:testRegexFormatError()
    local ok, err = pcall(
        lvalidate.regex,
        "Email",
        "not-an-email",
        "^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]+$",
        false
    )
    lu.assertTrue(not ok, "non-matching value should raise format error")
    lu.assertNotNil(string.find(err, "PropertyValueFormatError"), "should be format error")
end

function TestValidateRegex:testRegexOrNone()
    local ok, err
    ok = pcall(lvalidate.regex_or_none, "Email", nil, "^[0-9]+$", false)
    lu.assertTrue(ok, "nil should skip validation")

    ok = pcall(lvalidate.regex_or_none, "Email", "12345", "^[0-9]+$", false)
    lu.assertTrue(ok, "matching value should pass")

    ok, err = pcall(lvalidate.regex_or_none, "Email", 12345, "^[0-9]+$", false)
    lu.assertTrue(not ok, "non-string should raise type error")
    lu.assertNotNil(string.find(err, "PropertyValueTypeError"), "should be type error")

    ok, err = pcall(lvalidate.regex_or_none, "Email", "abc", "^[0-9]+$", false)
    lu.assertTrue(not ok, "non-matching value should raise format error")
    lu.assertNotNil(string.find(err, "PropertyValueFormatError"), "should be format error")
end

TestValidateJson = {}

function TestValidateJson:testJsonValid()
    local ok = pcall(lvalidate.Json, '{"key": 123, "flag": true}')
    lu.assertTrue(ok, "valid JSON should pass")
end

function TestValidateJson:testJsonNonString()
    local ok, err = pcall(lvalidate.Json, 12345)
    lu.assertTrue(not ok, "non-string should raise MalformedJSON error")
    lu.assertNotNil(string.find(err, "MalformedJSON"), "should be MalformedJSON error")
end

function TestValidateJson:testJsonMalformed()
    local ok, err = pcall(lvalidate.Json, "{invalid json}")
    lu.assertTrue(not ok, "malformed JSON should raise error")
    lu.assertNotNil(string.find(err, "MalformedJSON"), "should be malformed JSON error")
end

