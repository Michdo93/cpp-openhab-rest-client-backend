#include "crow.h"
#include <openhab/openhab.h>
#include <nlohmann/json.hpp>
#include <string>
#include <stdexcept>
#include <cstdlib>

using json = nlohmann::json;

// ── Helper: build client from request body ────────────────────────────────────

static openhab::OpenHABClient makeClient(const crow::request& req) {
    auto body = json::parse(req.body);
    std::string url      = body.value("url",      "https://myopenhab.org");
    std::string username = body.value("username", "");
    std::string password = body.value("password", "");
    std::string token    = body.value("token",    "");
    return openhab::OpenHABClient(url, username, password, token);
}

static std::string bodyStr(const crow::request& req) {
    auto b = json::parse(req.body);
    if (b.contains("body") && !b["body"].is_null())
        return b["body"].get<std::string>();
    return "";
}

static json bodyJson(const crow::request& req) {
    auto b = json::parse(req.body);
    if (b.contains("body") && !b["body"].is_null()) {
        if (b["body"].is_string())
            return json::parse(b["body"].get<std::string>());
        return b["body"];
    }
    return json::object();
}

static std::string param(const crow::request& req, const std::string& key,
                          const std::string& def = "") {
    auto b = json::parse(req.body);
    if (b.contains("params") && b["params"].contains(key))
        return b["params"][key].get<std::string>();
    return def;
}

// ── Response helpers with CORS headers ───────────────────────────────────────

static void addCors(crow::response& r) {
    r.set_header("Access-Control-Allow-Origin",  "*");
    r.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    r.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

static crow::response ok(const json& j) {
    crow::response r(j.dump());
    r.set_header("Content-Type", "application/json");
    addCors(r);
    return r;
}

static crow::response err(const std::exception& e, int code = 502) {
    json j = {{"error", e.what()}};
    crow::response r(code, j.dump());
    r.set_header("Content-Type", "application/json");
    addCors(r);
    return r;
}

// ── Main ─────────────────────────────────────────────────────────────────────

int main() {
    crow::App app;

    // OPTIONS preflight – must come first
    CROW_ROUTE(app, "/<path>").methods(crow::HTTPMethod::OPTIONS)
    ([](const crow::request&, crow::response& res, const std::string&) {
        res.set_header("Access-Control-Allow-Origin",  "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.code = 204;
        res.end();
    });

    // ── Connect ───────────────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/connect").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try {
            auto c = makeClient(req);
            json r = {{"loggedIn", c.isLoggedIn()},
                      {"isCloud",  c.isCloud()},
                      {"url",      c.baseUrl()}};
            return ok(r);
        } catch (const std::exception& e) { return err(e); }
    });

    // ── UUID + Systeminfo ─────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/uuid").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::UUID(c).getUUID()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/systeminfo").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Systeminfo(c).getSystemInfo()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/systeminfo/uom").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Systeminfo(c).getUoMInfo()); }
        catch (const std::exception& e) { return err(e); }
    });

    // ── Items ─────────────────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/items").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Items(c).getItems()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/items/<string>").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& name) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Items(c).getItem(name)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/items/<string>/state").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& name) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Items(c).getItemState(name)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/items/<string>/state/update").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& name) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Items(c).updateItemState(name, bodyStr(req))); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/items/<string>/command").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& name) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Items(c).sendCommand(name, bodyStr(req))); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/items/<string>/delete").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& name) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Items(c).deleteItem(name)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/items/<string>/tags/<string>/add").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& name, const std::string& tag) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Items(c).addTag(name, tag)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/items/<string>/tags/<string>/remove").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& name, const std::string& tag) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Items(c).removeTag(name, tag)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/items/<string>/members/<string>/add").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& name, const std::string& member) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Items(c).addGroupMember(name, member)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/items/<string>/members/<string>/remove").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& name, const std::string& member) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Items(c).removeGroupMember(name, member)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/items/<string>/metadata/namespaces").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& name) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Items(c).getMetadataNamespaces(name)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/items/metadata/purge").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Items(c).purgeOrphanedMetadata()); }
        catch (const std::exception& e) { return err(e); }
    });

    // ── Things ────────────────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/things").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Things(c).getThings()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/things/<string>").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& uid) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Things(c).getThing(uid)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/things/<string>/status").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& uid) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Things(c).getThingStatus(uid)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/things/<string>/enable").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& uid) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Things(c).enableThing(uid)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/things/<string>/disable").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& uid) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Things(c).disableThing(uid)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/things/<string>/delete").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& uid) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Things(c).deleteThing(uid)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/things/<string>/firmwares").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& uid) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Things(c).getThingFirmwares(uid)); }
        catch (const std::exception& e) { return err(e); }
    });

    // ── Rules ─────────────────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/rules").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Rules(c).getRules()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/rules/<string>").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& uid) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Rules(c).getRule(uid)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/rules/<string>/enable").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& uid) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Rules(c).enable(uid)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/rules/<string>/disable").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& uid) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Rules(c).disable(uid)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/rules/<string>/runnow").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& uid) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Rules(c).runNow(uid)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/rules/<string>/delete").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& uid) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Rules(c).deleteRule(uid)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/rules/<string>/actions").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& uid) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Rules(c).getActions(uid)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/rules/<string>/triggers").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& uid) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Rules(c).getTriggers(uid)); }
        catch (const std::exception& e) { return err(e); }
    });

    // ── Addons ────────────────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/addons").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Addons(c).getAddons()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/addons/types").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Addons(c).getAddonTypes()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/addons/suggestions").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Addons(c).getAddonSuggestions()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/addons/services").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Addons(c).getAddonServices()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/addons/<string>").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& id) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Addons(c).getAddon(id)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/addons/<string>/install").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& id) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Addons(c).installAddon(id)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/addons/<string>/uninstall").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& id) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Addons(c).uninstallAddon(id)); }
        catch (const std::exception& e) { return err(e); }
    });

    // ── Audio ─────────────────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/audio/defaultsink").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Audio(c).getDefaultSink()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/audio/defaultsource").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Audio(c).getDefaultSource()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/audio/sinks").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Audio(c).getSinks()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/audio/sources").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Audio(c).getSources()); }
        catch (const std::exception& e) { return err(e); }
    });

    // ── Logging ───────────────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/logging").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Logging(c).getLoggers()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/logging/<string>").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& name) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Logging(c).getLogger(name)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/logging/<string>/set").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& name) -> crow::response {
        try { auto c=makeClient(req); auto level=param(req,"level","INFO");
              return ok(openhab::Logging(c).modifyOrAddLogger(name, level)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/logging/<string>/delete").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& name) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Logging(c).removeLogger(name)); }
        catch (const std::exception& e) { return err(e); }
    });

    // ── Links ─────────────────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/links").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Links(c).getLinks()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/links/orphan").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Links(c).getOrphanLinks()); }
        catch (const std::exception& e) { return err(e); }
    });

    // ── ChannelTypes / ThingTypes / ConfigDescriptions ────────────────────────
    CROW_ROUTE(app, "/api/channel-types").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::ChannelTypes(c).getChannelTypes()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/thing-types").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::ThingTypes(c).getThingTypes()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/config-descriptions").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::ConfigDescriptions(c).getConfigDescriptions()); }
        catch (const std::exception& e) { return err(e); }
    });

    // ── Persistence ───────────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/persistence").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Persistence(c).getServices()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/persistence/items").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Persistence(c).getItemsFromService()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/persistence/items/<string>").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& item) -> crow::response {
        try { auto c=makeClient(req); auto svc=param(req,"serviceId","");
              return ok(openhab::Persistence(c).getItemPersistenceData(item, svc)); }
        catch (const std::exception& e) { return err(e); }
    });

    // ── Discovery + Inbox ─────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/discovery").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Discovery(c).getDiscoveryBindings()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/discovery/<string>/scan").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& bid) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Discovery(c).startBindingScan(bid)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/inbox").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Inbox(c).getDiscoveredThings()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/inbox/<string>/approve").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& uid) -> crow::response {
        try { auto c=makeClient(req);
              return ok(openhab::Inbox(c).approveDiscoveryResult(uid, bodyStr(req))); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/inbox/<string>/ignore").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& uid) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Inbox(c).ignoreDiscoveryResult(uid)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/inbox/<string>/unignore").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& uid) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Inbox(c).unignoreDiscoveryResult(uid)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/inbox/<string>/delete").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& uid) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Inbox(c).removeDiscoveryResult(uid)); }
        catch (const std::exception& e) { return err(e); }
    });

    // ── Sitemaps ──────────────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/sitemaps").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Sitemaps(c).getSitemaps()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/sitemaps/<string>").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& name) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Sitemaps(c).getSitemap(name)); }
        catch (const std::exception& e) { return err(e); }
    });

    // ── Tags / Templates / ModuleTypes / ProfileTypes ─────────────────────────
    CROW_ROUTE(app, "/api/tags").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Tags(c).getTags()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/tags/<string>").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& id) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Tags(c).getTag(id)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/templates").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Templates(c).getTemplates()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/module-types").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::ModuleTypes(c).getModuleTypes()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/profile-types").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::ProfileTypes(c).getProfileTypes()); }
        catch (const std::exception& e) { return err(e); }
    });

    // ── Transformations ───────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/transformations").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Transformations(c).getTransformations()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/transformations/services").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Transformations(c).getTransformationServices()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/transformations/<string>").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& uid) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Transformations(c).getTransformation(uid)); }
        catch (const std::exception& e) { return err(e); }
    });

    // ── UI / Services / Iconsets ──────────────────────────────────────────────
    CROW_ROUTE(app, "/api/ui/tiles").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::UI(c).getUITiles()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/ui/components/<string>").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& ns) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::UI(c).getUIComponents(ns)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/services").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Services(c).getServices()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/services/<string>").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& id) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Services(c).getService(id)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/services/<string>/config").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& id) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Services(c).getServiceConfig(id)); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/iconsets").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Iconsets(c).getIconsets()); }
        catch (const std::exception& e) { return err(e); }
    });

    // ── Auth ──────────────────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/auth/apitokens").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Auth(c).getAPITokens()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/auth/sessions").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Auth(c).getSessions()); }
        catch (const std::exception& e) { return err(e); }
    });

    // ── Voice ─────────────────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/voice/voices").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Voice(c).getVoices()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/voice/defaultvoice").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Voice(c).getDefaultVoice()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/voice/interpreters").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Voice(c).getInterpreters()); }
        catch (const std::exception& e) { return err(e); }
    });
    CROW_ROUTE(app, "/api/voice/say").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) -> crow::response {
        try { auto c=makeClient(req);
              return ok(openhab::Voice(c).sayText(bodyStr(req), "", "")); }
        catch (const std::exception& e) { return err(e); }
    });

    // ── Actions ───────────────────────────────────────────────────────────────
    CROW_ROUTE(app, "/api/actions/<string>").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req, const std::string& uid) -> crow::response {
        try { auto c=makeClient(req); return ok(openhab::Actions(c).getActions(uid)); }
        catch (const std::exception& e) { return err(e); }
    });

    // ── Port from environment variable (Render.com sets PORT) ─────────────────
    const char* portEnv = std::getenv("PORT");
    uint16_t port = portEnv ? static_cast<uint16_t>(std::stoi(portEnv)) : 8080;

    app.port(port).multithreaded().run();
    return 0;
}