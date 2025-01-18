#ifdef _WIN32
#    include <windows.h>
#else
#    include <fcntl.h>
#    include <termios.h>
#    include <stdlib.h>
#    include <unistd.h>
#endif

#include <stdio.h>

#ifdef _WIN32
#    define BAUDRATE_4800   CBR_4800
#    define BAUDRATE_9600   CBR_9600
#    define BAUDRATE_19200  CBR_19200
#    define BAUDRATE_38400  CBR_38400
#    define BAUDRATE_57600  CBR_57600
#    define BAUDRATE_115200 CBR_115200
#else
#    define BAUDRATE_4800   B4800
#    define BAUDRATE_9600   B9600
#    define BAUDRATE_19200  B19200
#    define BAUDRATE_38400  B38400
#    define BAUDRATE_57600  B57600
#    define BAUDRATE_115200 B115200
#endif

const int PORT_SPEED_MS = 1000;

#ifdef _WIN32
BOOL port_config(HANDLE hSerial, DWORD baud_rate)
{
    DCB dcbSerialParameters = {0};
    dcbSerialParameters.DCBlength = sizeof(dcbSerialParameters);

    if (!GetCommState(hSerial, &dcbSerialParameters)) {
        perror("GetCommState");
        return FALSE;
    }

    dcbSerialParameters.fOutxCtsFlow = FALSE;
    dcbSerialParameters.fOutxDsrFlow = FALSE;

    dcbSerialParameters.fDtrControl = DTR_CONTROL_ENABLE;
    dcbSerialParameters.fRtsControl = RTS_CONTROL_ENABLE;

    dcbSerialParameters.BaudRate = baud_rate;
    dcbSerialParameters.ByteSize = 8;
    dcbSerialParameters.StopBits = ONESTOPBIT;
    dcbSerialParameters.Parity   = NOPARITY;

    if (!SetCommState(hSerial, &dcbSerialParameters)) {
        perror("SetCommState");
        return FALSE;
    }
    return TRUE;
}
#else
void port_config(const char* port, speed_t baud_rate)
{
    int fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        perror("open port");
        exit(EXIT_FAILURE);
    }

    struct termios options;
    if (tcgetattr(fd, &options) < 0) {
        perror("tcgetattr");
        close(fd);
        exit(EXIT_FAILURE);
    }

    cfsetispeed(&options, baud_rate);
    cfsetispeed(&options, baud_rate);

    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CRTSCTS;

    if (tcsetattr(fd, TCSANOW, &options) < 0) {
        perror("tcsetattr");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);
}
#endif
