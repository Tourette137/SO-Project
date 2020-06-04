
typedef struct task {
    int id;
    pid_t pid;
    char* command;
    int status;
}*TASK;

TASK initTask(int id, pid_t pid, char* command, int status);
void freeTask(TASK task);
