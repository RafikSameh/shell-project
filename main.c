#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#define shell_builtin  1
#define command        2

// Maximum allowed command line length
const int MAX_LINE_LEN = 120;
char *str;
//Handles for child termination
void sig_hand()
{
	time_t t;
	time(&t);
	char *file="logs.txt";
	FILE *fp = fopen(file,"a");
	fprintf(fp,"Child process was terminated | %s",ctime(&t));
	fclose(fp);
}
//Handles for ctrl+c or exit to terminate the shell and any child
void quit_hand()
{
	char *file="logs.txt";
	FILE *fp = fopen(file,"w");
	if (fp == NULL)
	{
		perror("fopen");
		kill(0,SIGTERM);
		exit(EXIT_FAILURE);
	}
	fclose(fp);
	killpg(getpgid(0),SIGKILL); //kill child processes forcefully if any in this process group
	exit(EXIT_SUCCESS);
}
//Parse the inputs into tokens
void parse_input(char **args)
{
    int i = 0;
    //char *args[MAX_LINE_LEN / 2 + 1]; // Sufficient space for arguments
    ssize_t bytes_read;
    // Allocate memory dynamically for the input string using getline()
    str = (char *)malloc(sizeof(char) * MAX_LINE_LEN);
    if (str == NULL) {
        perror("malloc failed");
        exit(1);
    }
    // Read the entire command line, including spaces
    bytes_read = getline(&str, &MAX_LINE_LEN, stdin);
    if (bytes_read == -1) {
        perror("getline failed");
        //continue; // Avoid infinite loop due to errors
    }
    // Remove trailing newline, if present
    if (bytes_read > 0 && str[bytes_read - 1] == '\n') {
        str[bytes_read - 1] = '\0'; // Replace newline with null terminator
    }
    // Tokenize the input string
    char *token = strtok(str, " ");
    while (token != NULL) {
        if (i >= MAX_LINE_LEN / 2) {
            fprintf(stderr, "Error: Too many arguments! Maximum %d allowed.\n",
                    MAX_LINE_LEN / 2);
            break; // Prevent buffer overflow by limiting arguments
        }
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    // Terminate the argument list with a NULL pointer
    args[i] = NULL;
    int j = 0;
    while (args[j] != NULL) {
        if (args[j][0] == '$') {
            // Extract variable name after $
            char *varname = args[j] + 1;  // Start after the $

            // Get the variable value from the environment
            char *valueOr = getenv(varname);
            char *value= (char *)malloc(sizeof(valueOr));
            if(!value)
            {
                perror("Error allocating copy of varname");
            }
            strcpy(value,valueOr);
            if (value == NULL) {
                fprintf(stderr, "Error: Variable '%s' not found in the environment.\n", varname);
                // Consider handling the error differently (e.g., leave unexpanded or exit)
            } else {
                // Replace the argument with the variable value
                char *tok = strtok(value," ");
                int k=0;
                while(tok)
                {
                    args[j]=tok;
                    tok=strtok(NULL," ");
                    j++;
                }
                break;
            }
        }
        j++;
    }
}
//excute commands from STDIN
void execute_exp(char **args)
{
    // Create a child process
    pid_t pid = fork();
    if (pid < 0) { // failure
        perror("fork failed");
        exit(1);
    }
    else if(pid == 0)
    {
        if((execvp(args[0], args))== -1)    //check if execvp failed
        {
            perror("Error incorrect command for execvp");
        }
        exit(1); // Ensure abnormal termination is reported
    }
    else { // Parent process
        waitpid(pid, NULL, 0);
    }
}
//Handles the export command dependencies
void export_cmd(char **args)
{
    int status;
    int i=1;
    char *varname=strtok(args[1],"=");
    char *value=strtok(NULL,"");
    char *ptr;
    for (i = 2; args[i] != NULL; i++) {
        // Allocate memory for the concatenated string
        size_t new_value_len = strlen(value) + 1 + strlen(args[i]);  // space + next argument
        char *new_value = malloc(new_value_len + 1);  // +1 for null terminator
        if (new_value == NULL) {
            fprintf(stderr, "Error: Memory allocation failed.\n");
            return;
        }

        // Copy existing value and add space
        strcpy(new_value, value);
        strcat(new_value, " ");

        // Append the next argument
        strcat(new_value, args[i]);

        // Free the previous value (if not the first iteration)
        if (i > 2) {
            free(value);
        }

        // Update the pointer for the next iteration
        value = new_value;
    }
    if(value[0]=='"')
    {
        strsep(&value,"\"");
        value[strlen(value)-1]='\0';
    }
    status = setenv(varname,value,1);
    if(status)  //check if status!=0 if failed
    {
        perror("Error setting local variable");
    }
}
//excute builting commands (echo,cd,export,exit)
void execute_shell_bultin(char **args)
{
    if(!strcmp(args[0],"cd"))
    {
	    chdir(args[1]);
    }
    else if(!strcmp(args[0],"echo"))
    {
        for(int i=1;args[i]!=NULL;i++)
        {
           for(int j=0;args[i][j]!=NULL;j++)
            {
                if(args[i][j]=='\"')
                {
                    continue;
                }
                else
                {
                    printf("%c",args[i][j]);
                    //break;
                }
            }
            printf(" ");
        }
        printf("\n");
    }
    else if(!strcmp(args[0],"export"))
    {
	    export_cmd(args);
    }
    else
    {
        quit_hand();
    }
}
void setup_environment()
{
    char *buf;
    buf=(char *)malloc(100*sizeof(char));
    getcwd(buf,100);
    printf("%s& ",buf);
    free(buf);
}
int main() {
    int type;
    while (1) {
	    if (signal(SIGCHLD, sig_hand) == SIG_ERR) {
            	perror("signal");
            	exit(EXIT_FAILURE);
            }
        if (signal(SIGTERM, quit_hand) == SIG_ERR || signal(SIGINT,quit_hand) ==  SIG_ERR) {
             	perror("signal");
            	exit(EXIT_FAILURE);
	    }
        setup_environment();
        char *args[MAX_LINE_LEN / 2 + 1]; // Sufficient space for arguments
        parse_input(args);
        char *builtin_cmds[]={"echo","cd","export","exit"};
        for(int j=0;j<4;j++)
        {
            if(!strcmp(args[0],builtin_cmds[j]))
            {
                type=shell_builtin;
                break;
            }
            else{
                type = command;
            }
        }
        switch(type)
        {
            case shell_builtin:
                execute_shell_bultin(args);
                break;
            case command:
                execute_exp(args);
                break;
            default:
                perror("switch");
                break;
        }
        // Free the dynamically allocated memory
        free(str);
    }
    return 0;
}