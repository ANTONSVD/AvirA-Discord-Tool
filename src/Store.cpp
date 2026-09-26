#include "Store.hpp"
#include <Windows.h>
#include <ShlObj.h>

namespace AvirA
{
	bool C_Store::Initialize()
	{
		m_tracker.Attach(&m_client);
		m_spammer.Attach(&m_client);
		m_cleaner.Attach(&m_client);
		Load();
		if (!m_token.empty())
			m_client.SetToken(m_token);
		return true;
	}

	void C_Store::Shutdown()
	{
		m_tracker.Stop();
		Save();
	}

	void C_Store::Save()
	{
		std::string path = ConfigPath();
		std::string text;
		text += "[tracker]\n";
		text += "interval=" + FormatI32(m_tracker.Interval()) + "\n";
		text += "webhook=" + m_tracker.Webhook()->Url() + "\n";
		auto items = m_tracker.All();
		text += "watched=";
		for (size_t i = 0; i < items.size(); i++)
		{
			if (i)
				text += ",";
			text += items[i]->m_id;
		}
		text += "\n";
		FILE* file = nullptr;
		if (fopen_s(&file, path.c_str(), "wb") == 0 && file)
		{
			std::fwrite(text.data(), 1, text.size(), file);
			std::fclose(file);
		}
	}

	void C_Store::Load()
	{
		std::string path = ConfigPath();
		FILE* file = nullptr;
		if (fopen_s(&file, path.c_str(), "rb") != 0 || !file)
			return;
		std::fseek(file, 0, SEEK_END);
		long size = std::ftell(file);
		std::fseek(file, 0, SEEK_SET);
		std::string text;
		if (size > 0 && size < 65536)
		{
			text.resize((size_t)size);
			size_t got = std::fread(&text[0], 1, text.size(), file);
			text.resize(got);
		}
		std::fclose(file);
		size_t at = 0;
		std::vector<std::string> watched;
		while (at < text.size())
		{
			size_t end = text.find('\n', at);
			std::string line = Trimmed(text.substr(at, end == std::string::npos ? std::string::npos : end - at));
			if (line.rfind("interval=", 0) == 0)
				m_tracker.SetInterval(std::atoi(line.substr(9).c_str()));
			if (line.rfind("webhook=", 0) == 0)
				m_tracker.Webhook()->SetUrl(line.substr(8));
			if (line.rfind("watched=", 0) == 0)
			{
				std::string list = line.substr(8);
				size_t p = 0;
				while (p < list.size())
				{
					size_t comma = list.find(',', p);
					std::string id = Trimmed(list.substr(p, comma == std::string::npos ? std::string::npos : comma - p));
					if (!id.empty())
						watched.push_back(id);
					if (comma == std::string::npos)
						break;
					p = comma + 1;
				}
			}
			if (end == std::string::npos)
				break;
			at = end + 1;
		}
		for (size_t i = 0; i < watched.size(); i++)
		{
			std::string error;
			m_tracker.Add(watched[i], error);
		}
	}

	C_DiscordClient* C_Store::Client()
	{
		return &m_client;
	}

	C_Tracker* C_Store::Tracker()
	{
		return &m_tracker;
	}

	C_Spammer* C_Store::Spammer()
	{
		return &m_spammer;
	}

	C_Cleaner* C_Store::Cleaner()
	{
		return &m_cleaner;
	}

	std::string C_Store::Token() const
	{
		return m_token;
	}

	void C_Store::SetToken(const std::string& token)
	{
		m_token = Trimmed(token);
		m_client.SetToken(m_token);
	}

	bool C_Store::Logged() const
	{
		return !m_me_id.empty();
	}

	std::string C_Store::MeName() const
	{
		return m_me_name;
	}

	std::string C_Store::MeId() const
	{
		return m_me_id;
	}

	void C_Store::SetMe(const std::string& name, const std::string& id)
	{
		m_me_name = name;
		m_me_id = id;
		m_cleaner.SetMe(id);
	}

	std::string C_Store::ConfigPath() const
	{
		char roaming[MAX_PATH] = {};
		if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, roaming)))
			return std::string(roaming) + "\\AvirA_Discord_Tool.cfg";
		return "AvirA_Discord_Tool.cfg";
	}
}
