#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_LINE 80
#define PROMPT "osh>"

int getCommand(char *str) 
{
	char ch;
	int i = 0;
	
	while (((ch = getchar()) != '\n') && (i < MAX_LINE)) 
	{
		str[i++] = ch;
	}

	if (i == MAX_LINE && ch != '\n')
	{
		printf("Command too long.\n");
		i = -1;
	}
	else 
	{
		str[i] = 0;
	}
	while (ch != '\n') ch = getchar();
	return i;
}

int parseCommand(char command[], char *args[])
{
    int args_num = 0;
    char* arg;

    while (arg = strtok_r(command, " ", &command))
    {
        args[args_num++] = arg;
    }

    args[args_num] = NULL;

    return args_num;
}

int redirectInput(char dir[])
{
	int fd = open(dir, O_RDONLY);
	if (fd == -1)
	{
		printf("Can't open file.\n");
		exit(2);
	}

	if (dup2(fd, STDIN_FILENO) < 0)
	{
		printf("Failed redirecting input.\n");
		exit(2);
	}

	close(fd);

	return 0;
}

int redirectOutput(char dir[])
{
	int fd = creat(dir, O_WRONLY | O_CREAT | S_IRWXU);
	if (fd == -1)
	{
		printf("Can't open file.\n");
		exit(2);
	}

	if (dup2(fd, STDOUT_FILENO) < 0)
	{
		printf("Failed redirecting output.\n");
		exit(2);
	}

	close(fd);

	return 0;
}

int findPipePosition(char *args[], int args_num)
{
	for (int i = 0; i < args_num; i++)
	{
		if (strcmp(args[i], "|") == 0)
		{
			return i;
		}
	}
	return -1;
}

int pipeProcesses(char *args[], int args_num, int pipePosition)
{
	int p[2];
	if (pipe(p) < 0)
	{
		printf("Can't create pipe.\n");
		exit(2);
	}

	// Separate args[] to 2 parts
	char *args2[MAX_LINE];
	for (int i = pipePosition + 1; i < args_num; i++)
	{
		args2[i - pipePosition - 1] = args[i];
	}

	int args2_num = args_num - pipePosition - 1;
	args_num = pipePosition;

	args[args_num] = NULL;
	args2[args2_num] = NULL;

	pid_t pidProcess2 = fork();

	if (pidProcess2 < 0)
	{
		printf("Can't fork process.\n");
		exit(2);
	}
	else if (pidProcess2 == 0)
	{
		if (dup2(p[0], STDIN_FILENO) < 0)
		{
			printf("Failed redirecting input.\n");
			exit(2);
		}
		close(p[0]);
		close(p[1]);

		if (execvp(args2[0], args2) == -1)
		{
			printf("Failed to execute the command.\n");
			exit(2);
		}
	}
	else
	{
		pidProcess2 = fork();

		if (pidProcess2 < 0)
		{
			printf("Can't fork process.\n");
			exit(2);
		}
		else if (pidProcess2 == 0)
		{
			if (dup2(p[1], STDOUT_FILENO) < 0)
			{
				printf("Failed redirecting output.\n");
				exit(2);
			}
			close(p[1]);
			close(p[0]);

			if (execvp(args[0], args) == -1)
			{
				printf("Failed to execute the command.\n");
				exit(2);
			}
		}
		else
		{
			close(p[0]);
			close(p[1]);
			waitpid(pidProcess2, NULL, 0);
		}

		wait(NULL);
	}
}

int executeCommand(char* args[], int args_num)
{
    // Check if redirect to input file
	if (args_num > 2 && strcmp(args[args_num - 2], "<") == 0)
	{
		redirectInput(args[args_num - 1]);
		args[args_num - 2] = NULL;
		args_num = args_num - 2;
	}

	// Check if redirect to output file
	if (args_num > 2 && strcmp(args[args_num - 2], ">") == 0)
	{
		redirectOutput(args[args_num - 1]);
		args[args_num - 2] = NULL;
		args_num = args_num - 2;
	}

	int pipePosition;

	if (args_num > 2 && (pipePosition = findPipePosition(args, args_num)) != -1)
	{
		pipeProcesses(args, args_num, pipePosition);
	}

	if (execvp(args[0], args) == -1)
	{
		printf("Failed to execute the command.\n");
		exit(2);
	}

	return 0;
}

int main()
{
    bool running = true;
	int flagWait;
	char command[MAX_LINE + 1];
	char *args[MAX_LINE / 2 + 1];
	int args_num;
	char preCommand[MAX_LINE + 1] = "";
	pid_t pid;

    while (running)
    {
        flagWait = 1;
        printf(PROMPT);
        fflush(stdout);

        int cmdLenght = getCommand(command);

		// Can't input command or command is empty
        if (cmdLenght <= 0) continue;

		// Check history
		if (strcmp(args[0], "!!") == 0)
		{
			if (strcmp(preCommand, "") == 0)
			{
				printf("No command in history.\n");
			}
			strcpy(command, preCommand);
		}

		// Store current command to history
		strcpy(preCommand, command);

		// Parse command to list of arguments
		args_num = parseCommand(command, args);

		// Check if exit
        if (strcmp(args[0], "exit") == 0)
		{
			running = false;
			continue;
		}

		// Check if change directory
		if (strcmp(args[0], "cd") == 0)
		{
			if (chdir(args[1]) < 0)
			{
				printf("Can't change directory.\n");
			}
			continue;
		}

        // Check if process executes in background
		if (strcmp(args[args_num - 1], "&") == 0)
		{
			flagWait = 0;
			args[args_num - 1] = NULL;
			args_num--;
		}

		pid = fork();

		if (pid == 0)	// Child process
		{
			executeCommand(args, args_num);
		}
		else if (pid > 0)	// Parent process
		{
			if (flagWait == 0)
			{
				continue;
			}
			waitpid(pid, NULL, 0);
		}
		else	// Can't fork
		{
			printf("Can't fork process.\n");
		}
    }
    return 0;
}