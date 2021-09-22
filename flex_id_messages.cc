// File created from ../../../../src/hooks/dhcp/flex_id/flex_id_messages.mes

#include <cstddef>
#include <log/message_types.h>
#include <log/message_initializer.h>

extern const isc::log::MessageID FLEX_ID_LOAD = "FLEX_ID_LOAD";
extern const isc::log::MessageID FLEX_ID_LOAD_ERROR = "FLEX_ID_LOAD_ERROR";
extern const isc::log::MessageID FLEX_ID_UNLOAD = "FLEX_ID_UNLOAD";
extern const isc::log::MessageID FLEX_ID_NO_LINK = "FLEX_ID_NO_LINK";
extern const isc::log::MessageID FLEX_ID_BAD_IP = "FLEX_ID_BAD_IP";
extern const isc::log::MessageID FLEX_ID_IP = "FLEX_ID_IP";

namespace {

const char* values[] = {
    "FLEX_ID_LOAD", "Flex-ID hooks library has been loaded",
    "FLEX_ID_LOAD_ERROR", "Flex-ID hooks library failed: %1",
    "FLEX_ID_UNLOAD", "Flex-ID hooks library has been unloaded",
    "FLEX_ID_NO_LINK", "Flex-ID path %1 : %2",
    "FLEX_ID_BAD_IP", "Flex-ID %1 bad ipv4 '%2'",
    "FLEX_ID_IP", "Flex-ID %1 get ipv4 '%2'",
    NULL
};

const isc::log::MessageInitializer initializer(values);

} // Anonymous namespace

