#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#include <svsErr.h>
#include <svsLog.h>
#include <svsCommon.h>
#include <svsApi.h>
#include <svsCallback.h>
#include <svsBdp.h>
#include <svsSocket.h>
#include <libSVS.h>
#include "logger.h"


static bool do_fork = true;
static char *logfile;

void process_signal(int sig)
{
    switch (sig)
    {
        case SIGINT:
        case SIGQUIT:
        case SIGTERM:
            svsServerUninit();
            exit(0);
        break;
    }
}

static void usage(char *name)
{
    printf("Usage: %s [-d]\n", basename(name));
    printf("\t-d: don't automatically fork\n");
    exit(1);
}

static void process_args(int argc, char **argv)
{
    int c = 0;

    while ((c = getopt(argc, argv, "dl:")) != -1)
    {
        switch (c)
        {
            case 'd':
                do_fork = false;
            break;
            case 'l':
                logfile = optarg;
            break;
            default:
                usage(argv[0]);
            break;
        }
    }
}

int main(int argc, char *argv[])
{
    process_args(argc, argv);

    if (do_fork)
    {
        switch (fork())
        {
            case 0:
            break;
            case -1:
                printf("[main] unable to fork()\n");
            default:
                exit(0);
        }
    }

    signal(SIGINT, process_signal);
    signal(SIGQUIT, process_signal);
    signal(SIGTERM, process_signal);

    svsServerInit("SVSD", logfile);
    while (1)
    {
        int status;

        /* Block forever... */
        status = select(0, NULL, NULL, NULL, NULL);
        printf("svsd select returned\n");
    }

    exit(0);
}

