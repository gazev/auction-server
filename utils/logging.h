#ifndef __LOGGING_H__
#define __LOGGING_H__

typedef enum {
    LOG_NORMAL  = 0,
    LOG_VERBOSE = 1,
    LOG_DEBUG   = 2
} log_level_t;

void set_log_level(log_level_t level);
extern log_level_t g_level;

#define LOG(...)                                                             \
    do {                                                                     \
        char buf[2048];                                                      \
        snprintf(buf, 2048, __VA_ARGS__);                                    \
        fprintf(stdout, "[LOG]: %s\n", buf);                                 \
    } while (0);


#define LOG_VERBOSE(...)                                                     \
    do {                                                                     \
        if (g_level) {                                                       \
            char buf[2048];                                                  \
            snprintf(buf, 2048, __VA_ARGS__);                                \
            fprintf(stdout, "[LOG]: %s\n", buf);                             \
        }                                                                    \
    } while (0);


#define LOG_ERROR(...)                                    \
    do {                                                  \
        char buf[2048];                                   \
        snprintf(buf, 2048, __VA_ARGS__);                 \
        fprintf(stdout, "[ERROR]: %s\n", buf);            \
    } while (0);


#define LOG_WARN(...)                                                           \
    do {                                                                        \
        char buf[2048];                                                         \
        snprintf(buf, 2048, __VA_ARGS__);                                       \
        fprintf(stderr, "[WARN]: %s\n", buf);                                   \
    } while (0);


#define LOG_DEBUG(...)                                                         \
    do {                                                                       \
        if (g_level == LOG_DEBUG) {                                            \
            char buf[2048];                                                    \
            snprintf(buf, 2048, __VA_ARGS__);                                  \
            fprintf(stdout, "[DEBUG]: %s:%d :: %s :: %s\n", __FILE__,          \
                    __LINE__, __func__, buf);                                  \
        }                                                                      \
    } while (0);

#endif