// File created from flex_id_messages.mes

#include <cstddef>
#include <log/message_types.h>
#include <log/message_initializer.h>

extern const isc::log::MessageID FLEX_CLS_ADD = "FLEX_CLS_ADD";
extern const isc::log::MessageID FLEX_CLS_ADD_DEFAULT = "FLEX_CLS_ADD_DEFAULT";
extern const isc::log::MessageID FLEX_CLS_BAD = "FLEX_CLS_BAD";
extern const isc::log::MessageID FLEX_CLS_DEFAULT = "FLEX_CLS_DEFAULT";
extern const isc::log::MessageID FLEX_CLS_MODE = "FLEX_CLS_MODE";
extern const isc::log::MessageID FLEX_CLS_OPTSTR = "FLEX_CLS_OPTSTR";
extern const isc::log::MessageID FLEX_CLS_PATH = "FLEX_CLS_PATH";
extern const isc::log::MessageID FLEX_ID_BAD_IP = "FLEX_ID_BAD_IP";
extern const isc::log::MessageID FLEX_ID_IP = "FLEX_ID_IP";
extern const isc::log::MessageID FLEX_ID_LOAD = "FLEX_ID_LOAD";
extern const isc::log::MessageID FLEX_ID_LOAD_ERROR = "FLEX_ID_LOAD_ERROR";
extern const isc::log::MessageID FLEX_ID_NO_OPT = "FLEX_ID_NO_OPT";
extern const isc::log::MessageID FLEX_ID_OPTSTR = "FLEX_ID_OPTSTR";
extern const isc::log::MessageID FLEX_ID_PATH = "FLEX_ID_PATH";
extern const isc::log::MessageID FLEX_ID_SUBNET4_ID = "FLEX_ID_SUBNET4_ID";
extern const isc::log::MessageID FLEX_ID_UNLOAD = "FLEX_ID_UNLOAD";

namespace {

const char* values[] = {
    "FLEX_CLS_ADD", "add class '%1'",
    "FLEX_CLS_ADD_DEFAULT", "class '%1'",
    "FLEX_CLS_BAD", "Invalid class name '%1'. Allowed only isalnum().",
    "FLEX_CLS_DEFAULT", "default class '%1'",
    "FLEX_CLS_MODE", "mode '%1'",
    "FLEX_CLS_OPTSTR", "add template '%1'",
    "FLEX_CLS_PATH", "CLS '%1' -> '%2' '%3'",
    "FLEX_ID_BAD_IP", "%1 bad ipv4 '%2'",
    "FLEX_ID_IP", "%2 from '%1'",
    "FLEX_ID_LOAD", "Run Script hooks library has been loaded template_id %1, template_class %2",
    "FLEX_ID_LOAD_ERROR", "Run Script hooks library failed: %1",
    "FLEX_ID_NO_OPT", "template '%1' macro '%2' is not set",
    "FLEX_ID_OPTSTR", "add template '%1'",
    "FLEX_ID_PATH", "ID '%1' -> '%2' '%3'",
    "FLEX_ID_SUBNET4_ID", "select subnet4 '%1'",
    "FLEX_ID_UNLOAD", "Run Script hooks library has been unloaded",
    NULL
};

const isc::log::MessageInitializer initializer(values);

} // Anonymous namespace

