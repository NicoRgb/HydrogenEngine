#pragma once

#include "Scene.hpp"
#include "Physics.hpp"
#include "Hydrogen/AssetManager.hpp"

#define BEGIN_COMPONENT_REFLECTION(ClassName) \
	const std::vector<FieldInfo>& GetReflectionFields() const override { \
		using TClass = ClassName; \
		static const std::vector<FieldInfo> fields = {

#define REFLECT_MEMBER(MemberName) \
		{ \
			#MemberName, \
			GetFieldType<decltype(TClass::MemberName)>(), \
			offsetof(TClass, MemberName) \
		},

#define END_COMPONENT_REFLECTION() \
		}; \
		return fields; \
	}

#define REGISTER_COMPONENT(ComponentType, ComponentNameString) \
		static ComponentRegistrar<ComponentType> s_Registrar##ComponentType(ComponentNameString);

namespace Hydrogen
{
	enum class FieldType { Float, Int, UInt64, Bool, String, Vec2, Vec3, Vec4, Quaternion, Asset };

	template <typename T>
	constexpr FieldType GetFieldType()
	{
		if constexpr (std::is_same_v<T, float>) return FieldType::Float;
		else if constexpr (std::is_same_v<T, int>) return FieldType::Int;
		else if constexpr (std::is_same_v<T, uint64_t>) return FieldType::UInt64;
		else if constexpr (std::is_same_v<T, bool>) return FieldType::Bool;
		else if constexpr (std::is_same_v<T, std::string>) return FieldType::String;
		else if constexpr (std::is_same_v<T, glm::vec2>) return FieldType::Vec2;
		else if constexpr (std::is_same_v<T, glm::vec3>) return FieldType::Vec3;
		else if constexpr (std::is_same_v<T, glm::vec4>) return FieldType::Vec4;
		else if constexpr (std::is_same_v<T, glm::quat>) return FieldType::Quaternion;
		else if constexpr (is_asset_pointer<T>::value) return FieldType::Asset;
		else static_assert(false, "Invalid field type");
	}

	struct FieldInfo
	{
		std::string Name;
		FieldType Type;
		size_t Offset;
	};

	class GenericComponent
	{
	public:
		GenericComponent(Entity entity)
			: m_Entity(entity)
		{
		}

		Entity& GetEntity() { return m_Entity; }

		virtual const std::vector<FieldInfo>& GetReflectionFields() const = 0;

		virtual void Serialize(json& j) const;
		virtual void Deserialize(const json& j);

	private:
		Entity m_Entity;
	};

	class ComponentRegistry
	{
	public:
		struct ComponentHandlers
		{
			std::function<void(json& j, Entity entity)> Serialize;
			std::function<void(const json& j, Entity entity)> Deserialize;
		};

		static ComponentRegistry& Get()
		{
			static ComponentRegistry instance;
			return instance;
		}

		void Register(const std::string& name, ComponentHandlers handlers)
		{
			m_Registry[name] = handlers;
		}

		const std::unordered_map<std::string, ComponentHandlers>& GetAllHandlers() const
		{
			return m_Registry;
		}

	private:
		std::unordered_map<std::string, ComponentHandlers> m_Registry;
	};

	template <typename T>
	struct ComponentRegistrar
	{
		ComponentRegistrar(const char* componentName)
		{
			ComponentRegistry::Get().Register(componentName, {
				[componentName](json& j, Entity entity) {
					if (!entity.HasComponent<T>())
						return;
					auto& comp = entity.GetComponent<T>();
					comp.Serialize(j);
				},
				[componentName](const json& j, Entity entity) {
					auto& comp = entity.GetOrAddComponent<T>();
					comp.Deserialize(j);
				}
				});
		}
	};

	struct TagComponent : public GenericComponent
	{
		TagComponent(Entity entity)
			: GenericComponent(entity)
		{
			Name = "";
		}

		std::string Name;

		BEGIN_COMPONENT_REFLECTION(TagComponent)
			REFLECT_MEMBER(Name)
		END_COMPONENT_REFLECTION()
	};
	REGISTER_COMPONENT(TagComponent, "TagComponent")

	struct RelationshipComponent : public GenericComponent
	{
		RelationshipComponent(Entity entity)
			: GenericComponent(entity)
		{
			ParentUUID = 0;
		}

		uint64_t ParentUUID;

		static void ToJson(json& j, const RelationshipComponent& t)
		{
			j["parent"] = t.ParentUUID;
		}

		static void FromJson(const json& j, RelationshipComponent& t, AssetManager* assetManager)
		{
			t.ParentUUID = j.value("parent", 0);
		}

		BEGIN_COMPONENT_REFLECTION(RelationshipComponent)
			REFLECT_MEMBER(ParentUUID)
		END_COMPONENT_REFLECTION()
	};
	REGISTER_COMPONENT(RelationshipComponent, "RelationshipComponent")

	struct TransformComponent : public GenericComponent
	{
		TransformComponent(Entity entity)
			: GenericComponent(entity)
		{
			Translation = glm::vec3(0.0f, 0.0f, 0.0f);
			Rotation = glm::quat(0.0f, 0.0f, 0.0f, 0.0f);
			Scale = glm::vec3(1.0f, 1.0f, 1.0f);

			ModelCache = glm::mat4(1.0f);
			Dirty = true;
		}

		const glm::mat4& GetModel()
		{
			if (Dirty)
			{
				Dirty = false;

				ModelCache = glm::mat4(1.0f);
				ModelCache = glm::translate(ModelCache, Translation);
				ModelCache = ModelCache * glm::mat4_cast(Rotation);
				ModelCache = glm::scale(ModelCache, Scale);
			}

			return ModelCache;
		}

		glm::vec3 GetTranslation() const
		{
			return Translation;
		}

		void SetTranslation(const glm::vec3& newTranslation)
		{
			Translation = newTranslation;
			Dirty = true;
		}

		glm::quat GetRotation() const
		{
			return Rotation;
		}

		void SetRotation(const glm::quat& newRotation)
		{
			Rotation = newRotation;
			Dirty = true;
		}

		glm::vec3 GetScale() const
		{
			return Scale;
		}

		void SetScale(const glm::vec3& newScale)
		{
			Scale = newScale;
			Dirty = true;
		}

		glm::vec3 Translation;
		glm::quat Rotation;
		glm::vec3 Scale;

		BEGIN_COMPONENT_REFLECTION(TransformComponent)
			REFLECT_MEMBER(Translation)
			REFLECT_MEMBER(Rotation)
			REFLECT_MEMBER(Scale)
		END_COMPONENT_REFLECTION()

	private:
		bool Dirty = true;
		glm::mat4 ModelCache;
	};
	REGISTER_COMPONENT(TransformComponent, "TransformComponent")

	struct MeshRendererComponent : public GenericComponent
	{
		MeshRendererComponent(Entity entity)
			: GenericComponent(entity)
		{
			Mesh = nullptr;
			Material = nullptr;
		}

		std::shared_ptr<StaticMeshAsset> Mesh;
		std::shared_ptr<MaterialAsset> Material;

		BEGIN_COMPONENT_REFLECTION(MeshRendererComponent)
			REFLECT_MEMBER(Mesh)
			REFLECT_MEMBER(Material)
		END_COMPONENT_REFLECTION()
	};
	REGISTER_COMPONENT(MeshRendererComponent, "MeshRendererComponent")

	struct DirectionalLightComponent : public GenericComponent
	{
		DirectionalLightComponent(Entity entity)
			: GenericComponent(entity)
		{
			Color = glm::vec3(1.0f);
			Intensity = 10.0f;
		}

		glm::vec3 Color;
		float Intensity;

		BEGIN_COMPONENT_REFLECTION(DirectionalLightComponent)
			REFLECT_MEMBER(Color)
			REFLECT_MEMBER(Intensity)
		END_COMPONENT_REFLECTION()
	};
	REGISTER_COMPONENT(DirectionalLightComponent, "DirectionalLightComponent")

	struct PointLightComponent : public GenericComponent
	{
		PointLightComponent(Entity entity)
			: GenericComponent(entity)
		{
			Color = glm::vec3(1.0f);
			Intensity = 10.0f;
			Radius = 10.0f;
		}

		glm::vec3 Color;
		float Intensity;
		float Radius;

		BEGIN_COMPONENT_REFLECTION(PointLightComponent)
			REFLECT_MEMBER(Color)
			REFLECT_MEMBER(Intensity)
			REFLECT_MEMBER(Radius)
		END_COMPONENT_REFLECTION()
	};
	REGISTER_COMPONENT(PointLightComponent, "PointLightComponent")

	struct ScriptDesc
	{
		ScriptDesc() = default;

		std::shared_ptr<ScriptAsset> Script = nullptr;
		std::unique_ptr<ScriptInstance> Instance = nullptr;
		std::vector<ScriptFieldMetadata> ExposedFields;
	};

	struct ScriptsComponent : public GenericComponent
	{
		ScriptsComponent(Entity entity) : GenericComponent(entity)
		{
		}

		ScriptsComponent(const ScriptsComponent&) = delete;
		ScriptsComponent& operator=(const ScriptsComponent&) = delete;

		std::vector<std::unique_ptr<ScriptDesc>> Scripts;

		// empty because of custom serializer
		BEGIN_COMPONENT_REFLECTION(GenericComponent)
		END_COMPONENT_REFLECTION()

		virtual void Serialize(json& j) const override;
		virtual void Deserialize(const json& j) override;
	};
	REGISTER_COMPONENT(ScriptsComponent, "ScriptsComponent")

	struct RigidbodyComponent : public GenericComponent
	{
		reactphysics3d::RigidBody* Rigidbody = nullptr;

		int Type = (int)reactphysics3d::BodyType::STATIC; // reactphysics3d::BodyType
		float Mass = 1.0f;
		float LinearDamping = 0.0f;
		float AngularDamping = 0.0f;
		bool UseGravity = true;

		bool LockLinearX = false;
		bool LockLinearY = false;
		bool LockLinearZ = false;

		bool LockAngularX = false;
		bool LockAngularY = false;
		bool LockAngularZ = false;

		// in Physics.cpp
		RigidbodyComponent(Entity entity);

		void ApplyRotationLock();

		void SetType(reactphysics3d::BodyType type);
		void SetMass(float mass);
		void SetUseGravity(bool useGravity);

		void ApplyForce(glm::vec3 force);
		void ApplyTorque(glm::vec3 torque);
		void SetLinearVelocity(glm::vec3 velocity);
		void SetAngularVelocity(glm::vec3 velocity);

		virtual void Deserialize(const json& j) override;

		BEGIN_COMPONENT_REFLECTION(RigidbodyComponent)
			REFLECT_MEMBER(Type)
			REFLECT_MEMBER(Mass)
			REFLECT_MEMBER(LinearDamping)
			REFLECT_MEMBER(AngularDamping)
			REFLECT_MEMBER(UseGravity)
			REFLECT_MEMBER(LockLinearX)
			REFLECT_MEMBER(LockLinearY)
			REFLECT_MEMBER(LockLinearZ)
			REFLECT_MEMBER(LockAngularX)
			REFLECT_MEMBER(LockAngularY)
			REFLECT_MEMBER(LockAngularZ)
		END_COMPONENT_REFLECTION()
	};
	REGISTER_COMPONENT(RigidbodyComponent, "RigidbodyComponent")

	struct ColliderComponent : public GenericComponent
	{
		enum class Type
		{
			Box,
			Sphere,
			Capsule
		};

		RigidbodyComponent* Rigidbody = nullptr;
		reactphysics3d::Collider* Collider = nullptr;

		int ColliderType = (int)Type::Box;

		glm::vec3 Size = glm::vec3(1.0f);
		float Radius = 0.5f;
		float Height = 1.0f;

		glm::vec3 LocalPosition = glm::vec3(0.0f);
		glm::quat LocalRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

		// in Physics.cpp
		ColliderComponent(Entity entity);

		void DestroyCollider();
		void CreateCollider();

		virtual void Deserialize(const json& j) override;

		BEGIN_COMPONENT_REFLECTION(ColliderComponent)
			REFLECT_MEMBER(ColliderType)
			REFLECT_MEMBER(Size)
			REFLECT_MEMBER(Radius)
			REFLECT_MEMBER(Height)
			REFLECT_MEMBER(LocalPosition)
			REFLECT_MEMBER(LocalRotation)
		END_COMPONENT_REFLECTION()
	};
	REGISTER_COMPONENT(ColliderComponent, "ColliderComponent")
}
