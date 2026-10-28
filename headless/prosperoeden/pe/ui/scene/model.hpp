// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace pe::ui::scene {
using Json = nlohmann::json;
inline void require(bool test, const std::string& message) {
    if (!test) throw std::runtime_error("Scene: " + message);
}
inline bool identifier(const std::string& s) {
    return !s.empty() && s.size() <= 48 && s.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-") == s.npos;
}
inline void fields(const Json& j, std::initializer_list<const char*> allowed) {
    require(j.is_object(), "expected an object");
    for (auto i = j.begin(); i != j.end(); ++i)
        require(std::find(allowed.begin(), allowed.end(), i.key()) != allowed.end(), "unsupported field " + i.key());
}
inline float number(const Json& j, const char* key, float fallback, float low, float high) {
    if (!j.contains(key)) return fallback;
    require(j.at(key).is_number(), std::string("expected number: ") + key);
    const auto n = j.at(key).get<double>();
    require(std::isfinite(n) && n >= low && n <= high, std::string("out of range: ") + key);
    return static_cast<float>(n);
}
inline std::string text(const Json& j, const char* key, const std::string& fallback = "", std::size_t max = 4096) {
    if (!j.contains(key)) return fallback;
    require(j.at(key).is_string(), std::string("expected string: ") + key);
    const auto s = j.at(key).get<std::string>();
    require(s.size() <= max && s.find('\0') == s.npos, std::string("invalid string: ") + key);
    return s;
}
inline bool flag(const Json& j, const char* key, bool fallback = false) {
    if (!j.contains(key)) return fallback;
    require(j.at(key).is_boolean(), std::string("expected boolean: ") + key);
    return j.at(key).get<bool>();
}
inline const std::set<std::string> actions{
    "launcher.picker", "launcher.select", "theme_import.open", "library.refresh", "profile.choose", "profile.add", "profile.rename", "profile.remove",
    "preferences.set", "preferences.adjust", "game_settings.set", "game_settings.adjust", "game.docked", "mods.enabled", "mod.enabled", "cheats.enabled", "cheat.enabled",
    "save.import", "save.export", "game.delete", "launch.request", "launch.cancel", "sync.answer", "sync.cancel"
};
inline void action(const Json& a) {
    fields(a, {"screen", "action", "args", "state"});
    const int count = a.contains("screen") + a.contains("action") + a.contains("state");
    require(count == 1, "one navigation, action or state operation is required");
    if (a.contains("screen")) require(identifier(text(a, "screen")), "invalid screen target");
    if (a.contains("state")) require(identifier(text(a, "state")), "invalid state target");
    if (a.contains("action")) require(actions.contains(text(a, "action")), "unsupported host action");
    if (a.contains("args")) {
        require(a.at("args").is_object() && a.at("args").dump().size() <= 8192, "invalid action arguments");
        require(!a.at("args").contains("confirmed"), "themes cannot confirm host actions");
        require(!a.at("args").contains("$bind"), "action arguments must remain an object");
        auto validate_bindings = [&](auto&& self, const Json& arg, int depth) -> void {
            require(depth <= 8, "argument binding depth exceeded");
            if (arg.is_object()) {
                if (arg.contains("$bind")) {
                    require(arg.size() == 1 && arg.at("$bind").is_string(), "invalid argument binding");
                    const auto pointer = arg.at("$bind").get<std::string>();
                    require((pointer.starts_with("/selected/") || pointer=="/game/handle") && pointer.size() <= 256, "action bindings must identify the selected item");
                    try { (void)Json::json_pointer(pointer); } catch (...) { require(false, "invalid action binding"); }
                } else for (const auto& item : arg) self(self, item, depth + 1);
            } else if (arg.is_array()) for (const auto& item : arg) self(self, item, depth + 1);
        };
        validate_bindings(validate_bindings, a.at("args"), 0);
    }
}
inline void binding(const std::string& b) {
    require(b.size() <= 256 && b.starts_with('/'), "bindings use JSON pointers");
    const auto end = b.find('/', 1);
    const auto root = b.substr(1, end == b.npos ? b.size() - 1 : end - 1);
    static const std::set<std::string> roots{"library", "home", "profiles", "status", "preferences", "item", "selected", "game"};
    require(roots.contains(root), "unsupported binding root");
    try { (void)Json::json_pointer(b); } catch (...) { require(false, "invalid binding pointer"); }
}
struct Node {
    std::string id, type;
    Json properties, activate, links = Json::object();
    std::vector<Node> children;
};
struct Track {
    std::string node, property, trigger, interpolation = "linear";
    float duration{};
    bool loop{};
    std::vector<std::pair<float, float>> keys;
    std::vector<float> tangents;
};
struct Screen {
    std::string id, initial_focus, music;
    std::vector<Node> nodes;
    std::vector<Track> tracks;
    Json input = Json::object();
    float transition = 0.2f;
};
struct Model {
    std::string id, name, entry;
    std::map<std::string, Screen> screens;
    Json cues = Json::object();
};
inline Node node(const Json& j, std::size_t& total, int depth, std::set<std::string>& ids, const Json& images) {
    require(++total <= 512 && depth <= 12, "node/depth budget exceeded");
    fields(j, {"id", "type", "x", "y", "w", "h", "alpha", "radius", "color", "edge", "border", "size", "text", "bind",
        "asset", "uv", "slice", "children", "activate", "nav", "source", "columns", "rows", "cell_w", "cell_h", "gap",
        "empty", "count", "speed", "state", "visible", "focusable", "align", "thickness", "x2", "y2", "scale", "labels"});
    Node n; n.id = text(j, "id"); n.type = text(j, "type"); n.properties = j;
    static const std::set<std::string> types{"group", "mask", "rect", "circle", "ring", "line", "triangle", "image", "text", "nineslice", "focus", "list", "grid", "particles", "cover"};
    require(identifier(n.id) && ids.insert(n.id).second, "invalid or duplicate node ID");
    require(types.contains(n.type), "unsupported node type (no shaders or alpha masks)");
    number(j, "x", 0, -8192, 8192); number(j, "y", 0, -8192, 8192);
    number(j, "w", 0, 0, 8192); number(j, "h", 0, 0, 8192);
    number(j, "alpha", 1, 0, 1); number(j, "radius", 0, 0, 2048);
    number(j, "size", 24, 8, 160); number(j, "border", 2, 0, 32);
    number(j, "scale", 1, 0.25f, 4); number(j, "thickness", 2, 0.25f, 32);
    number(j, "x2", 0, -8192, 8192); number(j, "y2", 0, -8192, 8192);
    number(j, "speed", 12, 0, 100); number(j, "gap", 16, 0, 256);
    number(j, "cell_w", 250, 32, 1920); number(j, "cell_h", 300, 32, 1080);
    for (auto key : {"rows", "columns", "count"}) if (j.contains(key))
        require(j.at(key).is_number_integer() && j.at(key).get<int>() >= 1 && j.at(key).get<int>() <= 64, "invalid count");
    require(j.value("rows", 2) * (n.type == "list" ? 1 : j.value("columns", 4)) <= 64, "visible collection budget exceeded");
    for (auto key : {"color", "edge"}) if (j.contains(key)) {
        require(j.at(key).is_number_integer() && j.at(key).get<long long>() >= 0 && j.at(key).get<long long>() <= 0xffffff, "invalid color");
    }
    for (auto key : {"text", "empty", "state"}) if (j.contains(key)) text(j, key);
    if(j.contains("labels")) {
        const auto& labels=j.at("labels");require(labels.is_object() && labels.size()<=32,"invalid value labels");
        for(auto it=labels.begin();it!=labels.end();++it) {require(it.key().size()<=32,"invalid value label key");text(labels,it.key().c_str(),"",160);}
    }
    for (auto key : {"visible", "focusable"}) flag(j, key, true);
    const auto align = text(j, "align", "left");
    require(align == "left" || align == "right" || align == "center", "invalid text alignment");
    for (auto key : {"bind", "source"}) if (j.contains(key)) binding(text(j, key));
    if (n.type == "list" || n.type == "grid") require(j.contains("source"), "collection needs a data source");
    if (n.type == "image" || n.type == "nineslice") require(images.contains(text(j, "asset")), "unknown image");
    if (j.contains("uv")) {
        const auto& uv = j.at("uv"); require(uv.is_array() && uv.size() == 4, "invalid UV");
        for (const auto& v : uv) require(v.is_number() && v.get<double>() >= 0 && v.get<double>() <= 1, "invalid UV component");
        require(uv[0].get<double>() + uv[2].get<double>() <= 1.00001 && uv[1].get<double>() + uv[3].get<double>() <= 1.00001, "UV outside image");
    }
    if (n.type == "nineslice") {
        const auto& s = j.at("slice"); require(s.is_array() && s.size() == 4, "slice needs left/top/right/bottom fractions");
        for (const auto& v : s) require(v.is_number() && v.get<double>() >= 0 && v.get<double>() < 0.5, "invalid slice");
    }
    if (j.contains("activate")) { action(j.at("activate")); n.activate = j.at("activate"); }
    if (j.contains("nav")) {
        fields(j.at("nav"), {"up", "down", "left", "right"}); n.links = j.at("nav");
        for (const auto& target : n.links) require(target.is_string() && identifier(target.get<std::string>()), "invalid focus target");
    }
    if (j.contains("children")) {
        require(j.at("children").is_array(), "children must be an array");
        require(n.type == "group" || n.type == "mask" || n.type == "list" || n.type == "grid" || n.type == "focus", "leaf nodes cannot have children");
        for (const auto& child : j.at("children")) n.children.push_back(node(child, total, depth + 1, ids, images));
    }
    return n;
}
inline Track track(const Json& j, const std::set<std::string>& ids) {
    fields(j, {"node", "property", "trigger", "duration", "loop", "keys", "interpolation"});
    Track t; t.node = text(j, "node"); t.property = text(j, "property"); t.trigger = text(j, "trigger", "enter");
    require(ids.contains(t.node), "animation targets missing node");
    require(t.property == "x" || t.property == "y" || t.property == "alpha" || t.property == "scale", "unsupported animation property");
    require(t.trigger == "enter" || t.trigger == "focus" || (t.trigger.starts_with("state:") && identifier(t.trigger.substr(6))), "invalid animation trigger");
    t.duration = number(j, "duration", 0.2f, 0.001f, 60); t.loop = flag(j, "loop");
    t.interpolation=text(j,"interpolation","linear");
    require(t.interpolation=="linear"||t.interpolation=="step"||t.interpolation=="hermite","unsupported keyframe interpolation");
    const auto& keys = j.at("keys"); require(keys.is_array() && keys.size() >= 2 && keys.size() <= 64, "invalid keyframes");
    float previous = -1;
    for (const auto& k : keys) {
        fields(k, {"at", "value", "tangent"}); const float at = number(k, "at", 0, 0, 1);
        require(at > previous && k.contains("value"), "keyframes must be ordered"); previous = at;
        const float low = t.property == "alpha" ? 0 : t.property == "scale" ? 0.25f : -8192;
        const float high = t.property == "alpha" ? 1 : t.property == "scale" ? 4 : 8192;
        t.keys.emplace_back(at, number(k, "value", 0, low, high));
        t.tangents.push_back(number(k,"tangent",0,-65536,65536));
    }
    require(t.keys.front().first == 0 && t.keys.back().first == 1, "keyframes must include endpoints");
    return t;
}
inline Model parse(const Json& j, const Json& screen_files, const Json& animation_files = Json::object()) {
    fields(j, {"schema", "id", "name", "entry", "screens", "animations", "images", "font", "audio", "cues"});
    require(j.at("schema") == 1, "unsupported scene contract");
    Model m; m.id = text(j, "id"); m.name = text(j, "name", m.id, 160); m.entry = text(j, "entry");
    require(identifier(m.id) && identifier(m.entry), "invalid theme ID/entry");
    const auto images = j.value("images", Json::object());
    require(images.is_object() && images.size() <= 32, "invalid image table");
    const auto& screens = j.at("screens"); require(screens.is_object() && !screens.empty() && screens.size() <= 16, "invalid screen table");
    std::size_t total = 0, tracks = 0;
    for (auto it = screens.begin(); it != screens.end(); ++it) {
        require(identifier(it.key()), "invalid screen ID");
        const auto& s = screen_files.at(it.key());
        fields(s, {"nodes", "focus", "input", "music", "transition", "animations"});
        Screen out; out.id = it.key(); out.initial_focus = text(s, "focus"); out.music = text(s, "music");
        out.transition = number(s, "transition", 0.2f, 0, 2);
        const auto audio = j.value("audio", Json::object());
        require(out.music.empty() || audio.contains(out.music), "unknown music resource");
        require(s.at("nodes").is_array() && !s.at("nodes").empty(), "screen needs nodes");
        std::set<std::string> ids;
        for (const auto& n : s.at("nodes")) out.nodes.push_back(node(n, total, 0, ids, images));
        std::set<std::string> focus_ids;
        auto focus_targets = [&](auto&& self, const Node& n) -> void {
            const bool collection = n.type == "list" || n.type == "grid";
            if (n.properties.value("focusable", n.type == "focus" || collection || !n.activate.is_null())) focus_ids.insert(n.id);
            if (!collection) for (const auto& child : n.children) self(self, child);
        };
        for (const auto& n : out.nodes) focus_targets(focus_targets, n);
        require(out.initial_focus.empty() || focus_ids.contains(out.initial_focus), "missing/nonfocusable initial focus");
        auto verify = [&](auto&& self, const Node& n) -> void {
            for (const auto& target : n.links) require(focus_ids.contains(target.get<std::string>()), "missing/nonfocusable focus neighbor");
            if (n.activate.contains("screen")) require(screens.contains(n.activate.at("screen").get<std::string>()), "missing navigation screen");
            for (const auto& child : n.children) self(self, child);
        };
        for (const auto& n : out.nodes) verify(verify, n);
        auto cost = [&](auto&& self, const Node& n, bool repeated) -> std::size_t {
            const bool collection = n.type == "list" || n.type == "grid";
            require(!collection || !repeated, "nested data collections are unsupported");
            std::size_t children = 0;
            for (const auto& child : n.children) children += self(self, child, repeated || collection);
            if (collection) {
                const auto columns = n.type == "list" ? 1 : n.properties.value("columns", 4);
                const auto count = columns * n.properties.value("rows", 2);
                return 7 + count * (children + 3);
            }
            if (n.type == "nineslice") return 13;
            if (n.type == "particles") return 4 + n.properties.value("count", 24);
            return 7 + children;
        };
        std::size_t commands = 2;
        for (const auto& n : out.nodes) commands += cost(cost, n, false);
        require(commands <= 8192, "expanded scene exceeds frame command budget");
        out.input = s.value("input", Json::object());
        fields(out.input, {"select", "back", "secondary", "settings", "options", "l1", "r1"});
        for (const auto& a : out.input) { action(a); if (a.contains("screen")) require(screens.contains(a.at("screen").get<std::string>()), "missing input screen"); }
        if (s.contains("animations")) {
            require(s.at("animations").is_array(), "invalid animation references");
            for (const auto& ref : s.at("animations")) {
                require(ref.is_string() && animation_files.contains(ref.get<std::string>()), "unknown animation file");
                const auto& list = animation_files.at(ref.get<std::string>());
                require(list.is_array(), "animation file must be an array");
                for (const auto& t : list) { require(++tracks <= 128, "animation budget exceeded"); out.tracks.push_back(track(t, ids)); }
            }
        }
        m.screens.emplace(out.id, std::move(out));
    }
    require(m.screens.contains(m.entry), "entry screen missing");
    m.cues = j.value("cues", Json::object()); fields(m.cues, {"focus", "select", "back", "error"});
    for (const auto& cue : m.cues) require(cue.is_string() && j.value("audio", Json::object()).contains(cue.get<std::string>()), "unknown sound cue");
    return m;
}
} // namespace pe::ui::scene
