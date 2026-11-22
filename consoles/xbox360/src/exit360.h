// Where Menu > Exit takes the Xbox 360: the console's default dashboard, or
// a .xex of the player's choosing (Aurora, FreeStyle, XeXMenu ... on a USB
// stick or the hard drive), set in tetrispack.ini next to tetrispack.xex:
//
//   [xbox360]
//   exit = dashboard                     ; or a .xex:
//   exit = Usb0:\Aurora\default.xex
//   exit_name = Aurora                   ; the menu's "Exit to ..." (optional)
//
// Portable C++03: the parsing and the paths are tested on the PC
// (tests/test_exit360.cpp); platform_xsdk.cpp does the launching.
#pragma once

#include <string>

namespace x360 {

struct ExitTarget {
    std::string path;   // the kernel's path to the .xex ("\Device\Mass0\..."); empty: the dashboard
    std::string name;   // for the menu: "Aurora"; empty: "Dashboard"
    std::string error;  // why the setting was not used (logged)
};

// The [xbox360] section of tetrispack.ini (other sections, comments after
// ';' or '#', and unknown keys are skipped). `image` is the running .xex's
// own kernel path, for paths under game:.
ExitTarget parseExitConfig(const std::string& ini, const std::string& image);

// A path as the player writes it into the kernel's:
//   Usb0: Usb1: Usb2:  ->  \Device\Mass0 ... \Device\Mass2  (Usb: is Usb0:)
//   Hdd: Hdd1:          ->  \Device\Harddisk0\Partition1
//   Flash:              ->  \Device\Flash
//   game:               ->  the folder tetrispack.xex is in
//   \Device\...         ->  as it is
// '/' is taken as '\'. Empty if the drive isn't one of these.
std::string kernelPath(const std::string& path, const std::string& image);

}  // namespace x360
