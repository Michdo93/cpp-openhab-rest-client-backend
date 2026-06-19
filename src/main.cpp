#include "httplib.h"
#include <openhab/openhab.h>
#include <nlohmann/json.hpp>
#include <string>
#include <cstdlib>

using json = nlohmann::json;

// ── Helpers ───────────────────────────────────────────────────────────────────

static void setCORS(httplib::Response& res) {
    res.set_header("Access-Control-Allow-Origin",  "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    res.set_header("Access-Control-Max-Age",       "3600");
}

static void ok(httplib::Response& res, const json& j) {
    setCORS(res);
    res.set_content(j.dump(), "application/json");
}

static void err(httplib::Response& res, const std::exception& e, int code = 502) {
    setCORS(res);
    res.status = code;
    res.set_content(json{{"error", e.what()}}.dump(), "application/json");
}

static openhab::OpenHABClient makeClient(const httplib::Request& req) {
    auto body = json::parse(req.body);
    std::string url      = body.contains("url")      && body["url"].is_string()      ? body["url"].get<std::string>()      : "https://myopenhab.org";
    std::string username = body.contains("username") && body["username"].is_string() ? body["username"].get<std::string>() : "";
    std::string password = body.contains("password") && body["password"].is_string() ? body["password"].get<std::string>() : "";
    std::string token    = body.contains("token")    && body["token"].is_string()    ? body["token"].get<std::string>()    : "";
    return openhab::OpenHABClient(url, username, password, token);
}

static std::string bodyStr(const httplib::Request& req) {
    try {
        auto b = json::parse(req.body);
        if (b.contains("body") && b["body"].is_string())
            return b["body"].get<std::string>();
    } catch (...) {}
    return "";
}

static std::string param(const httplib::Request& req,
                          const std::string& key,
                          const std::string& def = "") {
    try {
        auto b = json::parse(req.body);
        if (b.contains("params") && !b["params"].is_null() && b["params"].contains(key)
            && b["params"][key].is_string())
            return b["params"][key].get<std::string>();
    } catch (...) {}
    return def;
}

// ── Macro helpers ─────────────────────────────────────────────────────────────

#define POST(path, body) \
    svr.Post(path, [](const httplib::Request& req, httplib::Response& res) { \
        try { auto c = makeClient(req); body } catch (const std::exception& e) { err(res, e); } \
    })

int main() {
    httplib::Server svr;

    // ── CORS preflight – catch-all OPTIONS ────────────────────────────────────
    svr.Options(".*", [](const httplib::Request&, httplib::Response& res) {
        setCORS(res);
        res.status = 204;
    });

    // ── Healthcheck ───────────────────────────────────────────────────────────
    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        setCORS(res);
        res.set_content(R"({"status":"ok"})", "application/json");
    });

    // ── Connect ───────────────────────────────────────────────────────────────
    svr.Post("/api/connect", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto c = makeClient(req);
            ok(res, {{"loggedIn", c.isLoggedIn()},
                     {"isCloud",  c.isCloud()},
                     {"url",      c.baseUrl()}});
        } catch (const std::exception& e) { err(res, e); }
    });

    // ── UUID + Systeminfo ─────────────────────────────────────────────────────
    POST("/api/uuid", {  ok(res, openhab::UUID(c).getUUID());  });
    POST("/api/systeminfo", {  ok(res, openhab::Systeminfo(c).getSystemInfo());  });
    POST("/api/systeminfo/uom", {  ok(res, openhab::Systeminfo(c).getUoMInfo());  });

    // ── Items ─────────────────────────────────────────────────────────────────
    POST("/api/items", {  ok(res, openhab::Items(c).getItems());  });
    POST("/api/items/metadata/purge", {  ok(res, openhab::Items(c).purgeOrphanedMetadata());  });
    svr.Post("/api/items/([^/]+)",       [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Items(c).getItem(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/items/([^/]+)/state", [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Items(c).getItemState(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/items/([^/]+)/state/update", [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Items(c).updateItemState(req.matches[1], bodyStr(req)));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/items/([^/]+)/command", [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Items(c).sendCommand(req.matches[1], bodyStr(req)));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/items/([^/]+)/delete", [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Items(c).deleteItem(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/items/([^/]+)/tags/([^/]+)/add", [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Items(c).addTag(req.matches[1], req.matches[2]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/items/([^/]+)/tags/([^/]+)/remove", [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Items(c).removeTag(req.matches[1], req.matches[2]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/items/([^/]+)/members/([^/]+)/add", [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Items(c).addGroupMember(req.matches[1], req.matches[2]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/items/([^/]+)/members/([^/]+)/remove", [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Items(c).removeGroupMember(req.matches[1], req.matches[2]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/items/([^/]+)/metadata/namespaces", [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Items(c).getMetadataNamespaces(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });

    // ── Things ────────────────────────────────────────────────────────────────
    POST("/api/things", {  ok(res, openhab::Things(c).getThings());  });
    svr.Post("/api/things/([^/]+)",          [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Things(c).getThing(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/things/([^/]+)/status",   [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Things(c).getThingStatus(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/things/([^/]+)/enable",   [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Things(c).enableThing(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/things/([^/]+)/disable",  [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Things(c).disableThing(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/things/([^/]+)/delete",   [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Things(c).deleteThing(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/things/([^/]+)/firmwares",[](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Things(c).getThingFirmwares(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });

    // ── Rules ─────────────────────────────────────────────────────────────────
    POST("/api/rules", {  ok(res, openhab::Rules(c).getRules());  });
    svr.Post("/api/rules/([^/]+)",            [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Rules(c).getRule(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/rules/([^/]+)/enable",     [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Rules(c).enable(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/rules/([^/]+)/disable",    [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Rules(c).disable(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/rules/([^/]+)/runnow",     [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Rules(c).runNow(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/rules/([^/]+)/delete",     [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Rules(c).deleteRule(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/rules/([^/]+)/actions",    [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Rules(c).getActions(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/rules/([^/]+)/triggers",   [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Rules(c).getTriggers(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/rules/([^/]+)/conditions", [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Rules(c).getConditions(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });

    // ── Addons ────────────────────────────────────────────────────────────────
    POST("/api/addons", {  ok(res, openhab::Addons(c).getAddons());  });
    POST("/api/addons/types", {  ok(res, openhab::Addons(c).getAddonTypes());  });
    POST("/api/addons/suggestions", {  ok(res, openhab::Addons(c).getAddonSuggestions());  });
    POST("/api/addons/services", {  ok(res, openhab::Addons(c).getAddonServices());  });
    svr.Post("/api/addons/([^/]+)/install",   [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Addons(c).installAddon(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/addons/([^/]+)/uninstall", [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Addons(c).uninstallAddon(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/addons/([^/]+)",           [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Addons(c).getAddon(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });

    // ── Audio ─────────────────────────────────────────────────────────────────
    POST("/api/audio/defaultsink", {  ok(res, openhab::Audio(c).getDefaultSink());  });
    POST("/api/audio/defaultsource", {  ok(res, openhab::Audio(c).getDefaultSource());  });
    POST("/api/audio/sinks", {  ok(res, openhab::Audio(c).getSinks());  });
    POST("/api/audio/sources", {  ok(res, openhab::Audio(c).getSources());  });

    // ── Logging ───────────────────────────────────────────────────────────────
    POST("/api/logging", {  ok(res, openhab::Logging(c).getLoggers());  });
    svr.Post("/api/logging/([^/]+)/set",    [](const httplib::Request& req, httplib::Response& res) {
        try { auto level = param(req,"level","INFO");
              auto c = makeClient(req); ok(res, openhab::Logging(c).modifyOrAddLogger(req.matches[1], level)); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/logging/([^/]+)/delete", [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Logging(c).removeLogger(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/logging/([^/]+)",        [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Logging(c).getLogger(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });

    // ── Links ─────────────────────────────────────────────────────────────────
    POST("/api/links", {  ok(res, openhab::Links(c).getLinks());  });
    POST("/api/links/orphan", {  ok(res, openhab::Links(c).getOrphanLinks());  });

    // ── ChannelTypes / ThingTypes / ConfigDescriptions ────────────────────────
    POST("/api/channel-types", {  ok(res, openhab::ChannelTypes(c).getChannelTypes());  });
    POST("/api/thing-types", {  ok(res, openhab::ThingTypes(c).getThingTypes());  });
    POST("/api/config-descriptions", {  ok(res, openhab::ConfigDescriptions(c).getConfigDescriptions());  });

    // ── Persistence ───────────────────────────────────────────────────────────
    POST("/api/persistence", {  ok(res, openhab::Persistence(c).getServices());  });
    POST("/api/persistence/items", {  ok(res, openhab::Persistence(c).getItemsFromService());  });
    svr.Post("/api/persistence/items/([^/]+)", [](const httplib::Request& req, httplib::Response& res) {
        try { auto svc = param(req,"serviceId","");
              auto c = makeClient(req); ok(res, openhab::Persistence(c).getItemPersistenceData(req.matches[1], svc)); }
        catch (const std::exception& e) { err(res, e); }
    });

    // ── Discovery + Inbox ─────────────────────────────────────────────────────
    POST("/api/discovery", {  ok(res, openhab::Discovery(c).getDiscoveryBindings());  });
    svr.Post("/api/discovery/([^/]+)/scan", [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Discovery(c).startBindingScan(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    POST("/api/inbox", {  ok(res, openhab::Inbox(c).getDiscoveredThings());  });
    svr.Post("/api/inbox/([^/]+)/approve",  [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Inbox(c).approveDiscoveryResult(req.matches[1], bodyStr(req)));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/inbox/([^/]+)/ignore",   [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Inbox(c).ignoreDiscoveryResult(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/inbox/([^/]+)/unignore", [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Inbox(c).unignoreDiscoveryResult(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/inbox/([^/]+)/delete",   [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Inbox(c).removeDiscoveryResult(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });

    // ── Sitemaps ──────────────────────────────────────────────────────────────
    POST("/api/sitemaps", {  ok(res, openhab::Sitemaps(c).getSitemaps());  });
    svr.Post("/api/sitemaps/([^/]+)/([^/]+)", [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Sitemaps(c).getSitemapPage(req.matches[1], req.matches[2]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/sitemaps/([^/]+)",         [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Sitemaps(c).getSitemap(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });

    // ── Tags ──────────────────────────────────────────────────────────────────
    POST("/api/tags", {  ok(res, openhab::Tags(c).getTags());  });
    svr.Post("/api/tags/([^/]+)/delete", [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Tags(c).deleteTag(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/tags/([^/]+)",        [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Tags(c).getTag(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });

    // ── Templates / ModuleTypes / ProfileTypes ────────────────────────────────
    POST("/api/templates", {  ok(res, openhab::Templates(c).getTemplates());  });
    POST("/api/module-types", {  ok(res, openhab::ModuleTypes(c).getModuleTypes());  });
    POST("/api/profile-types", {  ok(res, openhab::ProfileTypes(c).getProfileTypes());  });

    // ── Transformations ───────────────────────────────────────────────────────
    POST("/api/transformations", {  ok(res, openhab::Transformations(c).getTransformations());  });
    POST("/api/transformations/services", {  ok(res, openhab::Transformations(c).getTransformationServices());  });
    svr.Post("/api/transformations/([^/]+)", [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Transformations(c).getTransformation(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });

    // ── UI / Services / Iconsets ──────────────────────────────────────────────
    POST("/api/ui/tiles", {  ok(res, openhab::UI(c).getUITiles());  });
    svr.Post("/api/ui/components/([^/]+)", [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::UI(c).getUIComponents(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    POST("/api/services", {  ok(res, openhab::Services(c).getServices());  });
    svr.Post("/api/services/([^/]+)/config", [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Services(c).getServiceConfig(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/services/([^/]+)",        [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Services(c).getService(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });
    POST("/api/iconsets", {  ok(res, openhab::Iconsets(c).getIconsets());  });

    // ── Auth ──────────────────────────────────────────────────────────────────
    POST("/api/auth/apitokens", {  ok(res, openhab::Auth(c).getAPITokens());  });
    POST("/api/auth/sessions", {  ok(res, openhab::Auth(c).getSessions());  });

    // ── Voice ─────────────────────────────────────────────────────────────────
    POST("/api/voice/voices", {  ok(res, openhab::Voice(c).getVoices());  });
    POST("/api/voice/defaultvoice", {  ok(res, openhab::Voice(c).getDefaultVoice());  });
    POST("/api/voice/interpreters", {  ok(res, openhab::Voice(c).getInterpreters());  });
    POST("/api/voice/say", {  ok(res, openhab::Voice(c).sayText(bodyStr(req),"",""));  });

    // ── Actions ───────────────────────────────────────────────────────────────
    svr.Post("/api/actions/([^/]+)", [](const httplib::Request& req, httplib::Response& res) {
        try { auto c = makeClient(req); ok(res, openhab::Actions(c).getActions(req.matches[1]));  }
        catch (const std::exception& e) { err(res, e); }
    });

    // ── Start ─────────────────────────────────────────────────────────────────
    const char* portEnv = std::getenv("PORT");
    int port = portEnv ? std::stoi(portEnv) : 8080;

    svr.set_keep_alive_max_count(100);
    svr.set_read_timeout(30);
    svr.set_write_timeout(30);

    std::cout << "Listening on port " << port << std::endl;
    svr.listen("0.0.0.0", port);
    return 0;
}