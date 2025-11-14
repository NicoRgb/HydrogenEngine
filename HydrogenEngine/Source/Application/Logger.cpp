#include "Hydrogen/Logger.hpp"
#include <vector>

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace Hydrogen;

Logger::Logger(std::string name, LogLevel logLevel, std::string format, bool out, std::string filename)
{
	std::vector<spdlog::sink_ptr> logSinks;
	if (!filename.empty()) logSinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename, true));
	if (out) logSinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

	m_LogSink = std::make_shared<LogSink>();
	logSinks.emplace_back(m_LogSink);

	for (auto& logSink : logSinks) logSink->set_pattern(format);

	m_Logger = std::make_shared<spdlog::logger>(name, begin(logSinks), end(logSinks));
	m_Logger->set_level((spdlog::level::level_enum)logLevel);
}

std::shared_ptr<Logger> EngineLogger::s_Logger;
std::shared_ptr<Logger> AppLogger::s_Logger;
