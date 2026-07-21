#include "GUISystem.hpp"

void EditorPanel::SetDockspace(DockspaceManager* dockspace)
{
	Dockspace = dockspace;
}

void DockspaceManager::Init(const std::string& dockspaceName)
{
	m_DockspaceName = dockspaceName;
}

void DockspaceManager::Shutdown()
{
	for (auto& doc : m_DocumentTabs) doc->OnDetach();
	for (auto& panel : m_FixedPanels) panel->OnDetach();
	m_DocumentTabs.clear();
	m_FixedPanels.clear();
}

void DockspaceManager::OnUpdate(float deltaTime)
{
	for (auto& panel : m_FixedPanels)
		if (panel->IsOpen()) panel->OnUpdate(deltaTime);

	for (auto& doc : m_DocumentTabs)
		if (doc->IsOpen()) doc->OnUpdate(deltaTime);
}

void DockspaceManager::RenderMenuBar()
{
	if (ImGui::BeginMenuBar())
	{
		if (m_MenuBarCallback)
		{
			m_MenuBarCallback();
		}

		if (ImGui::BeginMenu("Window"))
		{
			if (ImGui::MenuItem("Reset Layout"))
			{
				BuildDefaultLayout();
			}

			ImGui::Separator();

			for (auto& panel : m_FixedPanels)
			{
				bool isOpen = panel->IsOpen();
				if (ImGui::MenuItem(panel->GetTitle().c_str(), nullptr, &isOpen))
				{
					panel->SetOpen(isOpen);
				}
			}

			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}
}

void DockspaceManager::RenderToolBar()
{
	if (!m_ToolBarCallback) return;

	ImGuiViewport* viewport = ImGui::GetMainViewport();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.12f, 1.00f));

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

	if (ImGui::BeginChild("##EngineToolBarStrip", ImVec2(0, 35.0f), false, flags))
	{
		m_ToolBarCallback();
	}
	ImGui::EndChild();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
}

void DockspaceManager::BeginDockspaceWindow()
{
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("###EngineDockSpaceWindow", nullptr, windowFlags);
	ImGui::PopStyleVar(3);

	RenderMenuBar();
	RenderToolBar();

	m_DockspaceID = ImGui::GetID(m_DockspaceName.c_str());
	ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
	ImGui::DockSpace(m_DockspaceID, ImVec2(0.0f, 0.0f), dockspaceFlags);
}

void DockspaceManager::EndDockspaceWindow()
{
	ImGui::End();
}

void DockspaceManager::BuildDefaultLayout()
{
	ImGui::DockBuilderRemoveNode(m_DockspaceID);
	ImGui::DockBuilderAddNode(m_DockspaceID, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(m_DockspaceID, ImGui::GetMainViewport()->Size);

	ImGuiID centerNode = m_DockspaceID;

	ImGuiID leftNode = ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Left, 0.20f, nullptr, &centerNode);
	ImGuiID rightNode = ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Right, 0.25f, nullptr, &centerNode);
	ImGuiID bottomNode = ImGui::DockBuilderSplitNode(centerNode, ImGuiDir_Down, 0.25f, nullptr, &centerNode);

	ImGuiID leftTopNode = leftNode;
	ImGuiID leftBottomNode = ImGui::DockBuilderSplitNode(leftTopNode, ImGuiDir_Down, 0.50f, nullptr, &leftTopNode);

	ImGuiID rightTopNode = rightNode;
	ImGuiID rightBottomNode = ImGui::DockBuilderSplitNode(rightTopNode, ImGuiDir_Down, 0.50f, nullptr, &rightTopNode);

	for (auto& panel : m_FixedPanels)
	{
		ImGuiID targetNode = centerNode;

		switch (panel->GetDefaultDockDirection())
		{
		case DockDirection::Left:        targetNode = leftNode; break;
		case DockDirection::Left_Top:    targetNode = leftTopNode; break;
		case DockDirection::Left_Bottom: targetNode = leftBottomNode; break;

		case DockDirection::Right:        targetNode = rightNode; break;
		case DockDirection::Right_Top:    targetNode = rightTopNode; break;
		case DockDirection::Right_Bottom: targetNode = rightBottomNode; break;

		case DockDirection::Bottom:       targetNode = bottomNode; break;
		default:                          targetNode = centerNode; break;
		}

		ImGui::DockBuilderDockWindow(panel->GetID().c_str(), targetNode);
	}

	ImGui::DockBuilderFinish(m_DockspaceID);
}

void DockspaceManager::OnImGuiRender()
{
	BeginDockspaceWindow();

	if (m_LayoutNeedsReset)
	{
		BuildDefaultLayout();
		m_LayoutNeedsReset = false;
	}

	for (auto& panel : m_FixedPanels)
	{
		if (!panel->IsOpen()) continue;

		bool open = panel->IsOpen();
		if (ImGui::Begin(panel->GetID().c_str(), &open))
		{
			panel->OnImGuiRender();
		}
		ImGui::End();
		panel->SetOpen(open);
	}

	std::vector<std::shared_ptr<DocumentTab>> tabsToClose;

	for (auto& doc : m_DocumentTabs)
	{
		bool open = doc->IsOpen();

		if (ImGui::Begin(doc->GetID().c_str(), &open))
		{
			doc->OnImGuiRender();
		}
		ImGui::End();

		doc->SetOpen(open);
		if (!open) tabsToClose.push_back(doc);
	}

	for (auto& doc : tabsToClose)
	{
		doc->OnDetach();
		std::erase(m_DocumentTabs, doc);
	}

	EndDockspaceWindow();
}

void DockspaceManager::ApplyHydrogenTheme()
{
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;

	// --- GEOMETRY & SPACING ---
	style.WindowRounding = 0.0f;
	style.ChildRounding = 0.0f;
	style.FrameRounding = 0.0f;
	style.PopupRounding = 0.0f;
	style.ScrollbarRounding = 0.0f;
	style.GrabRounding = 0.0f;
	style.TabRounding = 0.0f;

	style.WindowBorderSize = 1.0f;
	style.FrameBorderSize = 1.0f;
	style.PopupBorderSize = 1.0f;
	style.TabBorderSize = 1.0f;

	// --- COLOR PALETTE ---
	// Hex: #3DDCFF -> (61, 220, 255) -> (0.24f, 0.86f, 1.00f) [Electric Cyan]
	ImVec4 electricCyan = ImVec4(0.24f, 0.86f, 1.00f, 1.00f);
	ImVec4 electricCyanMuted = ImVec4(0.24f, 0.86f, 1.00f, 0.60f);

	// Hex: #0A3D91 -> (10, 61, 145) -> (0.04f, 0.24f, 0.57f) [Deep Blue]
	ImVec4 deepBlue = ImVec4(0.04f, 0.24f, 0.57f, 1.00f);
	ImVec4 deepBlueHover = ImVec4(0.06f, 0.30f, 0.68f, 1.00f);

	// Ultra-dark blue-tinted bases for the engine background
	ImVec4 bgBase = ImVec4(0.05f, 0.06f, 0.08f, 1.00f);
	ImVec4 bgSurface = ImVec4(0.08f, 0.09f, 0.12f, 1.00f);
	ImVec4 border = ImVec4(0.12f, 0.14f, 0.18f, 1.00f);

	colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

	// Backgrounds
	colors[ImGuiCol_WindowBg] = bgBase;
	colors[ImGuiCol_ChildBg] = bgBase;
	colors[ImGuiCol_PopupBg] = bgSurface;
	colors[ImGuiCol_Border] = border;
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f); // No shadows

	// Frames (Text boxes, Checkboxes, etc.)
	colors[ImGuiCol_FrameBg] = bgSurface;
	colors[ImGuiCol_FrameBgHovered] = deepBlue;
	colors[ImGuiCol_FrameBgActive] = electricCyan;

	// Titles
	colors[ImGuiCol_TitleBg] = bgSurface;
	colors[ImGuiCol_TitleBgActive] = bgSurface;
	colors[ImGuiCol_TitleBgCollapsed] = bgBase;

	// Buttons
	colors[ImGuiCol_Button] = bgSurface;
	colors[ImGuiCol_ButtonHovered] = deepBlueHover;
	colors[ImGuiCol_ButtonActive] = electricCyan;

	// Sliders & Grabs
	colors[ImGuiCol_SliderGrab] = deepBlue;
	colors[ImGuiCol_SliderGrabActive] = electricCyan;
	colors[ImGuiCol_CheckMark] = electricCyan;

	// Tabs
	colors[ImGuiCol_Tab] = bgBase;
	colors[ImGuiCol_TabHovered] = deepBlueHover;
	colors[ImGuiCol_TabActive] = deepBlue;
	colors[ImGuiCol_TabUnfocused] = bgBase;
	colors[ImGuiCol_TabUnfocusedActive] = bgSurface;

	// Headers (Collapsing headers, tree nodes)
	colors[ImGuiCol_Header] = bgSurface;
	colors[ImGuiCol_HeaderHovered] = deepBlueHover;
	colors[ImGuiCol_HeaderActive] = deepBlue;

	// Docking & Resizing
	colors[ImGuiCol_Separator] = border;
	colors[ImGuiCol_SeparatorHovered] = deepBlue;
	colors[ImGuiCol_SeparatorActive] = electricCyan;
	colors[ImGuiCol_ResizeGrip] = deepBlue;
	colors[ImGuiCol_ResizeGripHovered] = deepBlueHover;
	colors[ImGuiCol_ResizeGripActive] = electricCyan;
	colors[ImGuiCol_DockingPreview] = electricCyanMuted;
}
