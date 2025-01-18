#include "serial.c"

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#    define PORT_WR "COM8"
#else
#    define PORT_WR "/dev/pts/3"
#endif

double init_random_temp(int min, int max)
{
    double temp = rand() % (max - min + 1) + min;
    return temp;
}

double rand_temp_change(double min, double max)
{
    return ((double)rand() / RAND_MAX) * (max - min) + min;
}

int main(int argc, char *argv[])
{
    srand(time(0));

#ifdef _WIN32
    HANDLE hSerial = CreateFile(
        PORT_WR,
        GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
    if (hSerial == INVALID_HANDLE_VALUE) {
        perror("CreateFile (hSerial)");
        exit(EXIT_FAILURE);
    }
    if (!port_config(hSerial, BAUDRATE_115200)) {
        perror("port_config (hSerial)");
        CloseHandle(hSerial);
        exit(EXIT_FAILURE);
    }
#else
    const char* port_wr = PORT_WR;
    speed_t baud_rate = BAUDRATE_115200;
    port_config(port_wr, baud_rate);

    int fd = open(port_wr, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        perror("open port");
        exit(EXIT_FAILURE);
    }
#endif
    double temp = init_random_temp(-50, 50);
    char data[10];
    sprintf(data, "%.1f", temp);

#ifdef _WIN32
    while (1) {
        DWORD bytes_written;
        if (!WriteFile(hSerial, data, strlen(data), &bytes_written, NULL)) {
            perror("WriteFile");
            break;
        }
        temp += rand_temp_change(-0.2, 0.2);
        sprintf(data, "%.1f", temp);
        Sleep(PORT_SPEED_MS);
    }
#else
    while(1) {
        write(fd, data, strlen(data));
        temp += rand_temp_change(-0.2, 0.2);
        sprintf(data, "%.1f", temp);
        usleep(PORT_SPEED_MS * 1000);
    }
#endif
    return 0;
}