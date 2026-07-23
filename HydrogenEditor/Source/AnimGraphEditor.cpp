#include "AnimGraphEditor.hpp"
#include <Hydrogen/Application.hpp>
#include <misc/cpp/imgui_stdlib.h>
#include <fstream>
#include <json.hpp>

using json = nlohmann::json;
namespace ed = ax::NodeEditor;
using namespace Hydrogen;

AnimGraphEditor::AnimGraphEditor(const std::string& filePath)
	: DocumentTab(filePath)
{
}

AnimGraphEditor::~AnimGraphEditor()
{
	if (m_NodeContext)
		ed::DestroyEditor(m_NodeContext);
}

void AnimGraphEditor::ApplyBlueprintStyle()
{
	ed::Style& style = ed::GetStyle();

	style.Colors[ed::StyleColor_Bg] = ImColor(18, 18, 22, 255);
	style.Colors[ed::StyleColor_Grid] = ImColor(32, 32, 38, 200);
	style.Colors[ed::StyleColor_NodeBg] = ImColor(28, 29, 33, 230);
	style.Colors[ed::StyleColor_NodeBorder] = ImColor(45, 46, 52, 255);
	style.Colors[ed::StyleColor_HovNodeBorder] = ImColor(255, 170, 0, 255);
	style.Colors[ed::StyleColor_SelNodeBorder] = ImColor(255, 200, 0, 255);

	style.Colors[ed::StyleColor_HighlightLinkBorder] = ImColor(255, 180, 0, 255);
	style.Colors[ed::StyleColor_PinRect] = ImColor(60, 180, 255, 100);
	style.Colors[ed::StyleColor_PinRectBorder] = ImColor(60, 180, 255, 255);
	style.Colors[ed::StyleColor_Flow] = ImColor(255, 170, 0, 255);

	style.NodeRounding = 4.0f;
	style.NodeBorderWidth = 1.5f;
	style.HoveredNodeBorderWidth = 2.0f;
	style.SelectedNodeBorderWidth = 2.5f;
	style.LinkStrength = 100.0f;
}

bool AnimGraphEditor::IsPinLinked(ax::NodeEditor::PinId id) const
{
	if (!id)
		return false;

	for (const auto& transition : m_Transitions)
	{
		if (transition.OutputPinID == id || transition.InputPinID == id)
			return true;
	}

	return false;
}

void AnimGraphEditor::OnAttach()
{
	ed::Config config;
	std::string nodeConfigPath = GetFilePath() + ".nodes";
	config.SettingsFile = nodeConfigPath.c_str();

	m_NodeContext = ed::CreateEditor(&config);

	ed::SetCurrentEditor(m_NodeContext);
	ApplyBlueprintStyle();
	ed::SetCurrentEditor(nullptr);

	std::ifstream file(GetFilePath());
	if (file.is_open())
	{
		try
		{
			json j;
			file >> j;

			m_NextID = j.value("next_id", 1);

			for (const auto& p : j["parameters"])
			{
				AnimParameter param;
				param.Name = p["name"];
				param.Type = static_cast<ParameterType>(p["type"]);
				if (param.Type == ParameterType::Float) param.Value = p["value"].get<float>();
				else if (param.Type == ParameterType::Int) param.Value = p["value"].get<int>();
				else if (param.Type == ParameterType::Bool) param.Value = p["value"].get<bool>();
				m_Parameters.push_back(param);
			}

			for (const auto& s : j["states"])
			{
				AnimState state;
				state.ID = ed::NodeId(s["id"]);
				state.Name = s["name"];

				state.AnimationClip = nullptr;
				const auto& clipPath = s["clip_path"];
				if (clipPath != "NULL")
					state.AnimationClip = Application::Get()->MainAssetManager.GetAsset<AnimationAsset>(clipPath);

				state.InputPinID = ed::PinId(s["input_pin"]);
				state.OutputPinID = ed::PinId(s["output_pin"]);
				state.IsDefaultState = s["is_default"];
				state.PlaybackSpeed = s.value("speed", 1.0f);
				m_States.push_back(state);
			}

			for (const auto& t : j["transitions"])
			{
				AnimTransition trans;
				trans.ID = ed::LinkId(t["id"]);
				trans.FromStateID = ed::NodeId(t["from"]);
				trans.ToStateID = ed::NodeId(t["to"]);
				trans.OutputPinID = ed::PinId(t["output_pin"]);
				trans.InputPinID = ed::PinId(t["input_pin"]);
				trans.Duration = t.value("duration", 0.25f);

				for (const auto& c : t["conditions"])
				{
					trans.Conditions.push_back({
						c["param"],
						static_cast<ConditionMode>(c["mode"]),
						c["threshold"]
						});
				}
				m_Transitions.push_back(trans);
			}
		}
		catch (...) {}
	}

	if (m_States.empty())
	{
		AnimState idleState;
		idleState.ID = ed::NodeId(GetNextID());
		idleState.Name = "Idle";
		idleState.AnimationClip = nullptr;
		idleState.InputPinID = ed::PinId(GetNextID());
		idleState.OutputPinID = ed::PinId(GetNextID());
		idleState.IsDefaultState = true;

		m_States.push_back(idleState);
		m_IsDirty = true;
	}
}

void AnimGraphEditor::OnDetach()
{
	if (m_NodeContext)
	{
		ed::DestroyEditor(m_NodeContext);
		m_NodeContext = nullptr;
	}
}

void AnimGraphEditor::OnImGuiRender()
{
	DrawTopBar();

	ImGui::Columns(3, "AnimEditorColumns", true);

	DrawParameterSidebar();
	ImGui::NextColumn();

	DrawNodeCanvas();
	ImGui::NextColumn();

	DrawInspectorSidebar();

	ImGui::Columns(1);
}

void AnimGraphEditor::DrawTopBar()
{
	if (ImGui::Button(" Save "))
	{
		OnSave();
	}

	ImGui::SameLine();

	if (ImGui::Button("+ Add Clip"))
	{
		CreateNewState("New State", nullptr);
	}

	ImGui::SameLine();
	if (m_IsDirty)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "* Unsaved Changes");
	}
	else
	{
		ImGui::TextDisabled("Saved");
	}

	ImGui::Separator();
}

void AnimGraphEditor::DrawParameterSidebar()
{
	ImGui::TextDisabled("PARAMETERS");
	ImGui::Separator();

	if (ImGui::Button("+ Add Parameter", ImVec2(-1, 0)))
	{
		ImGui::OpenPopup("AddParamPopup");
	}

	if (ImGui::BeginPopup("AddParamPopup"))
	{
		if (ImGui::MenuItem("Float")) { m_Parameters.push_back({ "NewFloat", ParameterType::Float, 0.0f }); m_IsDirty = true; }
		if (ImGui::MenuItem("Int")) { m_Parameters.push_back({ "NewInt", ParameterType::Int, 0 }); m_IsDirty = true; }
		if (ImGui::MenuItem("Bool")) { m_Parameters.push_back({ "NewBool", ParameterType::Bool, false }); m_IsDirty = true; }
		ImGui::EndPopup();
	}

	ImGui::Spacing();

	for (size_t i = 0; i < m_Parameters.size(); ++i)
	{
		auto& param = m_Parameters[i];
		ImGui::PushID(static_cast<int>(i));

		ImGui::SetNextItemWidth(90.0f);
		if (ImGui::InputText("##Name", &param.Name)) m_IsDirty = true;
		ImGui::SameLine();

		if (param.Type == ParameterType::Float)
		{
			float v = std::get<float>(param.Value);
			if (ImGui::DragFloat("##Val", &v, 0.1f)) { param.Value = v; m_IsDirty = true; }
		}
		else if (param.Type == ParameterType::Bool)
		{
			bool v = std::get<bool>(param.Value);
			if (ImGui::Checkbox("##Val", &v)) { param.Value = v; m_IsDirty = true; }
		}

		ImGui::PopID();
	}
}

void AnimGraphEditor::DrawNodeCanvas()
{
	ed::SetCurrentEditor(m_NodeContext);
	ed::Begin("AnimGraphCanvas");

	for (const auto& state : m_States)
	{
		ed::BeginNode(state.ID);

		// Calculate Header Rect sizes BEFORE rendering text on top
		ImVec2 headerStartPos = ImGui::GetCursorScreenPos();
		std::string titleText = state.IsDefaultState ? state.Name + " (Default)" : state.Name;
		ImVec2 textSize = ImGui::CalcTextSize(titleText.c_str());
		ImVec2 headerSize = ImVec2(textSize.x + 20.0f, textSize.y + 10.0f);

		// 1. Draw Background Box FIRST (So it lands beneath text)
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImColor headerColor = state.IsDefaultState ? ImColor(40, 140, 60, 255) : ImColor(18, 92, 140, 255);

		drawList->AddRectFilled(
			headerStartPos,
			ImVec2(headerStartPos.x + headerSize.x, headerStartPos.y + headerSize.y),
			headerColor,
			4.0f,
			ImDrawFlags_RoundCornersTop
		);

		// 2. Render Header Text OVER background box
		ImGui::SetCursorScreenPos(ImVec2(headerStartPos.x + 10.0f, headerStartPos.y + 5.0f));
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", titleText.c_str());

		// Advance cursor below header box
		ImGui::SetCursorScreenPos(ImVec2(headerStartPos.x, headerStartPos.y + headerSize.y + 6.0f));

		ImGui::TextDisabled("Clip: %s", state.AnimationClip ? std::filesystem::path(state.AnimationClip->GetPath()).filename().string().c_str() : "NULL");
		ImGui::Spacing();

		// Pin Layouts
		ed::BeginPin(state.InputPinID, ed::PinKind::Input);
		ImGui::Text("-> In");
		ed::EndPin();

		ImGui::SameLine();
		ImGui::Dummy(ImVec2(30.0f, 0.0f));
		ImGui::SameLine();

		ed::BeginPin(state.OutputPinID, ed::PinKind::Output);
		ImGui::Text("Out ->");
		ed::EndPin();

		ed::EndNode();
	}

	for (const auto& transition : m_Transitions)
	{
		ed::Link(transition.ID, transition.OutputPinID, transition.InputPinID);
	}

	if (ed::BeginCreate())
	{
		ed::PinId startPinId, endPinId;
		if (ed::QueryNewLink(&startPinId, &endPinId))
		{
			if (startPinId && endPinId && ed::AcceptNewItem())
			{
				AnimState* fromState = nullptr;
				AnimState* toState = nullptr;

				for (auto& s : m_States)
				{
					if (s.OutputPinID == startPinId) fromState = &s;
					if (s.InputPinID == endPinId) toState = &s;
				}

				if (fromState && toState && fromState->ID != toState->ID)
				{
					AnimTransition newTrans;
					newTrans.ID = ed::LinkId(GetNextID());
					newTrans.FromStateID = fromState->ID;
					newTrans.ToStateID = toState->ID;
					newTrans.OutputPinID = startPinId;
					newTrans.InputPinID = endPinId;

					m_Transitions.push_back(newTrans);
					m_IsDirty = true;
				}
			}
		}
	}
	ed::EndCreate();

	if (ed::BeginDelete())
	{
		ed::NodeId deletedNodeId;
		while (ed::QueryDeletedNode(&deletedNodeId))
		{
			if (ed::AcceptDeletedItem())
			{
				std::erase_if(m_Transitions, [deletedNodeId](const AnimTransition& t) {
					return t.FromStateID == deletedNodeId || t.ToStateID == deletedNodeId;
					});

				std::erase_if(m_States, [deletedNodeId](const AnimState& s) {
					return s.ID == deletedNodeId;
					});

				m_IsDirty = true;
			}
		}
	}
	ed::EndDelete();

	m_SelectedNodeID = 0;
	m_SelectedLinkID = 0;
	if (ed::GetSelectedObjectCount() > 0)
	{
		ed::NodeId selectedNodes[1];
		ed::LinkId selectedLinks[1];

		if (ed::GetSelectedNodes(selectedNodes, 1) > 0) m_SelectedNodeID = selectedNodes[0];
		if (ed::GetSelectedLinks(selectedLinks, 1) > 0) m_SelectedLinkID = selectedLinks[0];
	}

	ed::Suspend();
	if (ed::ShowBackgroundContextMenu())
	{
		ImGui::OpenPopup("CanvasContextMenu");
	}

	if (ImGui::BeginPopup("CanvasContextMenu"))
	{
		if (ImGui::MenuItem("Add Animation State"))
		{
			CreateNewState("New State", nullptr);
		}
		ImGui::EndPopup();
	}
	ed::Resume();

	ed::End();
	ed::SetCurrentEditor(nullptr);

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE"))
		{
			std::filesystem::path newPath((const char*)payload->Data);
			auto asset = Application::Get()->MainAssetManager.GetAsset<AnimationAsset>(newPath.filename().string());
			if (asset)
			{
				CreateNewState("New State", asset);
			}
		}
		ImGui::EndDragDropTarget();
	}
}

void AnimGraphEditor::DrawInspectorSidebar()
{
	ImGui::TextDisabled("DETAILS");
	ImGui::Separator();

	if (m_SelectedNodeID)
	{
		for (auto& state : m_States)
		{
			if (state.ID == m_SelectedNodeID)
			{
				ImGui::Text("State: %s", state.Name.c_str());
				if (ImGui::InputText("Name", &state.Name)) m_IsDirty = true;

				if (state.AnimationClip)
				{
					ImGui::Text(std::filesystem::path(state.AnimationClip->GetPath()).filename().string().c_str());
				}
				else
				{
					ImGui::Text("NULL");
				}
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE"))
					{
						std::filesystem::path newPath((const char*)payload->Data);
						auto asset = Application::Get()->MainAssetManager.GetAsset<AnimationAsset>(newPath.filename().string());
						if (asset)
						{
							state.AnimationClip = asset;
						}
					}
					ImGui::EndDragDropTarget();
				}

				if (ImGui::DragFloat("Speed", &state.PlaybackSpeed, 0.05f, 0.0f, 10.0f)) m_IsDirty = true;

				if (ImGui::Checkbox("Is Default State", &state.IsDefaultState))
				{
					m_IsDirty = true;
					for (auto& s : m_States)
						if (s.ID != state.ID) s.IsDefaultState = false;
				}
				break;
			}
		}
	}
	else if (m_SelectedLinkID)
	{
		for (auto& trans : m_Transitions)
		{
			if (trans.ID == m_SelectedLinkID)
			{
				ImGui::TextUnformatted("Transition Properties");
				if (ImGui::DragFloat("Duration (s)", &trans.Duration, 0.01f, 0.0f, 5.0f)) m_IsDirty = true;

				ImGui::Separator();
				ImGui::TextUnformatted("Conditions");

				if (ImGui::Button("+ Add Condition"))
				{
					if (!m_Parameters.empty())
					{
						trans.Conditions.push_back({ m_Parameters[0].Name, ConditionMode::Greater, 0.0f });
						m_IsDirty = true;
					}
				}

				for (size_t i = 0; i < trans.Conditions.size(); ++i)
				{
					auto& cond = trans.Conditions[i];
					ImGui::PushID(static_cast<int>(i));

					if (ImGui::BeginCombo("##Param", cond.ParameterName.c_str()))
					{
						for (const auto& p : m_Parameters)
						{
							if (ImGui::Selectable(p.Name.c_str())) { cond.ParameterName = p.Name; m_IsDirty = true; }
						}
						ImGui::EndCombo();
					}

					if (ImGui::DragFloat("Threshold", &cond.Threshold, 0.1f)) m_IsDirty = true;
					ImGui::PopID();
				}
				break;
			}
		}
	}
	else
	{
		ImGui::TextDisabled("Select a State or Transition to inspect.");
	}
}

void AnimGraphEditor::CreateNewState(const std::string& name, const std::shared_ptr<AnimationAsset>& animationClip)
{
	AnimState newState;
	newState.ID = ed::NodeId(GetNextID());
	newState.Name = name.empty() ? "New State" : name;
	newState.AnimationClip = animationClip;

	newState.InputPinID = ed::PinId(GetNextID());
	newState.OutputPinID = ed::PinId(GetNextID());

	if (m_States.empty())
	{
		newState.IsDefaultState = true;
	}

	m_States.push_back(newState);
	m_IsDirty = true;
}

void AnimGraphEditor::OnSave()
{
	json j;

	j["next_id"] = m_NextID;

	j["parameters"] = json::array();
	for (const auto& p : m_Parameters)
	{
		json paramJson;
		paramJson["name"] = p.Name;
		paramJson["type"] = static_cast<int>(p.Type);

		if (p.Type == ParameterType::Float) paramJson["value"] = std::get<float>(p.Value);
		else if (p.Type == ParameterType::Int) paramJson["value"] = std::get<int>(p.Value);
		else if (p.Type == ParameterType::Bool) paramJson["value"] = std::get<bool>(p.Value);

		j["parameters"].push_back(paramJson);
	}

	j["states"] = json::array();
	for (const auto& s : m_States)
	{
		json stateJson;
		stateJson["id"] = s.ID.Get();
		stateJson["name"] = s.Name;
		stateJson["clip_path"] = s.AnimationClip ? std::filesystem::path(s.AnimationClip->GetPath()).filename().string() : "NULL";
		stateJson["input_pin"] = s.InputPinID.Get();
		stateJson["output_pin"] = s.OutputPinID.Get();
		stateJson["is_default"] = s.IsDefaultState;
		stateJson["speed"] = s.PlaybackSpeed;
		j["states"].push_back(stateJson);
	}

	j["transitions"] = json::array();
	for (const auto& t : m_Transitions)
	{
		json transJson;
		transJson["id"] = t.ID.Get();
		transJson["from"] = t.FromStateID.Get();
		transJson["to"] = t.ToStateID.Get();
		transJson["output_pin"] = t.OutputPinID.Get();
		transJson["input_pin"] = t.InputPinID.Get();
		transJson["duration"] = t.Duration;

		transJson["conditions"] = json::array();
		for (const auto& c : t.Conditions)
		{
			json condJson;
			condJson["param"] = c.ParameterName;
			condJson["mode"] = static_cast<int>(c.Mode);
			condJson["threshold"] = c.Threshold;
			transJson["conditions"].push_back(condJson);
		}

		j["transitions"].push_back(transJson);
	}

	//std::ofstream file(GetFilePath());
	//if (file.is_open())
	//{
	//	file << j.dump(4);
	//	m_IsDirty = false;
	//}
}
