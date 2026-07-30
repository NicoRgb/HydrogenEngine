#include <Hydrogen/Scripting/ScriptBindings.hpp>
#include <fstream>

namespace Hydrogen
{
	void SolScriptBackend::Build(const ScriptDatabase& database)
	{
		auto dummyTable = m_Lua.create_table();
		m_Lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::table, sol::lib::string);

		for (const auto& scriptEnum : database.Enums)
		{
			sol::table enumTable = m_Lua.create_named_table(scriptEnum.Name);
			for (const auto& [name, val] : scriptEnum.Values)
			{
				enumTable[name] = val;
			}
		}

		for (const auto& scriptClass : database.Classes)
		{
			if (!scriptClass.Binding)
				continue;

			sol::table usertype = scriptClass.Binding->BindToSol(m_Lua, dummyTable);

			for (const auto& method : scriptClass.Methods)
			{
				if (method.Binding) method.Binding->BindToSol(m_Lua, usertype);
			}

			for (const auto& field : scriptClass.Fields)
			{
				if (field.Binding) field.Binding->BindToSol(m_Lua, usertype);
			}

			for (const auto& prop : scriptClass.Properties)
			{
				if (prop.Binding) prop.Binding->BindToSol(m_Lua, usertype);
			}

			for (const auto& op : scriptClass.Operators)
			{
				if (op.Binding) op.Binding->BindToSol(m_Lua, usertype);
			}
		}

		for (const auto& ns : database.Namespaces)
		{
			sol::table nsTable = m_Lua.create_named_table(ns.Name);
			for (const auto& func : ns.Functions)
			{
				if (func.Binding) func.Binding->BindToSol(m_Lua, nsTable);
			}
		}

		for (const auto& global : database.Globals)
		{
			if (global.Binding)
			{
				sol::table globalsTable = m_Lua.globals();
				global.Binding->BindToSol(m_Lua, globalsTable);
			}
		}
	}

	void StubBackend::Build(const ScriptDatabase& database)
	{
		std::ofstream stream(m_OutputPath);
		if (!stream.is_open())
			return;

		stream << "-- ==========================================\n";
		stream << "-- AUTO-GENERATED HYDROGEN ENGINE LUA STUBS\n";
		stream << "-- ==========================================\n\n";

		for (const auto& scriptEnum : database.Enums)
		{
			stream << "---@class " << scriptEnum.Name << "\n";
			for (const auto& [name, val] : scriptEnum.Values)
			{
				stream << "---@field " << name << " integer\n";
			}
			stream << scriptEnum.Name << " = {}\n\n";
		}

		for (const auto& ns : database.Namespaces)
		{
			stream << "---@class " << ns.Name << "\n";
			stream << ns.Name << " = {}\n\n";

			for (const auto& func : ns.Functions)
			{
				if (!func.Documentation.empty())
				{
					stream << "--- " << func.Documentation << "\n";
				}
				stream << "function " << ns.Name << "." << func.Name << func.Signature << " end\n";
			}
			stream << "\n";
		}

		for (const auto& scriptClass : database.Classes)
		{
			stream << "---@class " << scriptClass.Name;
			if (!scriptClass.BaseClass.empty())
			{
				stream << " : " << scriptClass.BaseClass;
			}
			stream << "\n";

			for (const auto& prop : scriptClass.Properties)
			{
				stream << "---@field " << prop.Name << " " << prop.TypeName << "\n";
			}

			for (const auto& field : scriptClass.Fields)
			{
				stream << "---@field " << field.Name << " " << field.TypeName << "\n";
			}

			if (!scriptClass.Documentation.empty())
			{
				stream << "--- " << scriptClass.Documentation << "\n";
			}

			stream << scriptClass.Name << " = {}\n\n";

			for (const auto& ctor : scriptClass.Constructors)
			{
				stream << "function " << scriptClass.Name << ".new" << ctor.Signature << " return " << scriptClass.Name << " end\n";
			}

			for (const auto& method : scriptClass.Methods)
			{
				if (!method.Documentation.empty())
				{
					stream << "--- " << method.Documentation << "\n";
				}

				std::string separator = method.Static ? "." : ":";
				stream << "function " << scriptClass.Name << separator << method.Name << method.Signature << " end\n";
			}

			stream << "\n";
		}

		for (const auto& global : database.Globals)
		{
			if (!global.Documentation.empty())
			{
				stream << "--- " << global.Documentation << "\n";
			}
			stream << "function " << global.Name << global.Signature << " end\n";
		}

		stream.close();
	}
}
