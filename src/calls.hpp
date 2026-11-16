// Calls any method of cpp-openhab-rest-client by the class and method name of
// python-openhab-rest-client, with the arguments as JSON object (by parameter name).
#pragma once
#include <openhab/openhab.hpp>

#include <optional>
#include <string>

/** Result of the REST method, or nullopt if there is no such method. */
std::optional<openhab::json> openhabCall(openhab::OpenHABClient& c, const std::string& cls,
                                         const std::string& m, const openhab::json& args);

/** Event stream of a *Events method, or nullopt if there is no such stream. */
std::optional<openhab::EventStream> openhabOpenStream(openhab::OpenHABClient& c,
                                                      const std::string& cls,
                                                      const std::string& m,
                                                      const openhab::json& args);
