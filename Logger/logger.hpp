#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <mutex>
#include <format>
#include <chrono>
#include <source_location>
#include <filesystem>

enum LogLevel {
    LOGGER_LEVEL_TRACE,
    LOGGER_LEVEL_INFO,
    LOGGER_LEVEL_WARNING,
    LOGGER_LEVEL_ERROR,
    LOGGER_LEVEL_DEBUG,
    LOGGER_LEVEL_FATAL,
    LOGGER_LEVEL_OFF
};


class Logger {
public:
    static Logger& getInstance() { 
        static Logger instance;
        return instance;
    }

    void setLogLevel(LogLevel level) { std::lock_guard<std::mutex> lock(mMutex); mLogLevel = level; }
    void setConsoleOutput(bool consoleOutput) { mConsoleOutput = consoleOutput; }
    bool setOutputFile(const std::string& filename);
    template<typename... Args>
    void log(LogLevel level, const std::source_location& location, std::format_string<Args...> fmt, Args&&... args) {
        if (level < mLogLevel || level == LogLevel::LOGGER_LEVEL_OFF) return;

        std::string formatedMsg = std::format(fmt, std::forward<Args>(args)...);
        std::string LogEntry = formatLogEntry(level, formatedMsg, location);


        std::lock_guard<std::mutex> lock(mMutex);
        if (mConsoleOutput) {
            std::cout << LogEntry << std::endl;
        }

        if (mFileStream.is_open()) {
            mFileStream << LogEntry << std::endl;
            mFileStream.flush();
        }
    }

    template<typename... Args>
    void trace(const std::source_location& location, std::format_string<Args...> fmt, Args&&... args) { log(LogLevel::LOGGER_LEVEL_TRACE, location, fmt, std::forward<Args>(args)...);}

    template<typename... Args>
    void info(const std::source_location& location, std::format_string<Args...> fmt, Args&&... args) { log(LogLevel::LOGGER_LEVEL_INFO, location, fmt, std::forward<Args>(args)...);}

    template<typename... Args>
    void warning(const std::source_location& location, std::format_string<Args...> fmt, Args&&... args) { log(LogLevel::LOGGER_LEVEL_WARNING, location, fmt, std::forward<Args>(args)...);}

    template<typename... Args>
    void error(const std::source_location& location, std::format_string<Args...> fmt, Args&&... args) { log(LogLevel::LOGGER_LEVEL_ERROR, location, fmt, std::forward<Args>(args)...);}

    template<typename... Args>
    void debug(const std::source_location& location, std::format_string<Args...> fmt, Args&&... args) { log(LogLevel::LOGGER_LEVEL_DEBUG, location, fmt, std::forward<Args>(args)...);}

    template<typename... Args>
    void fatal(const std::source_location& location, std::format_string<Args...> fmt, Args&&... args) { log(LogLevel::LOGGER_LEVEL_FATAL, location, fmt, std::forward<Args>(args)...);}

private:

    Logger();
    ~Logger();

    Logger(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger& operator=(Logger&&) = delete;
    std::mutex mMutex;
    LogLevel mLogLevel;
    bool mConsoleOutput;
    std::ofstream mFileStream;
    std::string formatLogEntry(LogLevel level, std::string& message, const std::source_location& location);
};

#define LOG_TRACE(fmt, ...)     Logger::getInstance().trace(std::source_location::current(), fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)      Logger::getInstance().info(std::source_location::current(), fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...)   Logger::getInstance().warning(std::source_location::current(), fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)     Logger::getInstance().error(std::source_location::current(), fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)     Logger::getInstance().debug(std::source_location::current(), fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...)     Logger::getInstance().fatal(std::source_location::current(), fmt, ##__VA_ARGS__)