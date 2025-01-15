#ifdef WIN32
#    include <windows.h>
# 	 include <stdio.h>
#    define MAX_LEN MAX_PATH
#else
#    include <limits.h>
#    include <unistd.h>
# 	 include <stdio.h>
#    define MAX_LEN NAME_MAX
#endif

#define BLOCK_PARENT 1

struct Program {
    const char prog_name[MAX_LEN + 1];
    char *arv[10];
};

void start_process(struct Program program,
                      int block_parent);

int main()
{

#ifdef WIN32
    struct Program program = {"child_process.exe", {"child_process.exe", "1", NULL}};
#else
	struct Program program = {"child_process", {"child_process", "1", NULL}};
#endif
    printf("\n\n parent block true \n\n\n");
    start_process(program, BLOCK_PARENT);

    printf("\n\n parent block false\n\n\n");
    start_process(program, !BLOCK_PARENT);

    return 0;
}
