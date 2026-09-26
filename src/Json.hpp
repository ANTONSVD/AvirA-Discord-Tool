#pragma once
#include "AvirA.hpp"

namespace AvirA
{
	enum class E_JsonType
	{
		Empty,
		Null,
		Bool,
		Number,
		Text,
		List,
		Dict
	};

	class C_Json
	{
	public:
		E_JsonType m_type = E_JsonType::Empty;
		bool m_bool = false;
		double m_number = 0;
		std::string m_text;
		std::vector<C_Json> m_list;
		std::vector<std::pair<std::string, C_Json>> m_dict;

		static C_Json Parse(const std::string& text);
		static C_Json MakeDict();
		static C_Json MakeList();
		static std::string Escape(const std::string& text);
		std::string Dump() const;

		bool Valid() const;
		bool Has(const std::string& key) const;
		const C_Json* Find(const std::string& key) const;
		C_Json* Find(const std::string& key);
		std::string GetText(const std::string& key, const std::string& fallback = "") const;
		i64 GetInt(const std::string& key, i64 fallback = 0) const;
		bool GetBool(const std::string& key, bool fallback = false) const;
		std::string AsText() const;
		i64 AsInt() const;
		bool AsBool() const;
		size_t Size() const;
		const C_Json& At(size_t index) const;

		void Set(const std::string& key, const C_Json& value);
		void Set(const std::string& key, const std::string& value);
		void Set(const std::string& key, const char* value);
		void Set(const std::string& key, i64 value);
		void Set(const std::string& key, int value);
		void Set(const std::string& key, bool value);
		void Push(const C_Json& value);
		void Push(const std::string& value);

		static C_Json FromText(const std::string& value);
		static C_Json FromInt(i64 value);
		static C_Json FromBool(bool value);
		static C_Json FromNull();

	private:
		static void SkipSpaces(const std::string& text, size_t& at);
		static C_Json ParseValue(const std::string& text, size_t& at);
		static C_Json ParseDict(const std::string& text, size_t& at);
		static C_Json ParseList(const std::string& text, size_t& at);
		static std::string ParseRawText(const std::string& text, size_t& at);
		static double ParseRawNumber(const std::string& text, size_t& at);
	};
}
