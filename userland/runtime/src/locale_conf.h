#ifndef RELIEFOS_LIBC_LOCALE_CONF_H
#define RELIEFOS_LIBC_LOCALE_CONF_H

#include <stddef.h>

#define RELIEFOS_LOCALE_NAME_LEN  16
#define RELIEFOS_LOCALE_VALUE_LEN 32
#define RELIEFOS_LOCALE_MAX       8

struct reliefos_locale_setting {
    char name[RELIEFOS_LOCALE_NAME_LEN];
    char value[RELIEFOS_LOCALE_VALUE_LEN];
};

/**
 * @brief 解析 systemd 风格 locale.conf 文本；只接受 LANG 与 LC_<类别> 键，
 *        其余键与非法行忽略。文件读取由调用方完成，本函数不触任何 syscall。
 * @param text 文件内容，不要求 NUL 结尾。
 * @param length text 字节数。
 * @param out 输出数组，由调用方提供。
 * @param capacity out 的元素个数，须不小于 RELIEFOS_LOCALE_MAX 才能收满全部类别。
 * @return 写入的条目数；0 表示无有效条目。同名键后出现者覆盖先出现者。
 */
int reliefos_locale_parse(const char *text, size_t length,
                        struct reliefos_locale_setting *out, int capacity);

#endif
