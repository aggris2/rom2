/* Settings for the leak checker, which asks for them by these names
 * when the program ends. This file is part of a build with the
 * sanitizers only.
 */

const char *__lsan_default_suppressions(void);
const char *__lsan_default_options(void);

/* NVIDIA's graphics driver keeps a few blocks it never frees. Reported
 * at every exit, they would hide a leak of our own, so leaks that come
 * through that driver are passed over. Every other leak is reported. */
const char *__lsan_default_suppressions(void)
{
    return "leak:libnvidia-glcore\n";
}

/* No table of what was passed over at every exit. */
const char *__lsan_default_options(void)
{
    return "print_suppressions=0";
}
