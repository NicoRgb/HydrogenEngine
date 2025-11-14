#pragma once

#include <string>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>

namespace Hydrogen
{
	class LogSink : public spdlog::sinks::base_sink<std::mutex>
	{
	public:
		struct LogMsg
		{
			spdlog::level::level_enum level;
			std::string message;
		};

		const std::vector<LogMsg>& GetMessages() const { return m_Messages; }

	protected:
		void sink_it_(const spdlog::details::log_msg& msg) override
		{
			spdlog::memory_buf_t formatted;
			formatter_->format(msg, formatted);

			m_Messages.push_back(
				{
					msg.level,
					fmt::to_string(formatted)
				});
		}

		void flush_() override {}

	private:
		std::vector<LogMsg> m_Messages;
	};

	class Logger
	{
	public:
		enum class LogLevel { Trace = 0, Debug = 1, Info = 2, Warn = 3, Error = 4, Fatal = 5, Disable = 6 };
		Logger(std::string name, LogLevel logLevel = LogLevel::Info, std::string format = "%^[%T] %n: %v%$", bool out = true, std::string filename = "");

		template <typename... Args>
		inline void Trace(spdlog::format_string_t<Args...> fmt, Args &&...args) { m_Logger->trace(fmt, std::forward<Args>(args)...); }

		template <typename... Args>
		inline void Debug(spdlog::format_string_t<Args...> fmt, Args &&...args) { m_Logger->debug(fmt, std::forward<Args>(args)...); }

		template <typename... Args>
		inline void Info(spdlog::format_string_t<Args...> fmt, Args &&...args) { m_Logger->info(fmt, std::forward<Args>(args)...); }

		template <typename... Args>
		inline void Warn(spdlog::format_string_t<Args...> fmt, Args &&...args) { m_Logger->warn(fmt, std::forward<Args>(args)...); }

		template <typename... Args>
		inline void Error(spdlog::format_string_t<Args...> fmt, Args &&...args) { m_Logger->error(fmt, std::forward<Args>(args)...); }

		template <typename... Args>
		inline void Fatal(spdlog::format_string_t<Args...> fmt, Args &&...args) { m_Logger->critical(fmt, std::forward<Args>(args)...); }

		const std::shared_ptr<LogSink>& GetLogSink() { return m_LogSink; }

	private:
		std::shared_ptr<LogSink> m_LogSink;
		std::shared_ptr<spdlog::logger> m_Logger;
	};

	class EngineLogger
	{
	public:
		static void Init()
		{
			s_Logger = std::make_shared<Logger>("Engine", Logger::LogLevel::Debug);
		}

		static std::shared_ptr<Logger> GetLogger() { return s_Logger; }

	private:
		static std::shared_ptr<Logger> s_Logger;
	};

	class AppLogger
	{
	public:
		static void Init()
		{
			s_Logger = std::make_shared<Logger>("App", Logger::LogLevel::Debug);
		}

		static std::shared_ptr<Logger> GetLogger() { return s_Logger; }

	private:
		static std::shared_ptr<Logger> s_Logger;
	};

	#define HY_ENGINE_TRACE(...) Hydrogen::EngineLogger::GetLogger()->Trace(__VA_ARGS__)
	#define HY_ENGINE_DEBUG(...) Hydrogen::EngineLogger::GetLogger()->Debug(__VA_ARGS__)
	#define HY_ENGINE_INFO(...) Hydrogen::EngineLogger::GetLogger()->Info(__VA_ARGS__)
	#define HY_ENGINE_WARN(...) Hydrogen::EngineLogger::GetLogger()->Warn(__VA_ARGS__)
	#define HY_ENGINE_ERROR(...) Hydrogen::EngineLogger::GetLogger()->Error(__VA_ARGS__)
	#define HY_ENGINE_FATAL(...) Hydrogen::EngineLogger::GetLogger()->Fatal(__VA_ARGS__)

	#define HY_APP_TRACE(...) Hydrogen::AppLogger::GetLogger()->Trace(__VA_ARGS__)
	#define HY_APP_DEBUG(...) Hydrogen::AppLogger::GetLogger()->Debug(__VA_ARGS__)
	#define HY_APP_INFO(...) Hydrogen::AppLogger::GetLogger()->Info(__VA_ARGS__)
	#define HY_APP_WARN(...) Hydrogen::AppLogger::GetLogger()->Warn(__VA_ARGS__)
	#define HY_APP_ERROR(...) Hydrogen::AppLogger::GetLogger()->Error(__VA_ARGS__)
	#define HY_APP_FATAL(...) Hydrogen::AppLogger::GetLogger()->Fatal(__VA_ARGS__)
}
