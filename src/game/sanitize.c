/* Settings for the leak checker, which asks for them by these names
 * when the program ends. This file is part of a build with the
 * sanitizers only.
 */

const char *__lsan_default_suppressions(void);
const char *__lsan_default_options(void);

/* Some blocks are never freed by the libraries that made them, and
 * were never meant to be. Reported at every exit, they would hide a
 * leak of our own, so these are passed over, and every other leak is
 * reported:
 *
 * - what NVIDIA's graphics driver keeps;
 * - what the system's message bus library keeps, which the driver and
 *   SDL both use;
 * - the tables the X11 library fills when SDL starts it. They count as
 *   lost only because SDL unloads that library before the checker
 *   looks. */
const char *__lsan_default_suppressions(void)
{
    return "leak:libnvidia-glcore\n"
        "leak:libdbus-1\n"
        "leak:X11_VideoInit\n";
}

/* No table of what was passed over at every exit. */
const char *__lsan_default_options(void)
{
    return "print_suppressions=0";
}
