// Copyright 2016 Cheng Zhao. All rights reserved.
// Use of this source code is governed by the license that can be found in the
// LICENSE file.

#include "nativeui/button.h"

#include <windowsx.h>

#include "base/strings/utf_string_conversions.h"
#include "nativeui/container.h"
#include "nativeui/gfx/geometry/insets.h"
#include "nativeui/gfx/geometry/size_conversions.h"
#include "nativeui/gfx/win/text_win.h"
#include "nativeui/state.h"
#include "nativeui/win/base_view.h"
#include "nativeui/win/util/native_theme.h"
#include "nativeui/win/window_win.h"

namespace nu {

namespace {

const int kButtonPadding = 3;
const int kCheckBoxPadding = 1;

class ButtonView : public BaseView {
 public:
  ButtonView(Button::Type type, Button* delegate)
      : BaseView(type == Button::Normal ? ControlType::Button
                    : (type == Button::CheckBox ? ControlType::CheckBox
                                                : ControlType::Radio)),
        theme_(State::current()->GetNativeTheme()),
        color_(GetThemeColor(ThemeColor::Text)),
        delegate_(delegate) {
    if (type == Button::CheckBox)
      box_size_ = theme_->GetThemePartSize(NativeTheme::CheckBox, state());
    else if (type == Button::Radio)
      box_size_ = theme_->GetThemePartSize(NativeTheme::Radio, state());
  }

  void SetTitle(const base::string16& title) {
    title_ = title;
    text_size_ = ToCeiledSize(MeasureText(this, font_, title_));
  }

  base::string16 GetTitle() const {
    return title_;
  }

  Size GetPreferredSize() const {
    int padding = delegate_->DIPToPixel(
        type() == ControlType::Button ? kButtonPadding : kCheckBoxPadding);
    Size preferred_size(text_size_);
    preferred_size.Enlarge(box_size_.width() + padding * 2, padding * 2);
    return preferred_size;
  }

  void OnClick() {
    if (type() == ControlType::CheckBox)
      SetChecked(!IsChecked());
    else if (type() == ControlType::Radio && !IsChecked())
      SetChecked(true);
    delegate_->on_click.Emit();
  }

  void SetChecked(bool checked) {
    if (IsChecked() == checked)
      return;

    params_.checked = checked;
    Invalidate();

    // And flip all other radio buttons' state.
    if (checked && type() == ControlType::Radio && delegate_->parent() &&
        delegate_->parent()->view()->type() == ControlType::Container) {
      auto* container = static_cast<Container*>(delegate_->parent());
      for (int i = 0; i < container->child_count(); ++i) {
        View* child = container->child_at(i);
        if (child != delegate_ && child->view()->type() == ControlType::Radio)
          static_cast<Button*>(child)->SetChecked(false);
      }
    }
  }

  bool IsChecked() const {
    return params_.checked;
  }

  void OnMouseEnter() override {
    is_hovering_ = true;
    if (!is_capturing_) {
      set_state(ControlState::Hovered);
      Invalidate();
    }
  }

  void OnMouseLeave() override {
    is_hovering_ = false;
    if (!is_capturing_) {
      set_state(ControlState::Normal);
      Invalidate();
    }
  }

  bool OnMouseClick(UINT message, UINT flags, const Point& point) override {
    auto* toplevel_window = static_cast<TopLevelWindow*>(window());
    if (message == WM_LBUTTONDOWN) {
      is_capturing_ = true;
      toplevel_window->SetCapture(this);
      set_state(ControlState::Pressed);
      Invalidate();
    } else if (message == WM_LBUTTONUP) {
      if (state() == ControlState::Pressed)
        OnClick();
      set_state(ControlState::Hovered);
      Invalidate();
    }

    // Clicking a button moves the focus to it.
    toplevel_window->focus_manager()->TakeFocus(delegate_);
    return true;
  }

  void OnCaptureLost() override {
    is_capturing_ = false;
    set_state(is_hovering_ ? ControlState::Hovered : ControlState::Normal);
    Invalidate();
  }

  bool CanHaveFocus() const override {
    return true;
  }

  void Draw(PainterWin* painter, const Rect& dirty) override {
    HDC dc = painter->GetHDC();

    Size size = size_allocation().size();
    Size preferred_size = preferred_size();

    // Draw the button background
    if (type() == ControlType::Button)
      theme_->PaintPushButton(dc, state(), Rect(size) + painter->origin(),
                              params_);

    // Checkbox and radio are left aligned.
    Point origin;
    if (type() == ControlType::Button) {
      origin.Offset((size.width() - preferred_size.width()) / 2,
                    (size.height() - preferred_size.height()) / 2);
    }

    // Draw the box.
    Point box_origin = origin + painter->origin();
    box_origin.Offset(0, (preferred_size.height() - box_size_.height()) / 2);
    if (type() == ControlType::CheckBox)
      theme_->PaintCheckBox(dc, state(), Rect(box_origin, box_size_), params_);
    else if (type() == ControlType::Radio)
      theme_->PaintRadio(dc, state(), Rect(box_origin, box_size_), params_);

    // Draw focused ring.
    if (IsFocused()) {
      RECT rect;
      if (type() == ControlType::Button) {
        Rect bounds = Rect(size) + painter->origin();
        bounds.Inset(Insets(delegate_->DIPToPixel(1)));
        rect = bounds.ToRECT();
      } else {
        int padding = delegate_->DIPToPixel(kCheckBoxPadding);
        box_origin.Offset(box_size_.width() + padding, padding);
        Size ring_size = preferred_size;
        ring_size.Enlarge(-box_size_.width() - padding * 2, -padding * 2);
        rect = Rect(box_origin, ring_size).ToRECT();
      }
      DrawFocusRect(dc, &rect);
    }

    painter->ReleaseHDC(dc);

    // The text.
    int padding = delegate_->DIPToPixel(
        type() == ControlType::Button ? kButtonPadding : kCheckBoxPadding);
    Rect text_bounds(origin, preferred_size);
    text_bounds.Inset(box_size_.width() + padding, padding, padding, padding);
    painter->DrawString(title_, font_, color_, text_bounds);
  }

  NativeTheme::ButtonExtraParams* params() { return &params_; }
  Font font() const { return font_; }

 private:
  NativeTheme* theme_;
  NativeTheme::ButtonExtraParams params_ = {0};

  // The size of box for radio and checkbox.
  Size box_size_;

  // The size of text.
  Size text_size_;

  // Default text color and font.
  Color color_;
  Font font_;

  // Whether the button is capturing the mouse.
  bool is_capturing_ = false;

  // Whether the mouse is hovering the button.
  bool is_hovering_ = false;

  base::string16 title_;
  Button* delegate_;
};

}  // namespace

Button::Button(const std::string& title, Type type) {
  TakeOverView(new ButtonView(type, this));
  SetTitle(title);
}

Button::~Button() {
}

void Button::SetTitle(const std::string& title) {
  auto* button = static_cast<ButtonView*>(view());
  base::string16 wtitle = base::UTF8ToUTF16(title);
  button->SetTitle(wtitle);

  SetPixelPreferredSize(button->GetPreferredSize());
  button->Invalidate();
}

std::string Button::GetTitle() const {
  return base::UTF16ToUTF8(static_cast<ButtonView*>(view())->GetTitle());
}

void Button::SetChecked(bool checked) {
  static_cast<ButtonView*>(view())->SetChecked(checked);
}

bool Button::IsChecked() const {
  return static_cast<ButtonView*>(view())->IsChecked();
}

}  // namespace nu
