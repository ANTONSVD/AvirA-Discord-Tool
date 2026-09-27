#pragma once
#include "AvirA.hpp"
#include "Json.hpp"
#include "Discord.hpp"
#include <functional>

namespace AvirA
{
	class C_Gateway
	{
	public:
		using PresenceFn = std::function<void(const std::string&, const std::string&, const std::vector<S_Activity>&)>;

		void SetToken(const std::string& token);
		void SetPresence(const PresenceFn& callback);
		void Start();
		void Stop();
		bool Running() const;
		std::string State() const;

	private:
		void Worker();
		void SetState(const std::string& value);

		std::string m_token;
		PresenceFn m_presence;
		std::thread m_thread;
		std::atomic<bool> m_running = false;
		mutable std::mutex m_lock;
		std::string m_state = "off";
		void* m_ws = nullptr;
		std::mutex m_send;
	};
}
