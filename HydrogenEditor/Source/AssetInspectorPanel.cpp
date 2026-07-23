#include "AssetInspectorPanel.hpp"

using namespace Hydrogen;

extern ImGuiTextureCache TextureCache;

void AssetInspectorPanel::OnAttach()
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

	Dockspace->GetEventBus().Subscribe<AssetSelectedEvent>([this](const AssetSelectedEvent& e) {
		m_CurrentFile = e.AssetPath;
	});
}

void AssetInspectorPanel::OnDetach()
{
	m_MaterialPreviewRenderer.reset();
	m_MaterialPreviewScene.reset();
}

void AssetInspectorPanel::OnImGuiRender()
{
	if (m_CurrentFile.empty())
	{
		ImGui::Text("No asset selected");
	}
	else
	{
		std::ifstream fin(m_CurrentFile.string() + ".hyasset");
		if (!fin)
		{
			//Application::Get()->MainAssetManager.LoadAssets("assets");
			//fin = std::ifstream(m_CurrentFile.string() + ".hyasset");
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
}

void AssetInspectorPanel::DrawFileConfig(std::filesystem::path path, json& j)
{
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
			renderSettings.Display.RenderToSwapChain = false;

			VkImageView finalImage =
				DefaultRenderer::RenderSceneDeferred(m_MaterialPreviewRenderer.get(), renderSettings, activeCameraEntity.GetComponent<CameraComponent>(),
					activeCameraEntity.GetComponent<TransformComponent>().GetPosition(), m_MaterialPreviewScene->GetScene()).ImageView;

			ImGui::Image(TextureCache.GetTextureID(finalImage, m_MaterialPreviewRenderer->GetImguiSampler()), { 256, 256 });
		}

		ImGui::TreePop();
	}
}
