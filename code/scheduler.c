#include "headers.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <math.h>
struct Process        // to store process information and send them to scheduler
{                     // long mtype;//FOR MESSAGE
    int process_id;   // process id
    int arrive_time;  // process arival time
    int running_time; // needed time to run this process
    int priority;     // priority of the process
    int finish;       // finish time
    int start;        // first time it running by cpu
    int state;        // state of the process (0:running &  1:stopped or blocked & 2:term)
    int child_id;     // when fork to use it when make signals
    int remainingTime;
    int RRtime;     // store the time at when process start or resumed for RR
    bool firsttime; // first time it runs
};
int Start;
struct Time
{
    int remmining;
    int start_time;
};
// Node structure
typedef struct Node
{
    struct Process *data;
    struct Node *next;
    struct Node *prev;
} Node;

// Linked list structure
typedef struct
{
    Node *head;
    Node *tail;
} LinkedList;
///  global
void *shr;
FILE *scheduler_log;
FILE *scheduler_perf;
LinkedList Ready;
int cont = 1;
int RunningTime = 0;
Node *RunningProcess = NULL;      // running process
int number_of_finish_process = 0; // number of finished process
int processorState = 0;           // 0 ideal 1 is used
float SUMWTA = 0;
float SUMWATING = 0;
float *WATArray;
////////////////////////////////FILES PRINT////////////////

void Finish()
{
    printf("Finished\n");
    int TA = getClk() - (RunningProcess->data->arrive_time);
    int wait = (RunningProcess->data->start) - (RunningProcess->data->arrive_time);
    float WTA = (float)wait / (RunningProcess->data->running_time);
    WTA = roundf(WTA * 100) / 100;
    SUMWTA += WTA;                            // sum wating trun around time
    SUMWATING += wait;                        // sum waiting time
    WATArray[number_of_finish_process] = WTA; // TO CALULATE STANDERED WAT
    fprintf(scheduler_log, "At time %d process %d finished arr %d total %d remain 0 wait %d TA %d WTA %.2f \n", getClk(), RunningProcess->data->process_id, RunningProcess->data->arrive_time, RunningProcess->data->running_time, wait, TA, WTA);

    number_of_finish_process++;      // number of finished processes
    RunningProcess->data->state = 2; // term;
    RunningProcess->data->remainingTime = 0;
}
void StartorResume()
{
    if (RunningProcess != NULL)
    {
        int wait = (RunningProcess->data->start) - (RunningProcess->data->arrive_time);
        // printf("at clock= %d start =%d\n",getClk(),(RunningProcess->data->start));
        if (RunningProcess->data->firsttime)
        {
            fprintf(scheduler_log, "At time %d process %d started arr %d total %d remain %d wait %d\n", getClk(), RunningProcess->data->process_id, RunningProcess->data->arrive_time, RunningProcess->data->running_time, RunningProcess->data->remainingTime, wait);
            RunningProcess->data->firsttime = false;
        }
        else
            fprintf(scheduler_log, "At time %d process %d resumed arr %d total %d remain %d wait %d\n", getClk(), RunningProcess->data->process_id, RunningProcess->data->arrive_time, RunningProcess->data->running_time, RunningProcess->data->remainingTime, wait);
    }
}
void Stop()
{
    if (RunningProcess != NULL)
    {
        int wait = (RunningProcess->data->start) - (RunningProcess->data->arrive_time);

        fprintf(scheduler_log, "At time %d process %d stopped arr %d total %d remain %d wait %d\n", getClk(), RunningProcess->data->process_id, RunningProcess->data->arrive_time, RunningProcess->data->running_time, RunningProcess->data->remainingTime, wait);
    }
}
/////////////////////////////////////////////////////
void initializeList(LinkedList *list)
{
    list->head = NULL;
    list->tail = NULL;
}
///////////Reamove functions/////////////
void RemH(LinkedList *list)
{ // remove from head
    if (list->head != NULL)
    {
        Node *current = list->head;
        list->head = list->head->next;
        if (list->head == NULL)
        {
            list->tail = list->head;
        }

        current->next = NULL;
        if (list->head != NULL)
            list->head->prev = NULL;

        free(current);
    }
}
void RemT(LinkedList *list)
{ // remove from tail
    if (list->tail != NULL)
    {
        Node *current = list->tail;
        list->tail = list->tail->prev;
        if (list->tail == NULL)
        {
            list->head = list->tail;
        }
        current->next = NULL;
        if (list->tail != NULL)
            list->tail->next = NULL;
        free(current);
    }
}
////////////////////Insertion FUNCTIONS////////////////////
void AddSorted(LinkedList *list, struct Process *data)
{ // add to head
    Node *newNode = (Node *)malloc(sizeof(Node));
    if (newNode == NULL)
    {
        printf("Memory allocation failed.\n");
        return;
    }
    newNode->data = data;
    if (list->head == NULL)
    { // empty
        list->head = list->tail = newNode;
        newNode->next = NULL;
        newNode->prev = NULL;
    }
    else
    {
        if (RunningProcess != NULL) // there is a process running
        {
            if (RunningProcess->data->remainingTime - 1 == 0) // it is time to be finished
            {
                ((struct Time *)shr)->remmining = RunningProcess->data->remainingTime - 1; // send it to process
                RunningProcess->data->remainingTime--;
                RunningProcess->data->state = 2; // TERM
                Finish();                        // PRINT                                       // finish remove                                                                 // to print its information
                RemH(&Ready);                    // REMOVE
                processorState = 0;              // IDEL
                RunningProcess = NULL;           // PROCESSOR IDEL

                ////////////////   add the new one
                Node *prev = NULL;
                Node *current = Ready.head;
                if (current == NULL) // LIST IS EMPTY
                {
                    list->head = list->tail = newNode;
                    newNode->next = NULL;
                    newNode->prev = NULL;
                }
                else
                {
                    while (current != NULL && current->data->remainingTime < newNode->data->remainingTime)
                    {
                        prev = current;
                        current = current->next;
                    }
                    if (current == NULL)
                    {
                        newNode->prev = Ready.tail;
                        Ready.tail->next = newNode;
                        Ready.tail = newNode;
                    }
                    else if (current == Ready.head) // must be the first one in the list
                    {
                        newNode->next = Ready.head;
                        Ready.head->prev = newNode;
                        Ready.head = newNode;
                    }
                    else
                    {
                        newNode->next = prev->next;
                        newNode->prev = prev;
                        prev->next = newNode;
                    }
                }
            }
            else if (RunningProcess->data->remainingTime - 1 > newNode->data->remainingTime) // one came is shorter than the running one
            {
                RunningProcess->data->remainingTime--;         // BECAUSE IT FINISH 1 SECOND MUST STOP
                RunningProcess->data->state = 1;               // BLOCK
                Stop();                                        // print its information
                kill(RunningProcess->data->child_id, SIGUSR2); // stop old one
                newNode->next = Ready.head;
                Ready.head->prev = newNode;
                Ready.head = newNode;
                RunningProcess = NULL;
                processorState = 0; // ideal
            }
            else // not stop the process which is running
            {
                Node *prev = Ready.head;
                Node *current = prev->next;
                while (current != NULL && current->data->remainingTime < newNode->data->remainingTime)
                {
                    prev = current;
                    current = current->next;
                }
                if (current == NULL)
                {
                    newNode->prev = Ready.tail;
                    Ready.tail->next = newNode;
                    Ready.tail = newNode;
                }
                else
                {
                    newNode->next = prev->next;
                    newNode->prev = prev;
                    prev->next = newNode;
                }
            }
        }
        else // running process is null
        {
            Node *prev = NULL;
            Node *current = Ready.head;
            while (current != NULL && current->data->remainingTime < newNode->data->remainingTime)
            {
                prev = current;
                current = current->next;
            }
            if (current == Ready.head)
            {
                newNode->next = Ready.head;
                Ready.head->prev = newNode;
                Ready.head = newNode;
            }
            else
            {
                newNode->next = prev->next;
                newNode->prev = prev;
                prev->next = newNode;
            }
        }
    }
}

void AddT(LinkedList *list, struct Process *data)
{ // add to tail
    Node *newNode = (Node *)malloc(sizeof(Node));
    if (newNode == NULL)
    {
        printf("Memory allocation failed.\n");
        return;
    }
    newNode->data = data;
    newNode->next = NULL;

    if (list->head == NULL)
    { // List is empty
        list->head = list->tail = newNode;
    }
    else
    {
        list->tail->next = newNode; // Link the current tail to the new node
        newNode->prev = list->tail; // Update the previous pointer of the new node
        list->tail = newNode;       // Update the tail pointer to the new node
    }
}

void AddSortedPriority(LinkedList *list, struct Process *data)
{
    Node *newNode = (Node *)malloc(sizeof(Node));
    if (newNode == NULL)
    {
        printf("Memory allocation failed.\n");
        return;
    }
    newNode->data = data;
    newNode->next = NULL;
    newNode->prev = NULL; // Initialize prev pointer

    if (list->head == NULL)
    { // Empty list
        list->head = list->tail = newNode;
    }
    else
    {
        Node *current = list->head;
        if (list->head->data->arrive_time != data->arrive_time) // not came at same time
        {
            while (current->next != NULL && data->priority > current->next->data->priority) // first one is running
            {
                current = current->next;
            }
            if (current->next == NULL)
            { // Insert at the end
                current->next = newNode;
                newNode->prev = current;
                list->tail = newNode; // Update tail
            }
            else
            { // Insert in the middle
                newNode->next = current->next;
                current->next->prev = newNode;
                current->next = newNode;
                newNode->prev = current;
            }
        }
        else
        {
            while (current != NULL && data->priority > current->data->priority) // came at same time
            {

                current = current->next;
            }
            if (current == NULL)
            { // Insert at the end
                current = list->tail;
                current->next = newNode;
                newNode->prev = current;
                list->tail = newNode; // Update tail
            }
            else
            { // Insert in the middle
                if (current == list->head)
                {
                    newNode->next = current;
                    current->prev = newNode;
                    newNode->prev = NULL;
                    list->head = newNode;
                }
                else
                {
                    newNode->next = current;
                    newNode->prev = current->prev;
                    current->prev->next = newNode;
                    newNode->prev = newNode;
                }
            }
        }
    }
}
/// HPF
///
void SortAccordingRT(LinkedList *list)
{
    if (list->head == NULL || list->head == list->tail)
    {
        return; // Nothing to sort
    }

    Node *current1 = list->head;
    while (current1 != NULL)
    {
        Node *current2 = current1->next;
        Node *minNode = current1; // Assume current node has minimum remaining time

        // Find the node with minimum remaining time starting from current2
        while (current2 != NULL)
        {
            if (current2->data->remainingTime < minNode->data->remainingTime)
            {
                minNode = current2;
            }
            current2 = current2->next;
        }

        // Swap data between current1 and minNode
        struct Process *temp = current1->data;
        current1->data = minNode->data;
        minNode->data = temp;

        current1 = current1->next; // Move to next node
    }
}
/////////////////////////////////////////////////////////
void printList(LinkedList *list)
{
    if (list->head != NULL)
    {
        Node *current = list->head;
        while (current != NULL)
        {
            printf("%d, ", current->data->remainingTime);
            current = current->next;
        }
    }
}
// 0:idle & 1:working on process
struct statisticsP
{
    int RunningTime;
    int TA;
};
struct statisticsP *result; // I will need
////ALGO FUNCTION
void ALGO(LinkedList *list)
{
    // printf("hello\n");
    if (list->head != NULL)
    {
        if (RunningProcess == NULL && list->head != NULL)
            processorState = 0;
        if (!processorState) // ideal
        {                    // if idle
                             // printf("idel\n");
            RunningProcess = list->head;
            RunningProcess->data->state = 0; // running
            processorState = 1;              // busy
            ((struct Time *)shr)->remmining = RunningProcess->data->remainingTime;
            kill(RunningProcess->data->child_id, SIGCONT);
            // calc for start or resumed process
            if (RunningProcess->data->running_time == RunningProcess->data->remainingTime)
            {
                RunningProcess->data->start = getClk();
                RunningProcess->data->firsttime = true;
            }
            StartorResume();
        }
        else
        {
            if (RunningProcess != NULL)
            {
                RunningProcess->data->remainingTime--;
                ((struct Time *)shr)->remmining = RunningProcess->data->remainingTime;
            //    printf("process with time = %d at clk =%d remining =%d\n", RunningProcess->data->running_time, getClk(), RunningProcess->data->remainingTime);
                if (RunningProcess->data->remainingTime == 0)
                { // terminated
                    Finish();

                    RemH(list); // remove it from list

                    RunningProcess = list->head;
                    if (RunningProcess != NULL)
                    {
                        RunningProcess->data->state = 1; // blocked
                        processorState = 0;              // idle
                        kill(RunningProcess->data->child_id, SIGCONT);
                        RunningProcess->data->state = 0; // running
                        processorState = 1;              // busy
                        if (RunningProcess->data->running_time == RunningProcess->data->remainingTime)
                        {
                            RunningProcess->data->firsttime = true;

                            RunningProcess->data->start = getClk();
                        }
                        // calc for start or resumed process
                        StartorResume();
                    }
                }
            }
        }
    }
}
/////////////////////SIGNAL FUNCTIONS
void handleChild(int signum) // this is when a process send this to scheduler
{

    Start = ((struct Time *)shr)->start_time;
    if (Start == 0) // process first created
    {
        cont = 0;
    }

    signal(SIGUSR1, handleChild);
}
void ClearCock(int signum) // it process_generator interrupt;
{
    shmdt(shr);

    destroyClk(false); // release lock

    exit(0); // exit
}
////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
    initializeList(&Ready);
    Node *RRPointer = Ready.head; // pointer points to the first process for RR Algo
    signal(SIGUSR1, handleChild); // if child terminate process
    signal(SIGINT, ClearCock);
    int msg_id = atoi(argv[1]);    // ID OF MESSAGE QUEUE
    int shm_id = atoi(argv[2]);    // ID OF Shared memory
    int algorithm = atoi(argv[3]); // 1 RR 2 SRTN 3 HPF //Mohammed use this
    int slice = atoi(argv[4]);
    int number_of_system_process = atoi(argv[5]); // all system processes
    char process_run_time[10];                    // to send it to each process
    char share_id[10];                            // to send it to each process
    sprintf(share_id, "%d", shm_id);
    initClk();
    int i = 0;
    int prev = -1;
    WATArray = malloc(sizeof(float) * number_of_system_process);
    scheduler_log = fopen("scheduler.log", "w");
    fprintf(scheduler_log, "#At time x process y state arr w total z remain y wait k\n");
    scheduler_perf = fopen("scheduler.perf", "w");
    int num = 0;
    int busytime;
    struct Process *process = (struct Process *)malloc(number_of_system_process * sizeof(struct Process));
    while (number_of_finish_process < number_of_system_process) // untill finish
    {

        int size = 16;
        if (prev != getClk()) // 0 1 check prev run next
        {
            while (num != number_of_system_process && msgrcv(msg_id, &process[num], size, 0, IPC_NOWAIT) != -1) // recieve a process                                                                                                               // new process I think need to e scheduled                                                                                                          // receive a process
            {

                sprintf(process_run_time, "%d", process[num].running_time);
                busytime += process[num].running_time;
                if (process[num].arrive_time > getClk())//if process send process 
                {
                 //   printf("wait process which time is %d come at %d must be at %d\n",process[num].running_time,getClk(),process[num].arrive_time);;
                    ALGO(&Ready);//continue but dosent but the newest one
                    sleep(1);
                }
               // printf("clockrecieve: %d\n", getClk());

                int child_id = fork();
                // stop
                if (child_id == -1)
                {
                    printf("Error when try to fork \n");
                }
                else if (child_id == 0) // child code
                {
                    execl("process", "process", share_id, process_run_time, NULL);
                }
                else // scheduler
                {
                    process[num].child_id = child_id; // set process id

                    if (i == 0) // for first time only
                    {
                        shr = shmat(shm_id, NULL, 0);
                        i++;
                    }
                    process[num].remainingTime = process[num].running_time;
                    process[num].state = 1;
                    process[num].firsttime = false;

                    switch (algorithm)
                    {
                    case 1: // Round Robin
                        AddT(&Ready, &process[num]);
                        num++;
                        break;
                    case 2:
                        AddSorted(&Ready, &process[num]);
              //          printList(&Ready);
                   //     printf("clockbefore %d prev %d\n", getClk(), prev);

                        num++;
                        break;
                    case 3: // Highest Priority First (HPF) Sorted
                        // printf("hello from HPF\n");
                        AddSortedPriority(&Ready, &process[num]); // new one came
                        num++;

                        break;
                    default:
                        printf("Invalid algorithm!\n");
                        break;
                    }
                    // wait(NULL);
                    // printf("clock1: %d\n",getClk());
                    // while (cont)
                    //{
                    //}
                    // printf("clock1: %d\n",getClk());
                    // cont = 1;
                    raise(SIGSTOP);
                }
                //  prev = getClk() - 1; // to enter algorithm
            }

            // printf("clock%d\n", getClk());
            switch (algorithm)
            {
            case 1: // Round Robin
                if (Ready.head != NULL)
                {
                    if (RunningProcess != NULL)
                    {
                        if (RunningProcess->data->remainingTime - 1 == 0)
                        {
                            RunningProcess->data->remainingTime--;
                            RunningProcess->data->state = 2; // TERMINATE
                            Finish();                        // finish
                            processorState = 0;              // ideal
                            RunningProcess = NULL;
                            RemH(&Ready); // REMOVE IT
                        }
                        else if (((RunningProcess->data->running_time) - (RunningProcess->data->remainingTime - 1)) % slice == 0)
                        {
                            RunningProcess->data->remainingTime--;
                            RunningProcess->data->state = 1; // stop
                            Stop();
                            AddT(&Ready, (Ready.head->data)); // return to add it in the tail of list
                            RemH(&Ready);                     // remove it from head
                            processorState = 0;
                            RunningProcess = NULL;
                //            printList(&Ready);
                        }
                    }
                    ALGO(&Ready);
                }
                else
                {
                    processorState = 0; // Idle
                }
                break;

            case 2:

                if (Ready.head != NULL)
                {

                    // SortAccordingRT(&Ready);
                    ALGO(&Ready);
                }
                else
                {
                    processorState = 0; // idle
                }
                break;
            case 3: // Highest Priority First (HPF)
                if (Ready.head != NULL)
                {
                    ALGO(&Ready);
                }
                else
                {
                    processorState = 0; // idle
                                        //  ALGO(&Ready);
                }
                break;
            default:
                printf("Invalid algorithm!\n");
                break;
            }

            prev = getClk();
        }
    }

    /**
     calculation of second output file must be here
    */
    float CPUuti = 1.0 * busytime / getClk();          // cpu
    float AVGWAT = SUMWTA / number_of_system_process;  // avrage waited turn around time
    float AVGW = SUMWATING / number_of_system_process; // avrage waited
    AVGWAT = roundf(AVGWAT * 100) / 100;
    AVGW = roundf(AVGW * 100) / 100;
    float STDWAT = 0;
    for (int i = 0; i < number_of_system_process; i++)
    {
        STDWAT += pow((WATArray[i] - AVGWAT), 2);
    }
    STDWAT /= number_of_system_process;
    STDWAT = sqrt(STDWAT);
    STDWAT = round(STDWAT * 100) / 100;
    CPUuti = roundf(CPUuti * 10000) / 100;
    fprintf(scheduler_perf, "CPU utilization = %.2f\n", CPUuti);
    fprintf(scheduler_perf, "AWG WAT= %.2f\n", AVGWAT);
    fprintf(scheduler_perf, "AVG Waiting = %.2f\n", AVGW);
    fprintf(scheduler_perf, "STD WAT = %.2f\n", STDWAT);

    //////////////////////////////////CLEAR
    free(WATArray);
    shmdt(shr);
    fclose(scheduler_log);
    fclose(scheduler_perf);
    destroyClk(false);
    kill(getppid(), SIGINT); // IF scheduler terminate mean all process he fork also terminate so must terminate process_generator
    exit(0);
}
