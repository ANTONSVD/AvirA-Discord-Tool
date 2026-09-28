#pragma once
#include "Discord.hpp"

namespace AvirA
{
	struct S_TokenRow
	{
		S_TokenInfo m_info;
		std::string m_short;
	};

	class C_Checker
	{
	public:
		void Clear();
		bool Check(const std::string& token, S_TokenRow& row);
		bool CheckAll(const std::vector<std::string>& tokens, std::atomic<int>* done = nullptr);
		std::vector<S_TokenRow> Rows() const;
		size_t Alive() const;
		size_t Dead() const;
		bool Busy() const;
	private:
		static std::string ShortToken(const std::string& token);
		std::vector<S_TokenRow> m_rows;
		std::atomic<bool> m_busy = false;
		mutable std::mutex m_lock;
	};
}
