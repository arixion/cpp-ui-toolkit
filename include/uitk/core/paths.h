// Portable file-system helpers. uitk strings are UTF-8 everywhere; std::filesystem::path uses
// the native encoding (UTF-16 on Windows, where path::string() goes through the ANSI code page
// and can throw or garble non-ASCII names). Convert with these instead of path(std::string) /
// path::string().
#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace uitk::paths {

namespace stdfs = std::filesystem;

std::string to_utf8(const stdfs::path& p);          // display / UTF-8 APIs
stdfs::path from_utf8(std::string_view utf8);        // user-typed or UTF-8 config text

stdfs::path home_directory();                        // $HOME, %USERPROFILE%
// Top-level locations to offer in "Computer": "/" on Linux, every mounted drive on Windows.
std::vector<stdfs::path> roots();
// Hidden by convention: dotfiles everywhere, plus the hidden attribute on Windows.
bool is_hidden(const stdfs::path& p);

// "~" and "~/..." (also "~\..." on Windows) expand to the home directory.
stdfs::path expand_user(std::string_view utf8);

} // namespace uitk::paths
