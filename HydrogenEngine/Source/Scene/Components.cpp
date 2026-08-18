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
		if (!scriptItem->Script)
			continue;

		json scriptJson = json::object();

		std::string filename = std::filesystem::path(scriptItem->Script->GetPath()).filename().string();
		scriptJson["File"] = filename;

		scriptJson["Fields"] = json::array();
		for (const auto& field : scriptItem->ExposedFields)
		{
			json fieldJson = json::object();
			fieldJson["Name"] = field.Name;
			fieldJson["Type"] = static_cast<int>(field.Type);

			std::visit([&fieldJson](auto&& arg) {
				using T = std::decay_t<decltype(arg)>;
				if constexpr (std::is_same_v<T, Entity>)
				{
					fieldJson["Value"] = arg.IsValid() ? arg.GetUUID() : 0;
				}
				else
				{
					fieldJson["Value"] = arg;
				}
				}, field.Value);

			scriptJson["Fields"].push_back(fieldJson);
		}

		j["Scripts"].push_back(scriptJson);
	}
}

void ScriptsComponent::Deserialize(const json& j)
{
}

void ScriptsComponent::PostDeserialize(const json& j)
{
	Scripts.clear();

	if (!j.contains("Scripts") || !j["Scripts"].is_array())
		return;

	for (const auto& scriptJson : j["Scripts"])
	{
		std::string scriptPath = "";

		if (scriptJson.is_string())
		{
			scriptPath = scriptJson.get<std::string>();
		}
		else if (scriptJson.contains("File") && scriptJson["File"].is_string())
		{
			scriptPath = scriptJson["File"].get<std::string>();
		}

		if (scriptPath.empty())
			continue;

		auto script = std::make_unique<ScriptContainer>();
		script->Script = Application::Get()->MainAssetManager.TryGetAsset<ScriptAsset>(scriptPath);

		if (scriptJson.contains("Fields") && scriptJson["Fields"].is_array())
		{
			for (const auto& fieldJson : scriptJson["Fields"])
			{
				ScriptFieldMetadata field;
				field.Name = fieldJson["Name"].get<std::string>();
				field.Type = static_cast<ScriptFieldType>(fieldJson["Type"].get<int>());

				switch (field.Type)
				{
				case ScriptFieldType::Float:
					field.Value = fieldJson["Value"].get<double>();
					break;
				case ScriptFieldType::Int:
					field.Value = fieldJson["Value"].get<int64_t>();
					break;
				case ScriptFieldType::Bool:
					field.Value = fieldJson["Value"].get<bool>();
					break;
				case ScriptFieldType::String:
					field.Value = fieldJson["Value"].get<std::string>();
					break;
				case ScriptFieldType::Entity:
				{
					uint64_t uuid = fieldJson["Value"].get<uint64_t>();
					field.Value = GetEntity().GetScene()->GetEntityByUUID(uuid);
					break;
				}
				default:
					break;
				}

				script->ExposedFields.push_back(field);
			}
		}

		Scripts.push_back(std::move(script));
	}
}
