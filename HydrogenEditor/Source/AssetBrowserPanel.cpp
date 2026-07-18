#include <Hydrogen/Hydrogen.hpp>
#include <imgui.h>
#include <fstream>

#include "AssetBrowserPanel.hpp"

using json = nlohmann::json;
using namespace Hydrogen;

extern ImGuiTextureCache TextureCache;

AssetBrowserPanel::AssetBrowserPanel(const std::filesystem::path& assetDir, AssetEditorPanel& editor)
	: m_EditorPanel(editor),
	m_AssetDirectory(assetDir),
	m_CurrentDirectory(assetDir),
	m_CurrentFile("")
{
}

void AssetBrowserPanel::Setup()
{
	m_MaterialPreviewRenderer = std::make_unique<Renderer>(Application::Get()->GetRenderDevice());

	m_MaterialPreviewScene = Application::Get()->MainAssetManager.GetAsset<SceneAsset>("MaterialPreview.hyscene");
	m_MaterialPreviewScene->Load(&Application::Get()->MainAssetManager);

	m_MaterialPreviewScene->GetScene()->IterateComponents<CameraComponent>(
		[&](Entity entity, CameraComponent& camera)
		{
			if (camera.Active)
			{
				camera.CalculateView(entity);
				camera.ViewportWidth = 256;
				camera.ViewportHeight = 256;
				camera.CalculateProj();
			}
		});
}

void AssetBrowserPanel::Shutdown()
{
	m_MaterialPreviewRenderer.reset();
	m_MaterialPreviewScene.reset();
}

void AssetBrowserPanel::LoadTextures(const Texture* folderTex, const Texture* fileTex)
{
	m_FolderTexture = folderTex;
	m_FileTexture = fileTex;
}

void AssetBrowserPanel::DrawFileConfig(std::filesystem::path path, json& j) {
	std::string assetType = j["type"].get<std::string>();
	ImGui::Text("%s", j["name"].get<std::string>().c_str());
	ImGui::Indent();
	ImGui::Text("Type: %s", assetType.c_str());
	ImGui::Unindent();

	if (ImGui::TreeNode("Preferences"))
	{
		if (assetType == "Shader")
		{
			static const char* shaderStages[] =
			{
				"vertex",
				"fragment",
			};

			std::string stage = j["preferences"].value("stage", "vertex");
			int currentIndex = 0;
			for (int i = 0; i < IM_ARRAYSIZE(shaderStages); ++i)
			{
				if (stage == shaderStages[i])
				{
					currentIndex = i;
					break;
				}
			}

			if (ImGui::BeginCombo("Shader Stage", shaderStages[currentIndex]))
			{
				for (int i = 0; i < IM_ARRAYSIZE(shaderStages); ++i)
				{
					bool isSelected = (currentIndex == i);
					if (ImGui::Selectable(shaderStages[i], isSelected))
					{
						if (stage != shaderStages[i])
						{
							stage = shaderStages[i];
							j["preferences"]["stage"] = stage;

							if (m_CurrentFile.has_filename())
							{
								std::filesystem::path assetFile = m_CurrentFile.string() + ".hyasset";
								std::ofstream fout(assetFile);
								if (fout.is_open())
								{
									fout << j.dump(4);
									fout.close();
									Application::Get()->MainAssetManager.LoadAssets("assets");
								}
							}
						}
						currentIndex = i;
					}
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}
		else if (assetType == "Material")
		{
			auto materialAsset = Application::Get()->MainAssetManager.GetAsset<MaterialAsset>(path.filename().string());

			m_MaterialPreviewScene->GetScene()->IterateComponents<MeshRendererComponent>(
				[&](Entity entity, MeshRendererComponent& mesh)
				{
					mesh.Material = materialAsset;
				});

			float metallic = materialAsset->GetMetallicFactor();
			if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f))
				materialAsset->SetMetallicFactor(metallic);

			float roughness = materialAsset->GetRoughnessFactor();
			if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f))
				materialAsset->SetRoughnessFactor(roughness);

			glm::vec3 tint = materialAsset->GetTint();
			if (ImGui::ColorPicker3("Tint", glm::value_ptr(tint)))
				materialAsset->SetTint(tint);

			glm::vec3 emissive = materialAsset->GetEmissive();
			float emissiveStrength = materialAsset->GetEmissive().a;
			if (ImGui::ColorPicker3("Emissive", glm::value_ptr(emissive)))
				materialAsset->SetEmissive(glm::vec4(emissive, emissiveStrength));

			if (ImGui::SliderFloat("Emissive Strength", &emissiveStrength, 0.0f, 10.0f))
				materialAsset->SetEmissive(glm::vec4(emissive, emissiveStrength));

			auto albedo = materialAsset->GetAlbedoMap();
			auto normal = materialAsset->GetNormalMap();
			auto orm = materialAsset->GetORMMap();

			ImGui::Text("Albedo Map");
			if (albedo)
			{
				ImGui::Text(albedo->GetPath().c_str());
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
					auto asset = Application::Get()->MainAssetManager.GetAsset<TextureAsset>(newPath.filename().string());
					if (asset)
					{
						materialAsset->SetAlbedoMap(asset);
					}
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::Text("Normal Map");
			if (normal)
			{
				ImGui::Text(normal->GetPath().c_str());
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
					auto asset = Application::Get()->MainAssetManager.GetAsset<TextureAsset>(newPath.filename().string());
					if (asset)
					{
						materialAsset->SetNormalMap(asset);
					}
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::Text("ORM Map");
			if (orm)
			{
				ImGui::Text(orm->GetPath().c_str());
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
					auto asset = Application::Get()->MainAssetManager.GetAsset<TextureAsset>(newPath.filename().string());
					if (asset)
					{
						materialAsset->SetORMMap(asset);
					}
				}
				ImGui::EndDragDropTarget();
			}

			materialAsset->Save();
		}

		ImGui::Separator();

		Entity activeCameraEntity;
		m_MaterialPreviewScene->GetScene()->IterateComponents<CameraComponent>(
			[&](Entity entity, CameraComponent& camera)
			{
				if (camera.Active)
					activeCameraEntity = entity;
			});

		if (activeCameraEntity.IsValid())
		{
			RenderSettings renderSettings;
			renderSettings.Display.Width = 256;
			renderSettings.Display.Height = 256;

			VkImageView finalImage =
				DefaultRenderer::RenderSceneDeferred(m_MaterialPreviewRenderer.get(), renderSettings, activeCameraEntity.GetComponent<CameraComponent>(),
					activeCameraEntity.GetComponent<TransformComponent>().GetPosition(), m_MaterialPreviewScene->GetScene()).ImageView;

			ImGui::Image(TextureCache.GetTextureID(finalImage, m_MaterialPreviewRenderer->GetImguiSampler()), {256, 256});
		}

		ImGui::TreePop();
	}
}

void AssetBrowserPanel::OnImGuiRender()
{
	ImGui::Begin(GetName());

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
			continue;

		std::string filename = path.filename().string();

		ImGui::PushID(filename.c_str());
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));

		if (entry.is_directory())
		{
			ImGui::ImageButton("[DIR]", TextureCache.GetTextureID(m_FolderTexture->GetImageView(), m_MaterialPreviewRenderer->GetImguiSampler()), { thumbnailSize, thumbnailSize });
		}
		else
		{
			ImGui::ImageButton("[FILE]", TextureCache.GetTextureID(m_FileTexture->GetImageView(), m_MaterialPreviewRenderer->GetImguiSampler()), { thumbnailSize, thumbnailSize });

			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
			{
				std::string pathStr = path.string();
				ImGui::SetDragDropPayload("ASSET_FILE", pathStr.c_str(), pathStr.size() + 1);
				ImGui::Text("%s", filename.c_str());
				ImGui::EndDragDropSource();
			}
		}

		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			if (entry.is_directory())
			{
				m_CurrentDirectory /= path.filename();
			}
			else
			{
				std::string ext = path.extension().string();
				if (ext == ".json" || ext == ".shader" || ext == ".glsl" || ext == ".lua" || ext == ".txt")
					m_EditorPanel.OpenFile(path);
			}
		}

		if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !entry.is_directory())
		{
			m_CurrentFile = path;
		}

		ImGui::PopStyleColor();
		ImGui::TextWrapped("%s", filename.c_str());
		ImGui::NextColumn();
		ImGui::PopID();
	}

	ImGui::Columns(1);

	if (ImGui::BeginPopupContextWindow())
	{
		if (ImGui::MenuItem("Create Folder"))
		{
			showCreateFolderDialog = true;
			inputName[0] = '\0';
		}

		if (ImGui::MenuItem("Create File"))
		{
			showCreateFileDialog = true;
			inputName[0] = '\0';
		}
		ImGui::EndPopup();
	}

	// Folder creation dialog
	if (showCreateFolderDialog)
	{
		ImGui::OpenPopup("Create Folder");
		showCreateFolderDialog = false;
	}
	if (ImGui::BeginPopupModal("Create Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputText("Folder Name", inputName, sizeof(inputName));
		if (ImGui::Button("Create"))
		{
			if (strlen(inputName) > 0) {
				std::filesystem::path folderPath = m_CurrentDirectory / inputName;
				std::error_code ec;
				std::filesystem::create_directory(folderPath, ec);
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}

	if (showCreateFileDialog)
	{
		ImGui::OpenPopup("Create File");
		showCreateFileDialog = false;
	}
	if (ImGui::BeginPopupModal("Create File", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputText("File Name", inputName, sizeof(inputName));
		if (ImGui::Button("Create"))
		{
			if (strlen(inputName) > 0) {
				std::filesystem::path filePath = m_CurrentDirectory / inputName;
				std::ofstream file(filePath);
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}

	ImGui::End();

	ImGui::Begin("Asset Inspector");
	if (m_CurrentFile.empty())
	{
		ImGui::Text("No asset selected");
	}
	else
	{
		std::ifstream fin(m_CurrentFile.string() + ".hyasset");
		if (!fin)
		{
			Application::Get()->MainAssetManager.LoadAssets("assets");
			fin = std::ifstream(m_CurrentFile.string() + ".hyasset");
		}

		if (fin.is_open())
		{
			auto config = json::parse(fin);
			fin.close();
			DrawFileConfig(m_CurrentFile, config);
		}
		else
		{
			ImGui::Text("Failed to open asset metadata file");
		}
	}
	ImGui::End();
}
