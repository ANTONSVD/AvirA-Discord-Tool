#pragma once
#include "Http.hpp"
#include "Json.hpp"

namespace AvirA
{
	struct S_Activity
	{
		std::string m_name;
		std::string m_kind;
		std::string m_details;
		std::string m_state;
		std::string m_app;
	};

	struct S_Profile
	{
		std::string m_id;
		std::string m_name;
		std::string m_global;
		std::string m_avatar;
		std::string m_banner;
		std::string m_bio;
		std::string m_status;
		std::vector<S_Activity> m_games;
		std::vector<std::string> m_guilds;
		u64 m_stamp = 0;

		bool Same(const S_Profile& other) const;
		std::string GameText() const;
	};

	struct S_Guild
	{
		std::string m_id;
		std::string m_name;
		std::string m_icon;
		bool m_owner = false;
	};

	struct S_Channel
	{
		std::string m_id;
		std::string m_guild;
		std::string m_name;
		int m_kind = 0;
		int m_position = 0;
	};

	struct S_Message
	{
		std::string m_id;
		std::string m_channel;
		std::string m_guild;
		std::string m_channel_name;
		std::string m_text;
		u64 m_stamp = 0;
		std::string m_jump;
	};

	struct S_GuildEmoji
	{
		std::string m_id;
		std::string m_name;
		bool m_animated = false;
	};

	struct S_TokenInfo
	{
		std::string m_name;
		std::string m_id;
		std::string m_global;
		std::string m_email;
		std::string m_phone;
		std::string m_locale;
		std::string m_status;
		std::string m_error;
		bool m_verified = false;
		bool m_mfa = false;
		int m_nitro = 0;
	};

	struct S_Sound
	{
		std::string m_id;
		std::string m_name;
		std::string m_guild;
	};

	class C_DiscordClient
	{
	public:
		void SetToken(const std::string& token);
		void Clear();
		bool HasToken() const;
		std::string Token() const;
		C_Http* Http();

		bool CheckToken(std::string& name, std::string& id);
		bool FetchTokenInfo(S_TokenInfo& out);
		static std::string TokenUserId(const std::string& token);
		bool SendTts(const std::string& channel, const std::string& text, std::string& error, std::string* out_id = nullptr);
		bool SendPoll(const std::string& channel, const std::string& text, const std::string& question, const std::vector<std::string>& answers, int duration, bool multi, std::string& error, std::string* out_id = nullptr);
		bool CreateThread(const std::string& channel, const std::string& name, int archive, int type, std::string& error, std::string* out_id = nullptr);
		bool RingCall(const std::string& channel, const std::vector<std::string>& recipients, std::string& error);
		bool CreateGroupDM(const std::vector<std::string>& recipients, std::string& error, std::string* out_id = nullptr);
		bool PatchChannel(const std::string& channel, const std::string& name, const std::string& icon, std::string& error);
		bool AddGroupRecipient(const std::string& channel, const std::string& user, std::string& error);
		bool FetchSounds(const std::string& guild, std::vector<S_Sound>& out);
		bool PlayBoard(const std::string& channel, const std::string& sound, const std::string& guild, std::string& error);
		bool PatchMe(const std::string& global, std::string& error, const std::string& captcha = "", const std::string& rqdata = "", const std::string& rqtoken = "");
		std::string Rqdata() const;
		std::string Rqtoken() const;
		bool FetchUser(const std::string& id, S_Profile& out);
		bool FetchGuilds(std::vector<S_Guild>& out);
		bool FetchChannels(const std::string& guild, std::vector<S_Channel>& out);
		bool FetchThreads(const std::string& guild, std::vector<S_Channel>& out);
		bool FetchDMs(std::vector<S_Channel>& out);
		static bool SendableKind(int kind);
		static std::string KindLabel(int kind);
		static std::string ShortError(const S_HttpResult& result);
		bool SendText(const std::string& channel, const std::string& text, std::string& error, std::string* out_id = nullptr);
		bool SendFiles(const std::string& channel, const std::string& text, const std::vector<std::string>& paths, std::string& error, std::string* out_id = nullptr);
		bool FetchMessages(const std::string& channel, int limit, const std::string& before, std::vector<C_Json>& out);
		bool FetchMyMessages(const std::string& channel, const std::string& me, int limit, std::vector<S_Message>& out);
		bool FetchRecent(const std::string& channel, int limit, const std::string& after, std::vector<C_Json>& out);
		bool FetchGuildEmojis(const std::string& guild, std::vector<S_GuildEmoji>& out);
		bool ReplyText(const std::string& channel, const std::string& message, const std::string& text, std::string& error, std::string* out_id = nullptr);
		bool AddReaction(const std::string& channel, const std::string& message, const std::string& emoji, std::string& error);
		bool SendTyping(const std::string& channel, std::string& error, double* retry_after = nullptr, bool* was_global = nullptr);
		bool DeleteMessage(const std::string& channel, const std::string& id);
		bool PostWebhook(const std::string& url, const std::string& text);
		static std::string UrlEncode(const std::string& text);

		static S_Profile ProfileFromJson(const C_Json& root);
		static std::string AvatarUrl(const C_Json& root);
		static std::string BannerUrl(const C_Json& root);
		static std::vector<S_Activity> ActivitiesFromJson(const C_Json& root);
		static u64 SnowflakeTime(const std::string& id);

	private:
		C_Http m_http;
		std::string m_token;
		std::string m_rqdata;
		std::string m_rqtoken;
	};
}
