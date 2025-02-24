#include <vector>

namespace Hydrogen
{
	template<typename... Args>
	class Event
	{
	public:
		Event() = default;
		~Event() = default;

		void AddListener(void (*func)(Args...)) { m_Listeners.push_back(func); }
		void Invoke(Args... args) const
		{
			for (auto listener : m_Listeners)
			{
				listener(args...);
			}
		}

	private:
		std::vector<void (*)(Args...)> m_Listeners;
	};
}
