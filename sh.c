#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

// Simplifed xv6 shell.

#define MAXARGS 10
#define MAXPATHS 20
// All commands have at least a type. Have looked at the type, the code
// typically casts the *cmd to some specific cmd type.
struct execpath{
  char *path[MAXPATHS];
  int  num;
};

struct cmd {
  int type;          //  ' ' (exec), | (pipe), '<' or '>' for redirection
};

struct cmdLink{
  struct cmd* cmd;
  struct cmdLink* next;
};

struct execcmd {
  int type;              // ' '
  char *argv[MAXARGS];   // arguments to the command to be exec-ed
};

struct redircmd {
  int type;          // < or > 
  struct cmd *cmd;   // the command to be run (e.g., an execcmd)
  char *file;        // the input/output file
  int mode;          // the mode to open the file with
  int fd;            // the file descriptor number to use for the file
};

struct pipecmd {
  int type;          // |
  struct cmd *left;  // left side of pipe
  struct cmd *right; // right side of pipe
};

int fork1(void);  // Fork but exits on failure.
struct cmdLink *parsecmd(char*);
void parsepath();
struct execpath execpath;
// Execute cmd.  Never returns.

char *
catstr(char * str1, char * str2)
{
  int l,l1,l2;
  char *p;

  l1 = strlen( str1 );
  l2 = strlen( str2 );
  l = l1 + l2 + 2;

  p = malloc(l);
  memcpy(p, str1, l1);
  p[l1+1] = '/';
  memcpy(p+l1+2,str2,l2+1);
  return p;  
}

void
runcmd(struct cmd *cmd)
{
  int p[2], r, i, len, sign;
  struct execcmd *ecmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;
  char *path;

  if(cmd == 0)
    exit(0);
  
  switch(cmd->type){
  default:
    fprintf(stderr, "unknown runcmd\n");
    exit(-1);

  case ' ':
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      exit(0);
    // Your code here ...
 
  for( i=0; i<execpath.num; i++){
    path = catstr( execpath.path[i] , ecmd->argv[0]);
    printf("exec %s\n", path);
    
    if( sign = execv( path, ecmd->argv))
      break;
  }
  
  if ( -1==sign)
    fprintf(stderr, "unknow cmd\n");
  break;

  case '>':
  case '<':
    rcmd = (struct redircmd*)cmd;
    // Your code here ...
    close(rcmd->fd);
    open(rcmd->file , rcmd->mode, S_IRWXU);
    runcmd(rcmd->cmd);
    break;

  case '|':
    pcmd = (struct pipecmd*)cmd;
    // Your code here ...
    int p[2];
    pipe(p);
    if( fork() == 0) {
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    } else {
      close(1);
      dup(p[1]);
      runcmd(pcmd->left);
      close(p[0]);
      close(p[1]);
    }
    break;
  }    
  exit(0);
}

int
getcmd(char *buf, int nbuf)
{
  
  if (isatty(fileno(stdin)))
    fprintf(stdout, "6.828$ ");
  memset(buf, 0, nbuf);
  fgets(buf, nbuf, stdin);
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}


int
main(void)
{
  static char buf[100];
  int fd, r;

  parsepath();
  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
      // Clumsy but will have to do for now.
      // Chdir has no effect on the parent if run in the child.
      buf[strlen(buf)-1] = 0;  // chop \n
      if(chdir(buf+3) < 0)
        fprintf(stderr, "cannot cd %s\n", buf+3);
      continue;
    }
   // if(fork1() == 0){
      struct cmdLink *Head = parsecmd(buf);
      while(Head != NULL){
        printf("Another execution\n");
        if(fork1() ==0)
          runcmd(Head->cmd);
        Head = Head->next;		
      }
     // parsecmd(buf);
  //  }
    wait(&r);
  }
  exit(0);
}

int
fork1(void)
{
  int pid;
  
  pid = fork();
  if(pid == -1)
    perror("fork");
  return pid;
}

struct cmdLink *
linkcmd(struct cmd * subcmd)
{
  struct cmdLink *cmdlink;
	
  cmdlink = malloc(sizeof(*cmdlink));
  memset(cmdlink, 0, sizeof(*cmdlink));
  cmdlink->cmd = subcmd;
  cmdlink->next = NULL;
	
  return cmdlink;
}

struct cmd*
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = ' ';
  return (struct cmd*)cmd;
}

struct cmd*
redircmd(struct cmd *subcmd, char *file, int type)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = type;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->mode = (type == '<') ?  O_RDONLY : O_WRONLY|O_CREAT|O_TRUNC;
  cmd->fd = (type == '<') ? 0 : 1;
  return (struct cmd*)cmd;
}

struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = '|';
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>;:";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;
  
  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  ret = *s;
  switch(*s){
  case 0:
    break;
  case ':':
    s++;
    break;
  case ';':
    s++;
    break;
  case '|':
  case '<':
    s++;
    break;
  case '>':
    s++;
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;
  
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int
peek(char **ps, char *es, char *toks)
{
  char *s;
  
  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);

// make a copy of the characters in the input buffer, starting from s through es.
// null-terminate the copy to make it a string.
char 
*mkcopy(char *s, char *es)
{
  int n = es - s;
  char *c = malloc(n+1);
  assert(c);
  strncpy(c, s, n);
  c[n] = 0;
  return c;
}

void parsepath()
{
  char *es;
  char *q;
  char * eq;
 
  char *path = getenv("PATH");
  printf("Value is %s\n", path);
  if ( NULL == path) 
    exit(-1);
  
  execpath.num = 0;
  es = path + strlen(path);
  
  while( path < es ){
    gettoken(&path, es, &q, &eq);
    path ++;
    execpath.path[execpath.num] = mkcopy(q, eq);
    execpath.num ++; 
  }
}

struct cmdLink*
parsecmd(char *s)
{
  char *es;
  struct cmdLink *Head;
  struct cmdLink *temp;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  Head = linkcmd(cmd);
  temp = Head;
  while(peek(&s, es, ";")){
    printf("another cmd\n");
    gettoken(&s, es, 0, 0);
    cmd = parseline(&s, es);
    temp->next = linkcmd(cmd);
    temp = temp->next;
  }
  peek(&s, es, "");
  if(s != es){
    fprintf(stderr, "leftovers: %s\n", s);
    exit(-1);
  }
  return Head;
}

struct cmd*
parseline(char **ps, char *es)
{
  struct cmd *cmd;
  cmd = parsepipe(ps, es);

  return cmd;
}

struct cmd*
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a') {
      fprintf(stderr, "missing file for redirection\n");
      exit(-1);
    }
    switch(tok){
    case '<':
      cmd = redircmd(cmd, mkcopy(q, eq), '<');
      break;
    case '>':
      cmd = redircmd(cmd, mkcopy(q, eq), '>');
      break;
    }
  }
  return cmd;
}

struct cmd*
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;
  
  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|;")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a') {
      fprintf(stderr, "syntax error\n");
      exit(-1);
    }
    cmd->argv[argc] = mkcopy(q, eq);
    argc++;
    if(argc >= MAXARGS) {
      fprintf(stderr, "too many args\n");
      exit(-1);
    }
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  return ret;
}
