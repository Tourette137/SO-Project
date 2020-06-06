#define BUFFER_SIZE 2048

// FILE OR PIPE NAMES MACROS
#define CLIENT_SERVER_PIPENAME "client_server_pipe"
#define SERVER_CLIENT_PIPENAME "server_client_pipe"
#define ERROR_FILENAME "error.txt"
#define TASK_RESULT_OUTPUT_FILENAME "task_result_output_"
#define LOG_FILENAME "log.txt"
#define LOG_INDEX_FILENAME "log.idx"

// TASK STATUS MACROS
#define TASK_RUNNING 0
#define TASK_TERMINATED 1
#define TASK_TERMINATED_INACTIVITY 2
#define TASK_TERMINATED_EXECUTION_TIME 3
#define TASK_TERMINATED_INTERRUPTED 4

// SERVER CHILD EXIT STATUS MACROS
#define EXIT_STATUS_TERMINATED 1
#define EXIT_STATUS_INACTIVITY 2
#define EXIT_STATUS_EXECUTION_TIME 3
#define EXIT_STATUS_TERMINATED_INTERRUPTED 4

// PIPE COMMUNICATION MACROS
#define PIPE_COMMUNICATION_EOF "$$EOF$$"
#define PIPE_COMMUNICATION_EOF_SIZE 7
