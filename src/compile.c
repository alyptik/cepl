/*
 * compile.c - compile the generated source code
 *
 * AUTHOR: Joey Pabalinas <alyptik@protonmail.com>
 * See LICENSE file for copyright and license details.
 */

#include <err.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

int compile(char const *cc, char *src, char *const *args)
{
	pid_t pid;
	int fd;

	if ((pid = fork()) == -1)
		err(1, "%s", "error forking compiler process");

	switch (pid) {

	/* child */
	case 0:
		if ((fd = open("/dev/null", O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) == -1)
			err(1, "%s", "error opening /dev/null");
		/* redirect stdout to /dev/null */
		dup2(fd, 1);
		execvp(cc, args);
		/* execv should never return */
		err(1, "execv returned");
		break;

	/* parent */
	default: printf("%s - %s: %d\n", "parent", "child pid", pid);
	}

	return 0;
}
