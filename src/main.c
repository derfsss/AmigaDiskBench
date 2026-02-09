#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>

#include "debug.h"
#include "gui.h"
#include "version.h"

static const char *const __attribute__((used)) ver = VER_STRING;

int main(int argc, char **argv)
{
    LOG_DEBUG("Program starting: %s %s (%s)...", APP_TITLE, APP_VERSION_STR, APP_DATE);
    int result = StartGUI();
    LOG_DEBUG("Program exiting with code %d", result);
    return result;
}
