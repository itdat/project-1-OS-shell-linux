#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_LEN_CMD 80
#define PROMPT "osh>"

int getCmd(char *str) 
{
	char ch;
	int i = 0;
	
	while (((ch = getchar()) != '\n') && (i < MAX_LEN_CMD)) 
	{
		str[i++] = ch;
	}

	if (i == MAX_LEN_CMD && ch != '\n')
	{
		printf("Command exceeds maximum command length\n");
		i = -1;
	}
	else 
	{
		str[i] = 0;
	}
	while (ch != '\n') ch = getchar();
	return i;
}

int parseCmdToListArgs(char *cmdBuffer, char *args[])
{
    int args_num = 0;
    char* arg;

    while (arg = strtok_r(cmdBuffer, " ", &cmdBuffer))
    {
        args[args_num++] = arg;
    }

    args[args_num] = NULL;

    return args_num;
}

int execCmd(char* args[], int execInBg)
{
    pid_t pid = fork();
	if (pid < 0)
	{
		printf("Fork process error.\n");
		return 0;
	}

	int status;

	if (pid == 0)
	{
        // CHILD PROCESS
        // printf("Child pid #%d is running\n", getpid());
		status = execvp(args[0], args);
		if (status == -1)
		{
			printf("Failed to execute the command.\n");
		}
		return 0;
	} 
	else 
	{
        // PARENT PROCESS
		if (execInBg == 1)
		{
			printf("Pid #%d running in background.\n", pid);
		} 
		else 
		{
			pid_t childpid = wait(&status);
			// if (WIFEXITED(status))
			// {
			// 	printf("Exit status: %d\n", WEXITSTATUS(status)); 
			// }
            // printf("Child pid #%d is terminated\n", childpid);
		}
	}
	return 1;
}

int redirectFile(char *dir, char* args[], char* buffer, int execInBg, char mode)
{
	int fd;
	if (mode == 'i')
	{	
		//open file 
		fd = open(dir, O_RDONLY);
		if (fd == -1)
		{
			printf("Can't open file.\n");
			return -1;
		}
		
		//save stdin
		int backToStdin = dup(0);
		close(0);
		
		//redirect input from keyboard to file
		if(dup2(fd, STDIN_FILENO) == -1)
		{
			printf("Failed redirecting input.\n");
			return -1;
		}	
		
		//close fd (no longer need)
		close(fd);
		
		//run command
		execCmd(args, execInBg);
		
		//redirect input back from file to stdin
		dup2(backToStdin, 0);
		close(backToStdin);
		
		return 1;
	}
	else
	{
		//open file 
		fd = open(dir, O_WRONLY | O_CREAT, S_IRWXU);
		if (fd == -1) 
		{
			printf("Can open file.\n");
			return -1;
		}

		//save stdout
		int backToStdout = dup(1);
		close(1);

		//redirect output from stdout to file
		if(dup2(fd, STDOUT_FILENO) == -1)
		{
			printf("Failed redirecting output.\n");
			return -1;
		}

		//close fd (no longer need)
		close(fd);

		//run command
		execCmd(args, execInBg);

		//redirect outout back from file to stdout
		dup2(backToStdout, 1);
		close(backToStdout);

		return 1;
	}
}

int main()
{
    bool run = true;

    while (run)
    {
        wait(NULL);
        printf("%s", PROMPT);
        fflush(stdout);

        char cmdBuffer[MAX_LEN_CMD + 1];

        setbuf(stdin, NULL);
        int cmdLenght = getCmd(cmdBuffer);
        if (cmdLenght <= 0) continue;

        char *args[MAX_LEN_CMD];

        int args_num = parseCmdToListArgs(cmdBuffer, args);

        if (strcmp(args[0], "exit") == 0) exit(0);

        int execInBg = 0;
        // Check if process executes in background
		if (strcmp(args[args_num - 1], "&") == 0)
		{
			execInBg = 1;
			args[args_num - 1] = NULL;
			args_num--;
		}

        execCmd(args, execInBg);
    }
    return 0;
}