#ifndef LV_LAVA_SYSTEM_H
#define LV_LAVA_SYSTEM_H

#include "lvnd/lvnd.h"

#include "entity.hpp"
#include "transform.hpp"

using namespace lv;

#define REGISTER_SCRIPT_CLASS(scriptClass) \
extern "C" { \
    scriptClass* newEntity(entt::entity entityID, entt::registry* registry) { return new scriptClass(entityID, registry); } \
}

namespace Input {

LvndState getKeyState(Entity* entity, LvndKey key) {
    return lvndGetKeyState((LvndWindow*)entity->window, key);
}

} //namespace Input

#endif
