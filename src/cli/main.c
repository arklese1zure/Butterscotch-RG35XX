#include "args.h"
#include <loop.h>
#include <getopt.h>

/* For SDL_main */
#if defined(USE_SDL1)
#include <SDL/SDL_main.h>
#elif defined(USE_SDL2)
#include <SDL2/SDL_main.h>
#elif defined(USE_SDL3)
#include <SDL3/SDL_main.h>
#endif

static bool logColour;

void platformLog(const logType type, const char *format, va_list va) {
    FILE *out = stderr;
    const char* colourPrefix = ANSI_COLOUR_CODE_RESET;
    const char* textPrefix = "";
    switch (type) {
        case LOG_TYPE_NORMAL:
            out = stdout;
            break;
        case LOG_TYPE_WARNING:
            colourPrefix = ANSI_COLOUR_CODE_BOLD_YELLOW;
            textPrefix = "Warning: ";
            break;
        case LOG_TYPE_ERROR:
            colourPrefix = ANSI_COLOUR_CODE_BOLD_RED;
            textPrefix = "Error: ";
            break;
        case LOG_TYPE_DEBUG:
            colourPrefix = ANSI_COLOUR_CODE_BOLD_PURPLE;
            textPrefix = "Debug: ";
            break;
    }

    if (logColour) fputs(colourPrefix, out);
    fputs(textPrefix, out);
    if (logColour) fputs(ANSI_COLOUR_CODE_RESET, out);
    vfprintf(out, format, va);
}

#ifdef TARGET_RG35XX
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <ucontext.h>

extern char __executable_start, etext;

// There's no debugger on the device, so report enough to run addr2line on the PC:
// the fault registers, plus anything on the stack that looks like a return address.
static void rg35xxCrashHandler(int sig, siginfo_t* info, void* context) {
    const mcontext_t* m = &((const ucontext_t*) context)->uc_mcontext;
    uintptr_t textStart = (uintptr_t) &__executable_start;
    uintptr_t textEnd = (uintptr_t) &etext;

    fprintf(stderr, "CRASH signal=%d addr=%p pc=%08lx lr=%08lx sp=%08lx\n", sig, info->si_addr,
            (unsigned long) m->arm_pc, (unsigned long) m->arm_lr, (unsigned long) m->arm_sp);
    const uint32_t* sp = (const uint32_t*) m->arm_sp;
    for (int i = 0, found = 0; i < 256 && found < 32; i++) {
        if (sp[i] >= textStart && sp[i] < textEnd) {
            fprintf(stderr, "CRASH stack %08lx\n", (unsigned long) sp[i]);
            found++;
        }
    }

    signal(sig, SIG_DFL);
    raise(sig);
}

// Test hook: a sampling profiler for a single room, since the device has no perf or gprof.
// Writes "address count" lines to profile.txt on exit, for addr2line on the PC.
#include <stdlib.h>
#include <sys/time.h>

volatile int rg35xxProfileActive = 0;
static uint32_t* rgProfileCounts = NULL;
static uint32_t rgProfileOutside = 0;

static void rg35xxProfileHandler(int sig, siginfo_t* info, void* context) {
    (void) sig; (void) info;
    if (!rg35xxProfileActive) return;
    uintptr_t pc = ((const ucontext_t*) context)->uc_mcontext.arm_pc;
    uintptr_t textStart = (uintptr_t) &__executable_start;
    if (pc >= textStart && pc < (uintptr_t) &etext) rgProfileCounts[(pc - textStart) / 4]++;
    else rgProfileOutside++;
}

static void rg35xxWriteProfile(void) {
    struct itimerval off = {{0, 0}, {0, 0}};
    setitimer(ITIMER_PROF, &off, NULL);
    FILE* f = fopen("profile.txt", "w");
    if (f == NULL) return;
    uintptr_t textStart = (uintptr_t) &__executable_start;
    size_t words = ((uintptr_t) &etext - textStart) / 4;
    fprintf(f, "outside %lu\n", (unsigned long) rgProfileOutside);
    for (size_t i = 0; i < words; i++) {
        if (rgProfileCounts[i] != 0)
            fprintf(f, "%08lx %lu\n", (unsigned long) (textStart + i * 4), (unsigned long) rgProfileCounts[i]);
    }
    fclose(f);
}

static void rg35xxStartProfiler(void) {
    if (getenv("AM2R_PROFILE_ROOM") == NULL) return;
    rgProfileCounts = (uint32_t*) calloc(((uintptr_t) &etext - (uintptr_t) &__executable_start) / 4, sizeof(uint32_t));
    if (rgProfileCounts == NULL) return;

    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = rg35xxProfileHandler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(SIGPROF, &sa, NULL);

    struct itimerval timer = {{0, 1000}, {0, 1000}};
    setitimer(ITIMER_PROF, &timer, NULL);
    atexit(rg35xxWriteProfile);
}

void rg35xxInstallCrashHandler(void) {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = rg35xxCrashHandler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
}
#endif

int main(int argc, char* argv[]) {
    setbuf(stderr, NULL);
#ifdef TARGET_RG35XX
    rg35xxInstallCrashHandler();
    rg35xxStartProfiler();
#endif

    CommandLineArgs args;
    parseCommandLineArgs(&args, argc, argv);
    logColour = !args.disableLogColours;
    int ret = loop(args, argv[0]);
    freeCommandLineArgs(&args);
    return ret;
}
