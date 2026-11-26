/* The test runner. Every tests/<module>/test_*.c file adds its tests to
 * this one program. */
#include <stdio.h>

#include "utest.h"

UTEST_STATE();

int main(int argc, const char *const argv[])
{
    /* Print each line at once instead of collecting output first. When a
     * test fails under the sanitizers, the program can be stopped before
     * collected output is written, and the name of the failed test would
     * be lost. */
    setvbuf(stdout, NULL, _IONBF, 0);
    return utest_main(argc, argv);
}
