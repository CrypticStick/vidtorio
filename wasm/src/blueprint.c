#include "blueprint.h"

const string BLUEPRINT_PRE_ENTITIES = str("{\"blueprint\":{\"icons\":[{\"signal\":{\"name\":\"display-panel\"},\"index\":1}],\"entities\":[");
const string BLUEPRINT_PRE_WIRES = str("],\"wires\":[");
const string BLUEPRINT_END = str("],\"item\":\"blueprint\",\"version\":562949955977217}}");

entity_icon *entity_icon_new(string *name, arena *a) {
    entity_icon *new_entity_icon = a_malloc(a, entity_icon);
    new_entity_icon->name = name;
    return new_entity_icon;
}

entity *entity_new(int entity_number, string *name, int pos_x, int pos_y, entity_icon *icon, arena *a) {
    entity *new_entity = a_malloc(a, entity);
    new_entity->entity_number = entity_number;
    new_entity->name = name;
    new_entity->position_x = pos_x;
    new_entity->position_y = pos_y;
    new_entity->icon = icon;
    return new_entity;
}

string entity_string(entity *entity, string_buffer *buf, bool append_comma) {
    string_buffer_clear(buf);
    // entity_number
    string_buffer_add_str(buf, string("{\"entity_number\":"));
    string_buffer_add_int(buf, entity->entity_number);
    // name
    string_buffer_add_str(buf, string(",\"name\":\""));
    string_buffer_add_str(buf, *entity->name);
    // position: x
    string_buffer_add_str(buf, string("\",\"position\":{\"x\":"));
    string_buffer_add_int(buf, entity->position_x);
    // position: y
    string_buffer_add_str(buf, string(",\"y\":"));
    string_buffer_add_int(buf, entity->position_y);
    string_buffer_add_str(buf, string("}"));
    if (entity->icon) {
        // entity: type
        // entity: name
        string_buffer_add_str(buf, string(",\"icon\": {\"type\": \"virtual\",\"name\": \""));
        string_buffer_add_str(buf, *entity->icon->name);
        // show_in_chart
        string_buffer_add_str(buf, string("\"},\"show_in_chart\": true"));
    }
    // for comma-seperated list of entities
    if (append_comma) {
        string_buffer_add_str(buf, string("},"));
    } else {
        string_buffer_add_str(buf, string("}"));
    }
    return buf->str;
}