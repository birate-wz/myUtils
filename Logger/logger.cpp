#include "logger.hpp"

Logger::Logger(): mLogLevel(LogLevel::LOGGER_LEVEL_INFO), mConsoleOutput(true) 
{

}
Logger::~Logger() {
    std::lock_guard<std::mutex> lock(mMutex); 
    if (mFileStream.is_open()) 
        mFileStream.close(); 
}
std::string Logger::formatLogEntry(LogLevel level, std::string& message, const std::source_location& location)
{
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    // Retirve the time in HH:MM:SS format
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;

    std::string levelStr;
    std::string colorStart;
    std::string colorEnd = "\033[0m";  // 重置颜色的代码
    switch(level) {
        case LogLevel::LOGGER_LEVEL_TRACE:  levelStr = "TRACE"; colorStart = "\033[34m"; break; //blue
        case LogLevel::LOGGER_LEVEL_INFO:   levelStr = "INFO";  colorStart = "\033[32m"; break; // green
        case LogLevel::LOGGER_LEVEL_WARNING: levelStr = "WARNING"; colorStart = "\033[33m"; break; // yellow
        case LogLevel::LOGGER_LEVEL_ERROR:  levelStr = "ERROR"; colorStart = "\033[31m";  break; //red
        case LogLevel::LOGGER_LEVEL_DEBUG:  levelStr = "DEBUG"; colorStart = "\033[36m"; break; //cyan
        case LogLevel::LOGGER_LEVEL_FATAL:  levelStr = "FATAL"; colorStart = "\033[35m"; break; //magenta
        case LogLevel::LOGGER_LEVEL_OFF:    levelStr = "OFF"; colorStart = ""; break;
        default: levelStr = "UNKNOWN"; colorStart = "";
    }

    std::string filename = std::filesystem::path(location.file_name()).filename().string();

    char timeBuffer[256];
    std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", std::localtime(&time));
    
    return std::format("[{}.{:03d}] [{}{}{}] [{}:{}:{}] {}", timeBuffer, ms, colorStart, levelStr, colorEnd, filename, location.line(), location.function_name(), message);
}

bool Logger::setOutputFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mFileStream.is_open()) {
        mFileStream.close();
    }
    mFileStream.open(filename, std::ios::out | std::ios::app);
    return mFileStream.is_open();
}