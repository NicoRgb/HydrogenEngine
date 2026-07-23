#pragma once

#include "GUISystem.hpp"
#include <imgui.h>
#include <string>
#include <vector>

#include <imgui_node_editor.h>

namespace ed = ax::NodeEditor;

enum class ParameterType { Float, Int, Bool, Trigger };

struct AnimParameter
{
    std::string Name;
    ParameterType Type = ParameterType::Float;
    std::variant<float, int, bool> Value = 0.0f;
};

enum class ConditionMode { Greater, Less, Equal, NotEqual, True, False };

struct Condition
{
    std::string ParameterName;
    ConditionMode Mode = ConditionMode::Greater;
    float Threshold = 0.0f;
};

struct AnimTransition
{
    ed::LinkId ID;
    ed::NodeId FromStateID;
    ed::NodeId ToStateID;

    ed::PinId OutputPinID;
    ed::PinId InputPinID;

    float Duration = 0.25f;
    std::vector<Condition> Conditions;
};

struct AnimState
{
    ed::NodeId ID;
    std::string Name;
    std::shared_ptr<Hydrogen::AnimationAsset> AnimationClip;

    ed::PinId InputPinID;
    ed::PinId OutputPinID;

    bool IsDefaultState = false;
    float PlaybackSpeed = 1.0f;
};

class AnimGraphEditor : public DocumentTab
{
public:
    AnimGraphEditor(const std::string& filePath);
    virtual ~AnimGraphEditor();

    virtual void OnAttach() override;
    virtual void OnDetach() override;
    virtual void OnImGuiRender() override;
    virtual void OnSave() override;

private:
    ed::EditorContext* m_NodeContext = nullptr;
    uint32_t m_NextID = 1;

    std::vector<AnimParameter> m_Parameters;
    std::vector<AnimState> m_States;
    std::vector<AnimTransition> m_Transitions;

    ed::NodeId m_SelectedNodeID = 0;
    ed::LinkId m_SelectedLinkID = 0;

    void ApplyBlueprintStyle();

    bool IsPinLinked(ax::NodeEditor::PinId id) const;

    void DrawTopBar();
    void DrawParameterSidebar();
    void DrawNodeCanvas();
    void DrawInspectorSidebar();
    void CreateNewState(const std::string& name, const std::shared_ptr<Hydrogen::AnimationAsset>& animationClip);

    uint32_t GetNextID() { return m_NextID++; }
};
