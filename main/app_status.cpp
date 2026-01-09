#include "app_status.h"

#include "app_globals.h"

void build_selftest_text(char *buf, size_t buf_size) {
    system_status_build_selftest_text(&g_hw_status, buf, buf_size);
}
