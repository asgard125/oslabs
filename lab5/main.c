#include "sqlite3.h"
#include "serial.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <signal.h>
#else
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#ifdef _WIN32
#define PORT_RD "COM9"
#define SOCKET int
#else
#define PORT_RD "/dev/pts/5"
#endif

#define IP "127.0.0.1"
#define PORT 8080

// Определение временных констант
const int SEC_IN_DAY = 86400;
const int SEC_IN_HOUR = 3600;
const int READ_WAIT_MS = 100;
const int MAX_CLIENTS = 256;

struct thr_data {
#ifdef _WIN32
    HANDLE fd;
#else
    int fd;
#endif
    sqlite3 *db;
};

// Глобальная переменная для управления окончанием работы
volatile unsigned char need_exit = 0;

// Обработка прерывания для Windows
#ifdef _WIN32
BOOL WINAPI console_handler(DWORD signal) {
    if (signal == CTRL_C_EVENT) {
        need_exit = 1;
        return TRUE;
    }
    return FALSE;
}
// Обработка сигнала SIGINT для POSIX
#else
void sig_handler(int sig) {
    if (sig == SIGINT) {
        need_exit = 1;
    }
}
#endif

void handle_client(SOCKET client_socket) {
    char buffer[1024];
    int read_size = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (read_size < 0) {
        perror("recv failed");
        #ifdef _WIN32
        closesocket(client_socket);
        #else
        close(client_socket);
        #endif
        return;
    }

    buffer[read_size] = '\0';
    char *uri_start = strstr(buffer, "GET ");
    if (!uri_start) {
        #ifdef _WIN32
        closesocket(client_socket);
        #else
        close(client_socket);
        #endif
        return;
    }

    uri_start += 4; // Пропускаем "GET "
    char *uri_end = strchr(uri_start, ' ');
    if (uri_end) {
        *uri_end = '\0';
    }

    #ifdef _WIN32
    _putenv_s("REQUEST_URI", uri_start);
    #else
    setenv("REQUEST_URI", uri_start, 1);
    #endif

    FILE *cgi = _popen("temp.cgi", "r");
    if (!cgi) {
        perror("Failed to run CGI script");
        #ifdef _WIN32
        closesocket(client_socket);
        #else
        close(client_socket);
        #endif
        return;
    }

    char cgi_response[262144];
    char response_body[262144] = {0};
    while (fgets(cgi_response, sizeof(cgi_response), cgi)) {
        strcat(response_body, cgi_response);
    }
    fclose(cgi);

    char http_response[262144];
    snprintf(http_response, sizeof(http_response), 
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %zu\r\n\r\n"
             "%s", 
             strlen(response_body), response_body);

    send(client_socket, http_response, strlen(http_response), 0);

    #ifdef _WIN32
    closesocket(client_socket);
    #else
    close(client_socket);
    #endif
}

void execute_sql(sqlite3 *db, const char *sql) {
    char *err_msg = 0;
    if (sqlite3_exec(db, sql, 0, 0, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }
}

void prepare_bind_step(sqlite3 *db, const char *sql, sqlite3_stmt **statement, double value, int index) {
    if (sqlite3_prepare_v2(db, sql, -1, statement, 0) == SQLITE_OK) {
        sqlite3_bind_double(*statement, index, value);
        if (sqlite3_step(*statement) != SQLITE_DONE) {
            fprintf(stderr, "Error: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(*statement);
            sqlite3_close(db);
            exit(EXIT_FAILURE);
        }
    } else {
        fprintf(stderr, "Error: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }
    sqlite3_finalize(*statement);
}

#ifdef _WIN32
DWORD WINAPI thr_routine_db(void *args) {
#else
void* thr_routine_db(void *args) {
#endif
    struct thr_data *params = (struct thr_data *)args;
    char buffer[255];
    double cur_temp;
    time_t start_hour = time(NULL), start_day = time(NULL);
    char *sql;
    sqlite3_stmt *statement;

    while (!need_exit) {
        #ifdef _WIN32
        DWORD bytesRead;
        if (ReadFile(params->fd, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
        #else
        ssize_t bytesRead = read(params->fd, buffer, sizeof(buffer));
        if (bytesRead > 0) {
        #endif
            cur_temp = atof(buffer);
            sql = "INSERT INTO temp_all (date, temp) VALUES (DATETIME('now', 'localtime'), ?)";
            prepare_bind_step(params->db, sql, &statement, cur_temp, 1);

            sql = "DELETE FROM temp_all WHERE date < DATETIME('now', 'localtime', '-1 day');";
            execute_sql(params->db, sql);

            time_t now = time(NULL);
            if (difftime(now, start_hour) >= SEC_IN_HOUR) {
                sql = "INSERT INTO temp_hour (date, avg_temp) "
                      "SELECT DATETIME('now', 'localtime'), ROUND(AVG(temp), 1) "
                      "FROM temp_all WHERE date >= DATETIME('now', 'localtime', '-1 hour');";
                execute_sql(params->db, sql);

                sql = "DELETE FROM temp_hour WHERE date < DATETIME('now', 'localtime', '-1 month');";
                execute_sql(params->db, sql);
                start_hour = now;
            }

            // Daily record
            int db_year = -1;
            struct tm *tm_now = localtime(&now);
            int curr_year = tm_now->tm_year + 1900;

            if (difftime(now, start_day) >= SEC_IN_DAY) {
                sql = "INSERT INTO temp_day (date, avg_temp) "
                      "SELECT DATETIME('now', 'localtime'), ROUND(AVG(temp), 1) "
                      "FROM temp_all WHERE date >= DATETIME('now', 'localtime', '-1 day');";
                execute_sql(params->db, sql);

                if (curr_year != db_year) {
                    sql = "DELETE FROM temp_day WHERE strftime('%M', date) != strftime('%M', 'now');";
                    execute_sql(params->db, sql);
                    db_year = curr_year;
                }
                start_day = now;
            }
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    srand((unsigned int)time(NULL));

    // Настройка обработки сигнала
    #ifdef _WIN32
    if (!SetConsoleCtrlHandler(console_handler, TRUE)) {
        perror("Error setting handler");
        exit(EXIT_FAILURE);
    }
    #else
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = sig_handler;
    sigaction(SIGINT, &act, NULL);
    #endif

    // Конфигурация последовательного порта
    #ifdef _WIN32
    HANDLE fd = CreateFile(PORT_RD, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
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
    port_config(port_rd, BAUDRATE_115200);

    int fd = open(port_rd, O_RDONLY | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        perror("open port");
        exit(EXIT_FAILURE);
    }
    tcflush(fd, TCIFLUSH);
    #endif

    // Работа с базой данных
    sqlite3 *db;
    if (sqlite3_open("temperature.db", &db) != SQLITE_OK) {
        fprintf(stderr, "Error: %s\n", sqlite3_errmsg(db));
        exit(EXIT_FAILURE);
    }

    // Создание таблиц, если они не существуют
    execute_sql(db, "CREATE TABLE IF NOT EXISTS temp_all (date DATETIME DEFAULT CURRENT_TIMESTAMP PRIMARY KEY, temp REAL);");
    execute_sql(db, "CREATE TABLE IF NOT EXISTS temp_hour (date DATETIME DEFAULT CURRENT_TIMESTAMP PRIMARY KEY, avg_temp REAL);");
    execute_sql(db, "CREATE TABLE IF NOT EXISTS temp_day (date DATETIME DEFAULT CURRENT_TIMESTAMP PRIMARY KEY, avg_temp REAL);");

    // Включение WAL режима
    execute_sql(db, "PRAGMA journal_mode = WAL;");

    // Создание и запуск потока для работы с БД
    struct thr_data params_db = {fd, db};
    #ifdef _WIN32
    HANDLE thr_db = CreateThread(NULL, 0, thr_routine_db, &params_db, 0, NULL);
    if (!thr_db) {
        perror("CreateThread (thr_db)");
        exit(EXIT_FAILURE);
    }
    #else
    pthread_t db_thread;
    if (pthread_create(&db_thread, NULL, thr_routine_db, &params_db) != 0) {
        perror("pthread_create (thr_db)");
        exit(EXIT_FAILURE);
    }
    #endif

    // Инициализация сервера
    SOCKET server_socket;
    struct sockaddr_in server_addr;
    struct pollfd fds[MAX_CLIENTS] = {0};
    int nfds = 1;

    #ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        perror("WSAStartup failed");
        return 1;
    }
    #endif

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Failed to create socket");
        #ifdef _WIN32
        WSACleanup();
        #endif
        return 1;
    }

    // Настройка адреса и порта
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(IP);
    server_addr.sin_port = htons(PORT);

    // Привязка и прослушивание сокета
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        #ifdef _WIN32
        closesocket(server_socket);
        WSACleanup();
        #else
        close(server_socket);
        #endif
        return 1;
    }

    if (listen(server_socket, SOMAXCONN) < 0) {
        perror("Listen failed");
        #ifdef _WIN32
        closesocket(server_socket);
        WSACleanup();
        #else
        close(server_socket);
        #endif
        return 1;
    }

    printf("Server listening on port %d...\n", PORT);

    // Инициализация массива fds
    fds[0].fd = server_socket;
    fds[0].events = POLLIN;

    while (!need_exit) {
        int poll_count = poll(fds, nfds, READ_WAIT_MS);
        if (poll_count < 0) {
            perror("Failed to poll");
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            if (fds[i].fd == server_socket && (fds[i].revents & POLLIN)) {
                struct sockaddr_in client_addr;
                socklen_t client_addr_len = sizeof(client_addr);
                SOCKET client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);

                if (client_socket < 0) {
                    perror("Accept failed");
                    continue;
                }

                printf("New client connected\n");
                for (int j = 1; j < MAX_CLIENTS; ++j) {
                    if (fds[j].fd == -1) {
                        fds[j].fd = client_socket;
                        fds[j].events = POLLIN;
                        if (j >= nfds) {
                            nfds = j + 1;
                        }
                        break;
                    }
                }
            } else if (fds[i].fd > 0 && (fds[i].revents & POLLIN)) {
                handle_client(fds[i].fd);
                fds[i].fd = -1;
            }
        }
    }

    // Завершение работы
    #ifdef _WIN32
    WaitForSingleObject(thr_db, INFINITE);
    CloseHandle(thr_db);
    closesocket(server_socket);
    WSACleanup();
    CloseHandle(fd);
    #else
    pthread_join(db_thread, NULL);
    close(server_socket);
    close(fd);
    #endif

    sqlite3_close(db);
    return 0;
}