#pragma once

#include <algorithm>
#include <string>

#include <DirectXMath.h>

#include "SceneBase.h"
#include "Button.h"
#include "Image.h"
#include "Text.h"

struct UITextStyle
{
    WidgetLayer layer = WidgetLayer::HUD;
    UIDirection pivot = UIDirection::LeftTop;
    XMFLOAT4 color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    XMFLOAT2 offset = XMFLOAT2(0.0f, 0.0f);
};

struct UIImageStyle
{
    WidgetLayer layer = WidgetLayer::HUD;
    UIDirection pivot = UIDirection::LeftTop;
    std::wstring texturePath;
};

struct UIButtonStyle
{
    WidgetLayer buttonLayer = WidgetLayer::Panels;
    WidgetLayer labelLayer = WidgetLayer::Tooltip;
    UIDirection buttonPivot = UIDirection::LeftTop;
    XMFLOAT4 buttonColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    UIDirection labelAnchor = UIDirection::Left;
    UIDirection labelPivot = UIDirection::Left;
    XMFLOAT4 labelColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    XMFLOAT2 labelOffset = XMFLOAT2(12.0f, 0.0f);
    float labelHeight = 22.0f;
    std::wstring texturePath;
};

struct UIColumnLayout
{
    XMFLOAT3 cursor{ 0.0f, 0.0f, 0.0f };
    float gap = 0.0f;

    XMFLOAT3 Push(Float2 sizePx, float extraGap = 0.0f)
    {
        const XMFLOAT3 out = cursor;
        cursor.y += sizePx.y + gap + extraGap;
        return out;
    }

    XMFLOAT3 Peek() const
    {
        return cursor;
    }

    void AddGap(float extraGap)
    {
        cursor.y += extraGap;
    }
};

struct UILabeledButton
{
    Button* button = nullptr;
    Text* label = nullptr;
};

class UIScope
{
public:
    explicit UIScope(SceneBase& scene)
        : m_scene(scene)
    {
    }

    UIColumnLayout Column(float x, float y, float gap, float z = 0.0f) const
    {
        UIColumnLayout layout{};
        layout.cursor = XMFLOAT3(x, y, z);
        layout.gap = gap;
        return layout;
    }

    Text* CreateText(const std::wstring& name,
        Float2 sizePx,
        const XMFLOAT3& pos,
        const std::wstring& text = std::wstring(),
        const UITextStyle& style = UITextStyle())
    {
        Text* widget = m_scene.CreateWidget<Text>(name, sizePx, pos, style.pivot);
        widget->SetLayer(style.layer);
        widget->SetColor(style.color);
        widget->SetTextOffset(style.offset);
        if (!text.empty())
            widget->SetText(text);
        return widget;
    }

    Image* CreateImage(const std::wstring& name,
        Float2 sizePx,
        const XMFLOAT3& pos,
        const UIImageStyle& style = UIImageStyle())
    {
        Image* widget = m_scene.CreateWidget<Image>(name, sizePx, pos, style.pivot);
        widget->SetLayer(style.layer);
        if (!style.texturePath.empty())
            widget->ChangeTexture(style.texturePath);
        return widget;
    }

    Button* CreateButton(const std::wstring& name,
        Float2 sizePx,
        const XMFLOAT3& pos,
        const UIButtonStyle& style = UIButtonStyle())
    {
        Button* widget = m_scene.CreateWidget<Button>(name, sizePx, pos, style.buttonPivot);
        widget->SetLayer(style.buttonLayer);
        widget->SetColor(style.buttonColor);
        if (!style.texturePath.empty())
            widget->ChangeTexture(style.texturePath);
        return widget;
    }

    UILabeledButton CreateLabeledButton(const std::wstring& name,
        Float2 sizePx,
        const XMFLOAT3& pos,
        const std::wstring& labelText,
        const UIButtonStyle& style = UIButtonStyle())
    {
        UILabeledButton parts{};
        parts.button = CreateButton(name, sizePx, pos, style);

        const float labelWidth = std::max(0.0f, sizePx.x - (style.labelOffset.x * 2.0f));
        parts.label = m_scene.CreateWidget<Text>(
            name + L"_Label",
            Float2(labelWidth, style.labelHeight),
            XMFLOAT3(style.labelOffset.x, style.labelOffset.y, 0.0f),
            style.labelPivot);
        parts.label->SetText(labelText);
        parts.label->SetColor(style.labelColor);
        parts.label->SetAnchor(style.labelAnchor);
        parts.button->Attach(parts.label);
        parts.label->SetLayer(style.labelLayer);

        return parts;
    }

private:
    SceneBase& m_scene;
};
