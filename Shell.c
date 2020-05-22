#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include <assert.h>
#include <pwd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <netdb.h>
#include <sys/sysctl.h>
#include <sys/syscall.h>
#include <signal.h>

/*
 * In this struct, i save all necessary data about every command. I save command name, command parameters, input and output file names and if it contains command ender. 
 */
struct COMMAND{
    int intp, outp;
    int deskr;
    char *intpC, *outpC;
    char command[256];
    char **parameters;
    int parameterCount;
    int comment;
    int endComm;
};

/*
 *  Here i save socket path, so i can close and remove it more easily.
 */

char *pathSock;
int socketG, CL;
int PTime, PUname, PSname, timeOut;

/*
 * This function is used to print out help command.
 */

void help(int server, int socket){
    if(server == 1){
       char buffer[1061] = "Toto je jednoduchy interaktivny shell\nDokaze spracovat mnozstvo jednoduchych prikazov\nShell ma implementovane pipeline, multi pipeline, presmerovanie vstupu, vystupu, lokalny socket, server/client vztah\nShell rozoznava niekolko vstupnych prepinacov:\n   -h -> vypise help\n   -u [Sock]-> bez prepinaca -c vytvori socket a stane sa serverom, s prepinacom -c sa stane klientom a pripoji sa na server\n   -l [File]-> tento prepinac zapne logovanie prikazov do suboru File\n   -C [File]-> tento prepinac sluzi na nacitanie konfiguracneho suboru File\nVstavane prikazy pre vsetkych uzivatelov su: \n   help na vypisanie pomocky, \n   halt na ukoncenie shellu, \n   listen N na otvorenie noveho socketu\n   prompt na upravu promptu\nAk je shell serverom, tak ma pristup k prikazom: \n   close na uzavretie socketu,\n   stat na vypisanie pripojeni,\n   abort N na odpojenie zvoleneho klienta\nAk je shell klientom servera, tak ma pristup k prikazu quit na odpojenie\n";
       write(socket,buffer,1062);

    }else{
    printf("Toto je jednoduchy interaktivny shell\n"
           "Dokaze spracovat mnozstvo jednoduchych prikazov\n"
           "Shell ma implementovane pipeline, multi pipeline, presmerovanie vstupu, vystupu, lokalny socket, server/client vztah\n"
           "Shell rozoznava niekolko vstupnych prepinacov:\n   -h -> vypise help\n   -u [Sock]-> bez prepinaca -c vytvori socket a stane sa serverom, s prepinacom -c sa stane klientom a pripoji sa na server\n"
           "   -l [File]-> tento prepinac zapne logovanie prikazov do suboru File\n   -C [File]-> tento prepinac sluzi na nacitanie konfiguracneho suboru File\n"
	       "Vstavane prikazy pre vsetkych uzivatelov su: \n   help na vypisanie pomocky, \n   halt na ukoncenie shellu, \n   listen N na otvorenie noveho socketu\n   prompt na upravu promptu\n"
           "Ak je shell serverom, tak ma pristup k prikazom: \n   close na uzavretie socketu,\n   stat na vypisanie pripojeni,\n   abort N na odpojenie zvoleneho klienta\n"
           "Ak je shell klientom servera, tak ma pristup k prikazu quit na odpojenie\n"); 
   } 
}

/*
 * In this function, i handle trapped signals. If SIGQUIT (CTRL+\) signal is trapped, the help function is executed. 
 * If SIGTSTP (CTRL+Z) is trapped, client will be disconnected from server 
 */

void signal_handle(int sigT){

   if(sigT==SIGQUIT){
        help(0,0);
   }if(sigT==SIGTSTP){
        if(CL == 1){
        	CL=0;
	    	char buffer[5]="quit\0";
	    	write(socketG, buffer, 6);
		}
   	}

}

/*
 * This function is used to change directory. If no argument is given, error will pop up.
 */

void cd(char **parameters){
    if (parameters[1] == NULL) {
        fprintf(stderr, "err: expected argument to \"cd\"\n");
    }else {
        if (chdir(parameters[1]) != 0) {
            perror("err");
        }
    }
}

/*
 * This function is used to get machine name using inline assembler. To get the machine name, i use service 87, then i also push string in which my name will be saved (strojOS)
 * and lastly i push number of characters, in my case 256.
 */

void getMachName(){

    char strojOS[256];
    int retI, service = 87;              //service number

    asm("push %%ecx\n\t"                 //here i push contents of cx register
        "push %%ebx\n\t"                 //here i push contents of bx register
        "push %%eax\n\t"                 //here i push contents of ax register
        "int $0x80\n\t"
        : "=a"(retI)                     //used to check if executed successfully
        : "a"(service), "b"(strojOS), "c"(256)
        : "memory");

    printf("%s", strojOS);
    
}

/*
 * This function is used to get user name. First i ger userID using inline assembler. I use service 24. Then i user function getpwuid, which gives me all information about current user.
 * then i simply print out pw_name.
 */

void getUserName(){
      uid_t userID;
       __asm__("int $0x80"
               :"=r" (userID)         //here i save my ID
               :"0" (24));
               
      struct passwd *pas = getpwuid (userID);         //used to access information about user
      printf("%s",pas->pw_name);
}

/*
 * This function is used to get current time. I use similar function like in getMachName. The difference is in service number. Here i use 232.
 * when i successfully get my timespec, then i call localtime to get my current time. Then i can simply print out hours and minutes.
 */

struct tm *getTime(int Out){
	
    int Time_asm;
    struct timespec tp;
    struct tm *infoTime;
    int time_service = 232;
    asm volatile("push %%ecx\n\t"
                 "push %%ebx\n\t"
                 "push %%eax\n\t"
                 "int $0x80\n\t"
                 :"=a"(Time_asm)
                 :"a"(time_service), "b"(CLOCK_REALTIME),"c"(&tp)
                 :"memory");
    infoTime = localtime(&tp.tv_sec);                       //used to access local time
    if(Out == 1){                                           //if used for prompt
    	printf("%d:%d ",infoTime->tm_hour, infoTime->tm_min);
    }
    return infoTime;
}

/*
 * This function is used to change the prompt appearance. User can choose, which element or combination of them will be shown.
 * If input is 1 the time will be turned ON/OFF, if input is 2 then username will be turned ON/OFF, if input is 3 then machine name will be turned ON/OFF and 
 * if input is 0 then user will return back to prompt.
 */

void promptChange(){
   printf("Aktualny tvar promptu:\n");
   if(PTime == 1)
		getTime(1);                                         //vypisem aktualny cas
   if(PUname == 1)
   		getUserName();                                //tu ziskam meno pouzivatela
   if((PUname == 1)&&(PSname == 1))
   		printf("@");
   if(PSname == 1)
   		getMachName();                                  //tu ziskam meno stroja
   printf("#>  \n");
   printf("Ak zadate 1 tak zap/vyp cas, ak zadate 2 tak zad/vyp meno uzivatela, ak zadate 3 tak zap/vyp meno stroja\nPre ukoncenie zadajte 0\n");
   int charr;
   while(1){
       charr = getchar();
       if(charr == '1'){
           if(PTime == 1){
               printf("OFF\n");
               PTime = 0;
           }else{
               printf("ON\n");
               PTime = 1;
          }
       }if(charr == '2'){
            if(PUname == 1){
               printf("OFF\n");
               PUname = 0;
           }else{
               printf("ON\n");
               PUname = 1;
          }
       }if(charr == '3'){
            if(PSname == 1){
               printf("OFF\n");
               PSname = 0;
           }else{
               printf("ON\n");
               PSname = 1;
          }
       }if(charr == '0'){
           break;
      } 
   }
}

/*
 * This function is used to simply print out prompt. It prints out time, username and machine name
 */

void prompt(){
    if(PTime == 1)
    	getTime(1);                                   //vypisem aktualny cas
    if(PUname == 1)
    	getUserName();                                //tu ziskam meno pouzivatela
    if((PUname == 1)&&(PSname == 1))
    	printf("@");
    if(PSname == 1)
    	getMachName();                                  //tu ziskam meno stroja
    printf("#>  ");
}

/*
 * This function is used to parse incoming command from client. I use strtok function to parse *pom into fragments. I split string on every space.
 * Then i save this parsed string into **poleP. This 2D array will be later used to parse commands from parameters.
 */

char **serverParser(int *pom, char pomocny[]){
    int i=0, poleSize = 1;
    char *split, **poleP=malloc(64*sizeof(char*));
    split = strtok(pomocny, " \n");
    while(split != NULL) {											//split string
        poleP[i] = strdup(split);
        i++;
        if(i == (poleSize*64)-2){                                  //if buffer is too small, then realloc
        	poleSize++;
        	poleP = realloc(poleP,64*poleSize*sizeof(char*));
		}
        split = strtok(NULL, " \n");
    }
    poleP[i]='\0';
    *pom=i;
    return poleP;
}

/*
 * This function will read user input. It reads input until \n or EOF. This function can also alter text for easier parsing. If pipe is read and it has no space before or after it
 * my parser can easily put one so i can safely use strtok. When reading is done, then i need to parse said input. For this i use strtok to split string on every space character.
 */

char **read_input(int *pom, int client, int sock){
    char *split, **poleP=malloc(64*sizeof(char*));
    char *pomocny = malloc(1024*sizeof(char));
    int i=0, position = 0, sizeP = 1, poleSize = 1;
    int charr, swww=0;
    
    while(1){
       charr = getchar();
       
       if((charr == EOF)||(charr =='\n')){                 //end reading if EOF or \n
    		pomocny[position]='\0';
            break;
       }else{
           if((swww == 1)&&(charr != '&')){
              pomocny[position]=' ';
              position++;
           }
           swww = 0;
           if((charr == '|')||(charr == '<')||(charr == '>')){		//if pipe, or <, > then check if \ isnt before said special character
               if(position>0){
                  if(pomocny[position-1]=='\\'){ 
                     	pomocny[position]=charr;
                  }else{                                           //if isnt, then insert space
                     	pomocny[position]=' ';
                     	position++;
                     	pomocny[position]=charr;
                  }
               }else{												//else save character
                    pomocny[position]=charr;
               }
            swww = 1;
          }else{
             	pomocny[position]= charr;
           }
       }
         position++;
         if(position >= (1024*sizeP)){
            sizeP++;
            pomocny = realloc(pomocny, 1024*sizeP);
         }
    }
    
    if(position == 0){											   //if no input
       	*pom = 0;
       	return NULL;
     }
     
    if(client == 1){												//if said input is on client, then send to server
    	write(sock, pomocny, position+1);
     }
     
    split = strtok(pomocny, " \n");
    while(split != NULL) {										    //split string
        poleP[i] = strdup(split);
        i++;
        if(i == (poleSize*64)-2){									//if buffer is too small, then realloc
        	poleSize++;
        	poleP = realloc(poleP,64*poleSize*sizeof(char*));
		}
        split = strtok(NULL, " \n");
    }
    
    poleP[i]='\0';
    *pom=i;
    free(pomocny);
    return poleP;
}

/*
 * This function is used to execute sets of commands , that contain more that one command with pipe etc. Firstly i take commands to first commandEnder(;) and then i create pipes
 * which i would need. Then i can execute. If it is the first command, then i wont dup input from pipe(because there isnt any) and if it is the last command i wont dup output to pipe
 * (because also there isnt any). This rule doesnt apply to <,> because i can set input on first or redirect output from last. <,> have bigger priority than pipe. 
 * Lastly i need to consider, if this is the server. If so then i need to redirect output to client, if i can or need to.
 */

void launchCommand(struct COMMAND newHelpCom[], int num_new, int socket, int server){
    pid_t pid = 0;
    int status, i, j;
    int pipeOut[2 * (num_new-1)];					//here i create pipes for my commands
    for(i=0; i<num_new-1; i++){
		if(pipe(pipeOut + i*2) <0){
	   		printf("Pipe err\n");
	   		exit(1);
	}
   }
   int commPocHelp = 0;
    i=0;
    while(1){
		pid = fork();								//here i create duplicate process
		if(pid == 0){								//if child
	  		if(i != 0){								//if not first command
				if(dup2(pipeOut[(i - 2)], 0) < 0){			//then redirect input
					printf("Dup err\n");
		   			exit(1);
               }
			}if(commPocHelp < (num_new-1) ){				//if not last command
				if(dup2(pipeOut[(i + 1)], 1) < 0){			//then redirect output
					printf("Dup err\n");
		   			exit(1);
		}
	}         
        	if(newHelpCom[commPocHelp].intp == 1){			//if command contains < , then input is redirected from file
        		if(access(newHelpCom[commPocHelp].intpC,0) != -1){
		      		int in = open(newHelpCom[commPocHelp].intpC, O_RDWR);
	            	dup2(in, 0);
	            	close(in);
		  		}else{
                	printf("err input File\n");                 
		  }
        }
        	if(newHelpCom[commPocHelp].outp == 1){			//if command contains > , then output is redirected to file
            	int out = open(newHelpCom[commPocHelp].outpC, O_WRONLY | O_CREAT, 0600);
		    		if(dup2(out, newHelpCom[commPocHelp].deskr) == -1){
	            	perror("NN");
		    }
                	close(out);
          }
	  		if(server == 1){								//if server
	     		if(commPocHelp == (num_new-1)){				//if last command
           			if((newHelpCom[commPocHelp].outp == 1)&&(newHelpCom[commPocHelp].deskr==2)){		//if err output is redirected            
                	}else{																				//else redirect to client
						dup2(socket,2);
                	}
           			if((newHelpCom[commPocHelp].outp == 1)&&(newHelpCom[commPocHelp].deskr==1)){         //if output is redirected     
                	}else{																				 //else redirect to client
	          			dup2(socket,1);
                	}
	     			close(socket);
          }
	}
	
			for(j=0;j<2*(num_new-1);j++){																//close pipes
				close(pipeOut[j]);
			}
			if (execvp(newHelpCom[commPocHelp].parameters[0] , newHelpCom[commPocHelp].parameters) == -1) {			//execute command
            	perror("err");
            	exit(1);
        	}
     	}else if(pid < 0){
          	printf("Fork err\n");
          	exit(1);
     	}
		commPocHelp++;
		i+=2;
		if(commPocHelp == num_new)
	   		break;
}
   for(i=0;i<2*(num_new-1);i++){
	close(pipeOut[i]);
   }for(i=0; i<num_new;i++){																					//wait for commands to finish
	wait(&status);
   }
}

/*
 * This function is used to execute built-in commands like help, halt, cd, etc. I use it also to execute simple commands without pipes.
 */

int executeCommand(struct COMMAND command[], int commandPoc, int socket, int server){
    int i;
    if(commandPoc == 0){					//if no commands
        return 1;
    }
    int commOldM = 0;
    while(1){
    	if(commOldM == commandPoc){						//if last command, then break
       		break;
    	}
    	int num_new = 0, sizeNew = 1;
    	i=0;
    	struct COMMAND *newHelpCom = malloc(20*sizeof(struct COMMAND));
	    while(1){
	     	if(commOldM == commandPoc){					//if last command, then break
	        	break;
	      	}if(command[commOldM].endComm == 1){		//if command ender, then break
	        	newHelpCom[num_new]=command[commOldM];
	        	commOldM++;
	        	num_new++;
	       	 break;
	    }
	    
	      	newHelpCom[num_new]=command[commOldM];
	      	commOldM++;
	      	num_new++;
	      	if(num_new == (sizeNew*20)-2){				//if buffer too small, then realloc
	        	sizeNew++;
	        	newHelpCom = realloc(newHelpCom,(sizeNew*20*sizeof(struct COMMAND)));
	      }
	    }
		for(i=0;i<num_new;i++){				//execute built-in commands
	        if(strcmp(newHelpCom[i].parameters[0],"halt")==0){			//if halt, then exits
	            return 0;
	        }
	        else if(strcmp(newHelpCom[i].parameters[0],"help")==0){	//if help, then calls help function
	            help(server, socket);
	            return 1;
	        }else if(strcmp(newHelpCom[i].parameters[0], "quit")==0){	//if quit, then disconnects from server
		    	if(server == 1){
		    		char buffer[6]="connE\0";
		    		write(socket, buffer, 7);
		    		close(socket);
		    		return 1;
				}
			}else if(strcmp(newHelpCom[i].parameters[0], "prompt")==0){	//if prompts, then call prompt function
	          	if(server == 1){
	            	char buffer[52]="You can not change prompt while connected to server\0";
		    		write(socket, buffer, 53);
		    		return 1;
	          	}else{
	             	promptChange();
	             	return 1;
	          }
	        }
		}
	    if(strcmp(newHelpCom[0].parameters[0],"cd")==0){					//if cd, then change directory
	        cd(command[0].parameters);
		}else{
		   	if(commandPoc > 1){											//if more than one command
	           	launchCommand(newHelpCom, num_new, socket, server);
			}
		   else{														//else execute command
		    	pid_t pid = 0;
	            int status;
	            pid = fork();											//create duplicate process
	            if(pid == 0){
	               	if(newHelpCom[0].intp == 1){							//if command contains <, then redirect input from file
		          		if(access(newHelpCom[0].intpC,0) != -1){
			      			int in = open(newHelpCom[0].intpC, O_RDWR);
		              		dup2(in, 0);
		              		close(in);
			  			}else{
	                     	printf("err input File\n");                 
			  			}
					}if(newHelpCom[0].outp == 1){							//if command contains >, then redirect output to file
			    		int out = open(newHelpCom[0].outpC, O_WRONLY | O_CREAT, 0600);
			    		if(dup2(out, newHelpCom[0].deskr) == -1){
		                	perror("NN");
			    		}
	                    close(out);
		        	}
		       		if(server == 1){									//if server
			   			if((newHelpCom[0].outp == 1)&&(newHelpCom[0].deskr==2)){
	                 	}else{
			   				dup2(socket,2);
	                  	}
	                	if((newHelpCom[0].outp == 1)&&(newHelpCom[0].deskr==1)){
	                 	}else{
	                     	dup2(socket,1);
	                  	}
			   			close(socket);
					}
		       if (execvp(newHelpCom[0].parameters[0],newHelpCom[0].parameters) == -1) {			//execute command
	            	perror("err");
	            	exit(1);
	           }
		    }else if(pid < 0){
				printf("Fork err\n");
	            exit(1);
		   }else{																			//wait for command
			waitpid(pid, &status, 0);        
	        }
		}
	}
}
    return 1;
}

/*
 *  This function is used to parse string into command and parameters. I can then save these information into struct COMMAND. Firstly i check special characters, and save them accordingly
 *  Then i can return struct for the next step
 */

struct COMMAND *parseNew(char **param, int numOfP, int *ParseN){
     int commandSIZ = 1;
     struct COMMAND *command = malloc(20*sizeof(struct COMMAND));
     int commandNum=0,i, parameterNumber=0;
     char **helper = malloc(64*sizeof(char*));
     command[0].intp = 0;
     command[0].outp = 0;
     command[0].endComm = 0; 
     for(i=0;i<numOfP;i++){									//if buffer too small, then realloc
        if(commandNum == ((20*commandSIZ)-2)){
            commandSIZ++;
            command = realloc(command, (20*commandSIZ*sizeof(struct COMMAND)));
         }
        if(strcmp(param[i], "#")==0){						//if comment
              break;
          }
        if(param[i][0]=='#'){
           	break;
	 	}else{
         	if(strcmp(param[i],"|")==0){					//if pipe
          		helper[parameterNumber] = '\0';
          		command[commandNum].parameters = helper;
          		command[commandNum].parameterCount = parameterNumber;
	  			char **nov = malloc(64*sizeof(char*));
          		helper = nov;
          		commandNum++;
          		parameterNumber = 0;
	  			command[commandNum].intp = 0;
          		command[commandNum].outp = 0;
          		command[commandNum].endComm = 0;
			}else if(strcmp(param[i],">")==0){				//if output redirect
	  			command[commandNum].outp = 1;
          		command[commandNum].deskr = 1;
          		if(param[i+1][strlen(param[i+1])-1]==';'){	//if command separator, then go to next command
             		char *HelpStr = malloc((strlen(param[i+1])-1)*sizeof(char));
             		int pol;
             		for(pol=0;pol<(strlen(param[i+1])-1);pol++){
                 		HelpStr[pol]=param[i+1][pol];
             		}
             		command[commandNum].outpC = strdup(HelpStr);
             		helper[parameterNumber+1] = '\0';
             		command[commandNum].parameters = helper;
             		command[commandNum].parameterCount = parameterNumber+1;
             		command[commandNum].endComm = 1;
	     			char **nov = malloc(64*sizeof(char*));
             		helper = nov;
             		commandNum++;
             		parameterNumber = 0;
	     			command[commandNum].intp = 0;
             		command[commandNum].outp = 0;
             		command[commandNum].endComm = 0;
          		}else{
          			command[commandNum].outpC = strdup(param[i+1]);
          		}
          		i++;
        	}else if(strcmp(param[i],">&1")==0){		//if standard output redirect
	  			command[commandNum].outp = 1;
          		command[commandNum].deskr = 1;
          		if(param[i+1][strlen(param[i+1])-1]==';'){	//if command separator, then go to next command
             		char *HelpStr = malloc((strlen(param[i+1])-1)*sizeof(char));
             		int pol;
             		for(pol=0;pol<(strlen(param[i+1])-1);pol++){
                 		HelpStr[pol]=param[i+1][pol];
             		}
             		command[commandNum].outpC = strdup(HelpStr);
            		helper[parameterNumber+1] = '\0';
             		command[commandNum].parameters = helper;
             		command[commandNum].parameterCount = parameterNumber+1;
             		command[commandNum].endComm = 1;
	     			char **nov = malloc(64*sizeof(char*));
             		helper = nov;
             		commandNum++;
             		parameterNumber = 0;
	     			command[commandNum].intp = 0;
             		command[commandNum].outp = 0;
             		command[commandNum].endComm = 0;
          		}else{
          			command[commandNum].outpC = strdup(param[i+1]);
         		}
          		i++;
        	}else if(strcmp(param[i],">&2")==0){     //if error output redirect
	  			command[commandNum].outp = 1;
          		command[commandNum].deskr = 2;
          		if(param[i+1][strlen(param[i+1])-1]==';'){   //if command separator, then go to next command
             		char *HelpStr = malloc((strlen(param[i+1])-1)*sizeof(char));
             		int pol;
             		for(pol=0;pol<(strlen(param[i+1])-1);pol++){
                 		HelpStr[pol]=param[i+1][pol];
             		}
             		command[commandNum].outpC = strdup(HelpStr);
             		helper[parameterNumber+1] = '\0';
             		command[commandNum].parameters = helper;
             		command[commandNum].parameterCount = parameterNumber+1;
             		command[commandNum].endComm = 1;
	     			char **nov = malloc(64*sizeof(char*));
             		helper = nov;
             		commandNum++;
             		parameterNumber = 0;
	     			command[commandNum].intp = 0;
             		command[commandNum].outp = 0;
             		command[commandNum].endComm = 0;
          		}else{
          			command[commandNum].outpC = strdup(param[i+1]);
          		}
          		i++;
			}else if(strcmp(param[i],"<")==0){		//if input redirect
	  			command[commandNum].intp = 1;
          		if(param[i+1][strlen(param[i+1])-1]==';'){   //if command separator, then go to next command
             		char *HelpStr = malloc((strlen(param[i+1])-1)*sizeof(char));
             		int pol;
             		for(pol=0;pol<(strlen(param[i+1])-1);pol++){
                 		HelpStr[pol]=param[i+1][pol];
             		}
             		command[commandNum].intpC = strdup(HelpStr);
             		helper[parameterNumber+1] = '\0';
             		command[commandNum].parameters = helper;
             		command[commandNum].parameterCount = parameterNumber+1;
             		command[commandNum].endComm = 1;
	     			char **nov = malloc(64*sizeof(char*));
             		helper = nov;
             		commandNum++;
             		parameterNumber = 0;
	     			command[commandNum].intp = 0;
             		command[commandNum].outp = 0;
             		command[commandNum].endComm = 0;
          		}else{
          			command[commandNum].intpC = strdup(param[i+1]);
          		}
          		i++;
        	}else{     //if command or parameter
          		if(param[i][strlen(param[i])-1]==';'){    //if command separator, then go to next command
             		param[i][strlen(param[i])-1]='\0';
             		helper[parameterNumber] = param[i];
             		helper[parameterNumber+1] = '\0';
             		command[commandNum].parameters = helper;
             		command[commandNum].parameterCount = parameterNumber+1;
             		command[commandNum].endComm = 1;
	     			char **nov = malloc(64*sizeof(char*));
             		helper = nov;
             		commandNum++;
             		parameterNumber = 0;
	     			command[commandNum].intp = 0;
             		command[commandNum].outp = 0;
             		command[commandNum].endComm = 0;  
          		}else{
            		if(param[i][0]=='\\'){
             			char *HelpStr = malloc((strlen(param[i+1])-1)*sizeof(char));
             			int pol;
             			for(pol=1;pol<(strlen(param[i]));pol++){
                 			HelpStr[pol-1]=param[i][pol];
             			}
             			helper[parameterNumber] = strdup(HelpStr);
               			parameterNumber++;
           			}else{
	       				helper[parameterNumber] = param[i];
               			parameterNumber++; 
           			} 
         		}
			}
    	}
    } 
    helper[parameterNumber] = '\0';
    command[commandNum].parameters = helper;
    command[commandNum].parameterCount = parameterNumber; 
    if(parameterNumber > 0){
     	*ParseN = commandNum+1;
	}else{
		*ParseN = commandNum;
	}
    return command;
}

/*
 * This function is dedicated for server. If server is active, it can read input from socket(clients), or standard input. If client sends something, server executes that command
 * and sends output to client. Inputs from standard input are displayed directly on server. Server can handle up to 30 clients at the same time.
 */

int server_C(char *pathSock, struct sockaddr_un ad, int sock, struct COMMAND *command){
    int parsePoc, commandPoc, clientSock[40], clientSockSec[40];
    int maxClient = 40, maxDesc = 0, newsock,i;
    char **param;
    for(i=0;i<maxClient;i++){						//initialize sockets
		clientSock[i]=0;
	}
    if(pathSock != NULL){							//create socket
		strcpy(ad.sun_path, pathSock);
     	ad.sun_path[sizeof(ad.sun_path)-1] = '\0';
	}else{
        strcpy(ad.sun_path, "PLACE");
	}
    sock =  socket(PF_LOCAL, SOCK_STREAM , 0);
    if(sock == -1){
		perror("sock");
		exit(1);
	}
     
    if(bind (sock, (struct sockaddr*)&ad, sizeof(ad))<0){		//bind socket
		perror("bind");
		exit(1);
	}
	listen(sock,5);												//listen to up to 5 clients in queue
 	char buff[1024];
  	fd_set rs;
	FD_ZERO(&rs);
	FD_SET(0,&rs);
	FD_SET(sock, &rs);
	maxDesc = sock;
	for(i=0; i<maxClient; i++){									//create handlers for sockets
	    int pom = clientSock[i];
	    if(pom>0)
	     	FD_SET(pom, &rs);
	    if(pom> maxDesc){
			maxDesc = pom;
	}
	}
	int r, stat;
    while(1){
    	struct timeval timeout;
    	timeout.tv_sec = 10;
    	timeout.tv_usec = 0;
		stat = select(maxDesc+1, &rs, NULL, NULL, &timeout);			//select current input descriptor
		if(stat > 0){
			if(FD_ISSET(sock, &rs)){								//if local socket, then register new client
			   if((newsock = accept(sock, NULL, NULL))<0){			//accept client
					perror("acc");
					exit(1);
			}
			int cyk;
			for(cyk=0;cyk<maxClient;cyk++){							//add client number to array
				if(clientSock[cyk] == 0){
					clientSock[cyk] = newsock;
					struct tm *stamp = getTime(0);
					clientSockSec[cyk] = (stamp->tm_min*60+stamp->tm_sec);
					break;
				}
			}
			}
			else if(FD_ISSET(0, &rs)){								//if standard input
		        char *bufferP = malloc(1024*sizeof(char));
		        int charPP = 0, fo, currS=1;
		        while(1){											//read from console
			  		r = read(0, buff, 1024);
		          	for(fo=0;fo<r;fo++){
		             	bufferP[charPP] = buff[fo];
		             	charPP++;
		         	}
		          	if(r<1024){
		              	bufferP[charPP]='\0';
		              	break;
					}
		          	else{
		            	bufferP = realloc(bufferP, (currS+1)*1024);
		            	currS++; 
		         }
		        }
			  	commandPoc = 0;
			  	parsePoc = 0;
			  	param = serverParser(&commandPoc, bufferP);
		    	free(bufferP);
			  	command = parseNew(param, commandPoc, &parsePoc);
			  	if(strcmp(command[0].parameters[0],"stat")==0){				//if stat command, then display all current clients
			      	int c;
			      	for(c=0;c<maxClient;c++){
				  		if(clientSock[c]>0){
				     		printf("%d\n",clientSock[c]);
			         	}
			      	}
			  	}else if(strcmp(command[0].parameters[0],"abort")==0){		//if abort, then disconnect selected client
			        int c;
			        if(command[0].parameterCount>1){
				        int desc = atoi(command[0].parameters[1]);
				        for(c=0;c<maxClient;c++){
					    	if(clientSock[c]==desc){
					      		char buffer[6]="connE\0";
				    	      	write(desc, buffer, 7);
				              	close(desc);
				              	clientSock[c]=0;
				              	break;
					    	}
						}
					}else{
						printf("err: Not enough parameters\n");
					}
			  	}else if(strcmp(command[0].parameters[0],"close")==0){		//if close, then close socket
			   		char buffer[6]="connE\0";
			    	for(i=0;i<maxClient;i++){
						if(clientSock[i]>0){
				   			write(clientSock[i], buffer, 7);
			           		close(clientSock[i]);
			           		clientSock[i]=0;
						}
			   		}
			    	close(sock);
		            if(remove(pathSock)==0){
		               	printf("Closed\n");
		            }
			    	return 1;
			  	}else{														//else execute command
			  		if(executeCommand(command, parsePoc, 0, 0)==0){
			    		char buffer[6]="connE\0";
			    		for(i=0;i<maxClient;i++){
							if(clientSock[i]>0){
				   				write(clientSock[i], buffer, 7);
			           			close(clientSock[i]);
			           			clientSock[i]=0;
							}
			   			}
			    		close(sock);
		            	if(remove(pathSock)==0){
		               		printf("Closed\n");
		            	}
		            	exit(0);
		        	}
				}
			}else{															//input from client
				int cyk;
				for(cyk=0;cyk<maxClient;cyk++){								//get client number
					int ns = clientSock[cyk];
		        	if(FD_ISSET(ns, &rs)){
		        		struct tm *stamp = getTime(0);
						clientSockSec[cyk] = (stamp->tm_min*60+stamp->tm_sec);
		        		int sizeMess;
		        		char *bufferP = malloc(1024*sizeof(char));
		        		int charPP = 0, fo, currS=1;
		        		while(1){
							sizeMess = read(ns, &buff, 1024);
		          			for(fo=0;fo<sizeMess;fo++){
		              			bufferP[charPP] = buff[fo];
		              			charPP++;
		         			}
		        			if(sizeMess < 1024){
		            			bufferP[charPP]='\0';
		            			break;
		        			}else{
		            			bufferP =(char*)realloc(bufferP,(currS+1)*1024);
		            			currS++;
		        			}
		        		}
						commandPoc = 0;
						parsePoc = 0;
						param = serverParser(&commandPoc, bufferP);
		        		free(bufferP);
						command = parseNew(param, commandPoc, &parsePoc);
		        		//printf("%s\n",command[0].parameters[0]);
		        		if(strcmp(command[0].parameters[0],"halt")==0){		//if halt, close connection
		            		close(ns);
			    			clientSock[cyk]=0;
						}else if(strcmp(command[0].parameters[0],"quit")==0){	//if quit, close connection
			    			char buffer[6]="connE\0";
			    			write(ns, buffer, 7);
			    			close(ns);
			    			clientSock[cyk]=0;
						}else{													//else execute command
							if(executeCommand(command, parsePoc, ns, 1)==0){
			    				char buffer[6]="connE\0";
			    				for(i=0;i<maxClient;i++){
									if(clientSock[i]>0){
				   						write(clientSock[i], buffer, 7);
			           					close(clientSock[i]);
			           					clientSock[i]=0;
									}
			   					}
			    				close(sock);
		            			exit(0);
		        			}
		                	char buffer[2]=">\0";
		                	write(ns, buffer, 2);
		
						}
					}
				}
			}

	}else if(stat == 0){
		struct tm *stamp = getTime(0);
		int currTim = (stamp->tm_min*60+stamp->tm_sec);
		int cyk;
		for(cyk=0;cyk<maxClient;cyk++){
			if(clientSock[cyk]>0){
				if((currTim - clientSockSec[cyk])>=timeOut){
					char buffer[6]="connE\0";
					write(clientSock[cyk], buffer, 7);
			    	close(clientSock[cyk]);
			        clientSock[cyk]=0;
				}
			}
		}
	}else{
		break;
	}
			FD_ZERO(&rs);
			FD_SET(0,&rs);
			FD_SET(sock, &rs);
			maxDesc = sock;
			for(i=0; i<maxClient; i++){
			    int pom = clientSock[i];
			    if(pom>0)
			     	FD_SET(pom, &rs);
			    if(pom> maxDesc){
					maxDesc = pom;
				}
		    }
}
return 1;
}

/*
 *
 */

int main(int argc, char *argv[]) {
	timeOut = 300;
    if(signal(SIGQUIT, signal_handle)==SIG_ERR){
        printf("ERR\n");
    }if(signal(SIGTSTP, signal_handle)==SIG_ERR){
        printf("ERR\n");
    }
    CL=0;
    PTime = 1;
    PUname = 1;
    PSname = 1;
    char **param, *logP, *configFil;
    int commandPoc = 0,i, parsePoc=0, loG=0, config = 0;
    struct COMMAND *command = NULL;
    FILE * DeskripT = NULL;
    int server = 0, client = 0;
    //int port, serverTCP = 0;
    struct sockaddr_un ad;
    int sock = 0;
    for(i=0;i<argc;i++){					//here i handle input switches
		if(strcmp(argv[i], "-p")==0){	
	    	i++;
		}if(strcmp(argv[i], "-u")==0){		//if -u, then server will turn on
            server = 1;
            if((i+1)>(argc-1)){
            	printf("err: not enough parameters\n");
            	exit(1);
			}else{
				if(argv[i+1][0]=='-'){
					printf("err: no socket name given\n");
					exit(1);
				}else{
            		pathSock = strdup(argv[i+1]);
	    			i++;
	    		}
	    	}
		}if(strcmp(argv[i], "-h")==0){		//if -h, then help will be printed out
	    	help(0,0);
		}if(strcmp(argv[i], "-c")==0){		//if -c, then client will connect to server
            CL=1;
	    	client = 1;
		}if(strcmp(argv[i], "-l")==0){		//if -l, then log will be created
			if((i+1)>(argc-1)){
            	printf("err: not enough parameters\n");
            	exit(1);
			}else{
				if(argv[i+1][0]=='-'){
					printf("err: no file name given\n");
					exit(1);
				}else{
           			loG = 1;
           			logP = strdup(argv[i+1]);
           			DeskripT = fopen(logP, "ab+");
           }
        }
        }if(strcmp(argv[i], "-C")==0){		//if -C, then configuration file will be loaded
	        if((i+1)>argc-1){
	            	printf("err: not enough parameters\n");
	            	exit(1);
				}else{
					if(argv[i+1][0]=='-'){
						printf("err: no file name given\n");
						exit(1);
					}else{
	           			config = 1;
	           			configFil = strdup(argv[i+1]);
	           			loG = 1;
	           }
	          }
     }
 }
 	if((client == 1)&&(pathSock==NULL)){
 		printf("err: no socket given\n");
 		exit(1);
	 }
    if(config == 1){						//if config file, then read configuration
      	FILE *ConfigDe = fopen(configFil, "r");
      	char buff[255];
      	fgets(buff,255, (FILE*)ConfigDe);
      	PTime = buff[0] - '0';
      	PUname = buff[1] - '0';
      	PSname = buff[2] - '0';
      	fgets(buff,255, (FILE*)ConfigDe);
      	buff[strlen(buff)-1]='\0';
      	DeskripT = fopen(buff, "ab+");
    }
    if(client == 1)
		server = 0;
    memset(&ad,0,sizeof(ad));
    ad.sun_family = AF_LOCAL;
    if(server == 1){						//if server, then go to server function
     	server = server_C(pathSock, ad, sock, command);
    }
    if(client == 1){						//if client, then connect to server
     	strcpy(ad.sun_path, pathSock);
     	sock = socket(PF_LOCAL, SOCK_STREAM, 0);
     	if(sock == -1){
			perror("sock");
			exit(1);
     	}
       	if(connect(sock, (struct sockaddr*)&ad, sizeof(ad))<0){
			perror("con");
			exit(1);
		}else{
       		printf("Successfully connected\n");
		}
        socketG = sock;
    }
    while(1){								
        client = CL;
	 	if(client == 1){					//if connected to server, handle commands
	 		fd_set rs;
	 		FD_ZERO(&rs);
	 		FD_SET(0,&rs);
	 		FD_SET(sock, &rs);
	 		commandPoc = 0;
	  		parsePoc = 0;
         	while(select(sock+1, &rs, NULL, NULL, NULL)>0){				//select from either socket or standard input
          		int freeC,freeI;
            	for(freeC=0;freeC<parsePoc;freeC++){					//free memory
               		for(freeI=0;freeI<command[freeC].parameterCount-1;freeI++){
                   		free(command[freeC].parameters[freeI]);
            		}
           		}
	  			commandPoc = 0;
	  			parsePoc = 0;
	  			if(FD_ISSET(0,&rs)){									//if standard input
          			param=read_input(&commandPoc, client, sock);
          			if((loG == 1)&&(commandPoc > 0)){					//write to log
               			struct tm *curr = getTime(0);
               			fprintf(DeskripT,"%d.%d %d:%d:%d ",curr->tm_mday,curr->tm_mon,curr->tm_hour,curr->tm_min,curr->tm_sec);
               			int cyk;
               			for(cyk = 0;cyk<commandPoc;cyk++){
                   			fprintf(DeskripT,"%s ",param[cyk]);
               			}
                   		fprintf(DeskripT,"\n");
            		}
	  				command = parseNew(param, commandPoc, &parsePoc);
	  				int nread;
         	 		if(parsePoc>0){
          				if(strcmp(command[0].parameters[0],"halt")==0){	 //if halt, disconnect and exit
              				close(sock);
              				if(DeskripT!=NULL)
                 				fclose(DeskripT);
              				exit(0);
         			}
	  					while(1){
          					char bbb[1024];
	  						nread = read(sock, bbb, 1024);
	  						if(strcmp(bbb,"connE")==0){				//server disconnect
	  							client = 0;
         					 	CL=0;
	  							close(sock);
	  							break;
	  							}
	    					write(1, bbb, nread);
	  						if(nread < 1024)
	    						break;
						}
        			}
    			}if(FD_ISSET(sock,&rs)){							//read from socket
	  				char bbb[1024];
          			int size;
          			while(1){
	  					size = read(sock, bbb, 1024);
           				if(size<1024)
              				break;
           				write(1, bbb, size);
         			}
	  				if(strcmp(bbb,"connE")==0){						//server disconnect
	  					client = 0;
          				CL=0;
	  					close(sock);
	  					break;
	  				}else{
            			write(1, bbb, size);
          			}
				}
         		FD_ZERO(&rs);
	 			FD_SET(0,&rs);
	 			FD_SET(sock, &rs);
        	}
		}else{									//if not connected to server
			while(1){
            	int freeC,freeI;
            	for(freeC=0;freeC<parsePoc;freeC++){			//free memory
               		for(freeI=0;freeI<command[freeC].parameterCount-1;freeI++){
                   		free(command[freeC].parameters[freeI]);
            		}
           		}	
	    		commandPoc = 0;
	    		parsePoc = 0;
            	prompt();
            	param=read_input(&commandPoc, client, sock);
            	if((loG == 1)&&(commandPoc > 0)){				//append to log
               		struct tm *curr = getTime(0);
                 	fprintf(DeskripT,"%d.%d %d:%d:%d ",curr->tm_mday,curr->tm_mon,curr->tm_hour,curr->tm_min,curr->tm_sec);
               		int cyk;
               		for(cyk = 0;cyk<commandPoc;cyk++){
                   		fprintf(DeskripT,"%s ",param[cyk]);
               		}
                   	fprintf(DeskripT,"\n");
            	}
	    		command = parseNew(param, commandPoc, &parsePoc);
            	if(parsePoc>0){
	     			if(strcmp(command[0].parameters[0],"listen")==0){
	     				if(command[0].parameterCount>1){
	         				pathSock = strdup(command[0].parameters[1]);
	         				server_C(pathSock, ad, sock, command);
	         			}else{
	         				 printf("err: Not enough parameters\n");
					 	}
	     			}else{
             			if(executeCommand(command, parsePoc, 0, 0)==0){			//execute command
	      					close(sock);
              				if(DeskripT!=NULL)
                				fclose(DeskripT);
              				return 0;
        				}
        			}
				}
			}
      	}
    }
    close(sock);
    if(DeskripT!=NULL)
     	fclose(DeskripT);
    return 0;
}

/*
 * V zadani som splnil bonusove ulohy:
 * 3. (3 body) Intern˝ prÌkaz stat vypÌöe zoznam vöetk˝ch aktu·lnych spojenÌ na ktor˝ch prijÌma prÌkazy, prÌpadne aj vöetky sokety na ktor˝ch prijÌma novÈ spojenia.
 * 4. (2 body) Intern˝ prÌkaz abort n ukonËÌ zadanÈ spojenie.
 * 5. (4 body) InternÈ prÌkazy listen a close (s prÌsluön˝mi argumentami) pre otvorenie a zatvorenie soketu pre prijÌmanie spojenÌ.
 * 8. (4 body) Presmerovanie v˝stupu do æubovolnÈho zvolenÈho deskriptoru, >&n, kde n je deskriptor s˙boru.
 * 9. (3 body) S prepÌnaËom "-c" v kombin·cii s "-i", resp. "-u" sa bude program spr·vaù ako klient, teda pripojÌ sa na dan˝ soket a bude do neho posielaù svoj ötandardn˝ vstup a zobrazovaù prich·dzaj˙ci obsah na v˝stup.
 * 14. (3 body) Konfigurovateæn˝ tvar promptu, intern˝ prÌkaz prompt.
 * 18. (5 bodov) Ak je niektorÈ spojenie neËinnÈ zadan˙ dobu, bude zruöenÈ.
 * 24. (2 body) Program s prepÌnaËom "-l" a menom s˙boru bude do neho zapisovaù z·znamy o vykon·vanÌ prÌkazov (log-y).
 * 25. (2 body) Program s prepÌnaËom "-C" a menom s˙boru naËÌta konfigur·ciu zo s˙boru (prompt, doba neËinnosti, log s˙bor, ...).
 * 27. (5 bodov) Pouûitie sign·lov. E. g. znovunaËÌtanie konfiguraËnÈho s˙boru po prÌchode zvolenÈho sign·lu, zachytenie Ctrl+C, vykonanie prÌkazu halt, quit a help (alebo inÈ).
 * 30. (1 bod) DobrÈ koment·re, resp. dokument·cia, v anglickom jazyku.
 *
 */


