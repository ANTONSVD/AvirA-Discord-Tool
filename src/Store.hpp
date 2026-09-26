#pragma once
#include "Tracker.hpp"
#include "Spammer.hpp"
#include "Cleaner.hpp"

namespace AvirA
{
	class C_Store
	{
	public:
		bool Initialize();
		void Shutdown();
		void Save();
		void Load();

		C_DiscordClient* Client();
		C_Tracker* Tracker();
		C_Spammer* Spammer();
		C_Cleaner* Cleaner();

		std::string Token() const;
		void SetToken(const std::string& token);
		bool Logged() const;
		std::string MeName() const;
		std::string MeId() const;
		void SetMe(const std::string& name, const std::string& id);

		std::string ConfigPath() const;

	private:
		C_DiscordClient m_client;
		C_Tracker m_tracker;
		C_Spammer m_spammer;
		C_Cleaner m_cleaner;
		std::string m_token;
		std::string m_me_name;
		std::string m_me_id;
	};
}
