#include "Captcha.hpp"

namespace AvirA
{
	void C_Captcha::SetKey(const std::string& key)
	{
		std::lock_guard<std::mutex> guard(m_lock);
		m_key = Trimmed(key);
	}

	std::string C_Captcha::Key() const
	{
		std::lock_guard<std::mutex> guard(m_lock);
		return m_key;
	}

	bool C_Captcha::HasKey() const
	{
		std::lock_guard<std::mutex> guard(m_lock);
		return !m_key.empty();
	}

	int C_Captcha::Solves() const
	{
		return m_solves;
	}

	bool C_Captcha::Post(const std::string& method, const std::string& json, std::string& error, C_Json& out)
	{
		std::string key = Key();
		if (key.empty())
		{
			error = "No key";
			return false;
		}
		C_Http http;
		S_HttpResult result = http.PostJsonFull("https://api.capmonster.cloud/" + method, json);
		if (!result.m_ok)
		{
			error = "HTTP " + FormatI32(result.m_status);
			return false;
		}
		out = C_Json::Parse(result.m_body);
		if ((int)out.GetInt("errorId", 1) != 0)
		{
			error = out.GetText("errorCode", "unknown");
			std::string detail = out.GetText("errorDescription");
			if (!detail.empty())
				error += ": " + detail;
			return false;
		}
		return true;
	}

	bool C_Captcha::Balance(std::string& error, double& out)
	{
		out = 0;
		C_Json body = C_Json::MakeDict();
		body.Set("clientKey", Key());
		C_Json root;
		if (!Post("getBalance", body.Dump(), error, root))
			return false;
		const C_Json* found = root.Find("balance");
		if (!found)
		{
			error = "No balance";
			return false;
		}
		if (found->m_type == E_JsonType::Number)
			out = found->m_number;
		else
		{
			try
			{
				out = std::stod(found->AsText());
			}
			catch (...)
			{
				error = "Bad balance";
				return false;
			}
		}
		return true;
	}

	bool C_Captcha::Solve(const std::string& sitekey, const std::string& page, std::string& error, std::string& out_token)
	{
		out_token.clear();
		if (!HasKey())
		{
			error = "No key";
			return false;
		}
		C_Json task = C_Json::MakeDict();
		task.Set("type", "HCaptchaTaskProxyless");
		task.Set("websiteURL", page.empty() ? "https://discord.com/channels/@me" : page);
		task.Set("websiteKey", sitekey);
		C_Json create = C_Json::MakeDict();
		create.Set("clientKey", Key());
		create.Set("task", task);
		C_Json made;
		if (!Post("createTask", create.Dump(), error, made))
			return false;
		i64 id = made.GetInt("taskId", 0);
		if (id == 0)
		{
			error = "No task id";
			return false;
		}
		for (int attempt = 0; attempt < 45; attempt++)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(2000));
			C_Json ask = C_Json::MakeDict();
			ask.Set("clientKey", Key());
			ask.Set("taskId", id);
			C_Json state;
			std::string poll_error;
			if (!Post("getTaskResult", ask.Dump(), poll_error, state))
			{
				error = poll_error;
				return false;
			}
			if (state.GetText("status") != "ready")
				continue;
			const C_Json* solution = state.Find("solution");
			std::string token = solution ? solution->GetText("gRecaptchaResponse") : "";
			if (token.empty())
			{
				error = "Empty solution";
				return false;
			}
			out_token = token;
			m_solves++;
			return true;
		}
		error = "Timeout";
		return false;
	}
}
