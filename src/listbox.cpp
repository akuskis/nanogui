#include <nanogui/listbox.h>
#include <nanogui/vscrollpanel.h>
#include <nanogui/theme.h>
#include <nanogui/button.h>
#include <nanogui/layout.h>
#include <nanovg.h>

NAMESPACE_BEGIN(nanogui)

class ListboxItem : public Button
{
public:
  ListboxItem(Widget* parent, const std::string& str, bool inlist = true)
    : Button(parent, str) {}

  void performLayout(NVGcontext *ctx) {
    Button::performLayout(ctx);
    setWidth(parent()->width());
  }

  void draw(NVGcontext *ctx) override
  {
    if (mMouseFocus && mEnabled) {
      NVGcolor gradTop = mTheme->mButtonGradientTopFocused;
      NVGcolor gradBot = mTheme->mButtonGradientBotFocused;

      nvgBeginPath(ctx);

      nvgRoundedRect(ctx, mPos.x() + 1, mPos.y() + 1.0f, mSize.x() - 2,
        mSize.y() - 2, mTheme->mButtonCornerRadius - 1);

      if (mBackgroundColor.w() != 0) {
        nvgFillColor(ctx, Color(mBackgroundColor.head<3>(), 1.f));
        nvgFill(ctx);
        if (mPushed) {
          gradTop.a = gradBot.a = 0.8f;
        }
        else {
          double v = 1 - mBackgroundColor.w();
          gradTop.a = gradBot.a = mEnabled ? v : v * .5f + .5f;
        }
      }

      NVGpaint bg = nvgLinearGradient(ctx, mPos.x(), mPos.y(), mPos.x(),
        mPos.y() + mSize.y(), gradTop, gradBot);

      nvgFillPaint(ctx, bg);
      nvgFill(ctx);
    }

    NVGcolor textColor = mTextColor.w() == 0 ? mTheme->mTextColor : mTextColor;
    if (mPushed)
    {
      Vector2f center = mPos.cast<float>() + mSize.cast<float>() * 0.5f;
      nvgBeginPath(ctx);
      nvgCircle(ctx, width() * 0.05f, center.y(), 2);
      nvgFillColor(ctx, textColor);
      nvgFill(ctx);
    }

    int fontSize = mFontSize == -1 ? mTheme->mButtonFontSize : mFontSize;
    nvgFontSize(ctx, fontSize);
    nvgFontFace(ctx, "sans-bold");
    float tw = nvgTextBounds(ctx, 0, 0, mCaption.c_str(), nullptr, nullptr);

    Vector2f center = mPos.cast<float>() + mSize.cast<float>() * 0.5f;
    Vector2f textPos(center.x() - tw * 0.5f, center.y() - 1);
    if (!mEnabled)
      textColor = mTheme->mDisabledTextColor;

    if (mIcon) {
      auto icon = utf8(mIcon);

      float iw, ih = fontSize;
      if (nvgIsFontIcon(mIcon)) {
        ih *= icon_scale();
        nvgFontSize(ctx, ih);
        nvgFontFace(ctx, "icons");
        iw = nvgTextBounds(ctx, 0, 0, icon.data(), nullptr, nullptr);
      }
      else {
        int w, h;
        ih *= 0.9f;
        nvgImageSize(ctx, mIcon, &w, &h);
        iw = w * ih / h;
      }
      if (mCaption != "")
        iw += mSize.y() * 0.15f;
      nvgFillColor(ctx, textColor);
      nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
      Vector2f iconPos = center;
      iconPos.y() -= 1;

      if (mIconPosition == IconPosition::LeftCentered) {
        iconPos.x() -= (tw + iw) * 0.5f;
        textPos.x() += iw * 0.5f;
      }
      else if (mIconPosition == IconPosition::RightCentered) {
        textPos.x() -= iw * 0.5f;
        iconPos.x() += tw * 0.5f;
      }
      else if (mIconPosition == IconPosition::Left) {
        iconPos.x() = mPos.x() + 8;
      }
      else if (mIconPosition == IconPosition::Right) {
        iconPos.x() = mPos.x() + mSize.x() - iw - 8;
      }

      if (nvgIsFontIcon(mIcon)) {
        nvgText(ctx, iconPos.x(), iconPos.y() + 1, icon.data(), nullptr);
      }
      else {
        NVGpaint imgPaint = nvgImagePattern(ctx,
          iconPos.x(), iconPos.y() - ih / 2, iw, ih, 0, mIcon, mEnabled ? 0.5f : 0.25f);

        nvgFillPaint(ctx, imgPaint);
        nvgFill(ctx);
      }
    }

    nvgFontSize(ctx, fontSize);
    nvgFontFace(ctx, "sans-bold");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, mTheme->mTextColorShadow);
    nvgText(ctx, textPos.x(), textPos.y(), mCaption.c_str(), nullptr);
    nvgFillColor(ctx, textColor);
    nvgText(ctx, textPos.x(), textPos.y() + 1, mCaption.c_str(), nullptr);

    //Widget::draw(ctx);
  }
};

class WidgetsArea : public Widget
{
public:
  WidgetsArea(Widget* parent) : Widget(parent) {}
  void performLayout(NVGcontext *ctx) override
  {
    VScrollPanel* vpanel = dynamic_cast<VScrollPanel*>(parent());
    int xoffset = 0;
    if (vpanel)
      xoffset = vpanel->getSliderAreaWidth();
    setSize(parent()->size() - Vector2i(xoffset, 0));
    Widget::performLayout(ctx);
  }

  void draw(NVGcontext* ctx) override
  {
    Widget::draw(ctx);
  }
};

Vector2i Listbox::preferredSize(NVGcontext *ctx) const {
  if (!mItems)
    return Vector2i::Zero();
  return mItems->preferredSize(ctx);
}

Listbox::Listbox(Widget* parent)
  : Widget(parent)
{
  mPanel = add<VScrollPanel>();
  mItems = mPanel->add<WidgetsArea>();
  mItems->setLayout(new BoxLayout(Orientation::Vertical, Alignment::Minimum));
}

void Listbox::draw(NVGcontext* ctx)
{
  Widget::draw(ctx);
}

void Listbox::addItem(const std::string& str, const std::string& id)
{
  if (!mPanel)
    return;

  auto& lb = mItems->wdg<ListboxItem>(str);
  lb.setId(id);
}

void Listbox::performLayout(NVGcontext *ctx) {
  if (mPanel)
    mPanel->setFixedSize(size());
  Widget::performLayout(ctx);
}

NAMESPACE_END(nanogui)