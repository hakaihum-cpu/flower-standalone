#include <glob.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define REQUIRE(x) do { if (!(x)) { fprintf(stderr, "[FAIL] glob line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(int argc, char **argv)
{
    glob_t results = {0};
    int status;
    REQUIRE(argc == 2);
    status = glob(argv[1], 0, NULL, &results);
    REQUIRE(status == 0);
    REQUIRE(results.gl_pathc == 2);
    REQUIRE(strstr(results.gl_pathv[0], "a.pd") != NULL);
    REQUIRE(strstr(results.gl_pathv[1], "b.pd") != NULL);
    globfree(&results);
    REQUIRE(results.gl_pathv == NULL && results.gl_pathc == 0);
    status = glob("effects_unmatched_glob_test_?.pd", 0, NULL, &results);
    REQUIRE(status == GLOB_NOMATCH);
    globfree(&results);  /* The pinned Pure Data source does this too. */
    status = glob("effects_unmatched_glob_test_?.pd", GLOB_NOCHECK, NULL, &results);
    REQUIRE(status == 0 && results.gl_pathc == 1);
    globfree(&results);
    status = glob(argv[1], 0, NULL, &results);
    REQUIRE(status == 0 && results.gl_pathc == 2);
    status = glob(argv[1], GLOB_APPEND, NULL, &results);
    REQUIRE(status == 0 && results.gl_pathc == 4);
    REQUIRE(results.gl_pathv[4] == NULL);
    globfree(&results);
    puts("[PASS] Android API24-27 glob: wildcards, sorted, no-match, no-check, append, free");
    return 0;
}
