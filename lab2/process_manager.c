#include <stdio.h>

#ifdef WIN32
#    include <windows.h>
#    include <stdio.h>

#    define sleep(x) Sleep(x * 1000)

typedef CRITICAL_SECTION rc_mutex;
typedef HANDLE rc_thread;
typedef DWORD rc_thread_id;
#    define init_mutex(mutex) InitializeCriticalSection(mutex)
#    define delete_mutex(mutex) DeleteCriticalSection(mutex)
#    define lock_mutex(mutex) EnterCriticalSection(mutex)
#    define unlock_mutex(mutex) LeaveCriticalSection(mutex)
#else
#    include <limits.h>
#    include <pthread.h>
#    include <stdlib.h>
#    include <sys/wait.h>
#    include <unistd.h>
# 	 include <stdio.h>

#    define sleep(x) sleep(x)

typedef pthread_mutex_t rc_mutex;
typedef pthread_t rc_thread;
typedef pthread_t rc_thread_id;
#    define init_mutex(mutex) rc_mutex mutex = PTHREAD_MUTEX_INITIALIZER
#    define delete_mutex(mutex) pthread_mutex_destroy(mutex)
#    define lock_mutex(mutex) pthread_mutex_lock(mutex)
#    define unlock_mutex(mutex) pthread_mutex_unlock(mutex)
#endif

#ifdef WIN32
typedef struct {
    PROCESS_INFORMATION pi;
    rc_mutex *mutex;
} MonitorParams;
#endif

struct Program {
    char name[256];
    char *arv[32];
};

int child_is_exited = 0;

#ifdef WIN32
rc_thread_id WINAPI handle_child(LPVOID param)
{
    printf(" THREAD is handling child processes...\n");

    MonitorParams *params = (MonitorParams *)param;
    PROCESS_INFORMATION pi = params->pi;
    rc_mutex *mutex = params->mutex;

    while (1) {

        rc_thread_id waitResult = WaitForSingleObject(pi.hProcess, INFINITE);

        if (waitResult == WAIT_OBJECT_0) {

            rc_thread_id exitCode;

            if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
                printf("GetExitCodeProcess failed (%d). \n", GetLastError());
            }

            printf(" CHILD with PID %d exited with code %d\n", pi.dwProcessId,
                   exitCode);
			
			child_is_exited = 1;

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);


            free(params);
            break;
        }
        else {
            printf(" Error waiting: %lu\n", GetLastError());
            break;
        }
    }

    return 0;
}
#else


init_mutex(mutex);

void *handle_child(void *args)
{
    printf(" THREAD is handling child processes...\n");

    while (1) {

        int status;
        pid_t pid = waitpid(-1, &status, 0);

        if (pid > 0) {
            if (WIFEXITED(status)) {
                printf(" CHILD with PID %d exited with code %d\n", pid, WEXITSTATUS(status));

                lock_mutex(&mutex);
                child_is_exited = 1;
                unlock_mutex(&mutex);
            }
        }
        else if (pid == -1) {
            lock_mutex(&mutex);
            if (child_is_exited == 1) {
                unlock_mutex(&mutex);
                break;
            }
            unlock_mutex(&mutex);
            sleep(1);
        }
    }

    return 0;
}
#endif

void start_process(struct Program program,
                      int block_parent) {
#ifdef WIN32
	printf("1");
    rc_mutex mutex;
	
    if (!block_parent)
        init_mutex(&mutex);

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    rc_thread threadHandles;

	if (!CreateProcess(program.name, program.arv[1], NULL, NULL, FALSE, 0, NULL, NULL,
					   &si, &pi))
	{
		printf(" CreateProcess failed (%d). \n", GetLastError());
		exit(1);
	}
	else {
		printf(" PARENT with PID %d created CHILD with PID %d\n",
			   GetCurrentProcessId(), pi.dwProcessId);

	}

	if (block_parent) {

		WaitForSingleObject(pi.hProcess, INFINITE);
		rc_thread_id exitCode;

		if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
			printf("GetExitCodeProcess failed (%d). \n", GetLastError());
			exit(1);
		}

		printf(" CHILD with PID %d, PPID %d exited with code %d\n\n",
			   pi.dwProcessId, GetCurrentProcessId(), exitCode);
		
		child_is_exited = 1;
	}
	else {
		MonitorParams *params = (MonitorParams *)malloc(sizeof(MonitorParams));
		params->pi = pi;
		params->mutex = &mutex;

		threadHandles = CreateThread(NULL, 0, handle_child, params, 0, NULL);

		if (threadHandles == NULL) {
			printf(" Error creating thread: %d\n", GetLastError());
			free(params);
			exit(1);
		}
	}
#else

    rc_thread thread;

    int status;
    int status_addr;

    pid_t pid;
    pid_t parent_pid = getpid();

    if (!block_parent) {

        int status;

        status = pthread_create(&thread, NULL, handle_child, NULL);

        if (status != 0) {
            perror("main ");
        }

        status = pthread_detach(thread);

        if (status != 0) {
            perror("pthread_detach");
            exit(1);
        }
    }

	if (getpid() == parent_pid) {

		pid = fork();

		if (pid == 0)
			execv(program.name, program.arv);

		else if (getpid() == parent_pid) {
			printf(" PARENT with PID %d created CHILD with PID %d\n",
				   getpid(), pid);

			if (block_parent) {
				waitpid(pid, &status, 0);
				printf(
					" CHILD with PID %d, PPID %d exited with code %d\n\n",
					pid, getpid(), WEXITSTATUS(status));
			}
		}
		else {
			perror("fork");
			exit(1);
		}
	}
#endif

    // PARENT-process code block

    printf(" Parent process signal...\n");
    sleep(10);

    // PARENT-process code block

    if (!block_parent) {
        while (1) {
            lock_mutex(&mutex);
            if (child_is_exited = 1) {
                unlock_mutex(&mutex);
                delete_mutex(&mutex);
                break;
            }
            unlock_mutex(&mutex);
            sleep(10);

        }
    }
}
