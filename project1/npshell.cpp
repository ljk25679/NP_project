#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAXLINE 15001
#define MAXCMD 257

using namespace std;

struct npipe {
        int pipeCount;
        int pipe[2];
};

vector<npipe> num_pipe;
vector<npipe>::iterator it;
bool last_command;

void parse(string cmd_line);
void execute(string cmd);
void printenv(string env);
void setenv(string env);
void childHandler(int signo);

int main()
{
        string cmd_line;

        setenv("PATH", "bin:.", 1);
        num_pipe.clear();

        signal(SIGCHLD, childHandler);
	
	
        while(1)
        {
                last_command = 0;
                cout << "% ";
                getline(cin, cmd_line);
                if(cin.eof())
                        break;
                else if(cmd_line.find("setenv") != string::npos)
                        setenv(cmd_line);
                else if(cmd_line.find("printenv") != string::npos)
                        printenv(cmd_line);
                else if(cmd_line == "exit")
                {
                        for(it = num_pipe.begin(); it != num_pipe.end(); it++)
                                (*it).pipeCount--;
                        break;
                }
                else if(cmd_line == "")
                        continue;   
                else
                        parse(cmd_line);
        }
        return 0;
}

void parse(string cmd_line)
{
        int flag = 0;
        string num;
        string cmd;
        stringstream ss;
        ss << cmd_line;
        string split = "";
        while(ss >> split)
        {
                if((split[0] == '|') || (split[0] == '!'))
                {
                        flag = 1;
                        cmd = cmd + " " + split;
                        if(cmd[0] == ' ')
                                cmd.erase(0, 1);
                        execute(cmd);
                        cmd.clear();
                }
                else 
                {
                        flag = 0;
                        cmd = cmd + " " + split;
                }
        }
        if(flag == 0)
        {
                if (cmd[0] == ' ')
                        cmd.erase(0, 1);
                last_command = 1;
                execute(cmd);
        }
}

void execute(string cmd)
{
        char *argv[MAXCMD];
        int argc = 0;
        string token;
        int count = 0;
        int ret;
        int status;
        pid_t wpid;
        int stderror = 2;
        bool redirect = 0;
        int fd;
        string filename;

        // compose the argv for exec
        stringstream ss;
        ss << cmd;
        string split = "";
        while(ss >> split)
        {
                if(split[0] == '|')
                {
                        if(split.length() == 1)
                                count = 1;
                        else
                                count = stoi(split.substr(1));
                        stderror = 0;
                        bool flag = 0;
                        //------------- pipe -------------
                        for(it = num_pipe.begin(); it != num_pipe.end(); it++)
                        {
                                if((*it).pipeCount == count)
                                {
                                        flag = 1;
                                        break;
                                }
                        }
                        if(flag == 0)
                        {
                                struct npipe tmp_pipe;
                                if(pipe(tmp_pipe.pipe) < 0)
                                        perror("pipe error");
                                tmp_pipe.pipeCount = count;
                                num_pipe.push_back(tmp_pipe);
                        }
                        //------------- pipe -------------
                }
                else if(split[0] == '!')
                {
                        if(split.length() == 1)
                                count = 1;
                        else
                                count = stoi(split.substr(1));
                        stderror = 1;
                        bool flag = 0;
                        //------------- pipe -------------
                        for(it = num_pipe.begin(); it != num_pipe.end(); it++)
                        {
                                if((*it).pipeCount == count)
                                {
                                        flag = 1;
                                        break;
                                }
                        }
                        if(flag == 0)
                        {
                                struct npipe tmp_pipe;
                                if(pipe(tmp_pipe.pipe) < 0)
                                        perror("pipe error");
                                tmp_pipe.pipeCount = count;
                                num_pipe.push_back(tmp_pipe);
                        }
                        //------------- pipe -------------
                }
                else if(split[0] == '>')
                {
                        ss >> split;
                        filename = split;
                        redirect = 1;
                }
                else 
                        argv[argc++] = strdup(split.c_str());
        }
        argv[argc] = NULL;

        /*
        cout << count << ": ";
        for(int i = 0; i < argc; i++)
                cout << argv[i] << " ";
        cout << endl;
        */

        for(it = num_pipe.begin(); it != num_pipe.end(); it++)
        {
                if((*it).pipeCount == 0)
                {
                        close((*it).pipe[1]);
                        break;
                }
        }
        
        while((wpid = fork()) < 0)
                usleep(1000);
        if (wpid == 0)
        {       
                if(stderror == 0)
                {
                        //------------- pipe -------------
                        for(it = num_pipe.begin(); it != num_pipe.end(); it++)
                        {
                                if((*it).pipeCount == count)
                                {
                                        close((*it).pipe[0]);
                                        dup2((*it).pipe[1], 1);
                                        break;
                                }
                        }
                        //------------- pipe -------------
                }
                else if(stderror == 1)
                {
                        //------------- pipe -------------
                        for(it = num_pipe.begin(); it != num_pipe.end(); it++)
                        {
                                if((*it).pipeCount == count)
                                {
                                        close((*it).pipe[0]);
                                        dup2((*it).pipe[1], 1);
                                        dup2((*it).pipe[1], 2);
                                        break;
                                }
                        }
                        //------------- pipe -------------
                }

                for(it = num_pipe.begin(); it != num_pipe.end(); it++)
                {
                        if((*it).pipeCount == 0)
                        {
                                dup2((*it).pipe[0], 0);
                                break;
                        }
                }

                if(redirect)
                {
                        fd = open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
                        dup2(fd, 1);
                }

                ret = execvp(argv[0], argv);
                if (ret == -1)
                        fprintf(stderr, "%s%s%s", "Unknown command: [", argv[0], "].\n");
                exit(1);
        } 
        else 
        {
                if(last_command) 
		{
                        waitpid(wpid, &status, 0);
		}
                for(it = num_pipe.begin(); it != num_pipe.end(); it++)
                {
                        if((*it).pipeCount == 0)
                        {
                                close((*it).pipe[0]);
                                break;
                        }
                }
        }
        
        for(it = num_pipe.begin(); it != num_pipe.end(); it++)
        {
                if((*it).pipeCount < 0)
                {
                        num_pipe.erase(it);
                        break;
                }         
        }

        for(it = num_pipe.begin(); it != num_pipe.end(); it++)
                (*it).pipeCount--;
        
        for(int i = 0; i <= argc; i++)
                free(argv[i]);
}

void printenv(string env)
{
        size_t pos = 0;
        string token;
        pos = env.find(" ");
        token = env.substr(pos + 1, env.length());
        char *name = getenv(token.c_str());
        if (name == NULL)
                cout << "";
        else
                cout << name << endl;
        for(it = num_pipe.begin(); it != num_pipe.end(); it++)
                (*it).pipeCount--;
}

void setenv(string env)
{
        size_t pos = 0;
        string name;
        string value;
        env.erase(0, pos + 7);
        pos = env.find(" ");
        name = env.substr(0, pos);
        value = env.substr(pos + 1, env.length());
        setenv(name.c_str(), value.c_str(), 1);
        for(it = num_pipe.begin(); it != num_pipe.end(); it++)
                (*it).pipeCount--;
}

void childHandler(int signo)
{
        int status;
        while(waitpid(-1, &status, WNOHANG) > 0);
        // NON-BLOCKING WAIT
        // Return immediately if no child has exited.
}
