#ifndef MC_ENGINE_PROPERTY_RELATION_H
#define MC_ENGINE_PROPERTY_RELATION_H

#include <mc/reflect.h>
#include <mc/string.h>

namespace mc::engine {

struct relate_property {
    MC_REFLECTABLE("mc.engine.relate_property");

    mc::string type;
    mc::string object_name;
    mc::string property_name;
    mc::string full_name;
    mc::string interface;
};

} // namespace mc::engine

#endif // MC_ENGINE_PROPERTY_RELATION_H
