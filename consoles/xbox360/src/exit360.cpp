#include "exit360.h"

#include <cctype>

namespace x360 {

namespace {

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

std::string lower(std::string s) {
    for (size_t i = 0; i < s.size(); ++i) s[i] = char(std::tolower((unsigned char)s[i]));
    return s;
}

bool endsWith(const std::string& s, const std::string& tail) {
    return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
}

// The folder of a .xex's path, without its last separator.
std::string folderOf(const std::string& path) {
    const size_t slash = path.find_last_of('\\');
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

}  // namespace

std::string kernelPath(const std::string& written, const std::string& image) {
    std::string p = trim(written);
    for (size_t i = 0; i < p.size(); ++i)
        if (p[i] == '/') p[i] = '\\';
    if (lower(p.substr(0, 8)) == "\\device\\") return p;
    const size_t colon = p.find(':');
    if (colon == std::string::npos) return std::string();
    const std::string drive = lower(p.substr(0, colon));
    std::string rest = p.substr(colon + 1);
    while (!rest.empty() && rest[0] == '\\') rest.erase(0, 1);
    std::string base;
    if (drive == "usb" || drive == "usb0") base = "\\Device\\Mass0";
    else if (drive == "usb1") base = "\\Device\\Mass1";
    else if (drive == "usb2") base = "\\Device\\Mass2";
    else if (drive == "hdd" || drive == "hdd1") base = "\\Device\\Harddisk0\\Partition1";
    else if (drive == "flash") base = "\\Device\\Flash";
    else if (drive == "game") base = folderOf(image);
    if (base.empty()) return std::string();
    return rest.empty() ? base : base + "\\" + rest;
}

ExitTarget parseExitConfig(const std::string& ini, const std::string& image) {
    ExitTarget t;
    std::string section, exit, name;
    size_t pos = 0;
    while (pos <= ini.size()) {
        size_t end = ini.find('\n', pos);
        if (end == std::string::npos) end = ini.size();
        std::string line = ini.substr(pos, end - pos);
        pos = end + 1;
        const size_t comment = line.find_first_of(";#");
        if (comment != std::string::npos) line.erase(comment);
        line = trim(line);
        if (line.empty()) continue;
        if (line[0] == '[') {
            const size_t close = line.find(']');
            section = lower(trim(line.substr(1, close == std::string::npos ? std::string::npos : close - 1)));
            continue;
        }
        if (section != "xbox360") continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = lower(trim(line.substr(0, eq)));
        const std::string value = trim(line.substr(eq + 1));
        if (key == "exit") exit = value;
        else if (key == "exit_name") name = value;
    }

    const std::string l = lower(exit);
    if (l.empty() || l == "dashboard" || l == "default") {
        t.name = name;
        return t;
    }
    t.path = kernelPath(exit, image);
    if (t.path.empty() || !endsWith(lower(t.path), ".xex")) {
        // Not used: back to the dashboard, and say why.
        ExitTarget none;
        none.error = t.path.empty()
            ? "exit = " + exit + ": not a drive this knows (Usb0:, Usb1:, Usb2:, Hdd:, Flash:, game:, \\Device\\)"
            : "exit = " + exit + ": not a .xex";
        return none;
    }
    if (name.empty()) {
        // the folder the .xex is in: Usb0:\Aurora\default.xex is "Aurora"
        const std::string folder = folderOf(t.path);
        const size_t slash = folder.find_last_of('\\');
        name = slash == std::string::npos ? folder : folder.substr(slash + 1);
        if (lower(name).compare(0, 4, "mass") == 0 || lower(name).compare(0, 9, "partition") == 0) name.clear();
    }
    t.name = name;
    return t;
}

}  // namespace x360
