#include "serial.c"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#ifdef _WIN32
#    include <windows.h>
#else
#    include <termios.h>
#    include <pthread.h>
#    include <semaphore.h>
#    include <sys/time.h>
#    include <string.h>
#    include <signal.h>
#endif

#ifdef _WIN32
#    define PORT_RD "COM9"
#    define SemaphoreWait(sem) WaitForSingleObject(sem, INFINITE);
#    define SemaphorePost(sem) ReleaseSemaphore(sem, 1, NULL)
#    define sleep(x) Sleep(x * 1000)
#else
#    define PORT_RD "/dev/pts/4"
#    define SemaphoreWait(sem) sem_wait(sem)
#    define SemaphorePost(sem) sem_post(sem)
#    define sleep(x) sleep(x)
#endif

#define SEMAPHORE_OBJECT_NAME "/my_semaphore"

const int SEC_IN_HOUR = 3600;
const int SEC_IN_DAY = 86400;
const int HOURS_IN_MONTH = 720;

const int RECORD_LENGTH = 36;

struct thr_data {
    FILE *file;
    time_t *next;
    double *avg;
    int *counter;
    int *last_record_pos;
#ifdef _WIN32
    HANDLE thr_sem;
#else
    sem_t *thr_sem;
#endif
};

volatile unsigned char need_exit = 0;
#ifdef _WIN32
BOOL WINAPI sig_handler(DWORD signal)
{
    if (signal == CTRL_C_EVENT) {
        need_exit = 1;
        return TRUE;
    }
    return FALSE;
}
#else
void sig_handler(int sig)
{
    if (sig == SIGINT)
        need_exit = 1;
}
#endif

// get current time in YYYY-MM-DD hh:mm:ss.sss // TODO
char *get_time(char *date, size_t size)
{
#ifdef _WIN32
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(date, size, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
        st.wYear, st.wMonth, st.wDay, st.wHour,
        st.wMinute, st.wSecond, st.wMilliseconds);
#else
    struct timeval tv;
    struct tm *tmp;

    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        exit(EXIT_FAILURE);
    }
    tmp = localtime(&tv.tv_sec);
    if (tmp == NULL) {
        perror("localtime");
        exit(EXIT_FAILURE);
    }

    if (strftime(date, size, "%Y-%m-%d %H:%M:%S", tmp) == 0) {
        perror("strftime");
        exit(EXIT_FAILURE);
    }

    char ms[5];
    sprintf(ms, ".%03ld", tv.tv_usec / 1000);
    strcat(date, ms);
#endif
    return date;
}

#ifdef _WIN32
void write_log(HANDLE file, const char* record, int size, int *last_record_pos, const int CYCLE, int index, HANDLE sem)
{
    if (last_record_pos[index] >= CYCLE) {

        SemaphoreWait(sem);
        last_record_pos[index] = 0;
        SemaphorePost(sem);

        SetFilePointer(file, 0, NULL, FILE_BEGIN);
    }
    WriteFile(file, record, size, NULL, NULL);

    SemaphoreWait(sem);
    last_record_pos[index] = last_record_pos[index] + 1;
    SemaphorePost(sem);
}
#else
void write_log(FILE* file, const char *record, int size, int *last_record_pos, const int CYCLE, int index, sem_t *sem)
{
    if (last_record_pos[index] >= CYCLE) {

        SemaphoreWait(sem);
        last_record_pos[index] = 0;
        SemaphorePost(sem);

        fseek(file, 0, SEEK_SET);
    }
    fwrite(record, sizeof(char), size, file);

    SemaphoreWait(sem);
    last_record_pos[index] = last_record_pos[index] + 1;
    SemaphorePost(sem);
}
#endif

void make_fixed_record(char* fixed_record, char *record, int record_len)
{
    memset(fixed_record, ' ', RECORD_LENGTH - 1);
    fixed_record[RECORD_LENGTH - 1] = '\n';
    size_t copy_len = record_len < RECORD_LENGTH - 1 ? record_len : RECORD_LENGTH - 1;
    memcpy(fixed_record, record, copy_len);
}

#ifdef _WIN32
DWORD WINAPI thr_routine_hour(void *args)
{
    struct thr_data *params = (struct thr_data*)args;
    char curr_time[24];
    char record[RECORD_LENGTH];
    while (!need_exit) {
        time_t current_time = time(NULL);
        if (current_time >= *params->next) {

            SemaphoreWait(params->thr_sem);
            *params->next += SEC_IN_HOUR;
            SemaphorePost(params->thr_sem);

            SemaphoreWait(params->thr_sem);
            double avg = *params->avg;
            SemaphorePost(params->thr_sem);

            get_time(curr_time, sizeof(curr_time));
            int record_len = snprintf(record, sizeof(record), "%s %.1lf", curr_time, avg);

            char fixed_record[RECORD_LENGTH];
            make_fixed_record(fixed_record, record, record_len);

            write_log(params->file, fixed_record, sizeof(fixed_record), params->last_record_pos, HOURS_IN_MONTH, 1, params->thr_sem);

            SemaphoreWait(params->thr_sem);
            *params->avg = 0.0;
            SemaphorePost(params->thr_sem);

            SemaphoreWait(params->thr_sem);
            *params->counter = 0;
            SemaphorePost(params->thr_sem);
        }
        sleep(1);
    }
    return 0;
}
#else
void* thr_routine_hour(void *args)
{
    struct thr_data *params = (struct thr_data*)args;
    char curr_time[24];
    char record[RECORD_LENGTH];
    while (!need_exit) {
        time_t current_time = time(NULL);
        if (current_time >= *params->next) {

            SemaphoreWait(params->thr_sem);
            *params->next += SEC_IN_HOUR;
            SemaphorePost(params->thr_sem);

            SemaphoreWait(params->thr_sem);
            double avg = *params->avg;
            SemaphorePost(params->thr_sem);

            get_time(curr_time, sizeof(curr_time));
            int record_len = snprintf(record, sizeof(record), "%s %.1lf", curr_time, avg);

            char fixed_record[RECORD_LENGTH];
            make_fixed_record(fixed_record, record, record_len);

            write_log(params->file, fixed_record, sizeof(fixed_record), params->last_record_pos, HOURS_IN_MONTH, 1, params->thr_sem);


            SemaphoreWait(params->thr_sem);
            *params->avg = 0.0;
            SemaphorePost(params->thr_sem);

            SemaphoreWait(params->thr_sem);
            *params->counter = 0;
            SemaphorePost(params->thr_sem);
        }
        sleep(1);
    }
    return 0;
}
#endif

#ifdef _WIN32
DWORD WINAPI thr_routine_day(void *args)
{
    struct thr_data *params = (struct thr_data*)args;
    char curr_time[24];
    char record[RECORD_LENGTH];
    int first_opened = 1;
    int log_year;
    while (!need_exit) {
        time_t current_time = time(NULL);
        if (current_time >= *params->next) {

            struct tm local_time;
            localtime_s(&local_time, &current_time);
            int curr_year = local_time.tm_year + 1900;

            if (first_opened) {
                log_year = curr_year;
            } else if (curr_year != log_year) {

                CloseHandle(params->file);
                params->file = CreateFile(
                    "log_day.txt",
                    GENERIC_WRITE,
                    FILE_SHARE_READ,
                    NULL,
                    CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL
                );
                if (params->file == INVALID_HANDLE_VALUE) {
                    perror("Error reopening log_file_day");
                    exit(EXIT_FAILURE);
                }

                CloseHandle(params->file);
                params->file = CreateFile("log_day.txt",
                              FILE_APPEND_DATA,
                              FILE_SHARE_READ,
                              NULL,
                              OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
                              NULL);
                if (params->file == INVALID_HANDLE_VALUE) {
                    perror("Error reopening log_file_day");
                    exit(EXIT_FAILURE);
                }
                log_year = curr_year;
            }

            // change next
            SemaphoreWait(params->thr_sem);
            *params->next += SEC_IN_DAY;
            SemaphorePost(params->thr_sem);

            SemaphoreWait(params->thr_sem);
            double avg = *params->avg;
            SemaphorePost(params->thr_sem);

            get_time(curr_time, sizeof(curr_time));
            int record_len = snprintf(record, sizeof(record), "%s %.1lf", curr_time, avg);

            char fixed_record[RECORD_LENGTH];
            make_fixed_record(fixed_record, record, record_len);

            WriteFile(params->file, fixed_record, sizeof(fixed_record), NULL, NULL);
            first_opened = 0;

            SemaphoreWait(params->thr_sem);
            *params->avg = 0.0;
            SemaphorePost(params->thr_sem);

            SemaphoreWait(params->thr_sem);
            *params->counter = 0;
            SemaphorePost(params->thr_sem);
        }
        sleep(1);
    }
    return 0;
}
#else
void* thr_routine_day(void *args)
{
    struct thr_data *params = (struct thr_data*)args;
    char curr_time[24];
    char record[RECORD_LENGTH];
    int first_opened = 1;
    int log_year;
    while (!need_exit) {
        time_t current_time = time(NULL);
        if (current_time >= *params->next) {

            struct tm *local_time = localtime(&current_time);
            int curr_year = local_time->tm_year + 1900;

            if (first_opened) {
                log_year = curr_year;
            } else if (curr_year != log_year) {
                params->file = freopen("log_day.txt", "w", params->file);
                if (params->file == NULL) {
                    perror("Error reopening log_file_day (w)");
                    exit(EXIT_FAILURE);
                }
                params->file = freopen("log_day.txt", "a+", params->file);
                if (params->file == NULL) {
                    perror("Error reopening log_file_day (a+)");
                    exit(EXIT_FAILURE);
                }
                setvbuf(params->file, NULL, _IONBF, 0);
                log_year = curr_year;
            }

            SemaphoreWait(params->thr_sem);
            *params->next += SEC_IN_DAY;
            SemaphorePost(params->thr_sem);

            SemaphoreWait(params->thr_sem);
            double avg = *params->avg;
            SemaphorePost(params->thr_sem);

            get_time(curr_time, sizeof(curr_time));
            int record_len = snprintf(record, sizeof(record), "%s %.1lf", curr_time, avg);

            char fixed_record[RECORD_LENGTH];
            make_fixed_record(fixed_record, record, record_len);

            fwrite(fixed_record, sizeof(char), sizeof(fixed_record), params->file);
            first_opened = 0;

            SemaphoreWait(params->thr_sem);
            *params->avg = 0.0;
            SemaphorePost(params->thr_sem);

            SemaphoreWait(params->thr_sem);
            *params->counter = 0;
            SemaphorePost(params->thr_sem);
        }
        sleep(1);
    }
    return 0;
}
#endif

int main(int argc, char *argv[])
{

    int last_record_pos[] = {0, 0};

#ifdef _WIN32
    if (!SetConsoleCtrlHandler(sig_handler, TRUE)) {
        perror("Error setting handler");
        exit(EXIT_FAILURE);
    }
#else
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = sig_handler;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    act.sa_mask = set;
    if (sigaction(SIGINT, &act, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
#endif

#ifdef _WIN32
    HANDLE log_file = CreateFile(
        "log.txt",
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        NULL
    );
    if (log_file == INVALID_HANDLE_VALUE) {
        perror("CreateFile (log_file)");
        exit(EXIT_FAILURE);
    }

    HANDLE log_file_hour = CreateFile(
        "log_hour.txt",
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        NULL
    );
    if (log_file_hour == INVALID_HANDLE_VALUE) {
        perror("CreateFile (log_file_hour)");
        exit(EXIT_FAILURE);
    }

    HANDLE log_file_day = CreateFile("log_day.txt",
                              FILE_APPEND_DATA,
                              FILE_SHARE_READ,
                              NULL,
                              CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
                              NULL);
    if (log_file_day == INVALID_HANDLE_VALUE) {
        perror("CreateFile (log_file_day)");
        exit(EXIT_FAILURE);
    }
#else
    FILE* log_file = fopen("log.txt", "w");
    if (log_file == NULL) {
        perror("fopen (log_file)");
        exit(EXIT_FAILURE);
    }
    setvbuf(log_file, NULL, _IONBF, 0);

    FILE* log_file_hour = fopen("log_hour.txt", "w");
    if (log_file_hour == NULL) {
        perror("fopen (log_file_hour)");
        exit(EXIT_FAILURE);
    }
    setvbuf(log_file_hour, NULL, _IONBF, 0);

    FILE* log_file_day = fopen("log_day.txt", "w");
    fclose(log_file_day);

    log_file_day = fopen("log_day.txt", "a");
    if (log_file_day == NULL) {
        perror("fopen (log_file_day)");
        exit(EXIT_FAILURE);
    }
    setvbuf(log_file_day, NULL, _IONBF, 0);
#endif

#ifdef _WIN32
    HANDLE sem;
    sem = CreateSemaphore(
        NULL,
        1,
        1,
        SEMAPHORE_OBJECT_NAME
    );
#else
    sem_t *sem = sem_open(SEMAPHORE_OBJECT_NAME, O_CREAT, 0777, 1);
#endif

#ifdef _WIN32
    HANDLE fd = CreateFile(
        PORT_RD,
        GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
    if (fd == INVALID_HANDLE_VALUE) {
        perror("CreateFile (pd)");
        exit(EXIT_FAILURE);
    }

    if (!port_config(fd, BAUDRATE_115200)) {
        perror("configure_port (pd)");
        CloseHandle(fd);
        exit(EXIT_FAILURE);
    }
#else
    const char* port_rd = PORT_RD;
    speed_t baud_rate = BAUDRATE_115200;
    port_config(port_rd, baud_rate);

    int fd = open(port_rd, O_RDONLY | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        perror("open port");

        exit(EXIT_FAILURE);
    }

    if (tcflush(fd, TCIFLUSH) == -1) {
        perror("tcflush");

        exit(EXIT_FAILURE);
    }
#endif

    int count_hour = 0, count_day = 0;
    double avg_hour = 0.0, avg_day = 0.0;

    time_t start_time = time(NULL);
    time_t next_hour = start_time + SEC_IN_HOUR;
    time_t next_day = start_time + SEC_IN_DAY;

#ifdef _WIN32
    struct thr_data params_hour = {log_file_hour, &next_hour, &avg_hour, &count_hour, last_record_pos, sem};
    HANDLE thr_hour = CreateThread(
        NULL,
        0,
        thr_routine_hour,
        &params_hour,
        0,
        NULL
    );
    if (thr_hour == NULL) {
        perror("CreateThread (thr_hour)");
        exit(EXIT_FAILURE);
    }
#else
    struct thr_data params_hour = {log_file_hour, &next_hour, &avg_hour, &count_hour, last_record_pos, sem};
    pthread_t thr_hour;
    int status = pthread_create(&thr_hour, NULL, thr_routine_hour, &params_hour);
    if (status != 0) {
        perror("pthread_create (thr_hour)");
        exit(EXIT_FAILURE);
    }
#endif

#ifdef _WIN32
    struct thr_data params_day = {log_file_day, &next_day, &avg_day, &count_day, NULL, sem};
    HANDLE thr_day = CreateThread(
        NULL,
        0,
        thr_routine_day,
        &params_day,
        0,
        NULL
    );
    if (thr_day == NULL) {
        perror("CreateThread (thr_day)");
        exit(EXIT_FAILURE);
    }
#else
    struct thr_data params_day = {log_file_day, &next_day, &avg_day, &count_day, NULL, sem};
    pthread_t thr_day;
    status = pthread_create(&thr_day, NULL, thr_routine_day, &params_day);
    if (status != 0) {
        perror("pthread_create (thr_day)");
        exit(EXIT_FAILURE);
    }
#endif

    char curr_time[24];
    char buffer[255];
    char record[255];
    int record_len;
    double cur_temp;

#ifdef _WIN32
    while (!need_exit) {
        DWORD bytesRead;
        if (ReadFile(fd, buffer, sizeof(buffer), &bytesRead, NULL)) {
            if (bytesRead > 0) {

                get_time(curr_time, sizeof(curr_time));
                record_len = snprintf(record, sizeof(record), "%s %.*s", curr_time, (int)bytesRead, buffer);

                char fixed_record[RECORD_LENGTH];
                make_fixed_record(fixed_record, record, record_len);

                write_log(log_file, fixed_record, sizeof(fixed_record), last_record_pos, SEC_IN_DAY * 1000 / PORT_SPEED_MS, 0, sem);

                cur_temp = atof(buffer);
                count_hour++;
                avg_hour += (cur_temp - avg_hour) / count_hour;

                cur_temp = atof(buffer);
                count_day++;
                avg_day += (cur_temp - avg_day) / count_day;
            }
        } else {
            perror("ReadFile (pd)");
            break;
        }
    }
#else
    while (!need_exit) {
        ssize_t bytesRead = read(fd, buffer, sizeof(buffer));
        if (bytesRead > 0) {

            get_time(curr_time, sizeof(curr_time));
            record_len = snprintf(record, sizeof(record), "%s %.*s", curr_time, (int)bytesRead, buffer);

            char fixed_record[RECORD_LENGTH];
            make_fixed_record(fixed_record, record, record_len);

            write_log(log_file, fixed_record, sizeof(fixed_record), last_record_pos, SEC_IN_DAY * 1000 / PORT_SPEED_MS, 0, sem);

            cur_temp = atof(buffer);
            count_hour++;
            avg_hour += (cur_temp - avg_hour) / count_hour;

            cur_temp = atof(buffer);
            count_day++;
            avg_day += (cur_temp - avg_day) / count_day;
        }
    }
#endif

#ifdef _WIN32
    WaitForSingleObject(thr_hour, INFINITE);
    CloseHandle(thr_hour);
    WaitForSingleObject(thr_day, INFINITE);
    CloseHandle(thr_day);
#else
    pthread_join(thr_hour, NULL);
    pthread_join(thr_day, NULL);
#endif

    return 0;
}
