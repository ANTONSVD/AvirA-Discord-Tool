#include "Webhook.hpp"
#include "Discord.hpp"

namespace AvirA
{
	void C_Webhook::SetUrl(const std::string& url)
	{
		m_url = Trimmed(url);
	}

	std::string C_Webhook::Url() const
	{
		return m_url;
	}

	bool C_Webhook::Enabled() const
	{
		return m_url.find("discord.com/api/webhooks/") != std::string::npos || m_url.find("discordapp.com/api/webhooks/") != std::string::npos;
	}

	bool C_Webhook::Push(C_DiscordClient* client, const std::string& text)
	{
		if (!client || !Enabled() || text.empty())
			return false;
		return client->PostWebhook(m_url, text);
	}
}
