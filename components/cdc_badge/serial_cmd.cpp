// Serial Command Interface for CDC Badge
// Commands:
//   SET_TIME HH:MM:SS    - Set current time
//   SET_DATE YYYY-MM-DD  - Set current date (or Unix timestamp)
//   SET_NAME text        - Set name line on display
//   SET_INFO text        - Set info line on display
//   SET_INFO2 text       - Set info2 line on display
//   GET_TIME             - Get current time
//   GET_DATE             - Get current date
//   HELP                 - Show available commands

#include "serial_cmd.h"
#include "cdc_rtc.h"
#include "cdc_log.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <time.h>

#define CMD_BUFFER_SIZE 128

static char cmd_buffer[CMD_BUFFER_SIZE];
static int cmd_buffer_pos = 0;
static serial_cmd_text_callback_t text_callback = nullptr;
static serial_cmd_time_callback_t time_callback = nullptr;

// Show prompt
static void show_prompt(void) {
    printf("> ");
    fflush(stdout);
}

// Trim whitespace from string
static char* trim(char *str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    return str;
}

// Show help
static void show_help(void) {
    printf("=== CDC Badge Commands ===\r\n");
    printf("\r\n");
    printf("Time:\r\n");
    printf("  SET_TIME HH:MM:SS    - Set current time\r\n");
    printf("  SET_DATE YYYY-MM-DD  - Set current date\r\n");
    printf("  SET_DATE <timestamp> - Set date/time from Unix timestamp\r\n");
    printf("  GET_TIME             - Get current time\r\n");
    printf("  GET_DATE             - Get current date\r\n");
    printf("\r\n");
    printf("Display:\r\n");
    printf("  SET_NAME text        - Set name on display\r\n");
    printf("  SET_INFO text        - Set info line on display\r\n");
    printf("  SET_INFO2 text       - Set info2 line on display\r\n");
    printf("\r\n");
    printf("System:\r\n");
    printf("  HELP                 - Show this help\r\n");
    printf("\r\n");
    fflush(stdout);
}

// Parse and execute command
static void execute_command(char *cmd) {
    cmd = trim(cmd);
    if (strlen(cmd) == 0) return;

    LOG_I("CMD", "Received: %s", cmd);

    // HELP
    if (strcasecmp(cmd, "HELP") == 0) {
        show_help();
        return;
    }

    // SET_TIME HH:MM:SS
    if (strncasecmp(cmd, "SET_TIME ", 9) == 0) {
        int hour, minute, second;
        if (sscanf(cmd + 9, "%d:%d:%d", &hour, &minute, &second) == 3) {
            if (hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59 && second >= 0 && second <= 59) {
                cdc_rtc_set_time(hour, minute, second);
                printf("OK: Time set to %02d:%02d:%02d\r\n", hour, minute, second);
                if (time_callback) time_callback();
            } else {
                printf("ERROR: Invalid time values\r\n");
            }
        } else {
            printf("ERROR: Usage: SET_TIME HH:MM:SS\r\n");
        }
        return;
    }

    // SET_DATE YYYY-MM-DD or Unix timestamp
    if (strncasecmp(cmd, "SET_DATE ", 9) == 0) {
        char *arg = trim(cmd + 9);
        int year, month, day;
        long timestamp;

        // Try Unix timestamp first (pure number)
        if (sscanf(arg, "%ld", &timestamp) == 1 && strchr(arg, '-') == nullptr) {
            struct timeval tv = { .tv_sec = timestamp, .tv_usec = 0 };
            settimeofday(&tv, NULL);
            cdc_rtc_mark_time_set();
            struct tm timeinfo;
            localtime_r(&tv.tv_sec, &timeinfo);
            printf("OK: DateTime set from timestamp %ld (%04d-%02d-%02d %02d:%02d:%02d)\r\n",
                   timestamp,
                   timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                   timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            if (time_callback) time_callback();
        }
        // Try YYYY-MM-DD format
        else if (sscanf(arg, "%d-%d-%d", &year, &month, &day) == 3) {
            if (year >= 2020 && year <= 2099 && month >= 1 && month <= 12 && day >= 1 && day <= 31) {
                cdc_rtc_set_date(year, month, day);
                printf("OK: Date set to %04d-%02d-%02d\r\n", year, month, day);
                if (time_callback) time_callback();
            } else {
                printf("ERROR: Invalid date values\r\n");
            }
        } else {
            printf("ERROR: Usage: SET_DATE YYYY-MM-DD or SET_DATE <timestamp>\r\n");
        }
        return;
    }

    // SET_NAME text
    if (strncasecmp(cmd, "SET_NAME ", 9) == 0) {
        char *text = trim(cmd + 9);
        if (strlen(text) > 0) {
            printf("OK: Name set to: %s\r\n", text);
            if (text_callback) text_callback(0, text);
        } else {
            printf("ERROR: Usage: SET_NAME text\r\n");
        }
        return;
    }

    // SET_INFO text
    if (strncasecmp(cmd, "SET_INFO ", 9) == 0) {
        char *text = trim(cmd + 9);
        if (strlen(text) > 0) {
            printf("OK: Info set to: %s\r\n", text);
            if (text_callback) text_callback(1, text);
        } else {
            printf("ERROR: Usage: SET_INFO text\r\n");
        }
        return;
    }

    // SET_INFO2 text
    if (strncasecmp(cmd, "SET_INFO2 ", 10) == 0) {
        char *text = trim(cmd + 10);
        if (strlen(text) > 0) {
            printf("OK: Info2 set to: %s\r\n", text);
            if (text_callback) text_callback(2, text);
        } else {
            printf("ERROR: Usage: SET_INFO2 text\r\n");
        }
        return;
    }

    // GET_TIME
    if (strcasecmp(cmd, "GET_TIME") == 0) {
        char time_str[16];
        cdc_rtc_get_time_str(time_str, sizeof(time_str));
        printf("TIME: %s\r\n", time_str);
        return;
    }

    // GET_DATE
    if (strcasecmp(cmd, "GET_DATE") == 0) {
        char date_str[16];
        cdc_rtc_get_date_str(date_str, sizeof(date_str));
        printf("DATE: %s\r\n", date_str);
        return;
    }

    printf("ERROR: Unknown command. Type HELP for available commands.\r\n");
}

void serial_cmd_init(void) {
    cmd_buffer_pos = 0;
    memset(cmd_buffer, 0, sizeof(cmd_buffer));
    LOG_I("CMD", "Serial command interface initialized");
    show_prompt();
}

bool serial_cmd_process(void) {
    int c = getchar();
    if (c == EOF) {
        return false;
    }

    // Handle newline (CR or LF) - command complete
    if (c == '\r' || c == '\n') {
        printf("\r\n");
        fflush(stdout);
        if (cmd_buffer_pos > 0) {
            cmd_buffer[cmd_buffer_pos] = '\0';
            execute_command(cmd_buffer);
            cmd_buffer_pos = 0;
            memset(cmd_buffer, 0, sizeof(cmd_buffer));
            show_prompt();
            return true;
        }
        // Empty line - just show new prompt
        show_prompt();
        return false;
    }

    // Handle backspace (DEL=0x7F or BS=0x08)
    if (c == 0x7F || c == 0x08) {
        if (cmd_buffer_pos > 0) {
            cmd_buffer_pos--;
            cmd_buffer[cmd_buffer_pos] = '\0';
            // Erase character on terminal: backspace, space, backspace
            printf("\b \b");
            fflush(stdout);
        }
        return false;
    }

    // Handle Ctrl+C - cancel current line
    if (c == 0x03) {
        printf("^C\r\n");
        cmd_buffer_pos = 0;
        memset(cmd_buffer, 0, sizeof(cmd_buffer));
        show_prompt();
        return false;
    }

    // Handle Ctrl+U - clear line
    if (c == 0x15) {
        while (cmd_buffer_pos > 0) {
            printf("\b \b");
            cmd_buffer_pos--;
        }
        memset(cmd_buffer, 0, sizeof(cmd_buffer));
        fflush(stdout);
        return false;
    }

    // Add printable character to buffer
    if (c >= 0x20 && c < 0x7F && cmd_buffer_pos < CMD_BUFFER_SIZE - 1) {
        cmd_buffer[cmd_buffer_pos++] = (char)c;
        putchar(c);
        fflush(stdout);
    }

    return false;
}

void serial_cmd_set_text_callback(serial_cmd_text_callback_t callback) {
    text_callback = callback;
}

void serial_cmd_set_time_callback(serial_cmd_time_callback_t callback) {
    time_callback = callback;
}
