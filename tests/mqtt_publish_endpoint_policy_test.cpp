// mosquitto-config#27: POST /api/v1/custom/mqtt/publish publishes under this service's broker identity,
// bypassing broker ACLs. It must stay off unless LOADSHED_MQTT_PUBLISH_ENABLED is explicitly true, and
// when on it may only publish allow-listed, non-command topics.
#include "microservice/core/mqtt_publish_endpoint_policy.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

/* No framework; exit code is the result. */
static int g_fail = 0, g_run = 0;

static void check(bool cond, const std::string& what) {
    ++g_run;
    if (!cond) { ++g_fail; std::printf("  FAIL  %s\n", what.c_str()); }
}

int main() {
    namespace mp = microservice::mqtt_publish_policy;

    // --- opt-in flag ---------------------------------------------------------
    check(!mp::parseEnabled(nullptr), "unset -> off");
    for (const char* v : {"", "   ", "0", "false", "FALSE", "no", "off", "disabled", "2", "truthy", "enabled"}) {
        check(!mp::parseEnabled(v), std::string("'") + v + "' -> off");
    }
    for (const char* v : {"1", "true", "TRUE", "True", "yes", "YES", "on", "On", " true ", "\ton\n"}) {
        check(mp::parseEnabled(v), std::string("'") + v + "' -> on");
    }
    ::unsetenv(mp::kEnabledEnv);
    check(!mp::enabledFromEnv(), "env unset -> off");
    ::setenv(mp::kEnabledEnv, "on", 1);
    check(mp::enabledFromEnv(), "env on -> on");
    ::setenv(mp::kEnabledEnv, "nope", 1);
    check(!mp::enabledFromEnv(), "env garbage -> off");
    ::unsetenv(mp::kEnabledEnv);

    // --- allow-list parsing --------------------------------------------------
    check(mp::parseAllowedPrefixes(nullptr).empty(), "unset allow-list is empty");
    check(mp::parseAllowedPrefixes(" , ,").empty(), "blank allow-list is empty");
    {
        const auto p = mp::parseAllowedPrefixes(" test/a/# , replay/ ,,/x/");
        check(p.size() == 3 && p[0] == "test/a" && p[1] == "replay" && p[2] == "x",
              "allow-list trims, drops /# and slashes");
    }

    // --- empty allow-list refuses everything ---------------------------------
    check(!mp::refusalReason("test/a", {}).empty(), "empty allow-list refuses every topic");

    // --- prefix matching -----------------------------------------------------
    const std::vector<std::string> allow = {"test/a", "replay"};
    check(mp::refusalReason("test/a", allow).empty(), "exact prefix allowed");
    check(mp::refusalReason("test/a/b/c", allow).empty(), "topic under prefix allowed");
    check(mp::refusalReason("replay/x", allow).empty(), "second prefix allowed");
    check(!mp::refusalReason("test/abc", allow).empty(), "prefix must end at a segment");
    check(!mp::refusalReason("other/topic", allow).empty(), "unlisted topic refused");

    // --- command topics refused even when allow-listed ------------------------
    for (const char* t : {"modbus/dev-1/write", "dnp3/abc/operate", "snmp/dev/control",
                          "x/commands/y", "pycev/reviews/playback/cmd", "fluxara/services/dhs/run",
                          "vd/vd1/config", "MODBUS/dev-1/WRITE", "x/Cmd", "pm/control/processes/a/stop",
                          "PM/Control/processes", "ngvl/pm/control/processes/x", "fluxara/redundancy/g1/state",
                          "fluxara/commands/vd1/ack", "fluxara/services/x/ack", "fluxara/ha/n1/state",
                          "Fluxara/HA/n1/state", "fluxara/devices/d/ack", "fluxara/node/n1/x",
                          "ppc/commands/setpoints", "ppc/cmd/soc/1", "ppc/setpoints", "ppc/block/1/setpoint",
                          "PPC/SetPoints", "ppc-test/pfr/setpoints", "redundancy/g1/write", "/fluxara/ha/x"}) {
        const std::string topic(t);
        std::vector<std::string> everything = {topic};
        const auto slash = topic.find('/', 1);
        everything.push_back(topic.substr(0, slash));
        check(mp::isCommandTopic(topic), "command topic detected: " + topic);
        check(!mp::refusalReason(topic, everything).empty(), "command topic refused even if allow-listed: " + topic);
    }
    for (const char* t : {"test/a", "data/stream/x", "historical/d1/readings", "ppc/status", "ppc/measurements",
                          "health/service/x", "controller/x", "writer/x"}) {
        check(!mp::isCommandTopic(t), std::string("not a command topic: ") + t);
    }

    // --- wildcards and $ topics ----------------------------------------------
    const std::vector<std::string> all = {"test", "$SYS"};
    for (const char* t : {"test/#", "test/+/x", "#", "$SYS/broker/clients", ""}) {
        check(!mp::refusalReason(t, all).empty(), std::string("wildcard/$/empty refused: '") + t + "'");
    }

    std::printf("mqtt_publish_endpoint_policy_test: %d/%d passed\n", g_run - g_fail, g_run);
    return g_fail == 0 ? 0 : 1;
}
