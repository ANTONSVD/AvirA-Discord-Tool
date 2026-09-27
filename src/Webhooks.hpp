#pragma once
#include "Http.hpp"
#include "Json.hpp"

namespace AvirA
{
	struct S_HookInfo
	{
		std::string m_id;
		std::string m_created;
		std::string m_type;
		std::string m_name;
		std::string m_avatar;
		std::string m_guild;
		std::string m_channel;
		std::string m_app;
		std::string m_source;
	};

	struct S_HookMessage
	{
		bool m_found = false;
		std::string m_author;
		std::string m_content;
		std::string m_sent;
		std::string m_edited;
	};

	struct S_HookField
	{
		std::string m_name;
		std::string m_value;
	};

	struct S_HookEmbed
	{
		std::string m_title;
		std::string m_desc;
		int m_color = -1;
		std::string m_footer;
		std::string m_thumb;
		std::vector<S_HookField> m_fields;
	};

	struct S_HookSpam
	{
		int m_mode = 0;
		std::string m_text;
		int m_count = 10;
		int m_cooldown_ms = 500;
		std::string m_username;
		std::string m_avatar_url;
		std::string m_thread;
	};

	class C_Webhooks
	{
	public:
		static bool ValidUrl(const std::string& url);

		bool FetchInfo(const std::string& url, S_HookInfo& out, std::string& error);
		bool Spam(const std::string& url, const S_HookSpam& opts, std::string& error, std::atomic<int>* done = nullptr, std::atomic<int>* total = nullptr, std::atomic<int>* rl = nullptr, std::atomic<bool>* cancel = nullptr);
		bool DeleteHook(const std::string& url, std::string& error);
		bool ModifyHook(const std::string& url, const std::string& name, const std::string& avatar_url, std::string& error);
		bool SendHookFile(const std::string& url, const std::string& path, const std::string& content, std::string& error);
		bool FetchMessage(const std::string& url, const std::string& id, S_HookMessage& out, std::string& error);
		bool EditHookMessage(const std::string& url, const std::string& id, const std::string& text, std::string& error);
		bool DeleteHookMessage(const std::string& url, const std::string& id, std::string& error);
		bool SendEmbed(const std::string& url, const S_HookEmbed& embed, std::string& error);
		bool SendJson(const std::string& url, const std::string& json_text, std::string& error);

		static u64 IsoStamp(const std::string& text);

	private:
		C_Http m_http;
	};
}
