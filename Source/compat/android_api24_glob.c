/* EFFECTS: Android API24-27 glob/globfree for libpd [file glob].
 * Android bionic exposes these libc functions only since API 28.
 * Support basic POSIX globbing and the flags used by Pure Data.
 * No build-time change to the pinned upstream Pure Data sources.
 */
#if (defined(__ANDROID__) && defined(__ANDROID_API__) && __ANDROID_API__ < 28) || defined(EFFECTS_GLOB_COMPAT_TEST)
#include <glob.h>
#include <fnmatch.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

struct effects_glob_scan {
    char **entries;
    size_t count;
    size_t capacity;
    int flags;
    int (*on_error)(const char *, int);
};
static int effects_glob_cmp(const void *lhs, const void *rhs)
{
    return strcmp(*(const char *const *)lhs, *(const char *const *)rhs);
}
static void effects_glob_scan_free(struct effects_glob_scan *scan)
{
    size_t i;
    for (i = 0; i < scan->count; ++i) free(scan->entries[i]);
    free(scan->entries);
    scan->entries = NULL;
    scan->count = scan->capacity = 0;
}
static int effects_glob_push(struct effects_glob_scan *scan,
                             const char *path, int is_dir)
{
    size_t n = strlen(path), capacity;
    char *copy;
    if (scan->count >= 8192 || n >= PATH_MAX - 2) return GLOB_NOSPACE;
    if (scan->count == scan->capacity) {
        char **next;
        capacity = scan->capacity ? scan->capacity * 2 : 16;
        next = (char **)realloc(scan->entries, capacity * sizeof(char *));
        if (!next) return GLOB_NOSPACE;
        scan->entries = next;
        scan->capacity = capacity;
    }
    copy = (char *)malloc(n + 2);
    if (!copy) return GLOB_NOSPACE;
    memcpy(copy, path, n + 1);
    if ((scan->flags & GLOB_MARK) && is_dir && n && path[n - 1] != '/') {
        copy[n] = '/';
        copy[n + 1] = '\0';
    }
    scan->entries[scan->count++] = copy;
    return 0;
}
static int effects_glob_expand(struct effects_glob_scan *scan,
                               const char *base, const char *pattern,
                               unsigned depth)
{
    char part[PATH_MAX], full[PATH_MAX];
    size_t part_len, base_len, suffix_len;
    const char *slash, *next;
    int wild = 0;
    size_t i;
    struct stat st;
    if (depth > 64) return GLOB_NOSPACE;
    while (*pattern == '/') ++pattern;
    if (!*pattern) {
        const char *target = *base ? base : ".";
        if (lstat(target, &st)) return 0;
        return effects_glob_push(scan, target, S_ISDIR(st.st_mode));
    }
    slash = strchr(pattern, '/');
    part_len = slash ? (size_t)(slash - pattern) : strlen(pattern);
    if (!part_len || part_len >= sizeof(part)) return GLOB_NOSPACE;
    memcpy(part, pattern, part_len);
    part[part_len] = '\0';
    next = slash ? slash + 1 : "";
    for (i = 0; i < part_len; ++i)
        if (part[i] == '*' || part[i] == '?' || part[i] == '[') wild = 1;
    base_len = strlen(base);
    suffix_len = base_len && base[base_len - 1] != '/' ? 1 : 0;
    if (base_len + suffix_len + part_len >= sizeof(full)) return GLOB_NOSPACE;
    if (!wild) {
        memcpy(full, base, base_len);
        if (suffix_len) full[base_len++] = '/';
        memcpy(full + base_len, part, part_len + 1);
        return effects_glob_expand(scan, full, next, depth + 1);
    }
    {
        const char *dirname = base_len ? base : ".";
        DIR *dir = opendir(dirname);
        struct dirent *item;
        int failure = 0;
        if (!dir) {
            if ((scan->on_error && scan->on_error(dirname, errno)) ||
                (scan->flags & GLOB_ERR)) return GLOB_ABORTED;
            return 0;
        }
        errno = 0;
        while ((item = readdir(dir)) != NULL) {
            size_t n = strlen(item->d_name);
            int code;
            if (fnmatch(part, item->d_name,
                        FNM_PERIOD | ((scan->flags & GLOB_NOESCAPE) ? FNM_NOESCAPE : 0)))
                continue;
            if (base_len + suffix_len + n >= sizeof(full)) continue;
            memcpy(full, base, base_len);
            if (suffix_len) full[base_len] = '/';
            memcpy(full + base_len + suffix_len, item->d_name, n + 1);
            code = effects_glob_expand(scan, full, next, depth + 1);
            if (code) { failure = code; break; }
        }
        closedir(dir);
        return failure;
    }
}
int glob(const char *pattern, int flags,
         int (*errfunc)(const char *, int), glob_t *results)
{
    struct effects_glob_scan scan = {0};
    size_t offs, old_count, i;
    char **combined;
    int status;
    if (!pattern || !results) return GLOB_ABORTED;
    offs = (flags & GLOB_DOOFFS) ? results->gl_offs : 0;
    old_count = (flags & GLOB_APPEND) ? results->gl_pathc : 0;
    if (!(flags & GLOB_APPEND)) {
        results->gl_pathc = 0;
        results->gl_pathv = NULL;
        results->gl_offs = offs;
    }
    scan.flags = flags;
    scan.on_error = errfunc;
    status = effects_glob_expand(&scan, (*pattern == '/') ? "/" : "", pattern, 0);
    if (status) { effects_glob_scan_free(&scan); return status; }
    if (scan.count == 0 && (flags & GLOB_NOCHECK)) {
        status = effects_glob_push(&scan, pattern, 0);
        if (status) { effects_glob_scan_free(&scan); return status; }
    }
    if (scan.count == 0) {
        effects_glob_scan_free(&scan);
        return GLOB_NOMATCH;
    }
    if (offs > SIZE_MAX / sizeof(char *) - 1 ||
        old_count > SIZE_MAX / sizeof(char *) - offs - 1 ||
        scan.count > SIZE_MAX / sizeof(char *) - offs - old_count - 1) {
        effects_glob_scan_free(&scan);
        return GLOB_NOSPACE;
    }
    combined = (char **)realloc((flags & GLOB_APPEND) ? results->gl_pathv : NULL,
                               (offs + old_count + scan.count + 1) * sizeof(char *));
    if (!combined) { effects_glob_scan_free(&scan); return GLOB_NOSPACE; }
    if (!(flags & GLOB_APPEND))
        for (i = 0; i < offs; ++i) combined[i] = NULL;
    if (!(flags & GLOB_NOSORT))
        qsort(scan.entries, scan.count, sizeof(char *), effects_glob_cmp);
    for (i = 0; i < scan.count; ++i)
        combined[offs + old_count + i] = scan.entries[i];
    combined[offs + old_count + scan.count] = NULL;
    free(scan.entries);
    results->gl_pathv = combined;
    results->gl_pathc = old_count + scan.count;
    results->gl_offs = offs;
    return 0;
}
void globfree(glob_t *results)
{
    size_t i;
    if (!results) return;
    if (results->gl_pathv) {
        for (i = 0; i < results->gl_pathc; ++i)
            free(results->gl_pathv[results->gl_offs + i]);
        free(results->gl_pathv);
    }
    results->gl_pathv = NULL;
    results->gl_pathc = 0;
}
#endif
