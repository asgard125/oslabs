#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <mutex>
#include <iomanip>
#include <string>
#ifdef _WIN32
#   include <Windows.h>
#else
#   include <sys/mman.h>
#   include <fcntl.h>
#   include <unistd.h>
#endif

std::mutex ThreadMutex;

struct SharedMemory {
    int Id;
    int c;
    int mTh;
};

void logger(int Id, const SharedMemory& SMem, const std::string& action) {
    std::ofstream File("log_file.txt", std::ios::app);
	
    auto now = std::chrono::system_clock::now();
    auto point = std::chrono::system_clock::to_time_t(now);
    auto milisec = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
	
    File << "Date: " << std::put_time(std::localtime(&point), "%d.%m.%y; Time: %X.") << milisec.count() << "; ID: " << Id << "; Count: " << SMem.c << "; Action: " << action << "." << std::endl;
    File.close();
}

void copyLogger(int Id, SharedMemory* SMem) {
    while (true) {
        ThreadMutex.lock();
        logger(Id, *SMem, "Current count");
        ThreadMutex.unlock();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void processCopy(int Id, SharedMemory* SMem, int copyNum) {
    if (copyNum == 1) {
        ThreadMutex.lock();
        logger(Id, *SMem, "Copy 1: +10");
        SMem->c += 10;
        logger(Id, *SMem, "Copy 1: Exit");
        ThreadMutex.unlock();
    }
    else if (copyNum == 2) {
        ThreadMutex.lock();
        logger(Id, *SMem, "Copy 2: *2");
        SMem->c *= 2;
        ThreadMutex.unlock();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        ThreadMutex.lock();
        logger(Id, *SMem, "Copy 2: /2");
        SMem->c /= 2;
        logger(Id, *SMem, "Copy 2: Exit");
        ThreadMutex.unlock();
    }
}

void incCount(bool ismTh, int Id, SharedMemory* SMem) {
    if (ismTh == true) {
		
#ifdef _WIN32
        STARTUPINFO siPrint;
        PROCESS_INFORMATION piPrint;
		
        ZeroMemory(&siPrint, sizeof(siPrint));
        ZeroMemory(&piPrint, sizeof(piPrint));
		
        siPrint.cb = sizeof(siPrint);
        std::string cmdKeyPrint = "myprog.exe-print";

        CreateProcess(NULL, const_cast<char*>(cmdKeyPrint.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &siPrint, &piPrint); 
#else
        if (fork() == 0) {
            copyLogger(Id, SMem);
        }
#endif
    }
    int cycle = 0;
    while (true) {
        {
            ThreadMutex.lock();
            SMem->c++;
            ThreadMutex.unlock();
        }
        cycle++;
        if (cycle % 10 == 0 && ismTh == true) {
#ifdef _WIN32
            STARTUPINFO StartInfo1;
            PROCESS_INFORMATION ProcessInfo1;
            ZeroMemory(&StartInfo1, sizeof(StartInfo1));
            ZeroMemory(&ProcessInfo1, sizeof(ProcessInfo1));
            StartInfo1.cb = sizeof(StartInfo1);
            std::string cmdKey1 = "myprog.exe -copy 1";

            CreateProcess(NULL, const_cast<char*>(cmdKey1.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &StartInfo1, &ProcessInfo1);
            STARTUPINFO StartInfo2;
            PROCESS_INFORMATION ProcessInfo2;
			
            ZeroMemory(&StartInfo2, sizeof(StartInfo2));
            ZeroMemory(&ProcessInfo2, sizeof(ProcessInfo2));
			
            StartInfo2.cb = sizeof(StartInfo2);
            std::string cmdKey2 = "myprog.exe -copy 2";
			
            CreateProcess(NULL, const_cast<char*>(cmdKey2.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &StartInfo2, &ProcessInfo2);
			
            WaitForSingleObject(ProcessInfo1.hProcess, INFINITE);
            WaitForSingleObject(ProcessInfo2.hProcess, INFINITE);
			
            CloseHandle(ProcessInfo1.hProcess);
            CloseHandle(ProcessInfo1.hThread);
            CloseHandle(ProcessInfo2.hProcess);
            CloseHandle(ProcessInfo2.hThread);
#else
            if (fork() == 0) {
                ThreadMutex.lock();
                int processId1 = SMem->Id++;
                ThreadMutex.unlock();
                processCopy(processId1, SMem, 1);
                exit(0);
            }
            if (fork() == 0) {
                ThreadMutex.lock();
                int processId1 = SMem->Id++;
                ThreadMutex.unlock();
                processCopy(processId1, SMem, 2);
                exit(0);
            }
#endif
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
}

int main(int argc, char* argv[]) {
    SharedMemory* SMem;
#ifdef _WIN32
    HANDLE fileMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SharedMemory), "my_shared_memory");
    SMem = static_cast<SharedMemory*>(MapViewOfFile(fileMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(SharedMemory)));
#else
    const char* shMemName = "shMemtmp";
    int shMemSize = sizeof(SharedMemory);
    int shm_fd = shm_open(shMemName, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, shMemSize);
    SMem = static_cast<SharedMemory*>(mmap(0, shMemSize, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0));
#endif
    bool ismTh = false;
    ThreadMutex.lock();
    int Id = SMem->Id++;
    SMem->c = SMem->c;
	
    if (SMem->mTh == 0) {
        ismTh = true;
        SMem->mTh = 1;
    }
    ThreadMutex.unlock();
	
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "-copy" && argc > 2) {
            int copyNum = std::stoi(argv[2]);
            processCopy(Id, SMem, copyNum);
            return 0;
        }
        if (arg == "-print") {
            copyLogger(Id, SMem);
            return 0;
        }
    }
	
    std::thread incTh(incCount, ismTh, Id, SMem);
    while (true) {
        logger(Id, *SMem, "Main Thread");
        std::cout << "enter count: ";
        std::string input;
        std::cin >> input;
        try {
                int newValue = std::stoi(input);
                {
                    ThreadMutex.lock();
                    SMem->c = newValue;
                    ThreadMutex.unlock();
                }
            }
        catch (const std::invalid_argument& error) {
            std::cerr << "input is wrong" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    incTh.join();
#ifdef _WIN32
    UnmapViewOfFile(SMem);
    CloseHandle(fileMap);
#else
    munmap(SMem, shMemSize);
    shm_unlink(shMemName);
#endif
    return 0;
}