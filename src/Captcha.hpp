#pragma once
#include "Http.hpp"
#include "Json.hpp"

namespace AvirA
{
	class C_Captcha
	{
	public:
		void SetKey(const std::string& key);
		std::string Key() const;
		bool HasKey() const;
		bool Balance(std::string& error, double& out);
		bool Solve(const std::string& sitekey, const std::string& page, std::string& error, std::string& out_token);
		int Solves() const;
	private:
		bool Post(const std::string& method, const std::string& json, std::string& error, C_Json& out);
		std::string m_key;
		std::atomic<int> m_solves = 0;
		mutable std::mutex m_lock;
	};
}
