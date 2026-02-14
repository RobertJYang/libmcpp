--[[
Copyright (c) 2025 Huawei Technologies Co., Ltd.
openUBMC is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details.
]]

-- 通过文件 appender 写入日志文件，读取文件内容进行断言

local lu = require('luaunit')
local log = require('mc_log')

local TRACE  = log.TRACE
local DEBUG  = log.DEBUG
local INFO   = log.INFO

TestLog = {}

local TEST_LOG_FILE = '/dev/shm/debug_log_for_test'

function TestLog:setUp()
    lu.assertNotNil(log)
    log:set_level(TRACE)
    log:clear_appenders()
    local ok = log:add_file_appender(TEST_LOG_FILE, true, 'unit_test')
    lu.assertTrue(ok, 'add_file_appender should succeed')
end

function TestLog:tearDown()
    log:clear_appenders()
    os.remove(TEST_LOG_FILE)
end

local function read_log_file()
    local f = io.open(TEST_LOG_FILE, 'r')
    if not f then
        return ''
    end
    local content = f:read('*a')
    f:close()
    return content or ''
end

-- 校验打印格式：时间 module_name LEVEL 文件(行): 消息
local function assert_standard_log_format(content, level_str, module_name)
    module_name = module_name or 'module_name'
    lu.assertNotNil(string.find(content, module_name))
    lu.assertNotNil(string.find(content, level_str))
    lu.assertNotNil(string.find(content, 'test_log%.lua%(%d+%):'))
end

-- 校验打印格式：log:notice("module_name", "test log") 应呈现为 时间 module_name NOTICE 文件(行): test log
function TestLog:test_notice_with_module_name_print_format()
    log:notice('module_name', 'test log')
    os.execute('sleep 0.1')
    local content = read_log_file()
    lu.assertNotNil(string.find(content, 'module_name'))
    lu.assertNotNil(string.find(content, 'NOTICE'))
    lu.assertNotNil(string.find(content, 'test log'))
    lu.assertNotNil(string.find(content, 'test_log%.lua%(%d+%):'))
end

-- ---------- 仅消息 ----------
function TestLog:test_info_message_only()
    log:notice('module_name', 'message without attrs')
    os.execute('sleep 0.1')
    local content = read_log_file()
    assert_standard_log_format(content, 'NOTICE')
    lu.assertNotNil(string.find(content, 'message without attrs'))
end

-- ---------- 消息 + attrs ----------
function TestLog:test_info_message_and_attrs()
    log:notice('module_name', 'request done', { trace_id = 'abc-123', span_id = 'span-1', count = 42 })
    os.execute('sleep 0.1')
    local content = read_log_file()
    assert_standard_log_format(content, 'NOTICE')
    lu.assertNotNil(string.find(content, 'request done'))
    lu.assertNotNil(string.find(content, 'trace_id='))
    lu.assertNotNil(string.find(content, 'abc%-123'))
    lu.assertNotNil(string.find(content, 'span_id='))
    lu.assertNotNil(string.find(content, 'span%-1'))
    lu.assertNotNil(string.find(content, 'count='))
    lu.assertNotNil(string.find(content, '42'))
end

-- ---------- attrs 嵌套表（两层） ----------
function TestLog:test_info_message_and_attrs_nested_table()
    log:info('module_name', 'request done', {
        trace_id = 'abc-123',
        payload = { code = 200, detail = 'ok' },
    })
    os.execute('sleep 0.1')
    local content = read_log_file()
    assert_standard_log_format(content, 'INFO')
    lu.assertNotNil(string.find(content, 'request done'))
    lu.assertNotNil(string.find(content, 'trace_id='))
    lu.assertNotNil(string.find(content, 'trace_id=.-abc%-123') or string.find(content, 'abc%-123'))
    lu.assertNotNil(string.find(content, 'payload={'))
    lu.assertNotNil(string.find(content, 'code=200'))
    lu.assertNotNil(string.find(content, 'detail=ok'))
end

function TestLog:test_info_message_and_attrs_two_level_nested()
    log:notice('module_name', 'nested attrs', {
        a = 1,
        b = { c = 2, inner = { x = 'y' } },
    })
    os.execute('sleep 0.1')
    local content = read_log_file()
    assert_standard_log_format(content, 'NOTICE')
    lu.assertNotNil(string.find(content, 'nested attrs'))
    lu.assertNotNil(string.find(content, 'a='))
    lu.assertNotNil(string.find(content, '1'))
    lu.assertNotNil(string.find(content, 'b='))
    lu.assertNotNil(string.find(content, 'c=2'))
    lu.assertNotNil(string.find(content, 'inner={'))
    lu.assertNotNil(string.find(content, 'x=y'))
end

function TestLog:test_default_module_name()
    log:info('message with default module')
    os.execute('sleep 0.1')
    local content = read_log_file()
    assert_standard_log_format(content, 'INFO', 'unknown')
    lu.assertNotNil(string.find(content, 'message with default module'))
end

-- ---------- 模板参数 + 可选 attrs ----------
function TestLog:test_info_template_args_only()
    log:info('module_name', 'status ${code}', 'code', 200)
    os.execute('sleep 0.1')
    local content = read_log_file()
    assert_standard_log_format(content, 'INFO')
    lu.assertNotNil(string.find(content, 'status 200'))
end

function TestLog:test_info_template_args_and_attrs()
    log:info('module_name', 'user ${user} login', 'user', 'admin', { request_id = 'req-1' })
    os.execute('sleep 0.1')
    local content = read_log_file()
    assert_standard_log_format(content, 'INFO')
    lu.assertNotNil(string.find(content, 'user admin login'))
    lu.assertNotNil(string.find(content, 'request_id='))
    lu.assertNotNil(string.find(content, 'req%-1'))
end

-- ---------- _easy ----------
function TestLog:test_info_easy_message_only()
    log:info_easy('module_name','easy msg')
    os.execute('sleep 0.1')
    local content = read_log_file()
    assert_standard_log_format(content, 'INFO')
    lu.assertNotNil(string.find(content, 'easy msg'))
end

-- ----------  set_level / get_level / is_enabled ----------
function TestLog:test_set_level_get_level()
    log:set_level(DEBUG)
    lu.assertEquals(log:get_level(), DEBUG)
    log:set_level(INFO)
    lu.assertEquals(log:get_level(), INFO)
end

function TestLog:test_is_enabled()
    log:set_level(DEBUG)
    lu.assertTrue(log:is_enabled(DEBUG))
    lu.assertTrue(log:is_enabled(INFO))
    lu.assertFalse(log:is_enabled(TRACE))
end

-- ---------- system ----------
function TestLog:test_system_with_id()
    log:system(123):warn('test system log')
    os.execute('sleep 0.1')
    local content = read_log_file()
    lu.assertNotNil(string.find(content, '[System123]test system log'))
end

function TestLog:test_system_without_id()
    log:system():warn('test system log without id')
    os.execute('sleep 0.1')
    local content = read_log_file()
    lu.assertNotNil(string.find(content, 'test system log without id'))
end

-- ---------- period ----------
function TestLog:test_period_throttling()
    local ret = log:period(10)
    lu.assertEquals(ret.period_s, 10)
end

-- ---------- raise 抛错 ----------
function TestLog:test_raise_throws()
    local ok, err = pcall(function()
        log:raise('fail: ${reason}', 'reason', 'test')
    end)
    lu.assertFalse(ok, 'raise should throw')
    lu.assertStrContains(tostring(err), 'fail:', 'error should contain formatted template')
end

-- ---------- operation / running / maintenance /security ----------
function TestLog:test_operation_running_maintenance_examples()
    local initiator = { Interface = 'ok', UserName = 'test', ClientIp = '127.0.0.1' }
    log:operation(initiator, 'module_name', 'reset successfully')
    os.execute('sleep 0.1')
    local content = read_log_file()
    lu.assertNotNil(string.find(content, 'reset successfully'))

    log:operation(initiator, 'module_name', '用户 ${user} 执行 ${action}', 'user', 'admin', 'action', '登录')
    os.execute('sleep 0.1')
    content = read_log_file()
    lu.assertNotNil(string.find(content, '用户 admin 执行 登录'))

    log:running(log.RLOG_WARN, 'test running log')
    os.execute('sleep 0.1')
    content = read_log_file()
    lu.assertNotNil(string.find(content, 'test running log'))

    log:running(log.RLOG_INFO, 'service started')
    os.execute('sleep 0.1')
    content = read_log_file()
    lu.assertNotNil(string.find(content, 'service started'))

    log:maintenance(log.MLOG_ERROR, 'SVR-0001111', 'test maintenance log')
    os.execute('sleep 0.1')
    content = read_log_file()
    lu.assertNotNil(string.find(content, 'test maintenance log'))
    lu.assertNotNil(string.find(content, 'SVR%-0001111'))

    log:maintenance(log.MLOG_WARN, 'disk space low')
    os.execute('sleep 0.1')
    content = read_log_file()
    lu.assertNotNil(string.find(content, 'disk space low'))

    log:security('security event')
    os.execute('sleep 0.1')
    local content = read_log_file()
    lu.assertNotNil(string.find(content, 'security event'))
end

-- ---------- hw_stream / mc_stream ----------
function TestLog:test_hw_stream_info_message_only()
    log:hw_stream_info('message')
    os.execute('sleep 0.1')
    local content = read_log_file()
    lu.assertNotNil(string.find(content, 'message'))
end

function TestLog:test_mc_stream_info_message_only()
    log:mc_stream_info('message')
    os.execute('sleep 0.1')
    local content = read_log_file()
    lu.assertNotNil(string.find(content, 'message'))
end

-- ---------- 边界与异常 ----------
function TestLog:test_operation_wrong_arg_count()
    local initiator = { Interface = 'ok', UserName = 'test', ClientIp = '127.0.0.1' }
    log:operation(initiator, 'module_name', 'test ${key}', 'key')
    os.execute('sleep 0.1')
    local content = read_log_file()
    lu.assertNotNil(string.find(content, 'test'))
end

function TestLog:test_period_invalid_value()
    local ok, err = pcall(function()
        log:period(0)
    end)
    lu.assertFalse(ok, 'period(0) should fail')
    lu.assertNotNil(string.find(tostring(err), 'period') or string.find(tostring(err), 'Invalid'), 'error should mention period')

    ok, _ = pcall(function()
        log:period(-1)
    end)
    lu.assertFalse(ok, 'period(-1) should fail')
end

function TestLog:test_info_with_nil_message()
    local ok, _ = pcall(function()
        log:info(nil)
    end)
    lu.assertTrue(ok, 'nil is ok')
end

function TestLog:test_log_with_empty_message()
    local ok, _ = pcall(function()
        log:info(' ')
    end)
    lu.assertTrue(ok, 'empty is ok')
end

function TestLog:test_template_without_value()
    log:info('module_name', 'status ${key}')
    os.execute('sleep 0.1')
    local content = read_log_file()
    assert_standard_log_format(content, 'INFO')
    lu.assertNotNil(string.find(content, 'status ${key}'))
end
