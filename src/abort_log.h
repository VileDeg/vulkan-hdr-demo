#pragma once

#define STR_ABORT_LOG_FILENAME std::string("ABORT_LOG.txt")

#define TEXT_ABORT_MSG \
    "Program wasn't able to run, it may be because of invalid working directory. \n\
    Try running the program from the root directory (i.e. same dir, where assets/ folder is)."

// In case program crashes, this function will be called to dump some message to a log file
void dump_to_log_file(std::string message);