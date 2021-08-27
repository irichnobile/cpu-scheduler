/*******************************************************************************
 * sched.c      Author: Ian Nobile
 *
 * This memory-leak free program simulates either the Non-Preemptive Priority
 * (NPP) or Round Robin (RR) algorithms of a CPU scheduler based on user input,
 * and must be invoked via the command line as follows:
 *
 *    ./sched <input filepath> <output filepath> <NPP or RR>
 *    [quantum(RR only)] [limit(optional)]
 *
 *
 * Note that if RR is specified as the <algorithm>, then a positive integer
 * [quantum] value must also be provided as a command line argument
 * representing the length of the time quantum (time slice). A results file
 * called "out.txt" will appear at the location specified by the user, and each
 * line of this results file will contain a different process from the
 * previously provided input file in the following format:
 *
 *    <pid> <arrival-time> <finish-time> <waiting-time>
 *
 *
 * In the case of arrival ties, FCFS’s rule is used to break the tie in NPP and
 * new processes are put in the ready queue immediately after the process whose
 * time quantum has just expired while simulating RR. All units will be in
 * milliseconds, and all values, integers.
 *
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//------------------------------------------------------------------------------
//  Structs
//------------------------------------------------------------------------------
struct ProcessNode {
    int pid, arrival, burst, finish, waiting, leftover, priority;
    struct ProcessNode *next;
    struct ProcessNode *prev;
};

struct List {
    struct ProcessNode *head;
    struct ProcessNode *tail;
    int count;
};

//------------------------------------------------------------------------------
//  Function Prototypes
//------------------------------------------------------------------------------
struct ProcessNode *init_process(int pid, int arrivalTime, int burstTime, int priority);
void del_process(struct ProcessNode *node);
struct List *init_list();
void del_list(struct List *list);
void enqueue(struct List *list, struct ProcessNode *node);
struct ProcessNode *dequeue(struct List *queue);
void swapWithNext(struct ProcessNode *i);
void insSortPriority(struct List *list);
void arrivalChecker(struct List *ReadyQueue, struct List *JobQueue, int CLOCK);
void npp(struct List *ReadyQueue, struct List *ProcessQueue);
void rr(struct List *ReadyQueue, struct List *ProcessQueue, int iQuantum);
struct List *processQueue(struct List *ReadyQueue, char *cAlgorithm, int iQuantum);


int main(int argc, char *argv[]) {
    char cInputFilepath[256];
    char cOutputFilepath[256];
    char cAlgorithm[256];
    int iQuantum = 0;
    int iLimit = 0;

    //  handle command line args:
    if (argc < 4 || argc>6) {
        printf("Sorry, but something's not quite right about your invocation.");
        return 1;
    }
    if (argc >= 4 && argc <= 6) {
        strcpy(cInputFilepath, argv[1]);
        strcpy(cOutputFilepath, argv[2]);
        strcpy(cAlgorithm, argv[3]);
    }
    if (strcmp(cAlgorithm, "NPP") == 0 && argc == 5) {
        iLimit = atoi(argv[4]);
    } else if (strcmp(cAlgorithm, "RR") == 0 && argc == 4) {
        printf("Sorry, but simulating RR requires you specify a positive integer [quantum] value representing the length of the time quantum (time slice).\nPerhaps try the following invocation: ./sched in.txt out.txt RR 4\n");
        return 1;
    } else if (strcmp(cAlgorithm, "RR") == 0 && argc == 5) {
        iQuantum = atoi(argv[4]);
    } else if (strcmp(cAlgorithm, "RR") == 0 && argc == 6) {
        iQuantum = atoi(argv[4]);
        iLimit = atoi(argv[5]);
    }

    //  opening the input file for process import
    FILE *file;
    file = fopen(cInputFilepath, "r");
    if (file == NULL) {
        printf("Sorry, but there seems to be no such file at %s.\n", cInputFilepath);
        return 1;
    }

    //  process importation
    struct List *ReadyQueue = init_list();
    struct ProcessNode *currentProcess;
    int iPid;
    int iArrivalTime;
    int iBurstTime;
    int iPriority;
    //  handle limit first
    if (iLimit > 0) {
        for (int i = 0;i < iLimit;i++) {
            fscanf(file, "%d %d %d %d", &iPid, &iArrivalTime, &iBurstTime, &iPriority);
            currentProcess = init_process(iPid, iArrivalTime, iBurstTime, iPriority);
            currentProcess->leftover = currentProcess->burst;
            enqueue(ReadyQueue, currentProcess);
        }
    } else {
        //  otherwise, scan until eof
        while (fscanf(file, "%d %d %d %d", &iPid, &iArrivalTime, &iBurstTime, &iPriority) == 4) {
            currentProcess = init_process(iPid, iArrivalTime, iBurstTime, iPriority);
            currentProcess->leftover = currentProcess->burst;
            enqueue(ReadyQueue, currentProcess);
        }
    }
    fclose(file);

    //  creating and organising the final queue for printing
    struct List *ProcessQueue;
    ProcessQueue = processQueue(ReadyQueue, cAlgorithm, iQuantum);

    //  opens output file for process export
    file = fopen(cOutputFilepath, "w");
    if (file == NULL) {
        printf("Sorry, but the file %s could not be created.\n", cOutputFilepath);
        return 1;
    }

    //  queue printing
    int iAvgWait = 0;
    int iAvgTO = 0;
    int iNoProcesses = ProcessQueue->count;
    struct ProcessNode *processDeleter;
    for (int i = ProcessQueue->count;i > 0;i--) {
        currentProcess = dequeue(ProcessQueue);
        fprintf(file, "%d %d %d %d\n", currentProcess->pid, currentProcess->arrival, currentProcess->finish, currentProcess->waiting);
        iAvgWait += currentProcess->waiting;
        iAvgTO += currentProcess->finish - currentProcess->arrival;
        processDeleter = currentProcess;
        currentProcess = currentProcess->next;
        del_process(processDeleter);
    }
    fclose(file);
    file = NULL;
    del_list(ProcessQueue);

    iAvgWait /= iNoProcesses;
    iAvgTO /= iNoProcesses;
    printf("The average wait time was %d, and the average turnover time %d.\n", iAvgWait, iAvgTO);

    //  stop Valgrind's "FILE DESCRIPTORS open at exit" error:
    fclose(stdin);
    fclose(stdout);
    fclose(stderr);

    //  obtain user confirmation before exiting
    getchar();
    return 0;

} //    end main


//------------------------------------------------------------------------------
//  Function Definitions: Process Methods
//------------------------------------------------------------------------------
struct ProcessNode *init_process(int pid, int arrivalTime, int burstTime, int priority) {
    struct ProcessNode *newProcess = malloc(sizeof(struct ProcessNode));
    if (newProcess == NULL) {
        printf("Sorry, but memory was found to be unallocatable for the process.");
        exit(-1);
    }
    newProcess->pid = pid;
    newProcess->arrival = arrivalTime;
    newProcess->burst = burstTime;
    newProcess->priority = priority;

    newProcess->finish = 0;
    newProcess->waiting = 0;
    newProcess->leftover = 0;

    newProcess->next = NULL;
    newProcess->prev = NULL;

    return newProcess;
}

void del_process(struct ProcessNode *node) {
    node->next = NULL;
    node->prev = NULL;
    free(node);
    node = NULL;
}

//------------------------------------------------------------------------------
//  List/Queuing Methods
//------------------------------------------------------------------------------
struct List *init_list() {
    struct List *newList = malloc(sizeof(struct List));
    if (newList == NULL) {
        printf("Sorry, but memory was found to be unallocatable for the list.");
        exit(-1);
    }
    newList->head = NULL;
    newList->tail = NULL;
    newList->count = 0;
    return newList;
}

void del_list(struct List *list) {
    // while (list->head != NULL && list->tail != NULL) {
    //     struct Node *ptr = list->tail;
    //     del_process(list, ptr);
    // }
    list->head = NULL;
    list->tail = NULL;
    free(list);
    list = NULL;
}

void enqueue(struct List *list, struct ProcessNode *node) {
    //  If new list
    if (list->head == NULL && list->tail == NULL) {
        list->head = node;
        list->tail = node;
        node->next = NULL;
        node->prev = NULL;
    } else {
        //  If filled list
        list->tail->next = node;
        node->prev = list->tail;
        node->next = NULL;
        list->tail = node;
    }
    list->count++;
}

struct ProcessNode *dequeue(struct List *queue) {
    if (queue->head == queue->tail) {
        //  for the final item in a queue
        struct ProcessNode *final = queue->head;
        queue->head = NULL;
        queue->tail = NULL;
        queue->count--;
        return final;
    } else {
        //  normal use
        struct ProcessNode *first;
        first = queue->head;
        first->next->prev = NULL;
        queue->head = first->next;
        first->next = NULL;
        queue->count--;
        return first;
    }
}

//    swap preserving next/prev pointers
void swapWithNext(struct ProcessNode *i) {
    struct ProcessNode *tmp = init_process(0, 0, 0, 0);

    //    i -> tmp
    tmp->pid = i->pid;
    tmp->arrival = i->arrival;
    tmp->burst = i->burst;
    tmp->priority = i->priority;
    tmp->finish = i->finish;
    tmp->waiting = i->waiting;
    tmp->leftover = i->leftover;

    //    i->next -> i
    i->pid = i->next->pid;
    i->arrival = i->next->arrival;
    i->burst = i->next->burst;
    i->priority = i->next->priority;
    i->finish = i->next->finish;
    i->waiting = i->next->waiting;
    i->leftover = i->next->leftover;

    //    tmp -> i->next
    i->next->pid = tmp->pid;
    i->next->arrival = tmp->arrival;
    i->next->burst = tmp->burst;
    i->next->priority = tmp->priority;
    i->next->finish = tmp->finish;
    i->next->waiting = tmp->waiting;
    i->next->leftover = tmp->leftover;

    del_process(tmp);
}

//    insertion sort by priority:
void insSortPriority(struct List *list) {
    struct ProcessNode *ptr;
    struct ProcessNode *i;
    ptr = list->head;
    i = list->head;

    if (list->count == 1) { return; }

    do {
        ptr = ptr->next;
        //    ptr is in the right place
        if (ptr->priority >= i->priority) {
            i = i->next;
            continue;

        } else if (ptr->priority < i->priority) {
            //    	ptr less than i
            while (i != NULL && i->priority > i->next->priority) {
                swapWithNext(i);
                i = i->prev;
            }
            //    return i to "j-1"
            i = ptr;
        }

    } while (ptr->next != NULL);
}

//------------------------------------------------------------------------------
//  Scheduling Methods
//------------------------------------------------------------------------------
void arrivalChecker(struct List *ReadyQueue, struct List *JobQueue, int CLOCK) {
    struct ProcessNode *arrivalChecker;
    for (int i = JobQueue->count;i > 0;i--) {
        arrivalChecker = dequeue(JobQueue);
        if (arrivalChecker->arrival == CLOCK) {
            enqueue(ReadyQueue, arrivalChecker);
        } else {
            enqueue(JobQueue, arrivalChecker);
        }
    }
}

void npp(struct List *ReadyQueue, struct List *ProcessQueue) {
    int CLOCK = 0;
    struct List *JobQueue = init_list();
    struct ProcessNode *currentProcess;
    currentProcess = dequeue(ReadyQueue);
    //    place remaining processes into JobQueue
    while (ReadyQueue->count > 0) {
        enqueue(JobQueue, dequeue(ReadyQueue));
    }
    while (currentProcess != NULL) {
        for (int i = 0;i < currentProcess->burst;i++) {
            CLOCK++;
            arrivalChecker(ReadyQueue, JobQueue, CLOCK);
            if (ReadyQueue->head != NULL) { insSortPriority(ReadyQueue); }
        }
        currentProcess->waiting = CLOCK - currentProcess->arrival - currentProcess->burst;
        currentProcess->finish = CLOCK;
        enqueue(ProcessQueue, currentProcess);
        arrivalChecker(ReadyQueue, JobQueue, CLOCK);
        if (ReadyQueue->head != NULL) { insSortPriority(ReadyQueue); }
        currentProcess = dequeue(ReadyQueue);
    }
    free(currentProcess);
    currentProcess = NULL;
    del_list(JobQueue);
}

void rr(struct List *ReadyQueue, struct List *ProcessQueue, int iQuantum) {
    int CLOCK = 0;
    struct ProcessNode *currentProcess;
    struct List *JobQueue = init_list();
    currentProcess = dequeue(ReadyQueue);
    //  place remaining processes into JobQueue
    while (ReadyQueue->count > 0) {
        enqueue(JobQueue, dequeue(ReadyQueue));
    }
    while (currentProcess != NULL) {
        for (int i = 0;i < iQuantum;i++) {
            CLOCK++;
            currentProcess->leftover--;
            if (i < (iQuantum - 1)) {
                arrivalChecker(ReadyQueue, JobQueue, CLOCK);
            }
            if (currentProcess->leftover == 0) {
                break;
            }
        }
        //  process completed in for loop
        if (currentProcess->leftover == 0) {
            currentProcess->waiting = CLOCK - currentProcess->arrival - currentProcess->burst;
            currentProcess->finish = CLOCK;
            enqueue(ProcessQueue, currentProcess);
            arrivalChecker(ReadyQueue, JobQueue, CLOCK);
            currentProcess = dequeue(ReadyQueue);
            continue;
        }
        //  process incomplete
        enqueue(ReadyQueue, currentProcess);
        arrivalChecker(ReadyQueue, JobQueue, CLOCK);
        currentProcess = dequeue(ReadyQueue);
    }
    del_list(JobQueue);
}

//  send ReadyQueue to designated subroutine for processing
struct List *processQueue(struct List *ReadyQueue, char *cAlgorithm, int iQuantum) {
    struct List *ProcessQueue = init_list();
    if (strcmp(cAlgorithm, "NPP") == 0) {
        npp(ReadyQueue, ProcessQueue);
    } else if (strcmp(cAlgorithm, "RR") == 0) {
        rr(ReadyQueue, ProcessQueue, iQuantum);
    }
    del_list(ReadyQueue);
    return ProcessQueue;
}
