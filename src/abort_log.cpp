#include "abort_log.h"

void dump_to_log_file(std::string message) {
    std::ofstream log_file(STR_ABORT_LOG_FILENAME, std::ios::app);

    if (log_file.is_open()) {
        log_file << "Log entry: " << std::endl;
        
        time_t now = time(0);
        tm *ltm = localtime(&now);
        log_file << "Time: " << ltm->tm_hour << ":" << ltm->tm_min << ":" << ltm->tm_sec << std::endl;

        log_file << "Date: " << ltm->tm_mday << "/" << 1 + ltm->tm_mon << "/" << 1900 + ltm->tm_year << std::endl;
        log_file << "Message: " << std::endl;
        log_file << "\t" << message << std::endl;
        log_file << std::endl;
    } else {
        std::cerr << "Failed to open log file: " << STR_ABORT_LOG_FILENAME << std::endl;
    }

    log_file.close();
}