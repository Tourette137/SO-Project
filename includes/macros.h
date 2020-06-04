#define BUFFER_SIZE 1024

#define PIPENAME "server_pipe"
#define ERROR_FILENAME "error.txt"

// TASK STATUS MACROS
#define TASK_RUNNING 0
#define TASK_TERMINATED 1
#define TASK_TERMINATED_INACTIVITY 2
#define TASK_TERMINATED_EXECUTION_TIME 3
#define TASK_TERMINATED_INTERRUPTED 4

// SERVER CHILD EXIT STATUS
#define EXIT_STATUS_TERMINATED 1
#define EXIT_STATUS_INACTIVITY 2
#define EXIT_STATUS_EXECUTION_TIME 3
#define EXIT_STATUS_TERMINATED_INTERRUPTED 4