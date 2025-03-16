#include "ff.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_PATH_LEN 256
#define MAX_COMMAND_LEN 32
#define MAX_ARGS 4

typedef struct {
    char command[MAX_COMMAND_LEN];
    char args[MAX_ARGS][MAX_PATH_LEN];
    int arg_count;
} command_t;

/* Function prototypes */
static void show_prompt(void);
static command_t parse_command(const char *input);
static void process_command(const command_t *cmd);
static void cmd_help(void);
static void cmd_ls(const char *path);
static void cmd_cd(const char *path);
static void cmd_mkdir(const char *path);
static void cmd_rm(const char *path);
static void cmd_cp(const char *src, const char *dst);
static void cmd_mv(const char *src, const char *dst);
static void cmd_cat(const char *path);
static void cmd_write(const char *path, const char *content);

/* Global variables */
static FATFS fs;
static char current_path[MAX_PATH_LEN] = "/";

int main(void)
{
    char input[MAX_PATH_LEN];
    FRESULT res;
    
    /* Mount the file system */
    res = f_mount(&fs, "", 1);
    if (res != FR_OK) {
        printf("Failed to mount file system. Error: %d\n", res);
        return 1;
    }
    
    printf("Simple File Manager\n");
    printf("Type 'help' for command list\n\n");
    
    while (1) {
        show_prompt();
        
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        
        /* Remove trailing newline */
        input[strcspn(input, "\n")] = 0;
        
        if (strlen(input) == 0) continue;
        
        /* Parse and process command */
        command_t cmd = parse_command(input);
        process_command(&cmd);
    }
    
    /* Unmount the file system */
    f_mount(NULL, "", 0);
    return 0;
}

static void show_prompt(void)
{
    printf("%s> ", current_path);
}

static command_t parse_command(const char *input)
{
    command_t cmd = {0};
    char *token;
    char input_copy[MAX_PATH_LEN];
    
    strncpy(input_copy, input, sizeof(input_copy) - 1);
    
    /* Get command */
    token = strtok(input_copy, " ");
    if (token) {
        strncpy(cmd.command, token, sizeof(cmd.command) - 1);
        
        /* Get arguments */
        while ((token = strtok(NULL, " ")) && cmd.arg_count < MAX_ARGS) {
            strncpy(cmd.args[cmd.arg_count], token, MAX_PATH_LEN - 1);
            cmd.arg_count++;
        }
    }
    
    return cmd;
}

static void process_command(const command_t *cmd)
{
    if (strcmp(cmd->command, "help") == 0) {
        cmd_help();
    }
    else if (strcmp(cmd->command, "ls") == 0) {
        cmd_ls(cmd->arg_count > 0 ? cmd->args[0] : ".");
    }
    else if (strcmp(cmd->command, "cd") == 0) {
        if (cmd->arg_count > 0) {
            cmd_cd(cmd->args[0]);
        } else {
            printf("Usage: cd <path>\n");
        }
    }
    else if (strcmp(cmd->command, "mkdir") == 0) {
        if (cmd->arg_count > 0) {
            cmd_mkdir(cmd->args[0]);
        } else {
            printf("Usage: mkdir <path>\n");
        }
    }
    else if (strcmp(cmd->command, "rm") == 0) {
        if (cmd->arg_count > 0) {
            cmd_rm(cmd->args[0]);
        } else {
            printf("Usage: rm <path>\n");
        }
    }
    else if (strcmp(cmd->command, "cp") == 0) {
        if (cmd->arg_count > 1) {
            cmd_cp(cmd->args[0], cmd->args[1]);
        } else {
            printf("Usage: cp <source> <destination>\n");
        }
    }
    else if (strcmp(cmd->command, "mv") == 0) {
        if (cmd->arg_count > 1) {
            cmd_mv(cmd->args[0], cmd->args[1]);
        } else {
            printf("Usage: mv <source> <destination>\n");
        }
    }
    else if (strcmp(cmd->command, "cat") == 0) {
        if (cmd->arg_count > 0) {
            cmd_cat(cmd->args[0]);
        } else {
            printf("Usage: cat <file>\n");
        }
    }
    else if (strcmp(cmd->command, "write") == 0) {
        if (cmd->arg_count > 1) {
            cmd_write(cmd->args[0], cmd->args[1]);
        } else {
            printf("Usage: write <file> <content>\n");
        }
    }
    else if (strcmp(cmd->command, "exit") == 0) {
        exit(0);
    }
    else {
        printf("Unknown command. Type 'help' for command list.\n");
    }
}

static void cmd_help(void)
{
    printf("Available commands:\n");
    printf("  help           Show this help message\n");
    printf("  ls [path]      List directory contents\n");
    printf("  cd <path>      Change current directory\n");
    printf("  mkdir <path>   Create a new directory\n");
    printf("  rm <path>      Remove file or directory\n");
    printf("  cp <src> <dst> Copy file\n");
    printf("  mv <src> <dst> Move/rename file\n");
    printf("  cat <file>     Display file contents\n");
    printf("  write <file> <content>  Write content to file\n");
    printf("  exit           Exit the program\n");
} 