#include <Hydrogen/HydrogenMain.hpp>
#include <imgui.h>
#include <filesystem>
#include <commdlg.h>
#include <windows.h>

#include "MeshImporter.hpp"

using namespace Hydrogen;

struct ExportOptions
{
	std::wstring InputFilePath = L"";
	std::wstring OutputDirectory = L"";

	bool ExportStaticMesh = true;
	bool ExportSkeletalMesh = true;
	bool ExportSkeleton = true;
	bool ExportAnimation = true;

	std::string ModelName = "NewAsset";
};

class AssetExtractor
{
public:
	AssetExtractor(const ExportOptions& options)
	{
		const std::string inputPath(options.InputFilePath.begin(), options.InputFilePath.end());

		if (options.ExportSkeleton)
		{
			m_SkeletonAsset = std::make_shared<AssimpSkeletonAsset>();
		}
		if (options.ExportAnimation)
		{
			m_AnimationAsset = std::make_shared<AssimpAnimationAsset>();
		}
		if (options.ExportSkeletalMesh)
		{
			m_SkeletalMeshAsset = std::make_shared<AssimpSkeletalMeshAsset>(inputPath, m_SkeletonAsset, m_AnimationAsset);

			auto path = std::filesystem::path(options.OutputDirectory) / options.ModelName;
			m_SkeletalMeshAsset->WriteAssetFile(path.string() + ".hydynmesh");
		}
		if (options.ExportStaticMesh)
		{
			m_StaticMeshAsset = std::make_shared<AssimpStaticMeshAsset>(inputPath);

			auto path = std::filesystem::path(options.OutputDirectory) / options.ModelName;
			m_StaticMeshAsset->WriteAssetFile(path.string() + ".hymesh");
		}

		if (options.ExportSkeleton)
		{
			auto path = std::filesystem::path(options.OutputDirectory) / options.ModelName;
			m_SkeletonAsset->WriteAssetFile(path.string() + ".hyskel");
		}
		if (options.ExportAnimation)
		{
			auto path = std::filesystem::path(options.OutputDirectory) / options.ModelName;
			m_AnimationAsset->WriteAssetFile(path.string() + ".hyanim");
		}
	}

private:
	std::shared_ptr<AssimpSkeletonAsset> m_SkeletonAsset;
	std::shared_ptr<AssimpSkeletalMeshAsset> m_SkeletalMeshAsset;
	std::shared_ptr<AssimpStaticMeshAsset> m_StaticMeshAsset;
	std::shared_ptr<AssimpAnimationAsset> m_AnimationAsset;
};

class ToolApp : public Application
{
private:
	std::unique_ptr<Renderer> m_ImGuiRenderer;
	ExportOptions m_Options;
	std::string m_StatusMessage = "Ready";

public:
	virtual void OnSetup() override
	{
		ApplicationSpec.Name = "Hydrogen Tools";
		ApplicationSpec.Version = { 1, 0 };
		ApplicationSpec.ViewportTitle = "Hydrogen Tools";
		ApplicationSpec.ViewportSize = { 1920, 1080 };
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

		DrawAssetCompilerPanel();
		DrawLogMessages();
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
	std::string WStringToString(const std::wstring& wstr)
	{
		if (wstr.empty()) return "";
		int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
		std::string strTo(size_needed, 0);
		WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
		return strTo;
	}

	std::wstring OpenFileDialog(const wchar_t* filter)
	{
		OPENFILENAMEW ofn;
		wchar_t szFile[260] = { 0 };
		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = NULL;
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
		ofn.lpstrFilter = filter;
		ofn.nFilterIndex = 1;
		ofn.lpstrFileTitle = NULL;
		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = NULL;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (GetOpenFileNameW(&ofn) == TRUE)
		{
			return std::wstring(szFile);
		}
		return L"";
	}

	std::wstring OpenFolderDialog()
	{
		OPENFILENAMEW ofn;
		wchar_t szFile[260] = L"Select Folder";
		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = NULL;
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
		ofn.lpstrFilter = L"Folder Selection\0*.*\0";
		ofn.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;

		if (GetOpenFileNameW(&ofn) == TRUE)
		{
			std::filesystem::path p(szFile);
			return p.parent_path().wstring();
		}
		return L"";
	}

	void ExecuteAssetCompilation()
	{
		if (m_Options.InputFilePath.empty())
		{
			m_StatusMessage = "Error: No input path specified.";
			HY_APP_ERROR("Asset Compiler failed: Input source missing.");
			return;
		}
		if ((m_Options.ExportSkeleton || m_Options.ExportAnimation) && !m_Options.ExportSkeletalMesh)
		{
			m_StatusMessage = "Error: Cant export Skeleton or Animation without SkeletalMesh.";
			HY_APP_ERROR("Asset Compiler failed: Cant export Skeleton or Animation without SkeletalMesh.");
			return;
		}
		HY_APP_INFO("Beginning processing pipeline for asset generation...");
		m_StatusMessage = "Asset processing complete!";

		AssetExtractor extractor(m_Options);
	}

	void SetupDockspace(ImVec2 pos, ImVec2 size, ImGuiID viewportId)
	{
		static bool dockingEnabled = true;
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		ImGui::SetNextWindowPos(pos);
		ImGui::SetNextWindowSize(size);
		ImGui::SetNextWindowViewport(viewportId);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

		ImGui::Begin("MainDockSpace", &dockingEnabled, window_flags);
		ImGui::PopStyleVar(2);

		ImGuiID dockspace_id = ImGui::GetID("DockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

		DrawMenuBar();
		ImGui::End();
	}

	void DrawMenuBar()
	{
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Open File...", "Ctrl+O"))
				{
					m_Options.InputFilePath = OpenFileDialog(L"Model Files Layout\0*.fbx;*.gltf;*.glb;*.obj\0");
				}
				if (ImGui::MenuItem("Exit", "Alt+F4"))
				{
					MainViewport->Close();
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}
	}

	void DrawAssetCompilerPanel()
	{
		ImGui::Begin("Asset Extraction Pipeline");

		ImGui::Text("Target Import Settings");
		ImGui::Separator();
		ImGui::Spacing();

		std::string inputStr = WStringToString(m_Options.InputFilePath);
		char inputBuf[512];
		strncpy_s(inputBuf, inputStr.c_str(), sizeof(inputBuf));
		ImGui::InputText("Source Model File", inputBuf, sizeof(inputBuf), ImGuiInputTextFlags_ReadOnly);
		ImGui::SameLine();
		if (ImGui::Button("Browse...##Source"))
		{
			m_Options.InputFilePath = OpenFileDialog(L"Model Files Layout\0*.fbx;*.gltf;*.glb;*.obj\0");
		}

		std::string outputStr = WStringToString(m_Options.OutputDirectory);
		char outputBuf[512];
		strncpy_s(outputBuf, outputStr.c_str(), sizeof(outputBuf));
		ImGui::InputText("Export Destination", outputBuf, sizeof(outputBuf), ImGuiInputTextFlags_ReadOnly);
		ImGui::SameLine();
		if (ImGui::Button("Browse...##Dest"))
		{
			m_Options.OutputDirectory = OpenFolderDialog();
		}

		char nameBuf[128];
		strncpy_s(nameBuf, m_Options.ModelName.c_str(), sizeof(nameBuf));
		if (ImGui::InputText("Asset Base Identity", nameBuf, sizeof(nameBuf)))
		{
			m_Options.ModelName = nameBuf;
		}

		ImGui::Spacing();
		ImGui::Text("Pipeline Export Elements");
		ImGui::Separator();
		ImGui::Checkbox("Compile Static Mesh (.hymesh)", &m_Options.ExportStaticMesh);
		ImGui::Checkbox("Compile Skeletal Mesh (.hydynmesh)", &m_Options.ExportSkeletalMesh);
		ImGui::Checkbox("Compile Joint Skeleton (.hyskel)", &m_Options.ExportSkeleton);
		ImGui::Checkbox("Compile Linear Animation Sequences (.hyanim)", &m_Options.ExportAnimation);

		ImGui::Spacing();
		ImGui::Text("Actions");
		ImGui::Separator();

		if (ImGui::Button("Execute Compilation", ImVec2(200, 40)))
		{
			ExecuteAssetCompilation();
		}

		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.24f, 0.86f, 1.00f, 1.00f), "Status: %s", m_StatusMessage.c_str());

		ImGui::End();
	}

	void DrawLogMessages()
	{
		ImGui::Begin("Console Output");
		ImGui::BeginChild("LogScrollRegion");

		auto drawSink = [&](auto logger) {
			if (!logger || !logger->GetLogSink()) return;
			for (auto& m : logger->GetLogSink()->GetMessages())
			{
				ImVec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
				if (m.level == spdlog::level::debug) color = { 0.6f, 0.6f, 1.0f, 1.0f };
				else if (m.level == spdlog::level::warn) color = { 1.0f, 1.0f, 0.1f, 1.0f };
				else if (m.level == spdlog::level::err) color = { 1.0f, 0.3f, 0.3f, 1.0f };
				else if (m.level == spdlog::level::critical) color = { 1.0f, 0.0f, 0.0f, 1.0f };

				ImGui::PushStyleColor(ImGuiCol_Text, color);
				ImGui::TextUnformatted(m.message.c_str());
				ImGui::PopStyleColor();
			}
			};

		drawSink(EngineLogger::GetLogger());
		drawSink(AppLogger::GetLogger());

		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);

		ImGui::EndChild();
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
	return std::make_shared<ToolApp>();
}
