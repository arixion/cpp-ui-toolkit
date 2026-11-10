// Named icons from the bundled Adwaita symbolic set.
//
//   Button b(icons::symbolic("document-save"));
//
// Icons are looked up by name across the theme's category folders (actions, places, status...)
// and cached. Symbolic icons are single-colour: widgets tint them with the theme colour.
#pragma once
#include <filesystem>
#include <string>
#include <string_view>

#include "uitk/render/canvas.h"

namespace uitk::icons {

// Root of the symbolic icon set (contains actions/, places/, status/, ...). Defaults to the
// directory the toolkit was built with; override for installed applications.
void set_search_path(std::string directory);
const std::string& search_path();

// "document-save" -> <path>/actions/document-save-symbolic.svg. Invalid Image if not found.
const Image& symbolic(std::string_view name);

// Type icon for a file (by extension) or a folder: images, audio, video, archives, documents,
// spreadsheets, presentations, fonts, executables, text.
const Image& for_file(const std::filesystem::path& path, bool is_directory);

} // namespace uitk::icons
