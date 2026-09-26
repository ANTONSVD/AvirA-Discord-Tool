#pragma once
#include "AvirA.hpp"

namespace AvirA
{
	class C_DiscordClient;

	class C_Webhook
	{
	public:
		void SetUrl(const std::string& url);
		std::string Url() const;
		bool Enabled() const;
		bool Push(C_DiscordClient* client, const std::string& text);

	private:
		std::string m_url;
	};
}
