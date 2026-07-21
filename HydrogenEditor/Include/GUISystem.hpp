#pragma once

#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <filesystem>
#include <typeindex>
#include <imgui.h>
#include <imgui_internal.h>

#include <Hydrogen/Scene.hpp>

struct AssetSelectedEvent
{
	std::string AssetPath;
};

struct SceneChangeEvent
{
	Hydrogen::Scene* Scene;
};

struct EntitySelectedEvent
{
	Hydrogen::Entity SelectedEntity;
};

struct SceneViewportRenderedEvent
{
	VkImageView TextureView = VK_NULL_HANDLE;
};

struct GameViewportRenderedEvent
{
	VkImageView TextureView = VK_NULL_HANDLE;
};

struct SceneViewportStateEvent
{
	glm::vec2 Size{ 1920.0f, 1080.0f };
	glm::vec2 BoundsMin{ 0.0f, 0.0f };
	glm::vec2 BoundsMax{ 0.0f, 0.0f };
	bool IsVisible = false;
	bool IsHovered = false;
};

struct GameViewportStateEvent
{
	glm::vec2 Size{ 1920.0f, 1080.0f };
	bool IsVisible = false;
};

class EventBus
{
public:
	template<typename EventType>
	void Subscribe(std::function<void(const EventType&)> listener)
	{
		auto typeIdx = std::type_index(typeid(EventType));
		m_Subscribers[typeIdx].push_back([listener](const void* event)
			{
				listener(*static_cast<const EventType*>(event));
			});
	}

	template<typename EventType>
	void Publish(const EventType& event)
	{
		auto typeIdx = std::type_index(typeid(EventType));
		if (m_Subscribers.contains(typeIdx))
		{
			for (auto& callback : m_Subscribers[typeIdx])
			{
				callback(&event);
			}
		}
	}

private:
	using EventCallback = std::function<void(const void*)>;
	std::unordered_map<std::type_index, std::vector<EventCallback>> m_Subscribers;
};

enum class DockDirection
{
	None = 0,
	Center,
	Left,
	Right,
	Top,
	Bottom,
	Left_Top,
	Left_Bottom,
	Right_Top,
	Right_Bottom
};

class EditorPanel
{
public:
	virtual ~EditorPanel() = default;

	virtual void OnAttach() {}
	virtual void OnDetach() {}
	virtual void OnUpdate(float deltaTime) {}
	virtual void OnImGuiRender() = 0;

	virtual std::string GetTitle() const = 0;
	virtual std::string GetID() const { return GetTitle() + "###" + std::to_string(m_PanelID); }

	virtual DockDirection GetDefaultDockDirection() const { return DockDirection::None; }
	virtual float GetDefaultDockSplitRatio() const { return 0.25f; }

	bool IsOpen() const { return m_IsOpen; }
	void SetOpen(bool isOpen) { m_IsOpen = isOpen; }

	void SetDockspace(class DockspaceManager* dockspace);

protected:
	static inline uint64_t s_IDCounter = 1;
	uint64_t m_PanelID = s_IDCounter++;
	bool m_IsOpen = true;

	class DockspaceManager* Dockspace;
};

class DocumentTab : public EditorPanel
{
public:
	DocumentTab(const std::string& filePath)
		: m_FilePath(filePath)
	{
		m_FileName = std::filesystem::path(filePath).filename().string();
	}

	virtual std::string GetTitle() const override { return m_FileName; }

	virtual std::string GetID() const override
	{
		std::string title = m_FileName;
		if (m_IsDirty) title += " *";
		return title + "###doc_" + std::to_string(m_PanelID);
	}

	virtual DockDirection GetDefaultDockDirection() const override { return DockDirection::Center; }

	const std::string& GetFilePath() const { return m_FilePath; }
	bool IsDirty() const { return m_IsDirty; }
	void SetDirty(bool dirty) { m_IsDirty = dirty; }

	virtual void OnSave() {}
protected:
	std::string m_FilePath;
	std::string m_FileName;
	bool m_IsDirty = false;
};

class DockspaceManager
{
public:
	DockspaceManager() = default;
	~DockspaceManager() = default;

	void Init(const std::string& dockspaceName = "MainEngineDockSpace");
	void Shutdown();

	void OnUpdate(float deltaTime);
	void OnImGuiRender();

	void ApplyHydrogenTheme();

	template<typename T, typename... Args>
	std::shared_ptr<T> RegisterPanel(Args&&... args)
	{
		static_assert(std::is_base_of_v<EditorPanel, T>, "T must derive from EditorPanel");
		auto panel = std::make_shared<T>(std::forward<Args>(args)...);
		panel->SetDockspace(this);
		panel->OnAttach();
		m_FixedPanels.push_back(panel);
		return panel;
	}

	template<typename T, typename... Args>
	std::shared_ptr<T> OpenDocument(const std::string& filePath, Args&&... args)
	{
		static_assert(std::is_base_of_v<DocumentTab, T>, "T must derive from DocumentTab");

		for (auto& doc : m_DocumentTabs)
		{
			if (doc->GetFilePath() == filePath)
			{
				ImGui::SetWindowFocus(doc->GetID().c_str());
				return std::static_pointer_cast<T>(doc);
			}
		}

		auto doc = std::make_shared<T>(filePath, std::forward<Args>(args)...);
		doc->OnAttach();
		m_DocumentTabs.push_back(doc);
		return doc;
	}

	void SetMenuBarCallback(const std::function<void()>& callback) { m_MenuBarCallback = callback; }
	void SetToolBarCallback(const std::function<void()>& callback) { m_ToolBarCallback = callback; }

	void ResetToDefaultLayout() { m_LayoutNeedsReset = true; }

	EventBus& GetEventBus() { return m_EventBus; }

private:
	void RenderMenuBar();
	void RenderToolBar();
	void BeginDockspaceWindow();
	void EndDockspaceWindow();
	void BuildDefaultLayout();

private:
	std::string m_DockspaceName = "MainEngineDockSpace";
	ImGuiID m_DockspaceID = 0;
	bool m_LayoutNeedsReset = true;

	std::function<void()> m_MenuBarCallback = nullptr;
	std::function<void()> m_ToolBarCallback = nullptr;

	std::vector<std::shared_ptr<EditorPanel>> m_FixedPanels;
	std::vector<std::shared_ptr<DocumentTab>> m_DocumentTabs;

	EventBus m_EventBus;
};
