// Oświadczam, że niniejsza praca stanowiąca podstawę do uznania osiągnięcia efektów
// uczenia się z przedmiotu SOP2 została wykonana przeze mnie samodzielnie.
// [Jan Szablanowski]
// [305893]

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

//////////////////////  UTILS  //////////////////////

#define ERR(source) (perror(source),                                 \
                     fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), \
                     exit(EXIT_FAILURE))

void usage()
{
    fprintf(stderr, "USAGE: \n");
    exit(EXIT_FAILURE);
}

//////////////////////  SIGNALS  //////////////////////
volatile sig_atomic_t last_signal;

int set_handler(void (*f)(int), int sig)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sig, &act, NULL))
        return -1;

    return 0;
}

void sig_handler(int sig)
{
    last_signal = sig;
}

//////////////////////  CHILDREN //////////////////////

void child_work(int **fds, int N, int vertexNo)
{
    ssize_t count;
    char buf;
    int hasEdgeWith[N];          // neighbours
    int isConnectedWith[N];      // connections to other vertex (doesn't have to be direct)
    int isNegativeConnection[N]; // "negative" connections
                                 //      (there is a path consisting of edges facing opposite direction)

    isConnectedWith[vertexNo] = 1;

    for (int i = 0; i < N; i++)
        hasEdgeWith[i] = 0;

    if (set_handler(sig_handler, SIGINT))
        ERR("set_handler");

    // read text in loop from vertex file descriptor
    for (;;)
    {
        count = read(fds[vertexNo][0], &buf, sizeof(char));

        // exit if other end of pipe is closed or received SIGINT
        if (count == 0 || (count < 0 && errno == EINTR && last_signal == SIGINT))
            break;
        else if (count < 0)
            ERR("read");

        if (buf == 'p') // print info about edges from this vertex
        {
            for (int i = 0; i < N; i++)
            {
                if (hasEdgeWith[i])
                    printf("%d --> %d\n", vertexNo, i);
            }
        }
        else if (buf == 'a') // add new edge
        {
            int hasNewEdgeWith;
            if (TEMP_FAILURE_RETRY(read(fds[vertexNo][0], &hasNewEdgeWith, sizeof(int)) < 0))
                ERR("read");

            printf("ADDING NEW EDGE (%d --> %d)\n", vertexNo, hasNewEdgeWith);

            hasEdgeWith[hasNewEdgeWith] = 1;
            isConnectedWith[hasNewEdgeWith] = 1;

            // send info to other vertex from edge about my negative connections
            char comm = 'n';
            if (TEMP_FAILURE_RETRY(write(fds[hasNewEdgeWith][1], &comm, sizeof(char)) < 0))
                ERR("write");

            if (TEMP_FAILURE_RETRY(write(fds[hasNewEdgeWith][1], &vertexNo, sizeof(int)) < 0))
                ERR("write");

            if (TEMP_FAILURE_RETRY(write(fds[hasNewEdgeWith][1], isNegativeConnection, sizeof(int) * N) < 0))
                ERR("write");
        }
        else if (buf == 'c') // give info about connection with other vertex
        {
            int y;
            if (TEMP_FAILURE_RETRY(read(fds[vertexNo][0], &y, sizeof(int)) < 0))
                ERR("read");

            if (isConnectedWith[y] == 1)
                printf("%d is connected with %d\n", vertexNo, y);
            else
                printf("%d is NOT connected with %d\n", vertexNo, y);
        }
        else if (buf == 'n') // update info about negative connections
        {
            int src;
            if (TEMP_FAILURE_RETRY(read(fds[vertexNo][0], &src, sizeof(int)) < 0))
                ERR("read");

            isNegativeConnection[src] = 1;

            int newNeighbourNegativeConn[N];

            if (TEMP_FAILURE_RETRY(read(fds[vertexNo][0], newNeighbourNegativeConn, sizeof(int) * N) < 0))
                ERR("read");

            for (int i = 0; i < N; i++)
            {
                if (i != vertexNo && newNeighbourNegativeConn[i] == 1)
                {
                    isNegativeConnection[i] = 1;
                }
            }

            for (int i = 0; i < N; i++)
            {
                if (i != vertexNo && isNegativeConnection[i] == 1)
                {
                    char comm = 'u';
                    if (TEMP_FAILURE_RETRY(write(fds[i][1], &comm, sizeof(char)) < 0))
                        ERR("write");

                    if (TEMP_FAILURE_RETRY(write(fds[i][1], &vertexNo, sizeof(int)) < 0))
                        ERR("write");
                }
            }

            for (int i = 0; i < N; i++)
            {
                if (i != vertexNo && i != src && isConnectedWith[i] == 1)
                {
                    char comm = 'u';

                    if (TEMP_FAILURE_RETRY(write(fds[src][1], &comm, sizeof(char)) < 0))
                        ERR("write");

                    int temp = i;
                    if (TEMP_FAILURE_RETRY(write(fds[src][1], &temp, sizeof(int)) < 0))
                        ERR("write");
                }
            }
        }
        else if (buf == 'u') // upadate connections
        {
            int newConnection;
            if (TEMP_FAILURE_RETRY(read(fds[vertexNo][0], &newConnection, sizeof(int)) < 0))
                ERR("read");
            isConnectedWith[newConnection] = 1;

            // printf("UPDATE: %d is connected with %d\n", vertexNo, newConnection);
        }
    }

    // close file descriptors
    for (int i = 0; i < N; i++)
    {
        if (TEMP_FAILURE_RETRY(close(fds[i][0])))
            ERR("close");

        if (TEMP_FAILURE_RETRY(close(fds[i][1])))
            ERR("close");
    }

    // finish execution
    exit(EXIT_SUCCESS);
}

void create_children(int **fds, int N)
{
    pid_t pid;

    // create pipes
    for (int i = 0; i < N; i++)
    {
        if (pipe(fds[i]))
            ERR("pipe");
    }

    for (int i = 0; i < N; i++)
    {
        pid = fork();
        if (pid == 0) // child
        {
            child_work(fds, N, i);
        }
        else if (pid < 0)
            ERR("fork");
    }
}

//////////////////////  COMMANDS  //////////////////////

void printEdges(int **fds, int N)
{
    char command = 'p';

    printf("EDGES: \n");
    for (int i = 0; i < N; i++)
    {
        // request for edges from i-th process; child responses with printing its neighbours
        if (TEMP_FAILURE_RETRY(write(fds[i][1], &command, sizeof(char)) < 0))
            ERR("write");
    }
}

void addEdge(int **fds, int x, int y)
{
    // send info about new neighbour
    char buf = 'a';
    if (TEMP_FAILURE_RETRY(write(fds[x][1], &buf, sizeof(char)) < 0))
        ERR("write");
    int hasNewEdgeWith = y;
    if (TEMP_FAILURE_RETRY(write(fds[x][1], &hasNewEdgeWith, sizeof(int)) < 0))
        ERR("write");
}

void getConnection(int x, int y, int **fds)
{
    // request for connection info
    char command = 'c';
    int dest = y;
    if (TEMP_FAILURE_RETRY(write(fds[x][1], &command, sizeof(char)) < 0))
        ERR("write");

    if (TEMP_FAILURE_RETRY(write(fds[x][1], &dest, sizeof(int)) < 0))
        ERR("write");
}

int getNumArguments(int *x, int *y, int N)
{
    char *num = strtok(NULL, " ");
    if (num == NULL)
    {
        printf("Not enough arguments!!!\n");
        return 1;
    }
    *x = atoi(num);

    num = strtok(NULL, " ");
    if (num == NULL)
    {
        printf("Not enough arguments!!!\n");
        return 1;
    }
    *y = atoi(num);

    if ((*x < 0 || *x >= N) || (*y < 0 || *y >= N))
    {
        printf("Invalid arguments (vertex doesn't exist)!!!\n");
        return 1;
    }
    return 0;
}

void executeCommand(char *buf, int **fds, int N)
{
    char *commandType;
    commandType = strtok(buf, " ");
    if (commandType == NULL)
        return;

    if (!strcmp(buf, "print"))
    {
        printEdges(fds, N);
    }
    else if (!strcmp(commandType, "add"))
    {
        int x, y;
        if (getNumArguments(&x, &y, N) == 0)
            addEdge(fds, x, y);
    }
    else if (!strcmp(commandType, "conn"))
    {
        int x, y;
        if (getNumArguments(&x, &y, N) == 0)
            getConnection(x, y, fds);
    }
    else
        printf("Invalid operation!!!\n");
}

//////////////////////  SUPERVISOR PROCESS  //////////////////////

void supervisor_work(int **fds, int N)
{
    int fifo;

    // name of fifo file
    char *FIFO_FILE = "graph.fifo";

    // set handler to correctly handle SIGINT
    if (set_handler(sig_handler, SIGINT))
        ERR("set handler");

    // make fifo; if it exists, make new one
    if (mkfifo(FIFO_FILE, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP) < 0)
    {
        if (errno != EEXIST) // error occured
            ERR("mkfifo");
        else // fifo exists, delete it and create new one
        {
            if (unlink(FIFO_FILE) < 0)
                ERR("unlink");

            if (mkfifo(FIFO_FILE, S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP) < 0)
                ERR("mkfifo");
        }
    }

    // open fifo for reading
    if ((fifo = open(FIFO_FILE, O_RDONLY)) < 0)
        ERR("open");

    // buffer with commands read from fifo
    char buf[MAX_COMMAND];

    // read commands in loop from fifo
    for (;;)
    {
        int count;
        memset(buf, 0, MAX_COMMAND);
        count = read(fifo, buf, MAX_COMMAND);

        // check if parent process received SIGINT
        if (count < 0 && errno == EINTR && last_signal == SIGINT)
            break;
        else if (count == 0) // check if other end of fifo is closed; if so, send SIGINT to children and finish execution
        {
            kill(0, SIGINT);
            break;
        }
        else if (count < 0)
            ERR("read");

        // execute proper command
        executeCommand(buf, fds, N);
    }

    // wait for all children
    for (int i = 0; i < N; i++)
        wait(NULL);

    if (TEMP_FAILURE_RETRY(close(fifo)) < 0)
        ERR("close");

    // remove the FIFO entry
    if (unlink(FIFO_FILE) < 0)
        ERR("unlink");
}

//////////////////////  MAIN  //////////////////////

int main(int argc, char *argv[])
{
    // create children and pipes
    //int fds2[N][2];
    int **fds;

    if (argc != 2)
        usage();

    int N = atoi(argv[1]);

    // allocate memory for fds
    fds = (int **)malloc(sizeof(int *) * N);
    for (int i = 0; i < N; i++)
        fds[i] = (int *)malloc(sizeof(int) * 2);

    create_children(fds, N);

    supervisor_work(fds, N);

    // close all file descriptors in parent process
    for (int i = 0; i < N; i++)
    {
        if (TEMP_FAILURE_RETRY(close(fds[i][0])))
            ERR("close");
        if (TEMP_FAILURE_RETRY(close(fds[i][1])))
            ERR("close");
    }

    // free alocated memory
    for (int i = 0; i < N; i++)
        free(fds[i]);
    free(fds);

    return EXIT_SUCCESS;
}