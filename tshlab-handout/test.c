/* By JackalDire, Jan 29 2010 
 * Tested on Linux Kernel 2.6.32, gcc 4.4.3 */
#include <unistd.h>
#include <sys/wait.h>
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
 
#define LINE_MAX 8192
#define ARG_MAX 1024
#define ARG_NR_MAX 32
 
// #define CHKERR(ret, msg) if (ret < 0) {
        // fprintf(stderr, "%s %s\n", msg, strerror(errno)); 
        // exit(-1);   
    // } 
 
char * args[ARG_NR_MAX + 1];
extern char ** environ;
 
char line[LINE_MAX + 1];
 
void parse_command(char * cmd)
{
    char * res;
    size_t cnt = 0;
    /* tokenize the command string by space */
    while ((res = strsep(&cmd, " ")) != NULL) {
        printf("%sn", res);
        args[cnt++] = strdup(res);
    }
    args[cnt] = NULL;
}
 
void pr_exit(const char * name, int status)
{
    if (WIFEXITED(status)) // exit normally
        return;
    else if (WIFSIGNALED(status))
        fprintf(stderr, "%s exit abnormally, signal %d caught%s.n",
                name, WTERMSIG(status),
#ifdef WCOREDUMP
            WCOREDUMP(status) ? " (core file generated)" : "");
#else
            "");
#endif
    else if (WIFSTOPPED(status))
        fprintf(stderr, "child stopped, signal %d caught.",
                WSTOPSIG(status));
}
 
int main(int argc, char * argv[])
{
    char c;
    size_t idx;
    int r;
    int status;
 
    while (1) {
        idx = 0;
        bzero(line, LINE_MAX + 1);
 
        c = fgetc(stdin);
        while (c && c != 'n') {
            line[idx++] = c;
            c = fgetc(stdin);
        }
 
        parse_command(line);
        r = fork(); 
        // CHKERR(r, "fork");
        if (r == 0){
            r = execvp(args[0], args);
            //printf("ret : %dn", r);
            // CHKERR(r, args[0]);
        } else {
            wait(&status);
            pr_exit(args[0], status);
        }
    }
    return 0;
}