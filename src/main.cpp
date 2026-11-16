// Backend of the cpp-openhab-rest-client test app: makes every method of the C++ library
// callable over HTTP. The browser sends the openHAB address, the credentials, the class
// and method name of python-openhab-rest-client and the arguments as JSON object.
//
//   GET  /                     health check (wake-up on Render)
//   GET  /api/methods          all methods with parameters (methods.json)
//   POST /api/connect          {url, token | username+password}
//   POST /api/call             {url, token, ..., class, method, args: {...}}
//   GET  /api/events/stream    ?url&token&...&class&method&args=<json>  (Server-Sent Events)
#include "httplib.h"
#include "calls.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>

using openhab::json;

namespace {

std::string methodsJson;

void setCORS(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
    res.set_header("Access-Control-Max-Age", "3600");
}

void sendJson(httplib::Response& res, const json& value, int status = 200) {
    setCORS(res);
    res.status = status;
    res.set_content(value.dump(), "application/json");
}

void sendError(httplib::Response& res, const std::string& msg, int status) {
    sendJson(res, json{{"error", msg}}, status);
}

std::optional<std::string> opt(const std::string& s) {
    return s.empty() ? std::nullopt : std::optional<std::string>(s);
}

// Client from url/token/username/password; the constructor checks the connection.
std::unique_ptr<openhab::OpenHABClient> makeClient(const std::string& url,
                                                   const std::string& token,
                                                   const std::string& user,
                                                   const std::string& pass) {
    openhab::ClientOptions o;
    o.token = opt(token);
    o.username = opt(user);
    o.password = opt(pass);
    return std::make_unique<openhab::OpenHABClient>(url.empty() ? "https://myopenhab.org" : url, o);
}

std::unique_ptr<openhab::OpenHABClient> makeClient(const json& body) {
    return makeClient(openhab::str(body, "url"), openhab::str(body, "token"),
                      openhab::str(body, "username"), openhab::str(body, "password"));
}

std::string param(const httplib::Request& req, const char* key) {
    return req.has_param(key) ? req.get_param_value(key) : std::string();
}

}  // namespace

int main() {
    std::ifstream f("methods.json");
    std::stringstream ss;
    ss << f.rdbuf();
    methodsJson = ss.str().empty() ? "[]" : ss.str();

    httplib::Server svr;

    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        setCORS(res);
        res.status = 204;
    });

    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        sendJson(res, json{{"status", "ok"}, {"library", "cpp-openhab-rest-client"},
                           {"version", openhab::VERSION}});
    });

    svr.Get("/api/methods", [](const httplib::Request&, httplib::Response& res) {
        setCORS(res);
        res.set_content(methodsJson, "application/json");
    });

    svr.Post("/api/connect", [](const httplib::Request& req, httplib::Response& res) {
        json body = json::parse(req.body, nullptr, false);
        if (!body.is_object()) return sendError(res, "Invalid JSON body", 400);
        auto c = makeClient(body);
        json j{{"loggedIn", c->isLoggedIn()}, {"isCloud", c->isCloud()}, {"url", c->url()}};
        if (!c->lastError().empty()) j["error"] = c->lastError();
        sendJson(res, j);
    });

    svr.Post("/api/call", [](const httplib::Request& req, httplib::Response& res) {
        json body = json::parse(req.body, nullptr, false);
        if (!body.is_object()) return sendError(res, "Invalid JSON body", 400);
        std::string cls = openhab::str(body, "class"), method = openhab::str(body, "method");
        if (cls.empty() || method.empty()) return sendError(res, "class and method are required", 400);
        json args = body.contains("args") && body["args"].is_object() ? body["args"] : json::object();
        auto c = makeClient(body);
        try {
            auto result = openhabCall(*c, cls, method, args);
            if (!result) return sendError(res, "Unknown method " + cls + "." + method, 400);
            sendJson(res, *result);
        } catch (const json::exception& e) {   // an argument of the wrong type
            sendError(res, std::string("Invalid argument: ") + e.what(), 400);
        }
    });

    // Event stream: the C++ library reads openHAB, the backend forwards every event.
    svr.Get("/api/events/stream", [](const httplib::Request& req, httplib::Response& res) {
        setCORS(res);
        std::string cls = param(req, "class"), method = param(req, "method");
        if (cls.empty()) {
            cls = "Events";
            method = "getEvents";
        }
        json args = json::parse(param(req, "args"), nullptr, false);
        if (!args.is_object()) args = json::object();
        std::string topics = param(req, "topics");   // version 1.x of the test app
        if (!topics.empty() && !args.contains("topics")) args["topics"] = topics;

        auto client = std::shared_ptr<openhab::OpenHABClient>(
            makeClient(param(req, "url"), param(req, "token"), param(req, "username"),
                       param(req, "password")));
        std::optional<openhab::EventStream> opened;
        try {
            opened = openhabOpenStream(*client, cls, method, args);
        } catch (const json::exception& e) {
            return sendError(res, std::string("Invalid argument: ") + e.what(), 400);
        }
        if (!opened) return sendError(res, "Unknown event stream " + cls + "." + method, 400);
        if (opened->isError()) return sendJson(res, opened->errorResult(), 502);
        auto s = std::make_shared<openhab::EventStream>(std::move(*opened));
        res.set_header("Cache-Control", "no-cache");
        res.set_chunked_content_provider(
            "text/event-stream",
            [s, client](size_t, httplib::DataSink& sink) {
                // wait up to 15 s; ALIVE messages keep the connection to the browser open
                auto ev = s->nextEvent(15000, true);
                std::string chunk;
                if (ev) {
                    chunk = "data: " + ev->dump() + "\n\n";
                } else if (s->isOpen()) {
                    chunk = ": keep-alive\n\n";
                } else {
                    if (s->isError()) {
                        std::string msg = "event: error\ndata: " + s->errorResult().dump() + "\n\n";
                        sink.write(msg.data(), msg.size());
                    }
                    sink.done();
                    return true;
                }
                return sink.write(chunk.data(), chunk.size());
            },
            [s](bool) { s->close(); });
    });

    const char* portEnv = std::getenv("PORT");
    int port = portEnv ? std::atoi(portEnv) : 8080;
    svr.set_read_timeout(30);
    svr.set_write_timeout(30);
    std::cout << "cpp-openhab-rest-client " << openhab::VERSION << " backend listening on port "
              << port << std::endl;
    svr.listen("0.0.0.0", port);
    return 0;
}
