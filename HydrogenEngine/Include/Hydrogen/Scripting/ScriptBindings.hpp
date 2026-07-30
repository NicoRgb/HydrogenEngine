#pragma once

#include <string>
#include <typeindex>
#include <vector>
#include <memory>
#include <cstdint>

#include <sol/sol.hpp>

namespace Hydrogen
{
	class IScriptBackend
	{
	public:
		virtual ~IScriptBackend() = default;
		virtual void Build(const struct ScriptDatabase& database) = 0;
	};

	enum class BindingType
	{
		Class,
		Method,
		Constructor,
		Function,
		Field,
		Property,
		ReadOnlyProperty,
		Operator
	};

	enum class ScriptOperator
	{
		Add,
		Subtract,
		Multiply,
		Divide,
		Equal,
		NotEqual,
		Less,
		LessEqual,
		Greater,
		GreaterEqual,
		UnaryMinus,
		ToString,
		Length,
		Index
	};

	struct IScriptBinding
	{
		virtual ~IScriptBinding() = default;
		virtual BindingType Type() const = 0;
		virtual sol::table BindToSol(sol::state_view& state, sol::table& table) = 0;
	};

	struct ScriptMethod
	{
		std::string Name;
		std::string Signature;
		std::string Documentation;
		bool Static = false;
		std::shared_ptr<IScriptBinding> Binding;
	};

	struct ScriptField
	{
		std::string Name;
		std::string TypeName;
		std::string Documentation;
		std::shared_ptr<IScriptBinding> Binding;
	};

	struct ScriptProperty
	{
		std::string Name;
		std::string TypeName;
		std::string Documentation;
		bool ReadOnly = false;
		std::shared_ptr<IScriptBinding> Binding;
	};

	struct ScriptConstructor
	{
		std::string Signature;
		std::shared_ptr<IScriptBinding> Binding;
	};

	struct ScriptOperatorInfo
	{
		ScriptOperator Operator;
		std::string Documentation;
		std::shared_ptr<IScriptBinding> Binding;
	};

	struct ScriptClass
	{
		std::string Name;
		std::string ScriptTypeName;
		std::string Documentation;
		std::string BaseClass;

		std::vector<ScriptConstructor> Constructors;
		std::vector<ScriptMethod> Methods;
		std::vector<ScriptField> Fields;
		std::vector<ScriptProperty> Properties;
		std::vector<ScriptOperatorInfo> Operators;

		std::shared_ptr<IScriptBinding> Binding;
	};

	struct ScriptEnum
	{
		std::string Name;
		std::vector<std::pair<std::string, int64_t>> Values;
	};

	struct ScriptFunction
	{
		std::string Name;
		std::string Signature;
		std::string Documentation;
		std::shared_ptr<IScriptBinding> Binding;
	};

	struct ScriptNamespace
	{
		std::string Name;
		std::vector<ScriptFunction> Functions;
	};

	struct ScriptDatabase
	{
		std::vector<ScriptClass> Classes;
		std::vector<ScriptEnum> Enums;
		std::vector<ScriptNamespace> Namespaces;
		std::vector<ScriptFunction> Globals;
	};

	class SolScriptBackend : public IScriptBackend
	{
	public:
		void Build(const ScriptDatabase& database) override;

	private:
		sol::state m_Lua;
	};

	class StubBackend : public IScriptBackend
	{
	public:
		StubBackend(std::string outputPath)
			: m_OutputPath(std::move(outputPath))
		{
		}

		void Build(const ScriptDatabase& database) override;

	private:
		std::string m_OutputPath;
	};

	// ------------------------------------------------------------
	// Bindings (Self-contained with Names and Return Tables)
	// ------------------------------------------------------------

	template<typename T, typename... CtorArgs>
	struct ClassBinding : IScriptBinding
	{
		std::string ClassName;

		explicit ClassBinding(std::string className) : ClassName(std::move(className)) {}

		BindingType Type() const override { return BindingType::Class; }

		sol::table BindToSol(sol::state_view& state, sol::table& table) override
		{
			if constexpr (sizeof...(CtorArgs) > 0)
			{
				state.new_usertype<T>(ClassName, sol::constructors<CtorArgs...>());
			}
			else
			{
				state.new_usertype<T>(ClassName, sol::no_constructor);
			}
			return state[ClassName];
		}
	};

	template<typename T, typename... Args>
	struct ConstructorBinding : IScriptBinding
	{
		BindingType Type() const override { return BindingType::Constructor; }

		sol::table BindToSol(sol::state_view& state, sol::table& table) override
		{
			return table;
		}
	};

	template<typename T>
	struct MethodBinding : IScriptBinding
	{
		std::string Name;
		T Function;

		MethodBinding(std::string name, T fn) : Name(std::move(name)), Function(fn) {}

		BindingType Type() const override { return BindingType::Method; }

		sol::table BindToSol(sol::state_view& state, sol::table& table) override
		{
			table[Name] = Function;
			return table;
		}
	};

	template<typename T>
	struct FunctionBinding : IScriptBinding
	{
		std::string Name;
		T Function;

		FunctionBinding(std::string name, T fn) : Name(std::move(name)), Function(fn) {}

		BindingType Type() const override { return BindingType::Function; }

		sol::table BindToSol(sol::state_view& state, sol::table& table) override
		{
			table[Name] = Function;
			return table;
		}
	};

	template<typename T>
	struct FieldBinding : IScriptBinding
	{
		std::string Name;
		T Field;

		FieldBinding(std::string name, T field) : Name(std::move(name)), Field(field) {}

		BindingType Type() const override { return BindingType::Field; }

		sol::table BindToSol(sol::state_view& state, sol::table& table) override
		{
			table[Name] = Field;
			return table;
		}
	};

	template<typename Getter, typename Setter>
	struct PropertyBinding : IScriptBinding
	{
		std::string Name;
		Getter GetterFunction;
		Setter SetterFunction;

		PropertyBinding(std::string name, Getter getter, Setter setter)
			: Name(std::move(name)), GetterFunction(getter), SetterFunction(setter) {
		}

		BindingType Type() const override { return BindingType::Property; }

		sol::table BindToSol(sol::state_view& state, sol::table& table) override
		{
			table[Name] = sol::property(GetterFunction, SetterFunction);
			return table;
		}
	};

	template<typename Getter>
	struct ReadOnlyPropertyBinding : IScriptBinding
	{
		std::string Name;
		Getter GetterFunction;

		ReadOnlyPropertyBinding(std::string name, Getter getter)
			: Name(std::move(name)), GetterFunction(getter) {
		}

		BindingType Type() const override { return BindingType::ReadOnlyProperty; }

		sol::table BindToSol(sol::state_view& state, sol::table& table) override
		{
			table[Name] = sol::property(GetterFunction);
			return table;
		}
	};

	template<typename T>
	struct OperatorBinding : IScriptBinding
	{
		ScriptOperator OpType;
		T Function;

		OperatorBinding(ScriptOperator op, T fn) : OpType(op), Function(fn) {}

		BindingType Type() const override { return BindingType::Operator; }

		sol::table BindToSol(sol::state_view& state, sol::table& table) override
		{
			sol::meta_function meta = sol::meta_function::addition;
			switch (OpType)
			{
			case ScriptOperator::Add:          meta = sol::meta_function::addition; break;
			case ScriptOperator::Subtract:     meta = sol::meta_function::subtraction; break;
			case ScriptOperator::Multiply:     meta = sol::meta_function::multiplication; break;
			case ScriptOperator::Divide:       meta = sol::meta_function::division; break;
			case ScriptOperator::Equal:        meta = sol::meta_function::equal_to; break;
			case ScriptOperator::NotEqual:     meta = sol::meta_function::less_than; break;
			case ScriptOperator::Less:         meta = sol::meta_function::less_than; break;
			case ScriptOperator::LessEqual:    meta = sol::meta_function::less_than_or_equal_to; break;
			case ScriptOperator::UnaryMinus:   meta = sol::meta_function::unary_minus; break;
			case ScriptOperator::ToString:     meta = sol::meta_function::to_string; break;
			case ScriptOperator::Length:       meta = sol::meta_function::length; break;
			case ScriptOperator::Index:        meta = sol::meta_function::index; break;
			default: break;
			}
			table[meta] = Function;
			return table;
		}
	};

	// ------------------------------------------------------------
	// Builders
	// ------------------------------------------------------------

	class ScriptRegistry;

	template<typename T>
	class ClassBuilder
	{
	public:
		ClassBuilder(ScriptRegistry& registry, ScriptClass& info)
			: m_Registry(registry), m_Info(info) {
		}

		template<typename... Args>
		ClassBuilder& Constructor(std::string signature)
		{
			m_Info.Constructors.push_back({
				"constructor" + signature,
				std::make_shared<ConstructorBinding<T, Args...>>()
				});
			return *this;
		}

		template<typename Func>
		ClassBuilder& Method(std::string name, Func function, std::string signature, std::string documentation = "")
		{
			m_Info.Methods.push_back({
				name, name + signature, documentation, false,
				std::make_shared<MethodBinding<Func>>(name, function)
				});
			return *this;
		}

		template<typename Func>
		ClassBuilder& StaticMethod(std::string name, Func function, std::string signature, std::string documentation = "")
		{
			m_Info.Methods.push_back({
				name, name + signature, documentation, true,
				std::make_shared<FunctionBinding<Func>>(name, function)
				});
			return *this;
		}

		template<typename FieldType>
		ClassBuilder& Field(std::string name, std::string type_name, FieldType field, std::string documentation = "")
		{
			m_Info.Fields.push_back({
				name, type_name, documentation,
				std::make_shared<FieldBinding<FieldType>>(name, field)
				});
			return *this;
		}

		template<typename Getter, typename Setter>
		ClassBuilder& Property(std::string name, std::string type_name, Getter getter, Setter setter, std::string documentation = "")
		{
			m_Info.Properties.push_back({
				name, type_name, documentation, false,
				std::make_shared<PropertyBinding<Getter, Setter>>(name, getter, setter)
				});
			return *this;
		}

		template<typename Getter>
		ClassBuilder& ReadOnlyProperty(std::string name, std::string type_name, Getter getter, std::string documentation = "")
		{
			m_Info.Properties.push_back({
				name, type_name, documentation, true,
				std::make_shared<ReadOnlyPropertyBinding<Getter>>(name, getter)
				});
			return *this;
		}

		template<typename Func>
		ClassBuilder& Operator(ScriptOperator op, Func function, std::string documentation = "")
		{
			m_Info.Operators.push_back({
				op, documentation,
				std::make_shared<OperatorBinding<Func>>(op, function)
				});
			return *this;
		}

		ClassBuilder& Documentation(std::string text)
		{
			m_Info.Documentation = text;
			return *this;
		}

		ClassBuilder& Base(std::string name)
		{
			m_Info.BaseClass = name;
			return *this;
		}

	private:
		ScriptRegistry& m_Registry;
		ScriptClass& m_Info;
	};

	template<typename T>
	class EnumBuilder
	{
	public:
		EnumBuilder(ScriptRegistry& registry, ScriptEnum& info)
			: m_Registry(registry), m_Info(info) {
		}

		EnumBuilder& Value(std::string name, T value)
		{
			m_Info.Values.emplace_back(name, static_cast<int64_t>(value));
			return *this;
		}

	private:
		ScriptRegistry& m_Registry;
		ScriptEnum& m_Info;
	};

	class NamespaceBuilder
	{
	public:
		NamespaceBuilder(ScriptRegistry& registry, ScriptNamespace& info)
			: m_Registry(registry), m_Info(info) {
		}

		template<typename Func>
		NamespaceBuilder& Function(std::string name, Func function, std::string signature, std::string documentation = "")
		{
			m_Info.Functions.push_back({
				name, signature, documentation,
				std::make_shared<FunctionBinding<Func>>(name, function)
				});
			return *this;
		}

	private:
		ScriptRegistry& m_Registry;
		ScriptNamespace& m_Info;
	};

	class ScriptRegistry
	{
	public:
		template<typename T, typename... CtorArgs>
		ClassBuilder<T> BeginClass(std::string name)
		{
			m_Classes.push_back({
				name, name, "", "", {}, {}, {}, {}, {},
				std::make_shared<ClassBinding<T, CtorArgs...>>(name)
				});
			return ClassBuilder<T>(*this, m_Classes.back());
		}

		template<typename T>
		EnumBuilder<T> BeginEnum(std::string name)
		{
			m_Enums.push_back({ name, {} });
			return EnumBuilder<T>(*this, m_Enums.back());
		}

		NamespaceBuilder BeginNamespace(std::string name)
		{
			m_Namespaces.push_back({ name, {} });
			return NamespaceBuilder(*this, m_Namespaces.back());
		}

		template<typename Func>
		void GlobalFunction(std::string name, Func function, std::string signature, std::string documentation = "")
		{
			m_Globals.push_back({
				name, signature, documentation,
				std::make_shared<FunctionBinding<Func>>(name, function)
				});
		}

		const ScriptDatabase Database() const
		{
			return { m_Classes, m_Enums, m_Namespaces, m_Globals };
		}

	private:
		std::vector<ScriptClass> m_Classes;
		std::vector<ScriptEnum> m_Enums;
		std::vector<ScriptNamespace> m_Namespaces;
		std::vector<ScriptFunction> m_Globals;
	};
}
