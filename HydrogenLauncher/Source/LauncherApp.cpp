#include <Hydrogen/HydrogenMain.hpp>
#include <imgui.h>
#include <filesystem>
#include <commdlg.h>
#include <windows.h>
#include <vector>
#include <string>

using namespace Hydrogen;

struct ProjectInfo
{
	std::string Name;
	std::string Path;
	std::string LastModified;
};

class LauncherApp : public Application
{
private:
	std::unique_ptr<Renderer> m_ImGuiRenderer;
	std::vector<ProjectInfo> m_RecentProjects;

	// Creation State Variables
	char m_NewProjectName[128] = "MyNewHydrogenProject";
	std::string m_NewProjectPath = "";
	int m_SelectedTemplateIdx = 0;
	const char* m_Templates[2] = { "3D Standard (Deferred)", "2D Stylized (Forward+)" };

	std::string m_StatusMessage = "Ready";

public:
	virtual void OnSetup() override
	{
		ApplicationSpec.Name = "Hydrogen Launcher";
		ApplicationSpec.Version = { 1, 0 };
		ApplicationSpec.ViewportTitle = "Hydrogen Launcher";
		ApplicationSpec.ViewportSize = { 1280, 720 }; // Launcher doesn't need to be fullscreen by default
		ApplicationSpec.UseDebugGUI = true;
	}

	virtual void OnStartup() override
	{
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		std::filesystem::path fontPath = std::filesystem::path(MainAssetManager.GetAssetDirectory()) / "Editor/Montserrat-Regular.ttf";
		ImFont* customFont = io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), 16.0f);

		if (customFont == nullptr)
		{
			HY_APP_WARN("Failed to load custom font! Falling back to default ImGui font.");
		}
		else
		{
			io.FontDefault = customFont;
		}

		ApplyHydrogenTheme();
		m_ImGuiRenderer = std::make_unique<Renderer>(MainViewport, ActiveRenderDevice.get(), ActiveSwapChain.get());

		// Seed dummy data for visualization testing
		m_RecentProjects.push_back({ "HorrorGame3D", "C:\\Projects\\Hydrogen\\HorrorGame3D", "2 hours ago" });
		m_RecentProjects.push_back({ "PhysicsSandbox", "C:\\Projects\\Hydrogen\\PhysicsSandbox", "Yesterday" });
	}

	virtual void OnShutdown() override
	{
		m_ImGuiRenderer.reset();
	}

	virtual void OnUpdate(float deltaTime) override
	{
		DefaultRenderer::RenderImGui(m_ImGuiRenderer.get(), ActiveSwapChain.get());
	}

	virtual void OnImGuiRender() override
	{
		m_ImGuiRenderer->BeginImGuiFrame();

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		SetupDockspace(viewport->WorkPos, viewport->WorkSize, viewport->ID);

		DrawProjectDashboard();
		DrawProjectCreator();
	}

	virtual void OnSwapchainRecreation() override
	{
		m_ImGuiRenderer->UpdateSwapChain(ActiveSwapChain.get());
	}

	virtual void OnRenderDeviceChangeStart() override
	{
		m_ImGuiRenderer.reset();
	}

	virtual void OnRenderDeviceChangeFinish() override
	{
		m_ImGuiRenderer = std::make_unique<Renderer>(MainViewport, ActiveRenderDevice.get(), ActiveSwapChain.get());
	}

private:
	std::string OpenFolderDialog()
	{
		OPENFILENAMEA ofn;
		char szFile[260] = "Select Project Directory";
		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = NULL;
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
		ofn.lpstrFilter = "Folder Selection\0*.*\0";
		ofn.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;

		if (GetOpenFileNameA(&ofn) == TRUE)
		{
			std::filesystem::path p(szFile);
			return p.parent_path().string();
		}
		return "";
	}

	void LaunchEditor(const std::string& projectPath)
	{
		HY_APP_INFO("Attempting to boot Hydrogen Editor engine runtime instance...");

		std::string editorExecutable = "HydrogenEditor.exe";
		std::string commandLine = editorExecutable;
		std::string workingDir = "..\\HydrogenEditor";

		STARTUPINFOA si;
		PROCESS_INFORMATION pi;
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));

		// Fire execution handle out into standard OS process context
		if (CreateProcessA(
			NULL,
			&commandLine[0],
			NULL, NULL, FALSE,
			CREATE_NEW_CONSOLE,
			NULL, &workingDir[0],
			&si, &pi))
		{
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
			m_StatusMessage = "Editor booted successfully.";
		}
		else
		{
			m_StatusMessage = "Failed to launch editor executable. Ensure path alignment is valid.";
			HY_APP_ERROR("Process initialization fault occurred handling main editor context execution loops.");
		}
	}

	void GenerateNewProjectFiles()
	{
		if (m_NewProjectPath.empty())
		{
			m_StatusMessage = "Error: Invalid project path generation context targeted.";
			return;
		}

		std::filesystem::path rootDir(m_NewProjectPath);
		rootDir /= m_NewProjectName;

		try {
			// Generate standard structural layout directory configurations
			std::filesystem::create_directories(rootDir / "Assets");
			std::filesystem::create_directories(rootDir / "Cache");
			std::filesystem::create_directories(rootDir / "Config");

			// Inject a dummy project structural config context data block
			std::ofstream projectFile(rootDir / (std::string(m_NewProjectName) + ".hyproj"));
			projectFile << "{\n\t\"ProjectName\": \"" << m_NewProjectName << "\",\n\t\"EngineVersion\": \"1.0\"\n}";
			projectFile.close();

			m_StatusMessage = "Project file trees constructed smoothly!";
			m_RecentProjects.push_back({ m_NewProjectName, rootDir.string(), "Just Now" });
		}
		catch (const std::exception& e) {
			m_StatusMessage = std::string("Directory creation failed: ") + e.what();
		}
	}

	void SetupDockspace(ImVec2 pos, ImVec2 size, ImGuiID viewportId)
	{
		static bool dockingEnabled = true;
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		ImGui::SetNextWindowPos(pos);
		ImGui::SetNextWindowSize(size);
		ImGui::SetNextWindowViewport(viewportId);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

		ImGui::Begin("MainLauncherDock", &dockingEnabled, window_flags);
		ImGui::PopStyleVar(2);

		ImGuiID dockspace_id = ImGui::GetID("LauncherDockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

		ImGui::End();
	}

	void DrawProjectDashboard()
	{
		ImGui::Begin("Recent Projects Workspace");

		ImGui::Text("Select a project workspace template context below to access deployment trees.");
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("Import Existing Project Folder Path", ImVec2(-1, 35)))
		{
			std::string selectedFolder = OpenFolderDialog();
			if (!selectedFolder.empty())
			{
				std::filesystem::path p(selectedFolder);
				m_RecentProjects.push_back({ p.filename().string(), selectedFolder, "Loaded" });
			}
		}

		ImGui::Spacing();
		ImGui::BeginChild("ProjectItemDisplayRegion", ImVec2(0, -30), true);

		for (const auto& project : m_RecentProjects)
		{
			ImGui::PushID(project.Name.c_str());

			ImGui::TextColored(ImVec4(0.24f, 0.86f, 1.00f, 1.00f), "%s", project.Name.c_str());
			ImGui::TextDisabled("Location: %s", project.Path.c_str());
			ImGui::TextDisabled("Modified: %s", project.LastModified.c_str());

			ImGui::SameLine(ImGui::GetWindowWidth() - 120);
			if (ImGui::Button("Launch Engine", ImVec2(100, 40)))
			{
				LaunchEditor(project.Path);
			}

			ImGui::Separator();
			ImGui::PopID();
		}

		ImGui::EndChild();
		ImGui::TextColored(ImVec4(0.24f, 0.86f, 1.00f, 1.00f), "System Messages: %s", m_StatusMessage.c_str());

		ImGui::End();
	}

	void DrawProjectCreator()
	{
		ImGui::Begin("Construct New Project Root");

		ImGui::Text("Pipeline Template Framework Selection");
		ImGui::Separator();
		ImGui::Combo("Engine Pipeline Paradigm", &m_SelectedTemplateIdx, m_Templates, IM_ARRAYSIZE(m_Templates));

		ImGui::Spacing();
		ImGui::Text("Filesystem Paths Target Configuration");
		ImGui::Separator();

		ImGui::InputText("Project Identity Handle", m_NewProjectName, sizeof(m_NewProjectName));

		std::string pathStr = m_NewProjectPath;
		char pathBuf[512];
		strncpy_s(pathBuf, pathStr.c_str(), sizeof(pathBuf));
		ImGui::InputText("Directory Root Base", pathBuf, sizeof(pathBuf), ImGuiInputTextFlags_ReadOnly);
		ImGui::SameLine();
		if (ImGui::Button("Browse Location..."))
		{
			m_NewProjectPath = OpenFolderDialog();
		}

		ImGui::Spacing();
		if (ImGui::Button("Generate Project Infrastructure", ImVec2(-1, 45)))
		{
			GenerateNewProjectFiles();
		}

		ImGui::End();
	}

	void ApplyHydrogenTheme()
	{
		ImGuiStyle& style = ImGui::GetStyle();
		ImVec4* colors = style.Colors;

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

		ImVec4 electricCyan = ImVec4(0.24f, 0.86f, 1.00f, 1.00f);
		ImVec4 electricCyanMuted = ImVec4(0.24f, 0.86f, 1.00f, 0.60f);
		ImVec4 deepBlue = ImVec4(0.04f, 0.24f, 0.57f, 1.00f);
		ImVec4 deepBlueHover = ImVec4(0.06f, 0.30f, 0.68f, 1.00f);
		ImVec4 bgBase = ImVec4(0.05f, 0.06f, 0.08f, 1.00f);
		ImVec4 bgSurface = ImVec4(0.08f, 0.09f, 0.12f, 1.00f);
		ImVec4 border = ImVec4(0.12f, 0.14f, 0.18f, 1.00f);

		colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
		colors[ImGuiCol_WindowBg] = bgBase;
		colors[ImGuiCol_ChildBg] = bgBase;
		colors[ImGuiCol_PopupBg] = bgSurface;
		colors[ImGuiCol_Border] = border;
		colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_FrameBg] = bgSurface;
		colors[ImGuiCol_FrameBgHovered] = deepBlue;
		colors[ImGuiCol_FrameBgActive] = electricCyan;
		colors[ImGuiCol_TitleBg] = bgSurface;
		colors[ImGuiCol_TitleBgActive] = bgSurface;
		colors[ImGuiCol_TitleBgCollapsed] = bgBase;
		colors[ImGuiCol_Button] = bgSurface;
		colors[ImGuiCol_ButtonHovered] = deepBlueHover;
		colors[ImGuiCol_ButtonActive] = electricCyan;
		colors[ImGuiCol_SliderGrab] = deepBlue;
		colors[ImGuiCol_SliderGrabActive] = electricCyan;
		colors[ImGuiCol_CheckMark] = electricCyan;
		colors[ImGuiCol_Tab] = bgBase;
		colors[ImGuiCol_TabHovered] = deepBlueHover;
		colors[ImGuiCol_TabActive] = deepBlue;
		colors[ImGuiCol_TabUnfocused] = bgBase;
		colors[ImGuiCol_TabUnfocusedActive] = bgSurface;
		colors[ImGuiCol_Header] = bgSurface;
		colors[ImGuiCol_HeaderHovered] = deepBlueHover;
		colors[ImGuiCol_HeaderActive] = deepBlue;
		colors[ImGuiCol_Separator] = border;
		colors[ImGuiCol_SeparatorHovered] = deepBlue;
		colors[ImGuiCol_SeparatorActive] = electricCyan;
		colors[ImGuiCol_ResizeGrip] = deepBlue;
		colors[ImGuiCol_ResizeGripHovered] = deepBlueHover;
		colors[ImGuiCol_ResizeGripActive] = electricCyan;
		colors[ImGuiCol_DockingPreview] = electricCyanMuted;
	}
};

extern std::shared_ptr<Application> GetApplication()
{
	return std::make_shared<LauncherApp>();
}