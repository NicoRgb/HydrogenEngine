#include "Panels/SceneHierarchyPanel.hpp"
#include <imgui.h>

using namespace Hydrogen;

Entity SceneHierarchyPanel::GetSelectedEntity() const
{
	Entity selectedEntity;
	if (!m_Scene)
	{
		return selectedEntity;
	}

	m_Scene->IterateComponents<UUIDComponent>([&](Entity entity, const UUIDComponent& uuid)
		{
			if (uuid.UUID == m_SelectedEntityUUID)
			{
				selectedEntity = entity;
			}
		});

	return selectedEntity;
}

void SceneHierarchyPanel::OnAttach()
{
	m_SelectedEntityUUID = 0;

	Dockspace->GetEventBus().Subscribe<SceneChangeEvent>([this](const SceneChangeEvent& e) {
		m_Scene = e.Scene;
		});
}

static bool CheckCycle(Hydrogen::Scene* scene, uint64_t entityUUID, uint64_t targetParentUUID)
{
	if (entityUUID == targetParentUUID)
		return true;

	uint64_t currentParentUUID = targetParentUUID;

	while (currentParentUUID != 0)
	{
		if (currentParentUUID == entityUUID)
			return true;

		Hydrogen::Entity parentEntity = scene->GetEntityByUUID(currentParentUUID);
		if (!parentEntity.IsValid())
			break;

		auto* rel = parentEntity.TryGetComponent<Hydrogen::RelationshipComponent>();
		if (!rel)
			break;

		currentParentUUID = rel->ParentUUID;
	}

	return false;
}

void DrawEntityNode(Entity entity, Scene* scene, uint64_t& selectedUUID)
{
	auto& tag = entity.GetComponent<TagComponent>();
	auto* rel = entity.TryGetComponent<RelationshipComponent>();

	bool selected = (selectedUUID == entity.GetUUID());

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (selected)
		flags |= ImGuiTreeNodeFlags_Selected;

	bool opened = ImGui::TreeNodeEx((void*)(uint64_t)entity.GetUUID(), flags, "%s", tag.Name.c_str());

	if (ImGui::IsItemClicked())
	{
		selectedUUID = entity.GetUUID();
	}

	if (ImGui::BeginDragDropSource())
	{
		uint64_t entityUUID = entity.GetUUID();
		ImGui::SetDragDropPayload("HIERARCHY_NODE", &entityUUID, sizeof(uint64_t));
		ImGui::Text("Moving %s", tag.Name.c_str());
		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_NODE"))
		{
			uint64_t droppedEntityUUID = *(const uint64_t*)payload->Data;

			Entity droppedEntity = scene->GetEntityByUUID(droppedEntityUUID);

			if (droppedEntity.IsValid() && droppedEntity.GetUUID() != entity.GetUUID() && !CheckCycle(scene, droppedEntityUUID, entity.GetUUID()))
			{
				auto& droppedRel = droppedEntity.GetComponent<RelationshipComponent>();
				droppedRel.ParentUUID = entity.GetUUID();

				// TODO: If you implement full local-to-world matrix recomputations, trigger them here
			}
		}
		ImGui::EndDragDropTarget();
	}

	if (opened)
	{
		scene->IterateComponents<RelationshipComponent>([&](Entity childEntity, const RelationshipComponent& childRel)
			{
				if (childRel.ParentUUID == entity.GetUUID())
				{
					DrawEntityNode(childEntity, scene, selectedUUID);
				}
			});

		ImGui::TreePop();
	}
}

void DrawSceneHierarchyPanel(Scene* scene, uint64_t& m_SelectedEntityUUID)
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
	if (ImGui::TreeNodeEx("Scene", flags))
	{
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_NODE"))
			{
				uint64_t droppedEntityUUID = *(const uint64_t*)payload->Data;
				Hydrogen::Entity droppedEntity = scene->GetEntityByUUID(droppedEntityUUID);

				if (droppedEntity.IsValid())
				{
					auto& droppedRel = droppedEntity.GetComponent<Hydrogen::RelationshipComponent>();
					droppedRel.ParentUUID = 0;
				}
			}
			ImGui::EndDragDropTarget();
		}

		scene->IterateComponents<Hydrogen::TagComponent>([&](Hydrogen::Entity entity, const auto& tag)
			{
				auto* rel = entity.TryGetComponent<Hydrogen::RelationshipComponent>();
				bool isRoot = (rel == nullptr || rel->ParentUUID == 0);

				if (isRoot)
				{
					DrawEntityNode(entity, scene, m_SelectedEntityUUID);
				}
			});

		ImGui::TreePop();
	}
}

void SceneHierarchyPanel::OnImGuiRender()
{
	if (!m_Scene)
	{
		return;
	}

	 if (ImGui::Button("Add Entity"))
	 {
		 Entity(m_Scene, "New Entity");
	 }

	 ImGui::Separator();

	 DrawSceneHierarchyPanel(m_Scene, m_SelectedEntityUUID);
}
