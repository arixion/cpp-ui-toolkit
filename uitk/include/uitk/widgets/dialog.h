// Dialog: a modal, window-centred card with a title, a body and a row of response buttons.
//
//   auto& d = Dialog::show<Dialog>(host, "Rename");
//   d.body().add<TextField>("old name");
//   d.add_button("Cancel", Dialog::Cancel);
//   d.add_button("Rename", Dialog::Ok, ButtonStyle::Primary);
//   d.set_default_response(Dialog::Ok);           // Enter
//   d.responded.connect([](int r) { ... });       // after the dialog closed
//
//   Dialog::message(host, MessageKind::Warning, "Delete 3 files?", "This cannot be undone.",
//                   {{"Cancel", Dialog::Cancel}, {"Delete", Dialog::Ok, ButtonStyle::Destructive}},
//                   [](int r) { ... });
//
// Escape, the close button and Cancel-like buttons respond with the cancel response. Focus is
// trapped inside while it is open (the host's modal popup layer).
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "uitk/core/signal.h"
#include "uitk/widgets/button.h"
#include "uitk/widgets/frame.h"
#include "uitk/widgets/label.h"
#include "uitk/widgets/widget_host.h"

namespace uitk {

enum class MessageKind { None, Info, Warning, Error, Question };

struct DialogButton {
    std::string label;
    int response;
    ButtonStyle style = ButtonStyle::Secondary;
};

class Dialog : public Frame {
public:
    // Standard responses (any int works; these are conventions).
    static constexpr int Cancel = -1, None = 0, Ok = 1, Yes = 2, No = 3, Apply = 4;

    explicit Dialog(std::string title = {});

    Box& body() { return *body_; }
    Button& add_button(std::string label, int response, ButtonStyle style = ButtonStyle::Secondary);
    Button* button_for(int response) const;
    void set_default_response(int r) { default_response_ = r; }   // Enter (when not used by the focus)
    void set_cancel_response(int r) { cancel_response_ = r; }     // Escape / close button
    void set_title(std::string t);
    // Fixed size (e.g. 900x600 for a file dialog); default: natural size, 360..560 px wide.
    void set_preferred_size(Size s) { preferred_ = s; invalidate_layout(); }
    // Hairlines between the header, body and buttons (for dialogs whose body is a full panel).
    void set_section_separators(bool on);

    // Closes the dialog and emits `responded` (after it closed, so handlers may open another).
    void respond(int response);
    int response() const { return response_; }
    Signal<int> responded;

    // Shows a dialog of type D (constructed from args) modally in the host's window.
    template <class D = Dialog, class... Args>
    static D& show(WidgetHost& host, Args&&... args) {
        auto d = std::make_unique<D>(std::forward<Args>(args)...);
        D& ref = *d;
        present(host, std::move(d));
        return ref;
    }
    // Message box: icon + heading + text + buttons.
    static Dialog& message(WidgetHost& host, MessageKind kind, std::string heading, std::string text,
                           std::vector<DialogButton> buttons = {{"OK", Ok, ButtonStyle::Primary}},
                           std::function<void(int)> on_response = {});

    Size measure(float available_width, const Theme& theme) override;
    bool on_key(const KeyEvent& e) override;

protected:
    static void present(WidgetHost& host, std::unique_ptr<Dialog> dialog);

private:
    Box* header_;
    Label* title_;
    Box* body_;
    Box* buttons_;
    Separator *sep_top_, *sep_bottom_;
    std::vector<std::pair<int, Button*>> response_buttons_;
    int default_response_ = None, cancel_response_ = Cancel, response_ = None;
    bool responded_ = false;
    Size preferred_;
};

} // namespace uitk
