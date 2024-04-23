#include "abort_log.h"

void dump_to_log_file(std::string message) {
    std::string log_file_name = "ABORT_LOG.txt";

    std::ofstream log_file(log_file_name, std::ios::app);

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
        std::cerr << "Failed to open log file: " << log_file_name << std::endl;
    }

    log_file.close();
}