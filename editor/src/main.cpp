// Toad editor entry point (skeleton: an empty uitk window).
#include <cstdio>
#include <string>

#include "toad/engine.h"
#include "uitk/core/application.h"
#include "uitk/widgets/application_window.h"
#include "uitk/widgets/label.h"

using namespace uitk;

int main() {
    Application app;
    WindowDesc desc;
    desc.title = "Toad Editor";
    desc.app_id = "toad-editor";
    desc.size = {1280, 800};
    ApplicationWindow win(app, desc);
    win.set_theme(Theme::vscode_dark(), false);
    win.content().add<Label>(std::string("Toad engine ") + toad::version(), TextRole::Dim)
        .set_align(Align::Center, Align::Center);
    fprintf(stderr, "toad_editor: platform=%s renderer=%s\n", win.window().platform_name(),
            win.renderer().description().c_str());
    return app.run();
}
