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
    auto body    = json::parse(req.body);
    std::string url      = body.value("url",      "https://myopenhab.org");
    std::string username = body.value("username", "");
    std::string password = body.value("password", "");
    std::string token    = body.value("token",    "");
    return openhab::OpenHABClient(url, username, password, token);
}

static std::string bodyStr(const httplib::Request& req) {
    auto b = json::parse(req.body);
    if (b.contains("body") && !b["body"].is_null())
        return b["body"].get<std::string>();
    return "";
}

static std::string param(const httplib::Request& req,
                          const std::string& key,
                          const std::string& def = "") {
    auto b = json::parse(req.body);
    if (b.contains("params") && b["params"].contains(key))
        return b["params"][key].get<std::string>();
    return def;
}

// ── Macro helpers ─────────────────────────────────────────────────────────────

#define POST(path, body) \
    svr.Post(path, [](const httplib::Request& req, httplib::Response& res) { \
        try { body } catch (const std::exception& e) { err(res, e); } \
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
    POST("/api/uuid",           { ok(res, openhab::UUID(makeClient(req)).getUUID()); });
    POST("/api/systeminfo",     { ok(res, openhab::Systeminfo(makeClient(req)).getSystemInfo()); });
    POST("/api/systeminfo/uom", { ok(res, openhab::Systeminfo(makeClient(req)).getUoMInfo()); });

    // ── Items ─────────────────────────────────────────────────────────────────
    POST("/api/items",                   { ok(res, openhab::Items(makeClient(req)).getItems()); });
    POST("/api/items/metadata/purge",    { ok(res, openhab::Items(makeClient(req)).purgeOrphanedMetadata()); });
    svr.Post("/api/items/([^/]+)",       [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Items(makeClient(req)).getItem(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/items/([^/]+)/state", [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Items(makeClient(req)).getItemState(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/items/([^/]+)/state/update", [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Items(makeClient(req)).updateItemState(req.matches[1], bodyStr(req))); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/items/([^/]+)/command", [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Items(makeClient(req)).sendCommand(req.matches[1], bodyStr(req))); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/items/([^/]+)/delete", [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Items(makeClient(req)).deleteItem(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/items/([^/]+)/tags/([^/]+)/add", [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Items(makeClient(req)).addTag(req.matches[1], req.matches[2])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/items/([^/]+)/tags/([^/]+)/remove", [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Items(makeClient(req)).removeTag(req.matches[1], req.matches[2])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/items/([^/]+)/members/([^/]+)/add", [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Items(makeClient(req)).addGroupMember(req.matches[1], req.matches[2])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/items/([^/]+)/members/([^/]+)/remove", [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Items(makeClient(req)).removeGroupMember(req.matches[1], req.matches[2])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/items/([^/]+)/metadata/namespaces", [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Items(makeClient(req)).getMetadataNamespaces(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });

    // ── Things ────────────────────────────────────────────────────────────────
    POST("/api/things", { ok(res, openhab::Things(makeClient(req)).getThings()); });
    svr.Post("/api/things/([^/]+)",          [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Things(makeClient(req)).getThing(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/things/([^/]+)/status",   [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Things(makeClient(req)).getThingStatus(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/things/([^/]+)/enable",   [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Things(makeClient(req)).enableThing(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/things/([^/]+)/disable",  [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Things(makeClient(req)).disableThing(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/things/([^/]+)/delete",   [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Things(makeClient(req)).deleteThing(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/things/([^/]+)/firmwares",[](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Things(makeClient(req)).getThingFirmwares(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });

    // ── Rules ─────────────────────────────────────────────────────────────────
    POST("/api/rules", { ok(res, openhab::Rules(makeClient(req)).getRules()); });
    svr.Post("/api/rules/([^/]+)",            [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Rules(makeClient(req)).getRule(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/rules/([^/]+)/enable",     [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Rules(makeClient(req)).enable(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/rules/([^/]+)/disable",    [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Rules(makeClient(req)).disable(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/rules/([^/]+)/runnow",     [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Rules(makeClient(req)).runNow(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/rules/([^/]+)/delete",     [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Rules(makeClient(req)).deleteRule(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/rules/([^/]+)/actions",    [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Rules(makeClient(req)).getActions(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/rules/([^/]+)/triggers",   [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Rules(makeClient(req)).getTriggers(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/rules/([^/]+)/conditions", [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Rules(makeClient(req)).getConditions(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });

    // ── Addons ────────────────────────────────────────────────────────────────
    POST("/api/addons",             { ok(res, openhab::Addons(makeClient(req)).getAddons()); });
    POST("/api/addons/types",       { ok(res, openhab::Addons(makeClient(req)).getAddonTypes()); });
    POST("/api/addons/suggestions", { ok(res, openhab::Addons(makeClient(req)).getAddonSuggestions()); });
    POST("/api/addons/services",    { ok(res, openhab::Addons(makeClient(req)).getAddonServices()); });
    svr.Post("/api/addons/([^/]+)/install",   [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Addons(makeClient(req)).installAddon(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/addons/([^/]+)/uninstall", [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Addons(makeClient(req)).uninstallAddon(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/addons/([^/]+)",           [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Addons(makeClient(req)).getAddon(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });

    // ── Audio ─────────────────────────────────────────────────────────────────
    POST("/api/audio/defaultsink",   { ok(res, openhab::Audio(makeClient(req)).getDefaultSink()); });
    POST("/api/audio/defaultsource", { ok(res, openhab::Audio(makeClient(req)).getDefaultSource()); });
    POST("/api/audio/sinks",         { ok(res, openhab::Audio(makeClient(req)).getSinks()); });
    POST("/api/audio/sources",       { ok(res, openhab::Audio(makeClient(req)).getSources()); });

    // ── Logging ───────────────────────────────────────────────────────────────
    POST("/api/logging", { ok(res, openhab::Logging(makeClient(req)).getLoggers()); });
    svr.Post("/api/logging/([^/]+)/set",    [](const httplib::Request& req, httplib::Response& res) {
        try { auto level = param(req,"level","INFO");
              ok(res, openhab::Logging(makeClient(req)).modifyOrAddLogger(req.matches[1], level)); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/logging/([^/]+)/delete", [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Logging(makeClient(req)).removeLogger(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/logging/([^/]+)",        [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Logging(makeClient(req)).getLogger(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });

    // ── Links ─────────────────────────────────────────────────────────────────
    POST("/api/links",        { ok(res, openhab::Links(makeClient(req)).getLinks()); });
    POST("/api/links/orphan", { ok(res, openhab::Links(makeClient(req)).getOrphanLinks()); });

    // ── ChannelTypes / ThingTypes / ConfigDescriptions ────────────────────────
    POST("/api/channel-types",       { ok(res, openhab::ChannelTypes(makeClient(req)).getChannelTypes()); });
    POST("/api/thing-types",         { ok(res, openhab::ThingTypes(makeClient(req)).getThingTypes()); });
    POST("/api/config-descriptions", { ok(res, openhab::ConfigDescriptions(makeClient(req)).getConfigDescriptions()); });

    // ── Persistence ───────────────────────────────────────────────────────────
    POST("/api/persistence",       { ok(res, openhab::Persistence(makeClient(req)).getServices()); });
    POST("/api/persistence/items", { ok(res, openhab::Persistence(makeClient(req)).getItemsFromService()); });
    svr.Post("/api/persistence/items/([^/]+)", [](const httplib::Request& req, httplib::Response& res) {
        try { auto svc = param(req,"serviceId","");
              ok(res, openhab::Persistence(makeClient(req)).getItemPersistenceData(req.matches[1], svc)); }
        catch (const std::exception& e) { err(res, e); }
    });

    // ── Discovery + Inbox ─────────────────────────────────────────────────────
    POST("/api/discovery", { ok(res, openhab::Discovery(makeClient(req)).getDiscoveryBindings()); });
    svr.Post("/api/discovery/([^/]+)/scan", [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Discovery(makeClient(req)).startBindingScan(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    POST("/api/inbox", { ok(res, openhab::Inbox(makeClient(req)).getDiscoveredThings()); });
    svr.Post("/api/inbox/([^/]+)/approve",  [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Inbox(makeClient(req)).approveDiscoveryResult(req.matches[1], bodyStr(req))); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/inbox/([^/]+)/ignore",   [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Inbox(makeClient(req)).ignoreDiscoveryResult(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/inbox/([^/]+)/unignore", [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Inbox(makeClient(req)).unignoreDiscoveryResult(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/inbox/([^/]+)/delete",   [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Inbox(makeClient(req)).removeDiscoveryResult(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });

    // ── Sitemaps ──────────────────────────────────────────────────────────────
    POST("/api/sitemaps", { ok(res, openhab::Sitemaps(makeClient(req)).getSitemaps()); });
    svr.Post("/api/sitemaps/([^/]+)/([^/]+)", [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Sitemaps(makeClient(req)).getSitemapPage(req.matches[1], req.matches[2])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/sitemaps/([^/]+)",         [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Sitemaps(makeClient(req)).getSitemap(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });

    // ── Tags ──────────────────────────────────────────────────────────────────
    POST("/api/tags", { ok(res, openhab::Tags(makeClient(req)).getTags()); });
    svr.Post("/api/tags/([^/]+)/delete", [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Tags(makeClient(req)).deleteTag(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/tags/([^/]+)",        [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Tags(makeClient(req)).getTag(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });

    // ── Templates / ModuleTypes / ProfileTypes ────────────────────────────────
    POST("/api/templates",    { ok(res, openhab::Templates(makeClient(req)).getTemplates()); });
    POST("/api/module-types", { ok(res, openhab::ModuleTypes(makeClient(req)).getModuleTypes()); });
    POST("/api/profile-types",{ ok(res, openhab::ProfileTypes(makeClient(req)).getProfileTypes()); });

    // ── Transformations ───────────────────────────────────────────────────────
    POST("/api/transformations",          { ok(res, openhab::Transformations(makeClient(req)).getTransformations()); });
    POST("/api/transformations/services", { ok(res, openhab::Transformations(makeClient(req)).getTransformationServices()); });
    svr.Post("/api/transformations/([^/]+)", [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Transformations(makeClient(req)).getTransformation(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });

    // ── UI / Services / Iconsets ──────────────────────────────────────────────
    POST("/api/ui/tiles", { ok(res, openhab::UI(makeClient(req)).getUITiles()); });
    svr.Post("/api/ui/components/([^/]+)", [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::UI(makeClient(req)).getUIComponents(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    POST("/api/services", { ok(res, openhab::Services(makeClient(req)).getServices()); });
    svr.Post("/api/services/([^/]+)/config", [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Services(makeClient(req)).getServiceConfig(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    svr.Post("/api/services/([^/]+)",        [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Services(makeClient(req)).getService(req.matches[1])); }
        catch (const std::exception& e) { err(res, e); }
    });
    POST("/api/iconsets", { ok(res, openhab::Iconsets(makeClient(req)).getIconsets()); });

    // ── Auth ──────────────────────────────────────────────────────────────────
    POST("/api/auth/apitokens", { ok(res, openhab::Auth(makeClient(req)).getAPITokens()); });
    POST("/api/auth/sessions",  { ok(res, openhab::Auth(makeClient(req)).getSessions()); });

    // ── Voice ─────────────────────────────────────────────────────────────────
    POST("/api/voice/voices",       { ok(res, openhab::Voice(makeClient(req)).getVoices()); });
    POST("/api/voice/defaultvoice", { ok(res, openhab::Voice(makeClient(req)).getDefaultVoice()); });
    POST("/api/voice/interpreters", { ok(res, openhab::Voice(makeClient(req)).getInterpreters()); });
    POST("/api/voice/say",          { ok(res, openhab::Voice(makeClient(req)).sayText(bodyStr(req),"","")); });

    // ── Actions ───────────────────────────────────────────────────────────────
    svr.Post("/api/actions/([^/]+)", [](const httplib::Request& req, httplib::Response& res) {
        try { ok(res, openhab::Actions(makeClient(req)).getActions(req.matches[1])); }
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