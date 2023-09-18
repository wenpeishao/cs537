#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
char *to_uppercase(const char *input)
{
    char *result = malloc(strlen(input) + 1);
    if (!result)
    {
        perror("Memory allocation failed");
        exit(1);
    }

    char *ptr = result;
    while (*input)
    {
        *ptr++ = toupper(*input++);
    }
    *ptr = '\0';

    return result;
}

char *format_line(char *line)
{
    static char fl[1024];      // Make it static so it persists after function returns
    memset(fl, 0, sizeof(fl)); // Clear the array

    sprintf(fl, "       ");         // Indent 7 spaces
    char *fl_ptr = fl + strlen(fl); // Move pointer to the end of indentation

    while (*line != '\0')
    {
        if (strncmp(fl_ptr, "/fB", 3) == 0)
        {
            strcat(fl, "\033[1m");
            fl_ptr += 3;
        }
        else if (strncmp(fl_ptr, "/fI", 3) == 0)
        {
            strcat(fl, "\033[3m");
            fl_ptr += 3;
        }
        else if (strncmp(fl_ptr, "/fU", 3) == 0)
        {
            strcat(fl, "\033[4m");
            fl_ptr += 3;
        }
        else if (strncmp(fl_ptr, "/fP", 3) == 0)
        {
            strcat(fl, "\033[0m");
            fl_ptr += 3;
        }
        else if (strncmp(fl_ptr, "//", 2) == 0)
        {
            strcat(fl, "/");
            fl_ptr += 2;
        }
        else
        {
            strncat(fl, fl_ptr, 1);
            fl_ptr++;
        }
    }

    // strcat(fl_ptr, "\n"); // Append newline at the end

    return fl;
}

int wgroff(const char *input_file)
{
    FILE *ifp = fopen(input_file, "r");
    if (ifp == NULL)
    {
        printf("File doesn't exist\n");
        exit(0);
    }
    char line[512];
    fgets(line, 100, ifp);
    // check first line get the header
    char *header[4];
    int lineno = 1;
    // header 0 is ".TH"| header 1 is command| header 2 is section| header 3 is date TODO check Bad Header
    header[0] = strtok(line, " ");
    if (strstr(header[0], ".TH") == NULL)
    {
        printf("Improper formatting on line %i\n", lineno);
        exit(0);
    }
    for (int i = 1; i < 4; i++)
    {
        header[i] = strtok(NULL, " ");
    }
    char path[518];
    sprintf(path, "%s.%s", header[1], header[2]);
    int section = atoi(header[2]);
    if (section < 1 || section > 9)
    {
        printf("Improper formatting on line %i\n", lineno);
        exit(0);
    }

    char time[50];
    strcpy(time, header[3]);
    time[strlen(time) - 1] = 0;
    if (strlen(time) != 10) // check time
    {
        printf("Improper formatting on line %i\n", lineno);
        exit(0);
    }
    FILE *nfp = fopen(path, "w+");
    // write the first line
    char firstLine[500];
    char title[180];
    sprintf(title, "%s(%s)", header[1], header[2]);
    strcat(firstLine, title);
    while (strlen(firstLine) <= (79 - strlen(title)))
    {
        strcat(firstLine, " ");
    }
    strcat(firstLine, title);
    // strcat(firstLine, "\n");
    fputs(firstLine, nfp);
    fputs("\n", nfp);
    while (fgets(line, sizeof(line), ifp) != NULL)
    {
        lineno++;
        if (strstr(line, "#"))
        {
            continue;
        }
        else if (strstr(line, ".SH"))
        {
            char *sh;
            sh = strchr(line, ' ') + 1;
            sh = to_uppercase(sh);
            char sub_head[512];
            sprintf(sub_head, "\n\033[1m%s\033[0m", sh);
            fputs(sub_head, nfp);
        }
        else
        {
            char *formated_line = format_line(line);

            fputs(formated_line, nfp);
        }
    }

    char last_len[100];
    memset(last_len, 0, sizeof(last_len));
    while (strlen(last_len) < ((80 - strlen(time)) / 2))
    {
        strcat(last_len, " ");
    }
    strcat(last_len, time);
    while (strlen(last_len) < 80)
    {
        strcat(last_len, " ");
    }
    fputs(last_len, nfp);
    fputs("\n", nfp);

    fclose(nfp);
    fclose(ifp);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Improper number of arguments\nUsage: ./wgroff <file>\n");
        return 0;
    }
    char path[100];
    sprintf(path, "%s", argv[1]);
    wgroff(path);
    return 0;
}