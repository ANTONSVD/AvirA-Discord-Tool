#include "Json.hpp"

namespace AvirA
{
	void C_Json::SkipSpaces(const std::string& text, size_t& at)
	{
		while (at < text.size() && (text[at] == ' ' || text[at] == '\t' || text[at] == '\r' || text[at] == '\n'))
			at++;
	}

	std::string C_Json::ParseRawText(const std::string& text, size_t& at)
	{
		std::string out;
		at++;
		while (at < text.size())
		{
			char c = text[at++];
			if (c == '"')
				break;
			if (c == '\\' && at < text.size())
			{
				char e = text[at++];
				if (e == 'n')
					out.push_back('\n');
				else if (e == 'r')
					out.push_back('\r');
				else if (e == 't')
					out.push_back('\t');
				else if (e == 'u' && at + 4 <= text.size())
				{
					unsigned code = 0;
					for (int i = 0; i < 4; i++)
					{
						char h = text[at++];
						code <<= 4;
						if (h >= '0' && h <= '9')
							code |= (unsigned)(h - '0');
						else if (h >= 'a' && h <= 'f')
							code |= (unsigned)(h - 'a' + 10);
						else if (h >= 'A' && h <= 'F')
							code |= (unsigned)(h - 'A' + 10);
					}
					if (code < 0x80)
						out.push_back((char)code);
					else if (code < 0x800)
					{
						out.push_back((char)(0xC0 | (code >> 6)));
						out.push_back((char)(0x80 | (code & 0x3F)));
					}
					else
					{
						out.push_back((char)(0xE0 | (code >> 12)));
						out.push_back((char)(0x80 | ((code >> 6) & 0x3F)));
						out.push_back((char)(0x80 | (code & 0x3F)));
					}
				}
				else
					out.push_back(e);
			}
			else
				out.push_back(c);
		}
		return out;
	}

	double C_Json::ParseRawNumber(const std::string& text, size_t& at)
	{
		size_t start = at;
		while (at < text.size() && (text[at] == '-' || text[at] == '+' || text[at] == '.' || text[at] == 'e' || text[at] == 'E' || (text[at] >= '0' && text[at] <= '9')))
			at++;
		return at > start ? std::stod(text.substr(start, at - start)) : 0;
	}

	C_Json C_Json::ParseDict(const std::string& text, size_t& at)
	{
		C_Json out = MakeDict();
		at++;
		SkipSpaces(text, at);
		if (at < text.size() && text[at] == '}')
		{
			at++;
			return out;
		}
		while (at < text.size())
		{
			SkipSpaces(text, at);
			if (at >= text.size() || text[at] != '"')
				break;
			std::string key = ParseRawText(text, at);
			SkipSpaces(text, at);
			if (at < text.size() && text[at] == ':')
				at++;
			C_Json value = ParseValue(text, at);
			out.m_dict.push_back({ key, value });
			SkipSpaces(text, at);
			if (at < text.size() && text[at] == ',')
			{
				at++;
				continue;
			}
			if (at < text.size() && text[at] == '}')
			{
				at++;
				break;
			}
			break;
		}
		return out;
	}

	C_Json C_Json::ParseList(const std::string& text, size_t& at)
	{
		C_Json out = MakeList();
		at++;
		SkipSpaces(text, at);
		if (at < text.size() && text[at] == ']')
		{
			at++;
			return out;
		}
		while (at < text.size())
		{
			C_Json value = ParseValue(text, at);
			out.m_list.push_back(value);
			SkipSpaces(text, at);
			if (at < text.size() && text[at] == ',')
			{
				at++;
				continue;
			}
			if (at < text.size() && text[at] == ']')
			{
				at++;
				break;
			}
			break;
		}
		return out;
	}

	C_Json C_Json::ParseValue(const std::string& text, size_t& at)
	{
		SkipSpaces(text, at);
		C_Json out;
		if (at >= text.size())
			return out;
		char c = text[at];
		if (c == '{')
			return ParseDict(text, at);
		if (c == '[')
			return ParseList(text, at);
		if (c == '"')
		{
			out.m_type = E_JsonType::Text;
			out.m_text = ParseRawText(text, at);
			return out;
		}
		if (c == 't' && text.compare(at, 4, "true") == 0)
		{
			out.m_type = E_JsonType::Bool;
			out.m_bool = true;
			at += 4;
			return out;
		}
		if (c == 'f' && text.compare(at, 5, "false") == 0)
		{
			out.m_type = E_JsonType::Bool;
			out.m_bool = false;
			at += 5;
			return out;
		}
		if (c == 'n' && text.compare(at, 4, "null") == 0)
		{
			out.m_type = E_JsonType::Null;
			at += 4;
			return out;
		}
		if ((c >= '0' && c <= '9') || c == '-')
		{
			out.m_type = E_JsonType::Number;
			out.m_number = ParseRawNumber(text, at);
			return out;
		}
		return out;
	}

	C_Json C_Json::Parse(const std::string& text)
	{
		size_t at = 0;
		return ParseValue(text, at);
	}

	C_Json C_Json::MakeDict()
	{
		C_Json out;
		out.m_type = E_JsonType::Dict;
		return out;
	}

	C_Json C_Json::MakeList()
	{
		C_Json out;
		out.m_type = E_JsonType::List;
		return out;
	}

	std::string C_Json::Escape(const std::string& text)
	{
		std::string out;
		for (char c : text)
		{
			if (c == '"')
				out += "\\\"";
			else if (c == '\\')
				out += "\\\\";
			else if (c == '\n')
				out += "\\n";
			else if (c == '\r')
				out += "\\r";
			else if (c == '\t')
				out += "\\t";
			else
				out.push_back(c);
		}
		return out;
	}

	std::string C_Json::Dump() const
	{
		if (m_type == E_JsonType::Null)
			return "null";
		if (m_type == E_JsonType::Bool)
			return m_bool ? "true" : "false";
		if (m_type == E_JsonType::Number)
		{
			char buffer[64];
			if (m_number == (double)(i64)m_number)
				snprintf(buffer, sizeof(buffer), "%lld", (long long)m_number);
			else
				snprintf(buffer, sizeof(buffer), "%f", m_number);
			return buffer;
		}
		if (m_type == E_JsonType::Text)
			return "\"" + Escape(m_text) + "\"";
		if (m_type == E_JsonType::List)
		{
			std::string out = "[";
			for (size_t i = 0; i < m_list.size(); i++)
			{
				if (i)
					out += ",";
				out += m_list[i].Dump();
			}
			return out + "]";
		}
		if (m_type == E_JsonType::Dict)
		{
			std::string out = "{";
			for (size_t i = 0; i < m_dict.size(); i++)
			{
				if (i)
					out += ",";
				out += "\"" + Escape(m_dict[i].first) + "\":" + m_dict[i].second.Dump();
			}
			return out + "}";
		}
		return "null";
	}

	bool C_Json::Valid() const
	{
		return m_type != E_JsonType::Empty;
	}

	bool C_Json::Has(const std::string& key) const
	{
		return Find(key) != nullptr;
	}

	const C_Json* C_Json::Find(const std::string& key) const
	{
		if (m_type != E_JsonType::Dict)
			return nullptr;
		for (size_t i = 0; i < m_dict.size(); i++)
		{
			if (m_dict[i].first == key)
				return &m_dict[i].second;
		}
		return nullptr;
	}

	C_Json* C_Json::Find(const std::string& key)
	{
		if (m_type != E_JsonType::Dict)
			return nullptr;
		for (size_t i = 0; i < m_dict.size(); i++)
		{
			if (m_dict[i].first == key)
				return &m_dict[i].second;
		}
		return nullptr;
	}

	std::string C_Json::GetText(const std::string& key, const std::string& fallback) const
	{
		const C_Json* found = Find(key);
		if (!found)
			return fallback;
		if (found->m_type == E_JsonType::Text)
			return found->m_text;
		if (found->m_type == E_JsonType::Number)
			return FormatI64((i64)found->m_number);
		if (found->m_type == E_JsonType::Bool)
			return found->m_bool ? "true" : "false";
		if (found->m_type == E_JsonType::Null)
			return "";
		return fallback;
	}

	i64 C_Json::GetInt(const std::string& key, i64 fallback) const
	{
		const C_Json* found = Find(key);
		if (!found)
			return fallback;
		if (found->m_type == E_JsonType::Number)
			return (i64)found->m_number;
		if (found->m_type == E_JsonType::Text && !found->m_text.empty())
		{
			try
			{
				return std::stoll(found->m_text);
			}
			catch (...)
			{
				return fallback;
			}
		}
		if (found->m_type == E_JsonType::Bool)
			return found->m_bool ? 1 : 0;
		return fallback;
	}

	bool C_Json::GetBool(const std::string& key, bool fallback) const
	{
		const C_Json* found = Find(key);
		if (!found)
			return fallback;
		if (found->m_type == E_JsonType::Bool)
			return found->m_bool;
		return fallback;
	}

	std::string C_Json::AsText() const
	{
		if (m_type == E_JsonType::Text)
			return m_text;
		if (m_type == E_JsonType::Number)
			return FormatI64((i64)m_number);
		if (m_type == E_JsonType::Bool)
			return m_bool ? "true" : "false";
		return "";
	}

	i64 C_Json::AsInt() const
	{
		if (m_type == E_JsonType::Number)
			return (i64)m_number;
		if (m_type == E_JsonType::Text && !m_text.empty())
		{
			try
			{
				return std::stoll(m_text);
			}
			catch (...)
			{
				return 0;
			}
		}
		if (m_type == E_JsonType::Bool)
			return m_bool ? 1 : 0;
		return 0;
	}

	bool C_Json::AsBool() const
	{
		if (m_type == E_JsonType::Bool)
			return m_bool;
		return false;
	}

	size_t C_Json::Size() const
	{
		if (m_type == E_JsonType::List)
			return m_list.size();
		if (m_type == E_JsonType::Dict)
			return m_dict.size();
		return 0;
	}

	const C_Json& C_Json::At(size_t index) const
	{
		return m_list[index];
	}

	void C_Json::Set(const std::string& key, const C_Json& value)
	{
		if (m_type != E_JsonType::Dict)
			m_type = E_JsonType::Dict;
		for (size_t i = 0; i < m_dict.size(); i++)
		{
			if (m_dict[i].first == key)
			{
				m_dict[i].second = value;
				return;
			}
		}
		m_dict.push_back({ key, value });
	}

	void C_Json::Set(const std::string& key, const std::string& value)
	{
		Set(key, FromText(value));
	}

	void C_Json::Set(const std::string& key, const char* value)
	{
		Set(key, FromText(value ? value : ""));
	}

	void C_Json::Set(const std::string& key, i64 value)
	{
		Set(key, FromInt(value));
	}

	void C_Json::Set(const std::string& key, int value)
	{
		Set(key, FromInt(value));
	}

	void C_Json::Set(const std::string& key, bool value)
	{
		Set(key, FromBool(value));
	}

	void C_Json::Push(const C_Json& value)
	{
		if (m_type != E_JsonType::List)
			m_type = E_JsonType::List;
		m_list.push_back(value);
	}

	void C_Json::Push(const std::string& value)
	{
		Push(FromText(value));
	}

	C_Json C_Json::FromText(const std::string& value)
	{
		C_Json out;
		out.m_type = E_JsonType::Text;
		out.m_text = value;
		return out;
	}

	C_Json C_Json::FromInt(i64 value)
	{
		C_Json out;
		out.m_type = E_JsonType::Number;
		out.m_number = (double)value;
		return out;
	}

	C_Json C_Json::FromBool(bool value)
	{
		C_Json out;
		out.m_type = E_JsonType::Bool;
		out.m_bool = value;
		return out;
	}

	C_Json C_Json::FromNull()
	{
		C_Json out;
		out.m_type = E_JsonType::Null;
		return out;
	}
}
