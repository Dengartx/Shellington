#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h> //termios, TCSANOW, ECHO, ICANON
#include <string.h>
#include <stdbool.h>
#include <errno.h>

//For use in short function
#define BUF_SIZE 250

char w[BUF_SIZE];

const char *sysname = "shellington";

enum return_codes
{
	SUCCESS = 0,
	EXIT = 1,
	UNKNOWN = 2,
};
struct command_t
{
	char *name;
	bool background;
	bool auto_complete;
	int arg_count;
	char **args;
	char *redirects[3];		// in/out redirection
	struct command_t *next; // for piping
};
/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t *command)
{
	int i = 0;
	printf("Command: <%s>\n", command->name);
	printf("\tIs Background: %s\n", command->background ? "yes" : "no");
	printf("\tNeeds Auto-complete: %s\n", command->auto_complete ? "yes" : "no");
	printf("\tRedirects:\n");
	for (i = 0; i < 3; i++)
		printf("\t\t%d: %s\n", i, command->redirects[i] ? command->redirects[i] : "N/A");
	printf("\tArguments (%d):\n", command->arg_count);
	for (i = 0; i < command->arg_count; ++i)
		printf("\t\tArg %d: %s\n", i, command->args[i]);
	if (command->next)
	{
		printf("\tPiped to:\n");
		print_command(command->next);
	}
}
/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command)
{
	if (command->arg_count)
	{
		for (int i = 0; i < command->arg_count; ++i)
			free(command->args[i]);
		free(command->args);
	}
	for (int i = 0; i < 3; ++i)
		if (command->redirects[i])
			free(command->redirects[i]);
	if (command->next)
	{
		free_command(command->next);
		command->next = NULL;
	}
	free(command->name);
	free(command);
	return 0;
}
/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt()
{
	char cwd[1024], hostname[1024];
	gethostname(hostname, sizeof(hostname));
	getcwd(cwd, sizeof(cwd));
	printf("%s@%s:%s %s$ ", getenv("USER"), hostname, cwd, sysname);
	return 0;
}
/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command)
{
	const char *splitters = " \t"; // split at whitespace
	int index, len;
	len = strlen(buf);
	while (len > 0 && strchr(splitters, buf[0]) != NULL) // trim left whitespace
	{
		buf++;
		len--;
	}
	while (len > 0 && strchr(splitters, buf[len - 1]) != NULL)
		buf[--len] = 0; // trim right whitespace

	if (len > 0 && buf[len - 1] == '?') // auto-complete
		command->auto_complete = true;
	if (len > 0 && buf[len - 1] == '&') // background
		command->background = true;

	char *pch = strtok(buf, splitters);
	command->name = (char *)malloc(strlen(pch) + 1);
	if (pch == NULL)
		command->name[0] = 0;
	else
		strcpy(command->name, pch);

	command->args = (char **)malloc(sizeof(char *));

	int redirect_index;
	int arg_index = 0;
	char temp_buf[1024], *arg;
	while (1)
	{
		// tokenize input on splitters
		pch = strtok(NULL, splitters);
		if (!pch)
			break;
		arg = temp_buf;
		strcpy(arg, pch);
		len = strlen(arg);

		if (len == 0)
			continue;										 // empty arg, go for next
		while (len > 0 && strchr(splitters, arg[0]) != NULL) // trim left whitespace
		{
			arg++;
			len--;
		}
		while (len > 0 && strchr(splitters, arg[len - 1]) != NULL)
			arg[--len] = 0; // trim right whitespace
		if (len == 0)
			continue; // empty arg, go for next

		// piping to another command
		if (strcmp(arg, "|") == 0)
		{
			struct command_t *c = malloc(sizeof(struct command_t));
			int l = strlen(pch);
			pch[l] = splitters[0]; // restore strtok termination
			index = 1;
			while (pch[index] == ' ' || pch[index] == '\t')
				index++; // skip whitespaces

			parse_command(pch + index, c);
			pch[l] = 0; // put back strtok termination
			command->next = c;
			continue;
		}

		// background process
		if (strcmp(arg, "&") == 0)
			continue; // handled before

		// handle input redirection
		redirect_index = -1;
		if (arg[0] == '<')
			redirect_index = 0;
		if (arg[0] == '>')
		{
			if (len > 1 && arg[1] == '>')
			{
				redirect_index = 2;
				arg++;
				len--;
			}
			else
				redirect_index = 1;
		}
		if (redirect_index != -1)
		{
			command->redirects[redirect_index] = malloc(len);
			strcpy(command->redirects[redirect_index], arg + 1);
			continue;
		}

		// normal arguments
		if (len > 2 && ((arg[0] == '"' && arg[len - 1] == '"') || (arg[0] == '\'' && arg[len - 1] == '\''))) // quote wrapped arg
		{
			arg[--len] = 0;
			arg++;
		}
		command->args = (char **)realloc(command->args, sizeof(char *) * (arg_index + 1));
		command->args[arg_index] = (char *)malloc(len + 1);
		strcpy(command->args[arg_index++], arg);
	}
	command->arg_count = arg_index;
	return 0;
}
void prompt_backspace()
{
	putchar(8);	  // go back 1
	putchar(' '); // write empty over
	putchar(8);	  // go back 1 again
}
/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command)
{
	int index = 0;
	char c;
	char buf[4096];
	static char oldbuf[4096];

	// tcgetattr gets the parameters of the current terminal
	// STDIN_FILENO will tell tcgetattr that it should write the settings
	// of stdin to oldt
	static struct termios backup_termios, new_termios;
	tcgetattr(STDIN_FILENO, &backup_termios);
	new_termios = backup_termios;
	// ICANON normally takes care that one line at a time will be processed
	// that means it will return if it sees a "\n" or an EOF or an EOL
	new_termios.c_lflag &= ~(ICANON | ECHO); // Also disable automatic echo. We manually echo each char.
	// Those new settings will be set to STDIN
	// TCSANOW tells tcsetattr to change attributes immediately.
	tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

	//FIXME: backspace is applied before printing chars
	show_prompt();
	int multicode_state = 0;
	buf[0] = 0;
	while (1)
	{
		c = getchar();
		// printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

		if (c == 9) // handle tab
		{
			buf[index++] = '?'; // autocomplete
			break;
		}

		if (c == 127) // handle backspace
		{
			if (index > 0)
			{
				prompt_backspace();
				index--;
			}
			continue;
		}
		if (c == 27 && multicode_state == 0) // handle multi-code keys
		{
			multicode_state = 1;
			continue;
		}
		if (c == 91 && multicode_state == 1)
		{
			multicode_state = 2;
			continue;
		}
		if (c == 65 && multicode_state == 2) // up arrow
		{
			int i;
			while (index > 0)
			{
				prompt_backspace();
				index--;
			}
			for (i = 0; oldbuf[i]; ++i)
			{
				putchar(oldbuf[i]);
				buf[i] = oldbuf[i];
			}
			index = i;
			continue;
		}
		else
			multicode_state = 0;

		putchar(c); // echo the character
		buf[index++] = c;
		if (index >= sizeof(buf) - 1)
			break;
		if (c == '\n') // enter key
			break;
		if (c == 4) // Ctrl+D
			return EXIT;
	}
	if (index > 0 && buf[index - 1] == '\n') // trim newline from the end
		index--;
	buf[index++] = 0; // null terminate string

	strcpy(oldbuf, buf);

	parse_command(buf, command);

	// print_command(command); // DEBUG: uncomment for debugging

	// restore the old settings
	tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
	return SUCCESS;
}
int process_command(struct command_t *command);

// Added predefinitions for file_exists and search_path
int file_exists(const char *file_name);
char *search_path(const char *file_name);

const char *search_short(FILE *fp, const char *alias); // will return the alias token's corresponding file output
void jump_to(const char *loc, struct command_t *command);
int shortcut(struct command_t *command);

void bookmark(struct command_t *command);
void save(FILE *fp, char *argv, int save1);
void library(FILE *fp);

int ping_sweep(const char *subnet_command, const char *start_command, const char *end_command);

int main()
{
	// Get the first working directory to W
	getcwd(w, sizeof(w));

	while (1)
	{
		struct command_t *command = malloc(sizeof(struct command_t));
		memset(command, 0, sizeof(struct command_t)); // set all bytes to 0

		int code;
		code = prompt(command);
		if (code == EXIT)
			break;

		code = process_command(command);
		if (code == EXIT)
			break;

		free_command(command);
	}

	printf("\n");
	return 0;
}

int process_command(struct command_t *command)
{
	int r;
	bool builtinComm = false;

	if (strcmp(command->name, "") == 0)
		return SUCCESS;

	if (strcmp(command->name, "short") == 0)
	{
		// If you jump to a location and after that immediately trigger an blank success statement,
		// the cd reverts back to the location before jumping
		builtinComm = true;
		if (fork() == 0)
		{
			shortcut(command);
		}
		else
		{
			if (!command->background)
				wait(0); // wait for child process to finish
			return SUCCESS;
		}
	}
	if (strcmp(command->name, "bookmark") == 0)
	{
		builtinComm = true;
		char *bookmark_comm_set;

		if (strcmp(command->args[0], "-i") != 0 && strcmp(command->args[0], "-l") != 0 && strcmp(command->args[0], "-d") != 0)
		{
			// if a conventional command is not set then the command set is merged to a single command
			bookmark_comm_set = malloc(sizeof(command->args) + 20);

			for (int i = 0; command->args[i] != NULL; i++)
			{
				if (i == 0)
				{
					strcat(bookmark_comm_set, "\"");
				}
				strcat(bookmark_comm_set, command->args[i]);
				if (command->args[i + 1] == NULL)
				{
					strcat(bookmark_comm_set, "\"");
				}
				else
				{
					strcat(bookmark_comm_set, " ");
				}
				command->args[i] = NULL;
			}
			free(command->args[0]);
			command->args[0] = bookmark_comm_set;
		}
		bookmark(command);
		return SUCCESS;
	}
	if (strcmp(command->name, "pingsweep") == 0)
	{
		builtinComm = true;

		if (command->arg_count != 3)
		{
			printf("Usage: pingsweep $subnet $start $end\nIn range (0,254)\n");
			return SUCCESS;
		}

		const char *subnet_command = command->args[0];
		const char *start_command = command->args[1];
		const char *end_command = command->args[2];

		if (fork() == 0)
		{
			ping_sweep(subnet_command, start_command, end_command);
			exit(0);
		}
		else
		{
			if (!command->background)
				wait(0); // wait for child process to finish
			return SUCCESS;
		}
	}

	if (strcmp(command->name, "exit") == 0)
		return EXIT;

	if (strcmp(command->name, "cd") == 0)
	{
		if (command->arg_count > 0)
		{
			r = chdir(command->args[0]);
			if (r == -1)
				printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
			return SUCCESS;
		}
	}

	pid_t pid = fork();
	if (pid == 0) // child
	{
		/// This shows how to do exec with environ (but is not available on MacOs)
		// extern char** environ; // environment variables
		// execvpe(command->name, command->args, environ); // exec+args+path+environ

		/// This shows how to do exec with auto-path resolve
		// add a NULL argument to the end of args, and the name to the beginning
		// as required by exec

		// increase args size by 2
		command->args = (char **)realloc(
			command->args, sizeof(char *) * (command->arg_count += 2));

		// shift everything forward by 1
		for (int i = command->arg_count - 2; i > 0; --i)
			command->args[i] = command->args[i - 1];

		// set args[0] as a copy of name
		command->args[0] = strdup(command->name);
		// set args[arg_count-1] (last) to NULL
		command->args[command->arg_count - 1] = NULL;

		/*
		execvp(command->name, command->args); // exec+args+path
		exit(0);
		No Need for these lines after path resolving implementation
		*/
		/// TODO: do your own exec with path resolving using execv()

		/// response to TODO: finds the absolute path of the file for the first input then gets the args for execv().
		char *file_path = search_path(command->args[0]);
		if (file_path == NULL && !builtinComm)
		{
			printf("-%s: %s: command not found\n", sysname, command->name);
		}
		execv(file_path, command->args);
		exit(0);
	}
	else
	{
		if (!command->background)
			wait(0); // wait for child process to finish
		return SUCCESS;
	}

	// TODO: your implementation here

	printf("-%s: %s: command not found\n", sysname, command->name);
	return UNKNOWN;
}

// Added code for using execv
int file_exists(const char *path_name)
{
	// opens the path directory to read file path which if exists:
	// 1. the pointer will not be null
	// 2. under the condition of one will return true since path to file exists and is valid
	FILE *fp;

	if ((fp = fopen(path_name, "r")))
	{
		fclose(fp);
		return 1;
	}

	return 0;
}
char *search_path(const char *file_name)
{
	// is intended to traverse the existing shell path environment var
	// $PATH to get the individual file path tokens to scan them all
	char *env = getenv("PATH");
	// for the $PATH var the outputs delimiter is ":", for unix based devices
	const char delim[2] = ":";
	char *paths_env = strtok(env, delim);

	//initializing the path array to check if the concatenation of the desired file_name to it exists
	const int max_len = 30;

	while (paths_env != NULL)
	{
		// Intention is to search for the path in a manner where $PATH[i]/path
		// is an existing file meaning that it's the file at question to be executed
		char *path = malloc(sizeof(paths_env) + max_len);
		if (path == NULL)
		{
			//in case of a failed malloc
			return NULL;
		}
		path[max_len] = NULL;
		strcpy(path, paths_env);
		strcat(path, "/");
		strcat(path, file_name);

		if (file_exists(path))
		{
			return path;
		}
		paths_env = strtok(NULL, delim);
		free(path);
	}
	return NULL;
}
/// TODO: create new c files for each new custom command and in the end make a makefile to compile them together.

// Added code for short function

int shortcut(struct command_t *command)
{
	const char *set_jump = command->args[0];
	const char *alias = command->args[1];
	const char pwd[BUF_SIZE];
	getcwd(pwd, sizeof(pwd));

	const char *delim = ":";

	char *filedir = malloc(strlen("shorttxt") + strlen(w));

	strcat(filedir, w);
	strcat(filedir, "/shorttxt");

	FILE *fp = fopen(filedir, "a");
	if (fp != NULL)
	{
		// if file exists then append it to the file's last line
		if (strcmp(set_jump, "set") == 0)
		{
			// set command appends to the file the pwd

			char *token = malloc(sizeof(alias) + sizeof(pwd) + 2);
			strcpy(token, alias);
			strcat(token, delim);
			strcat(token, pwd);

			// write token to file and add a newline
			fputs(token, fp);
			fputs("\n", fp);
			printf("%s set\n", token);
		}
		else if (strcmp(set_jump, "jump") == 0)
		{
			// jump function
			FILE *fp2 = fopen(filedir, "r");
			if (fp2 == NULL)
			{
				perror("file couldn't be opened");
				return 1;
			}
			/// TODO: read the file to find the desired alias token and then its corresponding value is the directory execution shell will point to
			const char *jumpdir = search_short(fp2, alias);
			fclose(fp2);
			if (strcmp(jumpdir, "DNE") != 0)
			{
				jump_to(jumpdir, command);
			}
			else
			{
				printf("Invalid alias\n");
			}
		}
		else
		{
			printf("Invalid Command\n");
		}
		free(filedir);
		fclose(fp);
		return 0;
	}
	else
	{
		perror("Could not open or create file");
		return 1;
	}
	free(filedir);
	fclose(fp);
	return 0;
}

const char *search_short(FILE *fp, const char *alias)
{
	char *line = malloc(BUF_SIZE);

	while (fgets(line, BUF_SIZE, fp) != NULL)
	{
		// see if the corresponding token is the alias we're looking for
		// constrained to alias:dir\n
		const char *aliasToken = strtok(line, ":");
		const char *dirToken = strtok(NULL, "\n");
		if (strcmp(alias, aliasToken) == 0)
		{
			return dirToken;
		}
	}

	return "DNE";
}

void jump_to(const char *loc, struct command_t *command)
{
	/// TODO: jump to loc using system call cd

	char *arg_list[] = {loc, NULL};
	struct command_t *cdcomm = malloc(sizeof(struct command_t));

	cdcomm->args = malloc(sizeof(arg_list));
	cdcomm->name = malloc(sizeof("cd"));

	strcpy(cdcomm->name, "cd");

	cdcomm->args = arg_list;
	cdcomm->arg_count = 2;
	cdcomm->background = command->background;
	cdcomm->next = NULL;

	process_command(cdcomm);
}
// Added code for bookmark func
void bookmark(struct command_t *command)
{

	FILE *fp;
	char single_line[BUF_SIZE];

	char *task = command->args[0];
	char *target = command->args[1];

	char filedir[strlen("bookmarktxt") + strlen(w)];
	strcpy(filedir, w);
	strcat(filedir, "/bookmarktxt");
	if (strcmp(task, "bookmark") == 0)
	{
		printf("Illegal use of bookmark in bookmark may lead to abundant memory");
		return;
	}

	if (strcmp(task, "-l") == 0)
	{

		fp = fopen(filedir, "r");
		library(fp);
		fclose(fp);
	}

	else if (strcmp(task, "-d") == 0)
	{

		char *filedir2 = malloc(strlen("bookmarktxt") + strlen(w));

		strcat(filedir2, w);
		strcat(filedir2, "/bookmarktxt");

		FILE *fp2 = fopen(filedir2, "w");

		fp = fopen(filedir, "r");

		delete (fp, fp2, target, filedir, filedir2);

		fclose(fp);
		fp = fp2;
		remove(filedir);
		rename(filedir2, filedir);
		fclose(fp2);
		free(filedir2);
	}

	else if (strcmp(task, "-i") == 0)
	{

		if (target != NULL)
		{
			fp = fopen(filedir, "r");
			if (fp == NULL)
			{
				printf("No bookmarks exist\n");
				return;
			}

			char sinle_line[1000];
			while (fgets(single_line, 1000, fp) != NULL)
			{

				if (strncmp(single_line, target, 1) == 0)
				{
					char *tok = strtok(single_line, " ");
					tok = strtok(NULL, single_line);
					tok = tok + 1;

					struct command_t *comm_exec = malloc(sizeof(struct command_t));

					char *tokenized_name = strtok(tok, " ");
					comm_exec->name = malloc(sizeof(tokenized_name));
					comm_exec->name = tokenized_name;

					char *tokenized_comms[BUF_SIZE];

					int i = 0;
					tok = strtok(NULL, " ");
					char tok_end = tok[strlen(tok) - 2];

					for (i; tok_end != '\0' && tok_end != '"'; i++)
					{

						tokenized_comms[i] = tok;

						tok_end = tok[strlen(tok) - 1];
						if (tok_end == '"')
						{
							tokenized_comms[i] = strtok(tok, "\"");
							tok_end = tok[strlen(tok) - 2];
							break;
						}
						else
						{
							tokenized_comms[i] = strtok(tok, " ");
						}
					}
					if (tok_end == '"')
					{
						tokenized_comms[i] = strtok(tok, "\"");
					}

					comm_exec->arg_count = i + 1;

					int j;
					for (j = 0; j < comm_exec->arg_count; j++)
					{
						comm_exec->args = malloc(sizeof(comm_exec->args) + sizeof(tokenized_comms[j]));
						comm_exec->args[j] = tokenized_comms[j];
					}

					comm_exec->background = command->background;
					comm_exec->next = NULL;

					process_command(comm_exec);
				}
			}
		}
	}
	else
	{
		int save1 = 1;
		fp = fopen(filedir, "a+");
		save(fp, task, save1);
		fclose(fp);
	}
}

void save(FILE *fp, char *argv, int save1)
{
	char single_line[1000];
	int d = -1;
	while (!feof(fp))
	{

		fgets(single_line, 100, fp);
		d++;
		if (strncmp(&single_line[2], argv, strlen(argv)) == 0)
		{
			save1 = 0;
			break;
		}
	}
	if (save1 == 1)
		fprintf(fp, "%d %s\n", d, argv);
	save1 = 1;
}

void delete (FILE *fp, FILE *fp2, char *target, char *filedir, char *filedir2)
{
	char single_line[1000];

	while (fgets(single_line, 1000, fp) != NULL)
	{

		if (strncmp(single_line, target, 1) != 0)
		{
			fprintf(fp2, "%s", single_line);
		}
	}
}

void library(FILE *fp)
{

	char single_line[1000];

	while (fgets(single_line, 1000, fp) != NULL)
	{

		printf("%s", single_line);
	}
}

/// New Custom command for a local ping sweep to check which local devices are up:

int ping_sweep(const char *subnet, const char *range_start, const char *range_end) 
{	
	/// AUTHOR: Furkan Özgültekin 

	int stat;

	// the standard ping script used in linux devices is used to scan all desired ports in the interval
	char *file_path = search_path("ping");
	if (file_path == NULL)
	{
		printf("-%s: %s: command not found\n", sysname, "ping");
		return -1;
	}

	// use grep path if it exists

	// usage: pingsweep subnet range_start range_end
	// where range is between (0,254) by default
	int i = atoi(range_start);
	int end = atoi(range_end);

	if (i > 254 || end > 254 || i < 0 || end < 0)
	{
		// the subnet consists of 255 individual ip's eg: 192.168.0.0 to 192.168.0.254
		printf("Invalid range operators");
		return 0;
	}

	pid_t pid_main = fork();
	if (pid_main == 0)
	{
		// create child processes and then execute ping for the interval between start and end using for
		for (i; i <= end; i++)
		{

			int count = 1;
			if (i >= 10)
			{
				count = 2;
			}
			else if (i >= 100)
			{
				count = 3;
			}
			char cur_index[count];
			sprintf(cur_index, "%d", i);

			char *ping_net = malloc(sizeof(subnet) + sizeof(cur_index) + 2);

			// the concats are used to get the desired argument pass of $(SUBNET).$(CUR_INDEX)
			strcat(ping_net, subnet);
			strcat(ping_net, ".");
			strcat(ping_net, cur_index);

			// the -c 1 argument is to transmit only one ping for all subnets we want to peek
			char *const args[] = {"ping", "-c 1", ping_net, NULL};

			// each ping becomes a child process of their own
			pid_t pid = fork();
			if (pid == 0)
			{
				execv(file_path, args);
			}
			else
			{
				wait(NULL);
			}
			free(ping_net);
		}
	}
	else
	{
		printf("Scanning on subnet %s from %s to %s\n", subnet, range_start, range_end);
		wait(&stat);
		return SUCCESS;
	}
}