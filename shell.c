#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

struct elem {
	char *word;
	int key;
	struct elem *next;
};

typedef struct elem *list;

typedef struct
{
	int inFlag;
	int outFlag;
	char *inFile;
	char *outFile;
} reIO;

char *path, *stPath;

char *AddSym(char *s, int *pos, char sym);
char *CopyStr(char *s);
char *GetStr(void);

int StringExec(char *str, int banExec);
int ExecCmd(char **array, int fonFlag, reIO reIOblock);
int ExecCd(char **array);
void DoReIO(reIO reIOblock);

int isKey(int sym);
void EndWord(list *p, char **word, int key, int *wordSize);
void QuoteReaction(int *quoteFlag, int *emptyFlag);

int fonDetect(list p);
reIO reIODetect(list p);
void ClearStruct(reIO *block);
void DeleteKeys(list *p);
char **GetWordArr(list p);
void ClearArray(char **array);

void AddElem(list *p, char *s, int key);
void AddToTheEnd(list *p, char *s, int key);
void DeleteElem(list *p);
void PrintList(list p);
void ClearList(list *p);

int main(void) {
	char *str = "";
	int strFlag = 1;

	path = getcwd(NULL, 0);
	stPath = getcwd(NULL, 0);

	printf("%s$ ", path);
	while (str = GetStr()) {
		StringExec(str, 0);
		free(str);
		printf("%s$ ", path);
	}

	free(path); free(stPath);
	return 0;
}

char *AddSym(char *s, int *pos, char sym) {
	s = realloc(s, ++(*pos));
	*(s + (*pos) - 1) = sym;
	return s;
}

char *CopyStr(char *s) {
	char *t = NULL;
	int pos = 0;
	int sym;

	t = malloc(0);
	while ((sym = *(s + pos))) {
		t = AddSym(t, &pos, sym);
	}
	t = AddSym(t, &pos, '\0');
	return t;
}

char *GetStr(void) {
	int sym, bufSize = 0;
	char *s = NULL;

	s = malloc(0);
	while ((sym = getchar()) != '\n') {
		if (sym == EOF) {
			free(s);
			return NULL;
		}
		if (isgraph(sym) || isspace(sym))
			s = AddSym(s, &bufSize, sym);
	}
	s = AddSym(s, &bufSize, '\0');
	return s;
}

int StringExec(char *str, int banExec) {
	list p = NULL;
	int fd[2];
	char *word = NULL, *newStr = NULL;
	char **array;
	int sym = '\1', fdIn = 0;
	int pos = 0, num = 0, wordSize = 0;
	int quoteFlag = 0, emptyFlag = 0;
	int pipeFlag = 0, fonFlag = 0, execFlag = 0, endPipeFlag = 0;
	int bracketCnt = 0, cnt = 0, pid =0, wPid = 0, status = 0;
	reIO reIOblock = {0, 0, NULL, NULL};

	word = malloc(0);
	while (sym) {
		execFlag = execFlag || banExec;
		sym = *(str + (pos++));

		if (sym == '\"') {
			QuoteReaction(&quoteFlag, &emptyFlag);
			continue;
		}
		if (quoteFlag) {
			if (sym == '\0') {
				fprintf(stderr, "Error: amount of quotes is odd!\n");
				ClearList(&p); p = NULL;
				return 1;
			}
			word = AddSym(word, &wordSize, sym);
		}

		else {
			if (isKey(sym) || isspace(sym) || !sym) {
				if ((wordSize) || (emptyFlag))
					EndWord(&p, &word, 0, &wordSize);
			}
			else
				word = AddSym(word, &wordSize, sym);

			if (sym == '&' && *(str + (pos)) != '&' || /* < > & in process*/
				sym == '<' ||
				sym == '>' && *(str + (pos)) != '>') 
			{
				word = AddSym(word, &wordSize, sym);
				EndWord(&p, &word, 1, &wordSize);
			}

			if (sym == '>' && *(str + (pos)) == '>') { /* >> in process*/
				word = AddSym(word, &wordSize, sym);
				sym = *(str + (pos++));
				word = AddSym(word, &wordSize, sym);
				EndWord(&p, &word, 1, &wordSize);
			}

			if (sym == '|' && *(str + (pos)) != '|') { /* | between processes*/
				pipeFlag = 1;
				if (!num) {
					reIOblock = reIODetect(p);
					reIOblock.outFlag = 0;
				}
				DeleteKeys(&p);

				if (p) {
					array = GetWordArr(p);
					num++;
					pipe(fd);
					if (pid = fork()) {
						fdIn = fd[0];
						close(fd[1]);
					}
					else {
						if (num>1) {
							dup2(fdIn, 0);
							close(fdIn);
						}
						dup2(fd[1], 1);
						close(fd[1]);
						close(fd[0]); 
						if (!execFlag) {
							if (!strcmp(*array, "cd"))
								exit(0);
							else {
								ExecCmd(array, 0, reIOblock);
							}
							exit(0);
						}
					}

					ClearArray(array);
					free(array);
					ClearList(&p); p = NULL;
					if (!num)
						ClearStruct(&reIOblock);
				}
			}

			if (sym == '|' && *(str + (pos)) == '|' ||	/* || && ; between processes */
				sym == '&' && *(str + (pos)) == '&' ||	/* and end of processes      */
				sym == ';' || sym == '\0')
			{
				if (sym != ';' && sym != '\0')
					sym = *(str + (pos++));	

				if (sym == '\0')
					fonFlag = fonDetect(p);
				if (pipeFlag) {
					reIOblock = reIODetect(p);
					reIOblock.inFlag = 0;
				}
				else
					reIOblock = reIODetect(p);
				DeleteKeys(&p);

				if (p) {
					array = GetWordArr(p);
					num++;

					if (!execFlag) {
						if (pipeFlag) {
							if (pid = fork()) {
								close(fdIn);
								while (num--) {
									wPid = waitpid(-1, &status, 0);
									if (pid == wPid) {
										execFlag = WEXITSTATUS(status);
									}
								}
							} 
							else {
								dup2(fdIn, 0);
								close(fdIn); 
								if (!strcmp(*array, "cd"))
										exit(0);
								else {
									execFlag = ExecCmd(array, 0, reIOblock);
								}
								exit(execFlag);
							}
						}
						else
							if (!strcmp(*array, "cd"))
								execFlag = ExecCd(array);
							else
								execFlag = ExecCmd(array, fonFlag, reIOblock);
					}
					else
						execFlag = 0;

					if (sym == '|')
						execFlag = !execFlag;

					ClearArray(array);
					free(array);
					ClearList(&p); p = NULL;
				}
				else {
					if (pipeFlag) {
							while (num--) {
								wait(NULL);
							}
							return 1;
						}
					while ((wPid = waitpid(-1, &status, WNOHANG)) > 0)
						printf("Process %d was terminating with code #%d\n", wPid, WEXITSTATUS(status));
				}
				fonFlag = 0; ClearStruct(&reIOblock); pipeFlag = 0; num = 0;
			}

			if (sym == '(')
				if (p == NULL) {
					bracketCnt = 1; newStr = malloc(0); cnt = 0;

					while (bracketCnt) {
						sym = *(str + (pos++));

						if (sym == '(')
							bracketCnt++;
						if (sym == ')')
							bracketCnt--;
						if (bracketCnt)
							newStr = AddSym(newStr, &cnt, sym);

						if (sym == '\0') {
							fprintf(stderr, "Error: invalid amount of brackets!\n");
							free(newStr);
							ClearList(&p); p = NULL;
							return 1;
						}
					}
					sym = *(str + (pos++));
					while (isspace(sym))
						sym = *(str + (pos++));

					endPipeFlag = 0;
					if (sym == '|' && *(str + (pos)) != '|')
						pipeFlag = 1;
					else
						endPipeFlag = 1;
					newStr = AddSym(newStr, &cnt, '\0');

					if (pipeFlag) {
						if (!endPipeFlag) {
							num++;
							pipe(fd);
							if (fork()) {
								fdIn = fd[0];
								close(fd[1]);
							}
							else {
								if (num>1) {
									dup2(fdIn, 0);
									close(fdIn);
								}
								dup2(fd[1], 1);
								close(fd[1]);
								close(fd[0]);
								execFlag = StringExec(newStr, execFlag);
								exit(execFlag);
							}
						}
						else {
							num++;
							if (pid = fork()) {
								close(fdIn);
								while (num--) {
									wPid = waitpid(-1, &status, 0);
									if (pid == wPid) {
										execFlag = WEXITSTATUS(status);
									}
								}
								pipeFlag = 0;
							} 
							else {
								dup2(fdIn, 0);
								close(fdIn); 
								execFlag = StringExec(newStr, execFlag);
								exit(execFlag);
							}
						}
					}
					else
						execFlag = StringExec(newStr, execFlag);				
					if (sym == '|' && *(str + (pos)) == '|')
									execFlag = !execFlag;
					pos--;
					free(newStr);
				}
				else {
					fprintf(stderr, "Error: invalid position of bracket!\n");
					free(newStr);
					if (p) {
						ClearList(&p); 
						p = NULL;
					}
					return 1;
				}

			if (sym == ')') {
				ClearList(&p);
				fprintf(stderr, "Error: invalid position of \')\'\n");
				p = NULL;
				return 1;
			}
		}
	}

	free(word);
	return execFlag;
}

int ExecCmd(char **array, int fonFlag, reIO reIOblock) {
	pid_t pid, wPid;
	int status = 0;

	if ((pid = fork()))
		if (pid == (-1)) 
			perror("Error");
		else {
			if (!fonFlag) {
				while (((wPid = waitpid(-1, &status, 0)) != pid))
					printf("Process %d was terminating with code #%d\n", wPid, WEXITSTATUS(status));
				return WEXITSTATUS(status);
				}
			else {
				printf("Process %d is starting in backgrond mode\n", pid);
				while ((wPid = waitpid(-1, &status, WNOHANG)) > 0)
					printf("Process %d was terminating with code #%d\n", wPid, WEXITSTATUS(status));
				return 0;
			}
		} 
	else {
		DoReIO(reIOblock);
		execvp(*(array), array);
		perror("Error");
		exit(1);
	}
}

int ExecCd(char **array) {
	if ((*(array + 1) == NULL) || (*(array + 2) == NULL)) {
		if (*(array + 1) == NULL){
			if (chdir(stPath)) {
				perror("Error");
				return 1;
			}
		}
		else if	(chdir(*(array + 1))) {
			perror("Error");
			return 1;
		}
		free(path);
		path = getcwd(NULL, 0);
		printf("Current directory is %s\n", path);	
	}
	else {
		fprintf(stderr, "Error: invalid format of command!\n");
		return 1;
	}
	return 0;
}

void DoReIO(reIO reIOblock) {
	int fdIn, fdOut, fdAppEnd;

	if (reIOblock.inFlag) {
		if (((fdIn = open(reIOblock.inFile, O_RDONLY)) == (-1))) {
			perror("Error");
			return ;
		}
		if (dup2(fdIn, 0) == (-1)) {
			perror("Error");
			return ;
		}
		if (close(fdIn) == (-1)) {
			perror("Error");
			return ;
		}
	}
	if (reIOblock.outFlag == 1) {
		if (((fdOut = open(reIOblock.outFile, O_WRONLY|O_CREAT|O_TRUNC, 0666)) == (-1))) {
			perror("Error");
			return ;
		}
		if (dup2(fdOut, 1) == (-1)) {
			perror("Error");
			return ;
		}
		if (close(fdOut) == (-1)) {
			perror("Error");
			return ;
		}
	}
	if (reIOblock.outFlag == 2) {
		if (((fdAppEnd = open(reIOblock.outFile, O_WRONLY|O_CREAT|O_APPEND, 0666)) == (-1))) {
			perror("Error");
			return ;
		}
		if (dup2(fdAppEnd, 1) == (-1)) {
			perror("Error");
			return ;
		}
		if (close(fdAppEnd) == (-1)) {
			perror("Error");
			return ;
		}
	}
}

int isKey(int sym) {
	return (sym == '&' || sym == '<' || sym == '>' || sym == ';' 
		|| sym == '|' || sym == '(' || sym == ')');
}

void EndWord(list *p, char **word, int key, int *wordSize) {
	(*word) = AddSym(*word, wordSize, '\0');
	AddToTheEnd(p, *word, key);
	free(*word);
	*word = malloc(0);
	*wordSize = 0;
}

void QuoteReaction(int *quoteFlag, int *emptyFlag) {
	if (*quoteFlag) 
		*emptyFlag = 1;
	*quoteFlag = !(*quoteFlag);
}

int fonDetect(list p) {
	int fonFlag = 0;

	while (p) {
		if (!(strcmp(p->word, "&")) && p->key) {
			if (!(p->next))
				fonFlag = 1;
		}
		p = p->next;
	}

	return fonFlag;
}

reIO reIODetect(list p) {
	reIO block = {0, 0, NULL, NULL};

	while (p)
		if ((!strcmp(p->word, "<") || !strcmp(p->word, ">") || !strcmp(p->word, ">>")) && p->key) {
			if (p->next) {
				if (!strcmp(p->word, "<")) {
					block.inFlag = 1;
					block.inFile = CopyStr((p->next)->word);
				}
				if (!strcmp(p->word, ">")) {
					block.outFlag = 1;
					block.outFile = CopyStr((p->next)->word);
				}
				if (!strcmp(p->word, ">>")) {
					block.outFlag = 2;
					block.outFile = CopyStr((p->next)->word);
				}
				p = (p->next)->next;
			}
			else 
				p = p->next;
		}
		else
			p = p->next;

	return block;
}

void ClearStruct(reIO *block) {
	(*block).inFlag = 0;
	(*block).outFlag = 0;
	free((*block).inFile);
	free((*block).outFile);
}

void DeleteKeys(list *p) {
	int delFlag = 0;

	if (*p) {
		delFlag = 0;
		if (!(strcmp((*p)->word, "&")) && (*p)->key)
			DeleteElem(p);
		if (*p)
			if ((!strcmp((*p)->word, "<") || !strcmp((*p)->word, ">") || !strcmp((*p)->word, ">>")) && (*p)->key) {
				DeleteElem(p);
				if (*p)
					DeleteElem(p);
				delFlag = 1;
			}
		if (*p && !delFlag)
			DeleteKeys(&((*p)->next));
		else if (*p)
			DeleteKeys(p);
	}
}


char **GetWordArr(list p) {
	char **array = NULL;
	int cnt = 0;

	array = malloc(0);
	while (p) {
		array = realloc(array, (++cnt) * sizeof(char*));
		*(array + cnt - 1) = CopyStr(p->word);
		p = p->next;
	}
	array = realloc(array, (++cnt) * sizeof(char*));
	*(array + cnt - 1) = NULL;
	return array;
}

void ClearArray(char **array) {
	int cnt = 0;

	while (*(array + (cnt++))) {
		free(*(array + cnt - 1));
	}
}

void AddElem(list *p, char *s, int key) {
	list new = NULL;
	new = malloc(sizeof(struct elem));
	new->word = CopyStr(s);
	new->key = key;
	new->next = (*p);
	(*p) = new;
}

void AddToTheEnd(list *p, char *s, int key) {
	if (*p)
		AddToTheEnd(&((*p)->next), s, key);
	else
		AddElem(p, s, key);
}

void DeleteElem(list *p) {
	list temp;
	temp = *p;
	*p = (*p)->next;
	free(temp->word);
	free(temp);
}

void PrintList(list p) {
	if (p) {
		printf("%s %d\n", p->word, p->key);
		PrintList(p->next);
	}
}

void ClearList(list *p) {
	if (*p) {
		ClearList(&((*p)->next));
		free((*p)->word);
		free(*p);
	}
	else
		free(*p);
}
