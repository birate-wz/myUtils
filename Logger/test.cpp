#include "logger.hpp"
#include <gtest/gtest.h>


TEST(LoggerTest, TestLog) {
    LOG_INFO("test log info"); 
    LOG_WARNING("test log warning");
    LOG_ERROR("test log error");
}

TEST(LoggerTest, TestLogLevel) {
    Logger::getInstance().setLogLevel(LOGGER_LEVEL_ERROR);
    LOG_INFO("This should not be printed");
    LOG_WARNING("This should not be printed");
    LOG_ERROR("This should be printed");
    Logger::getInstance().setLogLevel(LOGGER_LEVEL_INFO); // 恢复默认级别
}

TEST(LoggerTest, TestVariable) {
    int a = 10;
    bool b = true;
    std::string c = "Hello, World!";
    LOG_INFO("Printing int: {}", a);
    LOG_INFO("Printing bool: {}", b);
    LOG_INFO("Printing string: {}", c);
}

// 测试文件输出
TEST(LoggerTest, TestFileOutput) {
    std::string testFile = "test_log.txt";
    
    // 确保测试前文件不存在
    if (std::filesystem::exists(testFile)) {
        std::filesystem::remove(testFile);
    }
    
    ASSERT_TRUE(Logger::getInstance().setOutputFile(testFile));
    Logger::getInstance().setConsoleOutput(false);
    
    LOG_INFO("File output test");
    
    // 检查文件是否创建
    ASSERT_TRUE(std::filesystem::exists(testFile));
    
    // 检查文件内容
    std::ifstream file(testFile);
    ASSERT_TRUE(file.is_open());
    
    std::string content;
    std::getline(file, content);
    ASSERT_FALSE(content.empty());
    ASSERT_TRUE(content.find("File output test") != std::string::npos);
    
    file.close();
    
    // 清理
    std::filesystem::remove(testFile);
}
int main() {
    ::testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}