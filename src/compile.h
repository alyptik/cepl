/*
 * compile.h - compile the generated source code
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#define UNUSED __attribute__ ((unused))
extern char **environ;
int pipemain[2];

int compile(char const *cc, char *src, char *const ccargs[], char *const execargs[]);
int pipe_fd(int in_fd, int out_fd);

/* silence linter */
long syscall(long number, ...);
int fexecve(int fd, char *const argv[], char *const envp[]);
