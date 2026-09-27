#include "shell.h"

int main()
{
    //Find the user home directory from the environment (3)
    char homeDir[MAX_PATH_LENGTH];
    get_home_directory(homeDir);

    //Save the current path (3)
    char savedPath[MAX_PATH_LENGTH];
    get_path_env(savedPath);

    //Set current working directory to user home directory (3)
    if (change_working_directory(homeDir) == -1)
    {
        printf("Error: Failed to change working directory.\n");
        return 1;
    }
    
    //Load history (6)
    char* history[HISTORY_SIZE];
    for (int i = 0; i < HISTORY_SIZE; i++)
    {
        history[i] = (char*)malloc(sizeof(char) * MAX_BUFFER_LENGTH);
    }
    int historyLen = load_history(homeDir, history);
    if (historyLen == -1)
        printf("Error: Failed to open the file \"%s\" at \"%s\" when trying to load history.\n", HIST_FILE_NAME, homeDir);
    if (historyLen == -2)
        printf("Error: Too many lines appeared in the file \"%s\" at \"%s\". Could not add to history. The limit is %d.\n", HIST_FILE_NAME, homeDir, HISTORY_SIZE);
    if (historyLen == -3)
        printf("Error: Failed to parse a line in the file \"%s\" at \"%s\". Expected at least 1 non-token character in every line, and for no lines to be history invocations.\n", HIST_FILE_NAME, homeDir);
    if (historyLen < 0)
    {
        printf("Creating new history list.\n");
        historyLen = 0;
    }
    
    //Load aliases (8)
    AliasPair aliasPairs[MAX_ALIASES];
    int aliasLen = read_alias_file(homeDir, aliasPairs);
    if (aliasLen == -1)
        printf("Error: Failed to open the file \"%s\" at \"%s\" when trying to load alias.\n", ALIASES_FILE_NAME, homeDir);
    if (aliasLen == -2)
        printf("Error: Failed to parse a line in the file \"%s\" at \"%s\". Expected each line to be in the format \"<alias-name> <command>\".\n", ALIASES_FILE_NAME, homeDir);
    if (aliasLen == -3)
        printf("Error: Failed to add to the list of aliases. There are too many aliases in the file. The limit is %d.\n", MAX_ALIASES);
    if (aliasLen < 0)
    {
        printf("Creating new alias list.\n");
        aliasLen = 0;
    }

    //Do while shell has not terminated
    while(1)
    {
        //Display prompt (1)
        display_prompt();

        //Read and parse user originInput (1)
        // reads input
        char originInput[MAX_BUFFER_LENGTH];
        if (retrieve_input(originInput) == -1)
            break;

        // creates a copy for manipulation elsewhere
        char recentInput[MAX_BUFFER_LENGTH];
        char backgroundInput[MAX_BUFFER_LENGTH];
        strcpy(backgroundInput, originInput);
        strcpy(recentInput, originInput);

        // parses input using copy
        char* args[MAX_ARGS_QUANTITY] = {0}; // initialises all values to 0/equivalent
        int argsLen = parse_input(backgroundInput, args);

        // linked list to store all iterated commands, used to look for cycles
        LinkedList list = NULL;

        // loop until an error occurs, the max iterations occur, or it naturally ends
        int error = 0;
        while (argsLen >= 1)
        {
            // the command has been repeated
            if (contains(args[0], list) == 0)
            {
                printf("Error: Could not run the command as a cycle had occured.\n");
                error = -1;
                break;
            }

            // adding it to the list so we can check if it is repeated later
            ListNode* node = (ListNode*)malloc(sizeof(ListNode));
            strcpy(node->command, args[0]);
            add_node(node, &list);

            // making a history invocation if it is one
            if (is_history_invocation(backgroundInput) == 0)
            {
                if (argsLen != 1)
                {
                    printf("Error: History invocation should have no arguments. Usage: \"!N\", \"!-N\", or \"!!\" where N is a positive whole number.\n");
                    error = -1;
                    break;
                }

                int errorCode = invoke_from_history(recentInput, backgroundInput, history, historyLen);
                if (errorCode == -2)
                {
                    printf("Error: Invalid format provided. Usage: \"!N\", \"!-N\", or \"!!\" where N is a positive whole number.\n");
                    error = -1;
                    break;
                }

                if (historyLen == 0)
                {
                    printf("Error: History is empty. Populate history by enterring non-history invocation commands.\n");
                    error = -1;
                    break;
                }

                if (errorCode == -1)
                {
                    printf("Error: Invalid number provided. Usage: \"!N\", \"!-N\", or \"!!\" where N is between 1 and %d.\n", historyLen);
                    error = -1;
                    break;
                }


            // replacing relevant alias if it is one
            }
            else if (argsLen > 0 && index_of_alias(args[0], aliasPairs, aliasLen) != -1)
            {
                int aliasIndex = index_of_alias(args[0], aliasPairs, aliasLen);
                if (argsLen > 1)
                {
                    // get the length of the first argument and add everything else from the input line
                    // consequently reconstructing the line to fit expectations
                    int len = 0;
                    while (args[0][len] != '\0')
                    {
                        len++;
                    }
                    strcpy(backgroundInput, aliasPairs[aliasIndex].command);
                    strcat(backgroundInput, &recentInput[len]);
                }
                else
                {
                    strcpy(backgroundInput, aliasPairs[aliasIndex].command);
                }
            
            // normal command/rubbish so can end this
            }
            else
            {
                break;
            }

            // parses command
            strcpy(recentInput, backgroundInput);
            argsLen = parse_input(backgroundInput, args);
        }

        // freeing list
        while (list != NULL)
        {
            ListNode* next = list->next;
            free(list);
            list = next;
        }

        // if it is an empty command, skip it
        if (argsLen <= 0)
            continue;

        // if the command is not a history invocation, add it to history
        if (is_history_invocation(originInput) != 0)
        {
            historyLen = add_to_history(originInput, history, historyLen);
        }

        // if any errors occur, end this command execution
        if (error != 0)
            continue;

        if (strcmp("exit", args[0]) == 0)
            break;

        if (strcmp("getpath", args[0]) == 0)
        {
            if (argsLen > 1)
            {
                printf("Error: \"getpath\" should have no arguments. Usage: \"getpath\".\n");
            }
            else
            {
                char path[MAX_PATH_LENGTH];
                get_path_env(path);
                printf("%s\n", path);
            }
        }
        else if (strcmp("setpath", args[0]) == 0)
        {
            if (argsLen < 2)
            {
                printf("Error: No argument provided for \"setpath\". Usage: \"setpath <path>\".\n");
            }
            else if (argsLen > 2)
            {
                printf("Error: Too many arguments provided for \"setpath\". Usage: \"setpath <path>\".\n");
            }
            else
            {
                char* newPath = args[1];
                change_path_env(newPath);
                printf("New path changed to \"%s\".\n", newPath);
            }
        }
        else if (strcmp("cd", args[0]) == 0)
        {
            if (argsLen == 1)
            {
                if (change_directory(homeDir) == 0)
                    printf("Successfully changed the directory to \"%s\".\n", homeDir);
            }
            else if(argsLen == 2)
            {
                if (change_directory(args[1]) == 0)
                    printf("Successfully changed the directory to \"%s\".\n", args[1]);
            }
            else
            {
                printf("Error: Too many arguments provided for \"cd\". Usage: \"cd\" or \"cd <path>\".\n");
            }
        }
        else if (strcmp("history", args[0]) == 0)
        {
            if (argsLen == 1)
            {
                print_history(history, historyLen);
            }
            else
            {
                printf("Error: \"history\" should have no arguments. Usage: \"history\".\n");
            }
        }
        else if (strcmp("alias", args[0]) == 0)
        {
            if (argsLen == 1)
            {
                print_alias(aliasPairs, aliasLen);
            }
            else if (argsLen == 2)
            {
                printf("Error: \"alias\" cannot accept only one argument. Usage: \"alias\" or \"alias <alias-name> <command>\".\n");
            }
            else
            {
                // creates a copy for manipulation elsewhere
                char inputCopy[MAX_BUFFER_LENGTH];
                strcpy(inputCopy, recentInput);

                // parses originInput using copy
                char* aliasArgs[2];
                if (parse_alias_line(&inputCopy[6], aliasArgs) != 0)
                {
                    printf("Error: Invalid command format provided. Usage: \"alias\" or \"alias <alias-name> <command>\".\n");
                    continue;
                }

                int tempLen = add_alias(aliasArgs[0], aliasArgs[1], aliasPairs, aliasLen);
                if (tempLen == -1)
                {
                    printf("Error: Could not add alias \"%s\" to the list since there are too many aliases currently existing. Please remove one via the \"unalias\" command if you would like to add it.\n", aliasArgs[0]);
                    continue;
                }
                
                aliasLen = tempLen;
                printf("Successfully added alias \"%s\" with command \"%s\".\n", aliasArgs[0], aliasArgs[1]);
            }
        }
        else if (strcmp("unalias", args[0]) == 0)
        {
            if (argsLen != 2)
            {
                printf("Error: \"unalias\" can only accept exactly one argument. Usage: \"unalias <alias>\".\n");
            }
            else
            {
                if (aliasLen == 0)
                {
                    printf("Error: No aliases currently exist in the list. Please add an alias via the \"alias\" command.\n");
                    continue;
                }

                int tempLen = remove_alias(&recentInput[8], aliasPairs, aliasLen);
                if (tempLen == -1)
                {
                    printf("Error: Could not find provided alias \"%s\". Please check you have used the correct alias via \"alias\".\n", &recentInput[8]);
                    continue;
                }

                aliasLen = tempLen;
                printf("Successfully removed alias \"%s\".\n", &recentInput[8]);
            }
        }
        else if (argsLen >= 2 && strcmp("clear", args[0]) == 0 && strcmp("history", args[1]) == 0)
        {
            if (argsLen > 2)
            {
                printf("Error: \"clear history\" does not accept arguments. Usage: \"clear history\".\n");
            }
            else
            {
                historyLen = 0;
                printf("Successfully cleared history.\n");
            }
        }
        else if (execute_external_command(args) == -1)
        {
            printf("Error: Failed to fork!\n");
        }
        
    }

    // when exiting, adds a new line so the next few outputs aren't weird
    printf("\n");
    
    //Save history (6)
    if (save_history(homeDir, history, historyLen) == -1)
        printf("Error: Could not write to \"%s\" at \"%s\" when trying to save history.\n", HIST_FILE_NAME, homeDir);

    // freeing history
    for (int i = 0; i < HISTORY_SIZE; i++)
    {
        free(history[i]);
    }
    
    //Save aliases (8)
    if (set_alias_file(homeDir, aliasPairs, aliasLen) == -1)
        printf("Error: Failed to add aliases to the file \"%s\" at \"%s\".\n", ALIASES_FILE_NAME, homeDir);
    
    //Restore original path (3)
    change_path_env(savedPath);
    char restoredPath[MAX_PATH_LENGTH];
    get_path_env(restoredPath);
    printf("Path restored: %s\n", restoredPath);
    
    return 0;
}
