#pragma once
#include "Tracker.hpp"
#include "Spammer.hpp"
#include "Cleaner.hpp"
#include "Auto.hpp"
#include "Typing.hpp"

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
		C_Auto* Auto();
		C_Typing* Typing();

		std::string Token() const;
		void SetToken(const std::string& token);
		bool Logged() const;
		std::string MeName() const;
		std::string MeId() const;
		void SetMe(const std::string& name, const std::string& id);

		std::string ConfigPath() const;
		std::vector<std::string> PendingFavorites() const;

		struct S_Account
		{
			std::string m_id;
			std::string m_name;
			std::string m_token;
		};

		std::vector<S_Account> Accounts() const;
		std::string ActiveAccount() const;
		void AddOrUpdateAccount(const std::string& id, const std::string& name, const std::string& token);
		void SetActiveAccount(const std::string& id);
		void RemoveAccount(const std::string& id);
		void ClearAccounts();
		std::string TokenFor(const std::string& id) const;

		std::unordered_map<std::string, std::vector<std::string>> PendingPicks() const;
		void ForgetPendingPicks(const std::string& guild);
		std::string PendingCleanGuild() const;
		std::string PendingCleanChannel() const;
		void ClearPendingClean();

		void SetCleanerState(const std::string& guild, const std::string& channel, int hours, int limit, const std::string& text, bool only);
		int CleanHours() const;
		int CleanLimit() const;
		std::string CleanText() const;
		bool CleanOnly() const;

	private:
		C_DiscordClient m_client;
		C_Tracker m_tracker;
		C_Spammer m_spammer;
		C_Cleaner m_cleaner;
		C_Auto m_auto;
		C_Typing m_typing;
		std::string m_token;
		std::string m_me_name;
		std::string m_me_id;
		std::vector<std::string> m_pending_favorites;
		std::vector<S_Account> m_accounts;
		std::string m_active_account;
		std::unordered_map<std::string, std::vector<std::string>> m_pending_picks;
		std::string m_pending_clean_guild = "all";
		std::string m_pending_clean_channel = "all";
		std::string m_clean_guild_id = "all";
		std::string m_clean_channel_id = "all";
		int m_clean_hours = 24;
		int m_clean_limit = 200;
		std::string m_clean_text;
		bool m_clean_only = false;
	};
}
