#pragma once

#include <vector>
#include <functional>

namespace Hydrogen
{
	template<typename... Args>
	class Event
	{
	public:
		Event() = default;
		~Event() = default;

		void AddListener(std::function<void(Args...)> func) { m_Listeners.push_back(func); }
		void AddTempListener(std::function<void(Args...)> func) { m_TempListeners.push_back(func); }
		void Invoke(Args... args)
		{
			for (auto listener : m_Listeners)
			{
				listener(args...);
			}

			for (auto listener : m_TempListeners)
			{
				listener(args...);
			}

			m_TempListeners.clear();
		}

	private:
		std::vector<std::function<void(Args...)>> m_Listeners;
		std::vector<std::function<void(Args...)>> m_TempListeners;
	};
}
