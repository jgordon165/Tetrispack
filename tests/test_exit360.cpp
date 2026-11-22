// Menu > Exit's target on the Xbox 360 (consoles/xbox360/src/exit360.cpp):
// tetrispack.ini's [xbox360] section and the kernel paths it becomes.
#include "exit360.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

const std::string kImage = "\\Device\\Mass0\\Games\\tetrispack\\tetrispack.xex";

}  // namespace

int main() {
    using x360::ExitTarget;
    using x360::kernelPath;
    using x360::parseExitConfig;

    // The drives as players write them.
    check(kernelPath("Usb0:\\Aurora\\default.xex", kImage) == "\\Device\\Mass0\\Aurora\\default.xex", "Usb0:");
    check(kernelPath("usb:/Aurora/default.xex", kImage) == "\\Device\\Mass0\\Aurora\\default.xex", "usb: and slashes");
    check(kernelPath("Usb1:\\FSD\\default.xex", kImage) == "\\Device\\Mass1\\FSD\\default.xex", "Usb1:");
    check(kernelPath("Hdd:\\Aurora\\default.xex", kImage) == "\\Device\\Harddisk0\\Partition1\\Aurora\\default.xex",
          "Hdd:");
    check(kernelPath("HDD1:\\Apps\\XeXMenu\\default.xex", kImage) ==
              "\\Device\\Harddisk0\\Partition1\\Apps\\XeXMenu\\default.xex", "Hdd1:, any case");
    check(kernelPath("game:\\..\\Aurora\\default.xex", kImage) ==
              "\\Device\\Mass0\\Games\\tetrispack\\..\\Aurora\\default.xex", "game: is tetrispack.xex's folder");
    check(kernelPath("\\Device\\Mass2\\Dash\\default.xex", kImage) == "\\Device\\Mass2\\Dash\\default.xex",
          "\\Device\\ paths as they are");
    check(kernelPath("C:\\default.xex", kImage).empty(), "unknown drives refused");
    check(kernelPath("default.xex", kImage).empty(), "no drive refused");

    // The settings.
    ExitTarget t = parseExitConfig("", kImage);
    check(t.path.empty() && t.name.empty() && t.error.empty(), "no file: the dashboard");

    t = parseExitConfig("[xbox360]\r\nexit = dashboard ; the default\r\n", kImage);
    check(t.path.empty() && t.error.empty(), "exit = dashboard");

    t = parseExitConfig("; Tetrispack\n[other]\nexit = Usb0:\\Nope\\default.xex\n\n"
                        "[XBOX360]\n  Exit=Usb0:\\Aurora\\default.xex   # my dash\n", kImage);
    check(t.path == "\\Device\\Mass0\\Aurora\\default.xex", "exit = a .xex, only from [xbox360]");
    check(t.name == "Aurora", "named after its folder");

    t = parseExitConfig("[xbox360]\nexit = Hdd:\\Apps\\FSD3\\default.xex\nexit_name = FreeStyle Dash\n", kImage);
    check(t.name == "FreeStyle Dash", "exit_name names it");

    t = parseExitConfig("[xbox360]\nexit = Usb0:\\default.xex\n", kImage);
    check(t.path == "\\Device\\Mass0\\default.xex" && t.name.empty(), "a .xex at a drive's root: no folder name");

    t = parseExitConfig("[xbox360]\nexit = X:\\Aurora\\default.xex\n", kImage);
    check(t.path.empty() && !t.error.empty(), "an unknown drive: the dashboard, and why");
    t = parseExitConfig("[xbox360]\nexit = Usb0:\\Aurora\\\n", kImage);
    check(t.path.empty() && !t.error.empty(), "not a .xex: the dashboard, and why");

    if (failures) return EXIT_FAILURE;
    std::printf("exit360: all checks passed\n");
    return EXIT_SUCCESS;
}
