#include "Hydrogen/Scene/Components.hpp"
#include "Hydrogen/Application.hpp"

using namespace Hydrogen;

void GenericComponent::Serialize(json& j) const
{
	char* basePtr = (char*)this;
	for (const auto& field : GetReflectionFields())
	{
		char* fieldPtr = basePtr + field.Offset;

		switch (field.Type)
		{
		case FieldType::Float: j[field.Name] = *reinterpret_cast<float*>(fieldPtr); break;
		case FieldType::Int: j[field.Name] = *reinterpret_cast<int*>(fieldPtr); break;
		case FieldType::UInt64: j[field.Name] = *reinterpret_cast<uint64_t*>(fieldPtr); break;
		case FieldType::Bool: j[field.Name] = *reinterpret_cast<bool*>(fieldPtr); break;
		case FieldType::String: j[field.Name] = *reinterpret_cast<std::string*>(fieldPtr); break;

		case FieldType::Vec2: { glm::vec2 value = *reinterpret_cast<glm::vec2*>(fieldPtr); j[field.Name] = { value.x, value.y }; break; }
		case FieldType::Vec3: { glm::vec3 value = *reinterpret_cast<glm::vec3*>(fieldPtr); j[field.Name] = { value.x, value.y, value.z }; break; }
		case FieldType::Vec4: { glm::vec4 value = *reinterpret_cast<glm::vec4*>(fieldPtr); j[field.Name] = { value.x, value.y, value.z, value.w }; break; }
		case FieldType::Quaternion: { glm::quat value = *reinterpret_cast<glm::quat*>(fieldPtr); j[field.Name] = { value.x, value.y, value.z, value.w }; break; }

		case FieldType::Asset:
		{
			std::shared_ptr<Asset> value = *reinterpret_cast<std::shared_ptr<Asset>*>(fieldPtr);
			if (value)
				j[field.Name] = std::filesystem::path(value->GetPath()).filename().string();
			else
				j[field.Name] = "NULL";

			break;
		}
		}
	}
}

void GenericComponent::Deserialize(const json& j)
{
	char* basePtr = (char*)this;
	for (const auto& field : GetReflectionFields())
	{
		if (!j.contains(field.Name)) continue;

		char* fieldPtr = basePtr + field.Offset;

		switch (field.Type)
		{
		case FieldType::Float: *reinterpret_cast<float*>(fieldPtr) = j.value<float>(field.Name, 0.0f); break;
		case FieldType::Int: *reinterpret_cast<int*>(fieldPtr) = j.value<int>(field.Name, 0); break;
		case FieldType::UInt64: *reinterpret_cast<uint64_t*>(fieldPtr) = j.value<uint64_t>(field.Name, 0); break;
		case FieldType::Bool: *reinterpret_cast<bool*>(fieldPtr) = j.value<bool>(field.Name, false); break;
		case FieldType::String: *reinterpret_cast<std::string*>(fieldPtr) = j.value<std::string>(field.Name, ""); break;

		case FieldType::Vec2:
		{
			glm::vec2* castedFieldPtr = reinterpret_cast<glm::vec2*>(fieldPtr);
			if (j.contains(field.Name) && j[field.Name].is_array() && j[field.Name].size() >= 2)
			{
				castedFieldPtr->x = j[field.Name][0].get<float>();
				castedFieldPtr->y = j[field.Name][1].get<float>();
			}
			else
			{
				*castedFieldPtr = glm::vec2(0.0f);
			}
			break;
		}
		case FieldType::Vec3:
		{
			glm::vec3* castedFieldPtr = reinterpret_cast<glm::vec3*>(fieldPtr);
			if (j.contains(field.Name) && j[field.Name].is_array() && j[field.Name].size() >= 3)
			{
				castedFieldPtr->x = j[field.Name][0].get<float>();
				castedFieldPtr->y = j[field.Name][1].get<float>();
				castedFieldPtr->z = j[field.Name][2].get<float>();
			}
			else
			{
				*castedFieldPtr = glm::vec3(0.0f);
			}
			break;
		}
		case FieldType::Vec4:
		{
			glm::vec4* castedFieldPtr = reinterpret_cast<glm::vec4*>(fieldPtr);
			if (j.contains(field.Name) && j[field.Name].is_array() && j[field.Name].size() >= 4)
			{
				castedFieldPtr->x = j[field.Name][0].get<float>();
				castedFieldPtr->y = j[field.Name][1].get<float>();
				castedFieldPtr->z = j[field.Name][2].get<float>();
				castedFieldPtr->w = j[field.Name][3].get<float>();
			}
			else
			{
				*castedFieldPtr = glm::vec4(0.0f);
			}
			break;
		}
		case FieldType::Quaternion:
		{
			glm::quat* castedFieldPtr = reinterpret_cast<glm::quat*>(fieldPtr);
			if (j.contains(field.Name) && j[field.Name].is_array() && j[field.Name].size() >= 4)
			{
				castedFieldPtr->x = j[field.Name][0].get<float>();
				castedFieldPtr->y = j[field.Name][1].get<float>();
				castedFieldPtr->z = j[field.Name][2].get<float>();
				castedFieldPtr->w = j[field.Name][3].get<float>();
			}
			else
			{
				*castedFieldPtr = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
			}
			break;
		}

		case FieldType::Asset:
		{
			std::shared_ptr<Asset>* castedFieldPtr = reinterpret_cast<std::shared_ptr<Asset>*>(fieldPtr);
			const auto& path = j.value<std::string>(field.Name, "NULL");
			if (path != "NULL")
				*castedFieldPtr = Application::Get()->MainAssetManager.TryGetAsset(path);
			break;
		}
		}
	}
}

void ScriptsComponent::Serialize(json& j) const
{
	j["Scripts"] = json::array();
	for (const auto& scriptItem : Scripts)
	{
		if (scriptItem->Script)
		{
			std::string filename = std::filesystem::path(scriptItem->Script->GetPath()).filename().string();
			j["Scripts"].push_back(filename);
		}
	}
}

void ScriptsComponent::Deserialize(const json& j)
{
	Scripts.clear();

	if (j.contains("Scripts") && j["Scripts"].is_array())
	{
		for (const auto& scriptPathJson : j["Scripts"])
		{
			std::string scriptPath = scriptPathJson.get<std::string>();
			if (!scriptPath.empty())
			{
				auto script = std::make_unique<ScriptDesc>();
				script->Script = Application::Get()->MainAssetManager.TryGetAsset<ScriptAsset>(scriptPath);
				Scripts.push_back(std::move(script));
			}
		}
	}
}
