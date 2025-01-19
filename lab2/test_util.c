#ifdef WIN32
#    include <windows.h>
# 	 include <stdio.h>
#else
#    include <limits.h>
#    include <unistd.h>
# 	 include <stdio.h>
#endif

struct Program {
    const char prog_name[256];
    char *arv[32];
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
    start_process(program, 1);

    printf("\n\n parent block false\n\n\n");
    start_process(program, 0);

    return 0;
}
