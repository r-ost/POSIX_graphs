#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <limits.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>

#define MAX_COMMAND 32

#define ERR(source) (perror(source),                                 \
                     fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), \
                     exit(EXIT_FAILURE))

// FIRST START ./prog BEFORE STARTING THIS PROGRAM (fifo file must exist)

int main(int argc, char *argv[])
{
    char *FIFO_FILE = "graph.fifo";
    int fifo;
    printf("To exit client program and close one end of fifo type 'exit'\n");

    if ((fifo = open(FIFO_FILE, O_WRONLY)) < 0)
        ERR("open");

    for (;;)
    {
        char buf[MAX_COMMAND];
        memset(buf, 0, MAX_COMMAND);

        if (fgets(buf, MAX_COMMAND, stdin) == NULL)
            ERR("fgets");

        buf[strlen(buf) - 1] = 0; // delete new line sign

        if (!strcmp(buf, "exit"))
            break;

        if (write(fifo, buf, MAX_COMMAND) < 0)
            ERR("write");
    }

    if (close(fifo) < 0)
        ERR("close");

    return EXIT_SUCCESS;
}