/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * openUBMC is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#include <mc/app/application.h>

#include <mc/engine/engine.h>
#include <mc/engine/engine_options.h>
#include <mc/engine/path.h>
#include <mc/engine/property/processors.h>
#include <mc/exception.h>
#include <mc/expr/builtin.h>
#include <mc/expr/engine.h>
#include <mc/filesystem.h>
#include <mc/json.h>
#include <mc/log/appender_factory.h>
#include <mc/log/log.h>
#include <mc/log/log_manager.h>
#include <mc/log/logger.h>
#include <mc/runtime/runtime_context.h>
#if defined(MCDBUS_USE_OLD_SHM) && MCDBUS_USE_OLD_SHM
#include <mc/dbus/shm/harbor.h>
#endif

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

mc::string resolve_default_log_appenders_directory_for_mcapp()
{
    const char* env = std::getenv("MCAPP_LOG_APPENDERS_DIR");
    if (env != nullptr && env[0] != '\0') {
        return mc::string(mc::string_view(env));
    }
#ifdef MCAPP_DEFAULT_LOG_APPENDERS_DIR
    return mc::string(MCAPP_DEFAULT_LOG_APPENDERS_DIR);
#else
    return mc::string("/opt/bmc/log/appenders");
#endif
}

} // namespace

namespace mc::app {
namespace detail {

struct service_override {
    std::optional<bool>                    enabled;
    std::optional<service_execution_model> execution_model;
    mc::dict                               properties;
};

struct bootstrap_options {
    std::string                                       config_file{"./config.json"};
    bool                                              config_explicit{false};
    std::vector<std::string>                          modules;
    std::size_t                                       io_threads{0};
    std::size_t                                       work_threads{0};
    std::unordered_map<std::string, service_override> service_overrides;
};

class application_root_service : public service {
public:
    application_root_service()
        : service(mc::string("mcapp.root.p") + mc::to_string(static_cast<std::int64_t>(::getpid())))
    {
        set_path("/");
    }

    bool on_start() override
    {
        return true;
    }

    bool on_stop() override
    {
        return true;
    }
};

service_execution_model parse_execution_model(std::string_view value)
{
    if (value == "io" || value == "io_strand") {
        return service_execution_model::io_strand;
    }
    if (value == "work" || value == "work_strand") {
        return service_execution_model::work_strand;
    }
    return service_execution_model::inherit;
}

mc::variant parse_cli_value(const std::string& value)
{
    try {
        return mc::json::json_decode(value);
    } catch (const std::exception&) {
        return mc::string(value);
    }
}

bool parse_bool_value(std::string_view value, bool& out)
{
    if (value == "true" || value == "1") {
        out = true;
        return true;
    }
    if (value == "false" || value == "0") {
        out = false;
        return true;
    }
    return false;
}

bool parse_size_value(std::string_view value, std::size_t& out)
{
    try {
        out = static_cast<std::size_t>(std::stoull(std::string(value)));
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool parse_service_override_token(const std::string& token, bootstrap_options& result)
{
    constexpr std::string_view prefix = "--service.";
    if (token.rfind(prefix.data(), 0) != 0) {
        return false;
    }

    auto assignment = token.substr(prefix.size());
    auto equal_pos  = assignment.find('=');
    if (equal_pos == std::string::npos) {
        return false;
    }

    auto key_path = assignment.substr(0, equal_pos);
    auto value    = assignment.substr(equal_pos + 1);
    auto dot_pos  = key_path.rfind('.');
    if (dot_pos == std::string::npos) {
        return false;
    }

    auto  service_name = key_path.substr(0, dot_pos);
    auto  option_name  = key_path.substr(dot_pos + 1);
    auto& override     = result.service_overrides[service_name];

    if (option_name == "enabled") {
        bool enabled = false;
        if (!parse_bool_value(value, enabled)) {
            return false;
        }
        override.enabled = enabled;
        return true;
    }

    if (option_name == "executor") {
        override.execution_model = parse_execution_model(value);
        return true;
    }

    override.properties[option_name] = parse_cli_value(value);
    return true;
}

bool read_arg_value(int argc, char** argv, int& index, std::string& out)
{
    if (index + 1 >= argc) {
        return false;
    }
    ++index;
    out = argv[index];
    return true;
}

bool parse_bootstrap_options(int argc, char** argv, bootstrap_options& result)
{
    for (int index = 1; index < argc; ++index) {
        std::string token = argv[index];

        if (token == "--config" || token == "-c") {
            result.config_explicit = true;
            if (!read_arg_value(argc, argv, index, result.config_file)) {
                return false;
            }
            continue;
        }

        if (token == "--module" || token == "-m") {
            std::string module_name;
            if (!read_arg_value(argc, argv, index, module_name)) {
                return false;
            }
            result.modules.push_back(std::move(module_name));
            continue;
        }

        if (token == "--threads" || token == "-t") {
            std::string thread_value;
            if (!read_arg_value(argc, argv, index, thread_value) ||
                !parse_size_value(thread_value, result.io_threads)) {
                return false;
            }
            continue;
        }

        if (token == "--work-threads") {
            std::string thread_value;
            if (!read_arg_value(argc, argv, index, thread_value) ||
                !parse_size_value(thread_value, result.work_threads)) {
                return false;
            }
            continue;
        }

        if (parse_service_override_token(token, result)) {
            continue;
        }
    }

    return true;
}

void append_unique_modules(mc::array<mc::string>& target, const std::vector<std::string>& source)
{
    for (const auto& module_name : source) {
        if (std::find(target.begin(), target.end(), module_name) == target.end()) {
            target.push_back(module_name);
        }
    }
}

void append_unique_modules(mc::array<mc::string>& target, const mc::array<mc::string>& source)
{
    for (const auto& module_name : source) {
        if (std::find(target.begin(), target.end(), module_name) == target.end()) {
            target.push_back(module_name);
        }
    }
}

mc::string dict_string(const mc::dict& data, std::string_view key, mc::string default_value = {})
{
    if (!data.contains(key)) {
        return default_value;
    }
    return data[key].as<std::string>();
}

bool dict_bool(const mc::dict& data, std::string_view key, bool default_value)
{
    if (!data.contains(key)) {
        return default_value;
    }
    return data[key].as<bool>();
}

std::size_t dict_size(const mc::dict& data, std::string_view key, std::size_t default_value)
{
    if (!data.contains(key)) {
        return default_value;
    }
    return static_cast<std::size_t>(data[key].as<int64_t>());
}

mc::dict dict_object(const mc::dict& data, std::string_view key)
{
    if (!data.contains(key)) {
        return {};
    }
    return data[key].as<mc::dict>();
}

mc::array<mc::string> dict_string_array(const mc::dict& data, std::string_view key)
{
    if (!data.contains(key)) {
        return {};
    }

    mc::array<mc::string> result;
    for (const auto& entry : data[key].as<mc::variants>()) {
        result.push_back(entry.as<std::string>());
    }
    return result;
}

service_definition* find_service_definition(service_plan& plan, mc::string_view service_name)
{
    for (auto& definition : plan.services) {
        if (definition.name == service_name) {
            return &definition;
        }
    }
    return nullptr;
}

service_definition& upsert_service_definition(service_plan& plan, mc::string_view service_name)
{
    auto* definition = find_service_definition(plan, service_name);
    if (definition != nullptr) {
        return *definition;
    }

    plan.services.push_back(service_definition{});
    plan.services.back().name = mc::string(service_name);
    return plan.services.back();
}

mc::filesystem::path resolve_config_path(const mc::filesystem::path& source_path, mc::string_view path)
{
    auto resolved = mc::filesystem::path(std::string(path));
    if (resolved.is_absolute()) {
        return mc::filesystem::normalize(resolved);
    }
    return mc::filesystem::normalize(source_path.parent_path() / resolved);
}

// 缺省 default_file appender 的固定名字，与外部约定保持一致。
constexpr mc::string_view k_default_file_appender_name{"default_file"};

void bootstrap_default_file_appender(mc::string_view module_name);

void append_default_logging_appender_dir(mc::log::logging_config& logging_config)
{
    auto appender_dir = resolve_default_log_appenders_directory_for_mcapp();
    auto it = std::find(logging_config.appender_dirs.begin(), logging_config.appender_dirs.end(), appender_dir);
    if (it != logging_config.appender_dirs.end()) {
        return;
    }

    logging_config.appender_dirs.push_back(appender_dir);
}

void apply_resolved_logging_config(mc::log::logging_config logging_config, mc::string_view module_name,
                                   bool append_default_appender_dir,
                                   bool bootstrap_default_file_appender_for_default_logger)
{
    if (append_default_appender_dir) {
        append_default_logging_appender_dir(logging_config);
    }
    mc::log::log_manager::instance().apply_config(logging_config);
    if (bootstrap_default_file_appender_for_default_logger) {
        bootstrap_default_file_appender(module_name);
    }
}

void bootstrap_default_file_appender(mc::string_view module_name)
{
    auto default_log = mc::log::default_logger();
    if (default_log.find_appender(k_default_file_appender_name) != nullptr) {
        return;
    }

    auto& factory  = mc::log::appender_factory::instance();
    auto  appender = factory.get_appender(k_default_file_appender_name);
    if (appender == nullptr) {
        mc::dict config;
        if (!module_name.empty()) {
            config["module_name"] = mc::string(module_name);
        }
        appender = factory.create(k_default_file_appender_name, "file", config);
    }
    if (appender != nullptr) {
        default_log.add_appender(appender);
    } else {
        wlog("mcapp create default file appender[${name}] failed", ("name", k_default_file_appender_name));
    }
}

void apply_default_logging_config(mc::string_view module_name)
{
    apply_resolved_logging_config(mc::log::logging_config{}, module_name, true, true);
}

bool apply_logging_config(const mc::variant& item)
{
    try {
        mc::log::logging_config logging_config;
        bool                    append_default_appender_dir = true;
        if (item.is_dict()) {
            append_default_appender_dir = !item.as<mc::dict>().contains("appender_dirs");
        }
        mc::from_variant(item, logging_config);
        apply_resolved_logging_config(std::move(logging_config), {}, append_default_appender_dir, false);
        return true;
    } catch (const std::exception& e) {
        elog("apply mcapp logging config failed: ${error}", ("error", e.what()));
        return false;
    }
}

void merge_service_definition(service_plan& plan, const mc::dict& data)
{
    auto       meta         = dict_object(data, "meta");
    auto       service_type = dict_string(data, "type");
    mc::string service_name;
    if (meta.contains("name")) {
        service_name = meta["name"].as<std::string>();
    } else {
        service_name = service_type;
    }
    if (service_name.empty()) {
        return;
    }

    auto& definition = upsert_service_definition(plan, service_name);
    if (!service_type.empty()) {
        definition.type = std::move(service_type);
    }
    if (meta.contains("path")) {
        definition.path = meta["path"].as<std::string>();
    }
    if (data.contains("enabled")) {
        definition.enabled = data["enabled"].as<bool>();
    }
    if (data.contains("executor")) {
        definition.execution_model = parse_execution_model(dict_string(data, "executor", "inherit"));
    }
    if (data.contains("properties")) {
        for (const auto& entry : data["properties"].as<mc::dict>()) {
            definition.properties[entry.key] = entry.value;
        }
    }
}

void merge_application_definition(service_plan& plan, const mc::dict& data)
{
    auto meta = dict_object(data, "meta");
    if (meta.contains("name")) {
        plan.application.name = meta["name"].as<std::string>();
    }
    append_unique_modules(plan.application.modules, dict_string_array(data, "modules"));
    if (data.contains("threads")) {
        plan.application.io_threads = dict_size(data, "threads", plan.application.io_threads);
    }
    if (data.contains("work_threads")) {
        plan.application.work_threads = dict_size(data, "work_threads", plan.application.work_threads);
    }
}

bool load_plan_variant(const mc::variant& root, const mc::filesystem::path& source_path, service_plan& plan,
                       std::unordered_set<std::string>& loading_files, bool& has_logging_config);

bool load_plan_file(const mc::filesystem::path& source_path, service_plan& plan,
                    std::unordered_set<std::string>& loading_files, bool& has_logging_config)
{
    auto normalized = mc::filesystem::normalize(source_path);
    auto key        = normalized.string();
    if (!loading_files.insert(key).second) {
        elog("detected recursive mcapp config import: ${path}", ("path", key));
        return false;
    }

    if (!mc::filesystem::exists(normalized)) {
        elog("mcapp config file does not exist: ${path}", ("path", normalized.string()));
        loading_files.erase(key);
        return false;
    }

    std::ifstream file(normalized.string());
    if (!file.is_open()) {
        elog("open mcapp config failed: ${path}", ("path", normalized.string()));
        loading_files.erase(key);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    mc::variant root;
    try {
        root = mc::json::json_decode(buffer.str());
    } catch (const std::exception& e) {
        elog("decode mcapp config failed: ${path} ${error}", ("path", normalized.string())("error", e.what()));
        loading_files.erase(key);
        return false;
    }

    auto loaded = load_plan_variant(root, normalized, plan, loading_files, has_logging_config);
    loading_files.erase(key);
    return loaded;
}

bool load_service_dir(const mc::filesystem::path& source_path, mc::string_view relative_dir, service_plan& plan,
                      std::unordered_set<std::string>& loading_files, bool& has_logging_config)
{
    auto directory = resolve_config_path(source_path, relative_dir);
    if (!mc::filesystem::exists(directory) || !mc::filesystem::is_directory(directory)) {
        elog("mcapp service directory is invalid: ${path}", ("path", directory.string()));
        return false;
    }

    std::vector<mc::filesystem::path> files;
    for (const auto& entry : mc::filesystem::list_files(directory)) {
        if (entry.extension() != ".json") {
            continue;
        }
        files.push_back(entry);
    }

    std::sort(files.begin(), files.end());
    for (const auto& file : files) {
        if (!load_plan_file(file, plan, loading_files, has_logging_config)) {
            return false;
        }
    }
    return true;
}

bool load_manifest_dict(const mc::dict& data, const mc::filesystem::path& source_path, service_plan& plan,
                        std::unordered_set<std::string>& loading_files, bool& has_logging_config)
{
    for (const auto& import_path : dict_string_array(data, "imports")) {
        if (!load_plan_file(resolve_config_path(source_path, import_path), plan, loading_files, has_logging_config)) {
            return false;
        }
    }

    merge_application_definition(plan, data);

    if (data.contains("logging")) {
        has_logging_config = true;
        if (!apply_logging_config(data["logging"])) {
            return false;
        }
    }

    if (data.contains("services")) {
        for (const auto& item : data["services"].as<mc::variants>()) {
            if (!item.is_dict()) {
                continue;
            }
            auto service_data = item.as<mc::dict>();
            if (!service_data.contains("kind")) {
                service_data["kind"] = "Service";
            }
            if (!load_plan_variant(service_data, source_path, plan, loading_files, has_logging_config)) {
                return false;
            }
        }
    }

    for (const auto& service_dir : dict_string_array(data, "service_dirs")) {
        if (!load_service_dir(source_path, service_dir, plan, loading_files, has_logging_config)) {
            return false;
        }
    }

    return true;
}

bool load_plan_variant(const mc::variant& root, const mc::filesystem::path& source_path, service_plan& plan,
                       std::unordered_set<std::string>& loading_files, bool& has_logging_config)
{
    auto process_item = [&](const mc::variant& item) {
        if (!item.is_dict()) {
            return true;
        }

        auto data = item.as<mc::dict>();
        auto kind = dict_string(data, "kind");

        if (kind == "ApplicationManifest" ||
            (kind.empty() &&
             (data.contains("imports") || data.contains("service_dirs") || data.contains("services")))) {
            return load_manifest_dict(data, source_path, plan, loading_files, has_logging_config);
        }

        if (kind == "Application") {
            merge_application_definition(plan, data);
            return true;
        }

        if (kind == "Service") {
            merge_service_definition(plan, data);
            return true;
        }

        if (kind == "Logging") {
            has_logging_config = true;
            return apply_logging_config(item);
        }

        return true;
    };

    if (root.is_array()) {
        for (const auto& item : root.as<mc::variants>()) {
            if (!process_item(item)) {
                return false;
            }
        }
        return true;
    }

    if (root.is_dict()) {
        return process_item(root);
    }

    elog("mcapp config root must be object or array");
    return false;
}

bool load_plan_from_config_file(const bootstrap_options& options, service_plan& plan, bool& has_logging_config)
{
    plan.application.config_file = options.config_file;

    if (!mc::filesystem::exists(options.config_file)) {
        return !options.config_explicit;
    }

    std::unordered_set<std::string> loading_files;
    return load_plan_file(options.config_file, plan, loading_files, has_logging_config);
}

void apply_bootstrap_options(service_plan& plan, const bootstrap_options& options)
{
    append_unique_modules(plan.application.modules, options.modules);
    if (options.io_threads > 0) {
        plan.application.io_threads = options.io_threads;
    }
    if (options.work_threads > 0) {
        plan.application.work_threads = options.work_threads;
    }

    for (auto& definition : plan.services) {
        auto it = options.service_overrides.find(std::string(definition.name));
        if (it == options.service_overrides.end()) {
            continue;
        }

        if (it->second.enabled.has_value()) {
            definition.enabled = *it->second.enabled;
        }
        if (it->second.execution_model.has_value()) {
            definition.execution_model = *it->second.execution_model;
        }
        for (const auto& entry : it->second.properties) {
            definition.properties[entry.key] = entry.value;
        }
    }
}

mc::string derive_service_path(mc::string_view service_name)
{
    std::string result = "/";
    for (char ch : std::string(service_name)) {
        result.push_back(ch == '.' ? '/' : ch);
    }
    return result;
}

void normalize_service_definition_path(service_definition& definition)
{
    if (definition.path.empty()) {
        definition.path = derive_service_path(definition.name);
    }
}

bool validate_service_definition_path(const service_definition& definition)
{
    return !definition.path.empty() && mc::engine::path::is_valid(definition.path);
}

} // namespace detail

application* application::get() noexcept
{
    return dynamic_cast<application*>(base_app::get());
}

application& application::instance()
{
    auto* app = get();
    MC_ASSERT(app != nullptr, "mcapp 当前实例不是 application");
    return *app;
}

void application::reset_for_test()
{
    base_app::reset_for_test();
}

application::application()
{
    m_registry.import_global_services();
}

application::~application()
{
    stop();
    clear_application_state();
}

service_registry& application::registry()
{
    return m_registry;
}

const service_registry& application::registry() const
{
    return m_registry;
}

bool application::initialize(const app_options& options)
{
    return initialize_internal(options);
}

void application::prepare_initialize()
{
    stop();
    clear_application_state();
    m_registry.import_global_services();
}

bool application::initialize_from_current_plan()
{
    for (auto& definition : m_plan.services) {
        detail::normalize_service_definition_path(definition);
        if (!detail::validate_service_definition_path(definition)) {
            elog("invalid mcapp service path: ${name} ${path}", ("name", definition.name)("path", definition.path));
            clear_application_state();
            return false;
        }
    }

    for (const auto& module_name : m_plan.application.modules) {
        if (!load_module(module_name)) {
            clear_application_state();
            return false;
        }
    }

    for (auto& definition : m_plan.services) {
        auto descriptor = m_registry.find(definition.type);
        if (descriptor == nullptr) {
            elog("unknown mcapp service type: ${type}", ("type", definition.type));
            clear_application_state();
            return false;
        }

        mc::dict merged_properties = descriptor->config.default_properties;
        for (const auto& entry : definition.properties) {
            merged_properties[entry.key] = entry.value;
        }
        definition.properties = std::move(merged_properties);

        if (definition.execution_model == service_execution_model::inherit) {
            definition.execution_model = descriptor->execution_model;
        }
    }

    if (!create_services()) {
        clear_application_state();
        return false;
    }

    m_initialized = true;
    return true;
}

bool application::initialize_with_plan(service_plan plan)
{
    prepare_initialize();
    m_plan = std::move(plan);
    if (!m_plan.has_logging_config) {
        detail::apply_default_logging_config(m_plan.application.name);
    }
    return initialize_from_current_plan();
}

bool application::initialize_internal(const app_options& options)
{
    prepare_initialize();

    detail::bootstrap_options bootstrap_options;
    if (options.argc > 0 && options.argv != nullptr &&
        !detail::parse_bootstrap_options(options.argc, options.argv, bootstrap_options)) {
        return false;
    }

    bool has_logging_config = false;
    if (!detail::load_plan_from_config_file(bootstrap_options, m_plan, has_logging_config)) {
        clear_application_state();
        return false;
    }

    detail::apply_bootstrap_options(m_plan, bootstrap_options);
    if (!has_logging_config) {
        detail::apply_default_logging_config(m_plan.application.name);
    }
    return initialize_from_current_plan();
}

bool application::load_module(mc::string_view module_name)
{
    auto module = mc::load_module(module_name);
    if (module == nullptr) {
        elog("load mcapp module failed: ${name}", ("name", module_name));
        return false;
    }

    m_modules.push_back(std::move(module));
    m_registry.import_global_services();
    return true;
}

bool application::create_services()
{
    m_root_service = mc::make_shared<detail::application_root_service>();
    m_root_service->set_parent(this);

    for (const auto& definition : m_plan.services) {
        if (!definition.enabled) {
            continue;
        }

        auto descriptor = m_registry.find(definition.type);
        if (descriptor == nullptr) {
            return false;
        }

        if (!m_registry.validate_service_properties(definition.type, definition.properties)) {
            elog("validate mcapp service properties failed: ${name}", ("name", definition.name));
            return false;
        }

        auto service_instance = m_registry.create_service(definition.type, definition.name);
        if (service_instance == nullptr) {
            elog("create mcapp service failed: ${name}", ("name", definition.name));
            return false;
        }

        service_instance->set_path(definition.path);
        service_context context(*this, m_plan, definition);
        if (!service_instance->configure(context, definition.properties)) {
            elog("configure mcapp service failed: ${name}", ("name", definition.name));
            return false;
        }

        service_instance->set_parent(m_root_service);
        m_service_order.push_back(definition.name);
        m_services[std::string(definition.name)] = std::move(service_instance);
    }

    return true;
}

void application::assign_service_executor(const service_definition& definition, const service_ptr& service_instance)
{
    auto& runtime = mc::runtime::get_runtime_context();
    switch (definition.execution_model) {
        case service_execution_model::io_strand:
            service_instance->set_executor(runtime.create_io_strand());
            break;
        case service_execution_model::work_strand:
            service_instance->set_executor(runtime.create_work_strand());
            break;
        case service_execution_model::inherit:
        default:
            break;
    }
}

bool application::start()
{
    if (!m_initialized) {
        return false;
    }
    if (m_running) {
        return true;
    }

    auto& runtime = mc::runtime::get_runtime_context();
    runtime.initialize(mc::runtime::runtime_config{m_plan.application.io_threads, m_plan.application.work_threads});
    runtime.start();
    m_runtime_started = true;

    if (!bootstrap_engine_endpoint()) {
        clear_runtime_state();
        return false;
    }

    std::vector<service_ptr> started_services;
    if (m_root_service != nullptr) {
        if (!m_root_service->start()) {
            teardown_engine_endpoint();
            clear_runtime_state();
            return false;
        }
        started_services.push_back(m_root_service);
    }

    for (const auto& service_name : m_service_order) {
        auto definition = m_plan.find_service(service_name);
        auto service_it = m_services.find(std::string(service_name));
        if (definition == nullptr || service_it == m_services.end()) {
            continue;
        }

        assign_service_executor(*definition, service_it->second);
        if (!service_it->second->start()) {
            for (auto it = started_services.rbegin(); it != started_services.rend(); ++it) {
                (*it)->stop();
            }
            teardown_engine_endpoint();
            clear_runtime_state();
            return false;
        }
        started_services.push_back(service_it->second);
    }

    m_running = true;
    return true;
}

bool application::stop_services()
{
    bool success = true;
    for (auto it = m_service_order.rbegin(); it != m_service_order.rend(); ++it) {
        auto service_it = m_services.find(std::string(*it));
        if (service_it == m_services.end()) {
            continue;
        }
        if (!service_it->second->stop()) {
            success = false;
        }
    }
    if (m_root_service != nullptr && !m_root_service->stop()) {
        success = false;
    }
    return success;
}

bool application::stop()
{
    if (!m_running && !m_runtime_started) {
        return true;
    }

    bool success = stop_services();

    teardown_engine_endpoint();

#if defined(MCDBUS_USE_OLD_SHM) && MCDBUS_USE_OLD_SHM
    mc::dbus::harbor::stop_if_created();
#endif

    if (m_runtime_started) {
        mc::runtime::get_runtime_context().stop();
        mc::runtime::get_runtime_context().join();
        m_runtime_started = false;
    }

    clear_runtime_state();
    m_running = false;
    return success;
}

int application::exec()
{
    if (!m_running && !start()) {
        return m_exit_code == 0 ? 1 : m_exit_code;
    }

    if (m_runtime_started) {
        mc::runtime::get_runtime_context().join();
        m_runtime_started = false;
    }

    stop_services();
    clear_runtime_state();
    m_running = false;
    return m_exit_code;
}

void application::quit(int exit_code)
{
    m_exit_code = exit_code;
    if (m_runtime_started) {
        mc::runtime::get_runtime_context().stop();
    }
}

bool application::is_initialized() const noexcept
{
    return m_initialized;
}

bool application::is_running() const noexcept
{
    return m_running;
}

int application::exit_code() const noexcept
{
    return m_exit_code;
}

const service_plan& application::plan() const noexcept
{
    return m_plan;
}

service_ptr application::root_service() const
{
    return m_root_service;
}

service_ptr application::get_service(mc::string_view service_name) const
{
    auto it = m_services.find(std::string(service_name));
    if (it != m_services.end()) {
        return it->second;
    }
    return nullptr;
}

mc::array<mc::string> application::get_service_names() const
{
    mc::array<mc::string> result;
    for (const auto& name : m_service_order) {
        result.push_back(name);
    }
    return result;
}

void application::clear_runtime_state()
{
    mc::runtime::reset_runtime_context();
    m_runtime_started = false;
}

bool application::bootstrap_engine_endpoint()
{
#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM
    auto app_name = m_plan.application.name;
    if (app_name.empty()) {
        return true;
    }

    mc::engine::engine_options options;
    options.endpoint_name = app_name;
    if (!mc::engine::engine::init(options)) {
        elog("mcapp: engine::init 失败 name=${name}", ("name", app_name));
        return false;
    }
#endif
    mc::engine::register_property_processors();
    mc::expr::register_path_template_backend();
    mc::expr::register_builtin_modules();
    return true;
}

void application::teardown_engine_endpoint()
{
#if defined(MCENGINE_USE_SHM) && MCENGINE_USE_SHM
    mc::engine::engine::shutdown();
#endif
}

void application::clear_application_state()
{
    for (auto& entry : m_services) {
        if (entry.second != nullptr) {
            entry.second->set_parent(static_cast<mc::object*>(nullptr));
        }
    }
    if (m_root_service != nullptr) {
        m_root_service->set_parent(static_cast<mc::object*>(nullptr));
    }

    m_services.clear();
    m_service_order.clear();
    m_root_service.reset();
    m_modules.clear();
    m_plan        = service_plan{};
    m_exit_code   = 0;
    m_initialized = false;
    m_running     = false;
}

service_context::service_context(application& app, const service_plan& plan, const service_definition& definition)
    : m_app(&app), m_plan(&plan), m_definition(&definition)
{}

application& service_context::app() const
{
    MC_ASSERT(m_app != nullptr, "mcapp service context is invalid");
    return *m_app;
}

mc::runtime::runtime_context& service_context::runtime() const
{
    return mc::runtime::get_runtime_context();
}

const service_plan& service_context::plan() const
{
    MC_ASSERT(m_plan != nullptr, "mcapp service plan is invalid");
    return *m_plan;
}

const application_definition& service_context::application_config() const
{
    return plan().application;
}

const service_definition& service_context::definition() const
{
    MC_ASSERT(m_definition != nullptr, "mcapp service definition is invalid");
    return *m_definition;
}

service_ptr service_context::find_service(mc::string_view service_name) const
{
    return app().get_service(service_name);
}

mc::runtime::any_executor service_context::get_default_executor() const
{
    return runtime().get_executor();
}

mc::runtime::any_executor service_context::create_io_strand() const
{
    return runtime().create_io_strand();
}

mc::runtime::any_executor service_context::create_work_strand() const
{
    return runtime().create_work_strand();
}

void service_context::quit(int exit_code) const
{
    app().quit(exit_code);
}

} // namespace mc::app
