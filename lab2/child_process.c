#include <stdlib.h>

#ifdef _WIN32
#    include <windows.h>
#    include <stdio.h>
#    define sleep(x) Sleep(x * 1000)
#else
#    include <unistd.h>
# 	 include <stdio.h>
#    define sleep(x) sleep(x)
#endif

int main(int argc, char *argv[])
{
    if (argc > 1) {
        int sleep_time = atoi(argv[1]);
        int exit_code = sleep_time;
		
		printf(" I'm a child! \n");
		
        sleep(sleep_time);
        exit(exit_code);
    }
    sleep(10);
    exit(0);
}