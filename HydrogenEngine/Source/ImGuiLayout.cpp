#include "ImGuiLayout.hpp"
#include <imgui_internal.h>
#include <vector>

namespace
{
    enum class LayoutType
    {
        Horizontal,
        Vertical
    };

    struct LayoutSpring
    {
        float Weight;
        float Spacing;
    };

    struct LayoutGroup
    {
        ImGuiID ID;
        LayoutType Type;
        ImVec2 RequestedSize;
        float Alignment;

        ImVec2 StartPos;
        ImVec2 WorkSize;

        float TotalSpringWeight = 0.0f;
        int SpringCount = 0;

        std::vector<LayoutSpring> Springs;
    };

    static std::vector<LayoutGroup> g_LayoutStack;

    LayoutGroup* GetCurrentLayout()
    {
        return g_LayoutStack.empty() ? nullptr : &g_LayoutStack.back();
    }
}

namespace ImGui
{
    static void BeginLayoutGroup(ImGuiID id, LayoutType type, const ImVec2& size, float align)
    {
        g_LayoutStack.push_back({});
        LayoutGroup& group = g_LayoutStack.back();

        group.ID = id;
        group.Type = type;
        group.RequestedSize = size;
        group.Alignment = align;
        group.StartPos = ImGui::GetCursorScreenPos();

        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (type == LayoutType::Horizontal)
        {
            ImGui::BeginGroup();
        }
        else
        {
            ImGui::BeginGroup();
        }
    }

    void BeginHorizontal(const char* str_id, const ImVec2& size, float align)
    {
        BeginLayoutGroup(ImGui::GetID(str_id), LayoutType::Horizontal, size, align);
    }

    void BeginHorizontal(const void* ptr_id, const ImVec2& size, float align)
    {
        BeginLayoutGroup(ImGui::GetID(ptr_id), LayoutType::Horizontal, size, align);
    }

    void EndHorizontal()
    {
        if (g_LayoutStack.empty()) return;

        LayoutGroup group = g_LayoutStack.back();
        g_LayoutStack.pop_back();

        ImGui::EndGroup();

        // Handle auto-spacing for next standard ImGui element
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = g.CurrentWindow;
        window->DC.CursorPos.y = window->DC.CursorPosPrevLine.y;
    }

    void BeginVertical(const char* str_id, const ImVec2& size, float align)
    {
        BeginLayoutGroup(ImGui::GetID(str_id), LayoutType::Vertical, size, align);
    }

    void BeginVertical(const void* ptr_id, const ImVec2& size, float align)
    {
        BeginLayoutGroup(ImGui::GetID(ptr_id), LayoutType::Vertical, size, align);
    }

    void EndVertical()
    {
        if (g_LayoutStack.empty()) return;

        LayoutGroup group = g_LayoutStack.back();
        g_LayoutStack.pop_back();

        ImGui::EndGroup();
    }

    void Spring(float weight, float spacing)
    {
        LayoutGroup* group = GetCurrentLayout();
        if (!group) return;

        ImGuiContext& g = *GImGui;
        ImGuiStyle& style = g.Style;

        if (spacing < 0.0f)
        {
            spacing = (group->Type == LayoutType::Horizontal) ? style.ItemSpacing.x : style.ItemSpacing.y;
        }

        float availSpace = 0.0f;
        if (group->Type == LayoutType::Horizontal)
        {
            availSpace = ImGui::GetContentRegionAvail().x;
            if (availSpace > 0.0f && weight > 0.0f)
            {
                float springWidth = ImMax(0.0f, availSpace * (weight / (group->TotalSpringWeight + weight)));
                ImGui::Dummy(ImVec2(springWidth, 0.0f));
                ImGui::SameLine(0.0f, spacing);
            }
            else
            {
                ImGui::Dummy(ImVec2(spacing, 0.0f));
                ImGui::SameLine(0.0f, 0.0f);
            }
        }
        else // Vertical
        {
            availSpace = ImGui::GetContentRegionAvail().y;
            if (availSpace > 0.0f && weight > 0.0f)
            {
                float springHeight = ImMax(0.0f, availSpace * (weight / (group->TotalSpringWeight + weight)));
                ImGui::Dummy(ImVec2(0.0f, springHeight));
            }
            else
            {
                ImGui::Dummy(ImVec2(0.0f, spacing));
            }
        }

        group->TotalSpringWeight += weight;
        group->SpringCount++;
    }
}
