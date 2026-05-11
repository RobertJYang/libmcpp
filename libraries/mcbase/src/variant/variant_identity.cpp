#include <mc/variant/extension_type_id.h>
#include <mc/variant/variant_extension.h>

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace mc {
namespace {

struct extension_type_entry {
    std::string         display_name;
    extension_type_info info;
};

struct payload_type_entry {
    std::string                            display_name;
    std::vector<const variant_payload_type*> bases;
    variant_payload_type                   payload;
};

struct extension_registry_state {
    std::mutex                                        mutex;
    extension_type_id_t                              next_id = extension_type_ids::user_defined_start;
    std::unordered_map<std::string, extension_type_entry> entries;
};

struct payload_registry_state {
    std::mutex                                     mutex;
    std::unordered_map<std::string, payload_type_entry> entries;
};

extension_registry_state& extension_registry()
{
    static extension_registry_state state;
    return state;
}

payload_registry_state& payload_registry()
{
    static payload_registry_state state;
    return state;
}

void refresh_payload_entry(payload_type_entry& entry)
{
    entry.payload = variant_payload_type(
        mc::string_view(entry.display_name),
        entry.bases.empty() ? nullptr : entry.bases.data(),
        entry.bases.size());
}

} // namespace

namespace detail {

bool payload_type_is_a(const variant_payload_type* source, const variant_payload_type* target)
{
    if (!source || !target) {
        return false;
    }
    if (source == target) {
        return true;
    }
    for (std::size_t i = 0; i < source->base_count; ++i) {
        if (payload_type_is_a(source->bases[i], target)) {
            return true;
        }
    }
    return false;
}

void* upcast_payload_exact_type(
    void* payload, const variant_payload_type* source, const variant_payload_type& target)
{
    if (!payload || !source) {
        return nullptr;
    }
    return source == &target ? payload : nullptr;
}

const void* upcast_payload_exact_type(
    const void* payload, const variant_payload_type* source, const variant_payload_type& target)
{
    if (!payload || !source) {
        return nullptr;
    }
    return source == &target ? payload : nullptr;
}

} // namespace detail

extension_type_id_t extension_type_registry::next_user_type_id()
{
    auto& state = extension_registry();
    std::lock_guard<std::mutex> lock(state.mutex);
    return state.next_id++;
}

extension_type_info extension_type_registry::register_type(
    mc::string_view identity, mc::string_view display_name, extension_type_trait traits)
{
    auto& state = extension_registry();
    std::lock_guard<std::mutex> lock(state.mutex);

    auto [it, inserted] = state.entries.try_emplace(std::string(identity));
    auto& entry = it->second;
    if (inserted) {
        entry.display_name = std::string(display_name);
        entry.info = extension_type_info(state.next_id++, mc::string_view(entry.display_name), traits);
        return entry.info;
    }

    if (entry.display_name.empty()) {
        entry.display_name = std::string(display_name);
        entry.info.name = mc::string_view(entry.display_name);
    }
    entry.info.traits = entry.info.traits | traits;
    return entry.info;
}

const variant_payload_type* variant_payload_registry::register_type(
    mc::string_view identity,
    mc::string_view display_name,
    const variant_payload_type* const* bases,
    std::size_t base_count)
{
    auto& state = payload_registry();
    std::lock_guard<std::mutex> lock(state.mutex);

    auto [it, inserted] = state.entries.try_emplace(std::string(identity));
    auto& entry = it->second;
    if (inserted) {
        entry.display_name = std::string(display_name);
        if (bases && base_count > 0) {
            entry.bases.assign(bases, bases + base_count);
        }
        refresh_payload_entry(entry);
        return &entry.payload;
    }

    if (entry.display_name.empty()) {
        entry.display_name = std::string(display_name);
    }
    if (entry.bases.empty() && bases && base_count > 0) {
        entry.bases.assign(bases, bases + base_count);
    }
    refresh_payload_entry(entry);
    return &entry.payload;
}

} // namespace mc
