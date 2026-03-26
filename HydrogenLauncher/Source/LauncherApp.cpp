#include <Hydrogen/HydrogenMain.hpp>
#include <filesystem>

class Launcherapp : public Hydrogen::Application
{
public:
    void CreateProject(const std::string& name, const std::string& path)
    {
        HY_APP_INFO("Creating project '{}' at '{}'", name, path);

        std::filesystem::path targetPath = std::filesystem::path(path) / name;

        if (std::filesystem::exists(targetPath))
        {
            HY_APP_ERROR("Project already exists!\n");
            ImGui::OpenPopup("ProjectExistsPopup");
            return;
        }

        std::filesystem::copy(std::filesystem::path(MainAssetManager.GetAssetDirectory()) / "ProjectTemplate", targetPath,
            std::filesystem::copy_options::recursive |
            std::filesystem::copy_options::overwrite_existing);
	}

	virtual void OnSetup() override
	{
		ApplicationSpec.Name = "Hydrogen Launcher";
		ApplicationSpec.Version = { 1, 0 };
		ApplicationSpec.ViewportTitle = "Hydrogen Launcher";
		ApplicationSpec.ViewportSize = { 1920, 1080 };
		ApplicationSpec.UseDebugGUI = true;
	}

	virtual void OnStartup() override
	{
	}

	virtual void OnShutdown() override
	{
	}

	virtual void OnUpdate() override
	{
	}

	virtual void OnImGuiRender() override
	{
        ImGuiViewport* viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);

        ImGui::Begin("Launcher",
            nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoSavedSettings
        );

        auto& style = ImGui::GetStyle();

        // ---- Background ----
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 min = ImGui::GetWindowPos();
        ImVec2 max = ImVec2(min.x + viewport->Size.x, min.y + viewport->Size.y);

        // gradient background
        drawList->AddRectFilledMultiColor(
            min, max,
            IM_COL32(20, 20, 30, 255),
            IM_COL32(30, 30, 50, 255),
            IM_COL32(20, 20, 30, 255),
            IM_COL32(10, 10, 20, 255)
        );

        // ---- Centering ----
        ImVec2 center = ImVec2(viewport->Size.x * 0.5f, viewport->Size.y * 0.5f);

        ImGui::SetCursorPos(ImVec2(center.x - 200, center.y - 150));
        ImGui::BeginGroup();

        // ---- Logo / Title ----
        ImGui::PushFont(ImGui::GetFont()); // replace with big font if you have one
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 40);
        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "HYDROGEN");
        ImGui::PopFont();

        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::TextDisabled("Project Launcher");
        ImGui::Spacing();
        ImGui::Spacing();

        // ---- Buttons styling ----
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));

        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(50, 100, 180, 200));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(70, 130, 220, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(40, 90, 160, 255));

        // ---- Create Project ----
        if (ImGui::Button("Create New Project", ImVec2(300, 40)))
        {
            ImGui::OpenPopup("CreateProjectPopup");
        }

        ImGui::Spacing();

        // ---- Open Project ----
        if (ImGui::Button("Open Project", ImVec2(300, 40)))
        {
            HY_APP_INFO("Picked Project '{}'", Hydrogen::Viewport::OpenFolderDialog());
            ImGui::OpenPopup("InvalidProjectPopup");
        }

        ImGui::PopStyleColor(3);

        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::TextDisabled("v1.0");

        ImGui::PopStyleVar(2);

        ImGui::EndGroup();

        // ---- Create Project Popup ----
        static char projectName[128] = "MyProject";
        static char projectPath[256] = "C:/Projects/";

        if (ImGui::BeginPopupModal("CreateProjectPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Create New Project");
            ImGui::Separator();

            ImGui::InputText("Name", projectName, sizeof(projectName));

            static char projectPath[256] = "C:/Projects/";

            ImGui::InputText("Path", projectPath, sizeof(projectPath));
            ImGui::SameLine();

            if (ImGui::Button("..."))
            {
                std::string selected = Hydrogen::Viewport::OpenFolderDialog();
                if (!selected.empty())
                {
                    strncpy(projectPath, selected.c_str(), sizeof(projectPath));
                    projectPath[sizeof(projectPath) - 1] = '\0';
                }
            }

            ImGui::Spacing();

            if (ImGui::Button("Create", ImVec2(120, 0)))
            {
				CreateProject(std::string(projectName), std::string(projectPath));
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("InvalidProjectPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Selected folder is not a valid project.");
            ImGui::Spacing();

            if (ImGui::Button("OK", ImVec2(120, 0)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("ProjectExistsPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Project already exists.");
            ImGui::Spacing();

            if (ImGui::Button("OK", ImVec2(120, 0)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        ImGui::End();
	}

	virtual void OnImGuiMenuBarRender() override
	{
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}
	}
};

extern std::shared_ptr<Hydrogen::Application> GetApplication()
{
	return std::make_shared<Launcherapp>();
}
