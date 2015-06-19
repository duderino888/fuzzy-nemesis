#ifndef LOG_H
#define LOG_H


#define LOG_INFO        0
#define LOG_WARN        1
#define LOG_ERROR       2
#define LOG_DEBUG       3


#define log_info(...)   log_msg(LOG_INFO, __VA_ARGS__)
#define log_warn(...)   log_msg(LOG_WARN, ___VA_ARGS__)
#define log_error(...)  log_msg(LOG_ERROR, __VA_ARGS__)
#define log_debug(...)  do{if(DEBUG)log_msg(LOG_DEBUG,__VA_ARGS__);}while(0)


void log_msg(int level, const char *msg, ...);


#endif /* LOG_H */

