#include <Hydrogen/Hydrogen.hpp>
#include <imgui.h>
#include <json.hpp>

#include <filesystem>

void DrawFileConfig(json& j, const std::string& label = "")
{
	std::string assetType = j["type"].get<std::string>();

	ImGui::Text(j["name"].get<std::string>().c_str());
	ImGui::Indent();
	ImGui::Text(assetType.c_str());
	ImGui::Unindent();

	if (ImGui::TreeNode("Preferences"))
	{
		if (assetType == "Shader")
		{
			std::string stage = j["preferences"]["stage"];
			const char* current_item = stage.c_str();
			/*const char* items[] = {"vertex", "fragment"};
			if (ImGui::BeginCombo("##combo", current_item))
			{
				for (int n = 0; n < IM_ARRAYSIZE(items); n++)
				{
					bool is_selected = (current_item == items[n]);
					if (ImGui::Selectable(items[n], is_selected))
					{
						current_item = items[n];
						if (is_selected)
						{
							ImGui::SetItemDefaultFocus();
						}
					}
				}
				ImGui::EndCombo();
			}*/
			ImGui::Text(current_item);
		}

		ImGui::TreePop();
	}
}

static Hydrogen::Entity SelectedEntity;

void DrawSceneHierarchy(const std::shared_ptr<Hydrogen::Scene>& scene)
{
	ImGui::Begin("Scene Hierarchy");

	if (scene != nullptr)
	{
		scene->IterateComponents<Hydrogen::TagComponent>([&](Hydrogen::Entity entity, const auto& tag)
			{
			std::string name = tag.Name;

			bool isSelected = (SelectedEntity == entity);
			if (ImGui::Selectable(name.c_str(), isSelected))
			{
				SelectedEntity = entity;
			}
			});
	}
	else
	{
		SelectedEntity = Hydrogen::Entity();
	}

	ImGui::End();
}

template<typename T>
void DrawComponent(const std::shared_ptr<Hydrogen::Scene>& scene, Hydrogen::Entity entity)
{
	T& component = entity.GetComponent<T>();
	T::OnImGuiRender(component);
}

using ComponentTypes = std::tuple<Hydrogen::TagComponent, Hydrogen::TransformComponent, Hydrogen::MeshRendererComponent>;

template<typename... Ts>
void DrawAllComponents(const std::shared_ptr<Hydrogen::Scene>& scene, Hydrogen::Entity entity, std::tuple<Ts...>)
{
	(DrawComponent<Ts>(scene, entity), ...);
}

void DrawInspector(const std::shared_ptr<Hydrogen::Scene>& scene)
{
	ImGui::Begin("Inspector");

	if (SelectedEntity.IsValid())
	{
		DrawAllComponents(scene, SelectedEntity, ComponentTypes{});
	}
	else
	{
		ImGui::Text("No entity selected");
	}

	ImGui::End();
}

class AssetBrowserPanel
{
public:
	AssetBrowserPanel(const std::filesystem::path& assetDirectory)
		: m_AssetDirectory(assetDirectory), m_CurrentDirectory(assetDirectory), m_CurrentFile("")
	{
	}

	void LoadTextures(std::shared_ptr<Hydrogen::Texture> folderTexture, std::shared_ptr<Hydrogen::Texture> fileTexture)
	{
		m_FolderTexture = folderTexture;
		m_FileTexture = fileTexture;
	}

	void OnImGuiRender()
	{
		ImGui::Begin("Asset Browser");

		if (m_CurrentDirectory != m_AssetDirectory)
		{
			if (ImGui::Button("<-"))
			{
				m_CurrentDirectory = m_CurrentDirectory.parent_path();
			}
		}

		static float thumbnailSize = 64.0f;
		static float padding = 16.0f;
		float cellSize = thumbnailSize + padding;
		float panelWidth = ImGui::GetContentRegionAvail().x;
		int columnCount = (int)(panelWidth / cellSize);
		if (columnCount < 1)
			columnCount = 1;

		ImGui::Columns(columnCount, 0, false);

		for (auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory))
		{
			const auto& path = entry.path();
			if (path.extension() == ".hyasset")
			{
				continue;
			}

			std::string filename = path.filename().string();

			ImGui::PushID(filename.c_str());

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));

			if (entry.is_directory())
			{
				ImGui::ImageButton("[DIR]", m_FolderTexture->GetImGuiImage(), {thumbnailSize, thumbnailSize});
			}
			else
			{
				ImGui::ImageButton("[FILE]", m_FileTexture->GetImGuiImage(), {thumbnailSize, thumbnailSize});
				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
				{
					std::string pathStr = path.string();
					ImGui::SetDragDropPayload("ASSET_FILE", pathStr.c_str(), pathStr.size() + 1);
					ImGui::Text("%s", filename.c_str());
					ImGui::EndDragDropSource();
				}
			}

			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && entry.is_directory())
			{
				m_CurrentDirectory /= path.filename();
			}

			if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !entry.is_directory())
			{
				m_CurrentFile = path;
			}

			ImGui::PopStyleColor(1);

			ImGui::TextWrapped("%s", filename.c_str());

			ImGui::NextColumn();
			ImGui::PopID();
		}

		ImGui::Columns(1);

		ImGui::End();

		ImGui::Begin("Asset Inspector");
		if (m_CurrentFile.empty())
		{
			ImGui::Text("No entity selected");
		}
		else
		{
			std::ifstream fin(m_CurrentFile.string() + ".hyasset");
			if (!fin)
			{
				Hydrogen::Application::Get()->MainAssetManager.LoadAssets("assets", Hydrogen::Application::Get()->_RenderContext);
				fin = std::ifstream(m_CurrentFile.string() + ".hyasset");
				HY_ASSERT(fin, "Invalid asset file");
			}

			auto config = json::parse(fin);
			fin.close();

			DrawFileConfig(config);
		}
		ImGui::End();
	}

private:
	std::filesystem::path m_AssetDirectory;
	std::filesystem::path m_CurrentDirectory;
	std::filesystem::path m_CurrentFile;

	std::shared_ptr<Hydrogen::Texture> m_FolderTexture;
	std::shared_ptr<Hydrogen::Texture> m_FileTexture;
};

static AssetBrowserPanel EditorAssetBrowserPanel("assets");

class EditorApp : public Hydrogen::Application
{
public:
	virtual void OnSetup() override
	{
		ApplicationSpec.Name = "Hydrogen Editor";
		ApplicationSpec.Version = { 1, 0 };
		ApplicationSpec.ViewportTitle = "Hydrogen Editor";
		ApplicationSpec.ViewportSize = { 1920, 1080 };
		ApplicationSpec.UseDebugGUI = true;
	}

	virtual void OnStartup() override
	{
		auto folderTextureAsset = MainAssetManager.GetAsset<Hydrogen::TextureAsset>("folder_icon.png");
		auto fileTextureAsset = MainAssetManager.GetAsset<Hydrogen::TextureAsset>("file_icon.png");

		EditorAssetBrowserPanel.LoadTextures(folderTextureAsset->GetTexture(), fileTextureAsset->GetTexture());
	}

	virtual void OnShutdown() override
	{
	}

	virtual void OnUpdate() override
	{
	}

	virtual void OnImGuiRender() override
	{
		EditorAssetBrowserPanel.OnImGuiRender();
		DrawSceneHierarchy(CurrentScene);
		DrawInspector(CurrentScene);
	}
};

extern std::shared_ptr<Hydrogen::Application> GetApplication()
{
	return std::make_shared<EditorApp>();
}
