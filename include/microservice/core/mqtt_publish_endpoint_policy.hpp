#pragma once

/**
 * Opt-in gate and topic guard for POST /api/v1/custom/mqtt/publish.
 *
 * The endpoint publishes under this service's own broker identity, so once the
 * broker enforces per-service topic ACLs it is a way around them
 * (Fluxara-co/mosquitto-config#27, risk R6 in docs/clients.md).
 *
 * - Off unless LOADSHED_MQTT_PUBLISH_ENABLED is 1/true/yes/on (case-insensitive, trimmed).
 *   Off: the route is not registered (404).
 * - On: a topic is published only if it equals, or sits under, a prefix listed
 *   in LOADSHED_MQTT_PUBLISH_ALLOWED_PREFIXES (comma-separated). An empty list refuses every topic.
 * - Command topics are refused even when allow-listed: any write, operate,
 *   control, commands, cmd, run or config segment; anything under pm/control,
 *   fluxara/{redundancy,commands,services,devices,node,ha}, ppc/commands,
 *   ppc/cmd or redundancy; ppc setpoints; wildcards and $ topics.
 *   Segments are lower-cased before this check.
 */

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

namespace microservice {
namespace mqtt_publish_policy {

constexpr const char* kEnabledEnv = "LOADSHED_MQTT_PUBLISH_ENABLED";
constexpr const char* kAllowedPrefixesEnv = "LOADSHED_MQTT_PUBLISH_ALLOWED_PREFIXES";

inline std::string trim(const std::string& s) {
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

inline std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

/** Only 1/true/yes/on (case-insensitive, surrounding spaces ignored) enable. */
inline bool parseEnabled(const char* value) {
    if (value == nullptr) return false;
    const std::string v = toLower(trim(value));
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

/** Comma-separated prefixes; a trailing "/#" and surrounding slashes are dropped. */
inline std::vector<std::string> parseAllowedPrefixes(const char* value) {
    std::vector<std::string> out;
    if (value == nullptr) return out;
    const std::string raw(value);
    std::size_t start = 0;
    while (start <= raw.size()) {
        const auto comma = raw.find(',', start);
        std::string p = trim(raw.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        if (p.size() >= 2 && p.compare(p.size() - 2, 2, "/#") == 0) p.resize(p.size() - 2);
        const auto b = p.find_first_not_of('/');
        const auto e = p.find_last_not_of('/');
        p = (b == std::string::npos) ? "" : p.substr(b, e - b + 1);
        if (!p.empty()) out.push_back(p);
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return out;
}

inline bool matchesPrefix(const std::string& topic, const std::string& prefix) {
    if (topic == prefix) return true;
    return topic.size() > prefix.size() && topic.compare(0, prefix.size(), prefix) == 0 &&
           topic[prefix.size()] == '/';
}

/** True for command topics this endpoint must never publish. */
inline bool isCommandTopic(const std::string& topic) {
    std::string t = toLower(topic);
    const auto b = t.find_first_not_of('/');
    t = (b == std::string::npos) ? "" : t.substr(b);

    static const char* const kPrefixes[] = {
        "pm/control",       "fluxara/redundancy", "fluxara/commands", "fluxara/services",
        "fluxara/devices",  "fluxara/node",       "fluxara/ha",       "ppc/commands",
        "ppc/cmd",          "redundancy",
    };
    for (const char* p : kPrefixes) {
        if (matchesPrefix(t, p)) return true;
    }

    std::vector<std::string> segs;
    std::size_t start = 0;
    while (true) {
        const auto slash = t.find('/', start);
        segs.push_back(t.substr(start, slash == std::string::npos ? std::string::npos : slash - start));
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    static const char* const kSegments[] = {"write", "operate", "control", "commands", "cmd", "run", "config"};
    for (const auto& s : segs) {
        for (const char* g : kSegments) {
            if (s == g) return true;
        }
    }
    // PPC setpoints (ppc/..., ppc-test/...), whatever the rest of the path is.
    if (!segs.empty() && (segs[0] == "ppc" || segs[0] == "ppc-test")) {
        for (const auto& s : segs) {
            if (s == "setpoint" || s == "setpoints") return true;
        }
    }
    return false;
}

/** Empty when ``topic`` may be published; otherwise the reason it is refused. */
inline std::string refusalReason(const std::string& topic, const std::vector<std::string>& allowed_prefixes) {
    if (topic.empty()) return "empty topic";
    if (topic.find_first_of("+#") != std::string::npos || topic[0] == '$' ||
        topic.find('\0') != std::string::npos) {
        return "wildcard or system topic: " + topic;
    }
    if (isCommandTopic(topic)) return "refusing to publish to command topic " + topic;
    for (const auto& p : allowed_prefixes) {
        if (matchesPrefix(topic, p)) return "";
    }
    return "topic " + topic + " is not in " + kAllowedPrefixesEnv;
}

inline bool enabledFromEnv() { return parseEnabled(std::getenv(kEnabledEnv)); }

inline std::vector<std::string> allowedPrefixesFromEnv() {
    return parseAllowedPrefixes(std::getenv(kAllowedPrefixesEnv));
}

}  // namespace mqtt_publish_policy
}  // namespace microservice
