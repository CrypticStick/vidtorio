#ifndef BLUEPRINT_H_
#define BLUEPRINT_H_

#include "arena.h"
#include "my_string.h"

// String builder max capacity (8 KB)
#define STEP_BUILDER_CAP 1<<13

// String containing the beginning of a blueprint.
const extern string BLUEPRINT_PRE_ENTITIES;
// String containing the end of entities and beginning of wires.
const extern string BLUEPRINT_PRE_WIRES;
// String containing the end of a blueprint.
const extern string BLUEPRINT_END;

typedef struct entity_icon_ {
    // "type" : "virtual"
    string *name;
} entity_icon;

typedef struct entity_ {
    string *name;
    entity_icon *icon;
    int entity_number;
    int position_x;
    int position_y;
    // if name == "display-panel" --> "show_in_chart" : true
} entity;

entity_icon *entity_icon_new(string *name, arena *a);

entity *entity_new(int entity_number, string *name, int pos_x, int pos_y, entity_icon *icon, arena *a);

string entity_string(entity *entity, string_buffer *buf, bool append_comma);

#endif // BLUEPRINT_H_
