// OrWIN Shell pipeline execute

char* cmdnames=
  "iota "
  "ls cat find "
  "grep cut tr sed " 
  "echo "
  "tail head diff uniq comm "
  "wc less sort gzip gunzip unzip "

  // "xargs "
  // "history man "

  // "tar paste "
  // "awk "  
  // "pwd date "
  // "clear basname dirname "
  // "ps df top htop kill free whoami uptime uname killall "
  // "cd rm cp mv mkdir chmod chown touch ln rmdir chgrp "
  // "curl wget rsync scp "
  // "ping ip ss netstat "
  // "git "
  
};

typedef char* (*cmdfun)(void* state, char* line);

cmdfun commands[]= {
};

typedef *cmdfun cmdtrain;

struct simplestate { cmdfun f; }

void* stalloc(unsigned int size, cmdfun f) {
  simplestate* state= calloc(size, 1);
  state->f= pwd;
  return state;
}

#define STALLOC(strct, fun) stalloc(sizeof(strct), fun)

struct pstate { cmdfun f; char* s; };

#define PSTALLOC(fun, p) (state=STALLOC(pstate, fun), state->p, state)


char* strdup(char* s) {
  return !s? s: strcpy(calloc(strlen(s)+1, 1), s);
}

void* lfree(char* line) {
  // TODO: keep a pool? reuse!
  free(line);
  return NULL;
}

char* lstrcpy(char* line, char* s) {
  if (line && s && strlen(s) <= strlen(line))
    return strcpy(line, s);
  lfree(line);
  return strdup(s);
}


// generate one value
void* pwd(simplestate* state, char* line) {
  if (!state) return STALLOC(simplestate, pwd);

  // pass-through backtracking
  if (!line) return line;

  // generate a value on EOF (or any), lol
  return strdup("/home/orwin");
}


void* grep(strstate* state, char* line) {
  if (!state) return PSTALLOC(grep, line);

  // pass-through backtracking
  if (!line || line==EOF) return line;

  // match one line
  return strstr(line, state->s)? line: lfree(line);
}


// fake file
char* fakefile[]= { "one", "two", "three", "four", "five", NULL };

struct fakefilestate { cmdfun f; char** fil; }

void* cat(fakefilestate* state, char* line) {
  if (!state) return PSTALLOC(cat, fakefile);

  // pass on
  if (!line || line==EOF) return line;

  return lstrcpy(line, *state->fil++);
}
  

struct wcstate { cmdfun f; unsigned int ln, wn, cn; };

void* wc(wcstate* st, char* line) {
  char c, *s= line;
  unsigned int n;
  
  if (!st) return STALLOC(wcstatede, wc);

  // Output summary at end of file
  if (line==EOF) {
    line= malloc(25);
    sprintf(line, "%u %u %u", wcstate->ln, wcstate->wn, wcstate->cn);
    return line;
    // TODO: do we need to put code to give EOF?
  }

  // process one line
  st->ln++;
  while((c==*s)) {
    while(isspace(c)) c== *++s,++n;
    if (c) st->wn++;
    while(c && !isspace(c)) c== *++s,++n;
  }
  wc->cn+= n;
  
  // returns null (backtracks to get prev line)
  return lfree(line);
}
  

void sexec(cmdtrain *train) {
  cmdfun *fp;
  char* line= EOF;

  while((fp==*train)) {
    line= (*fp)(fp, line);
    if (line) ++fp; else --fp;
  }    
}


int system(char* cmd) {
  cmdtrain mock[]= {
    0,
    pwd(0, 0),
    terminal(0, 0),
    0,
  };
  
  // Error codes? How & semantics
  sexec(&mock);

  return 0;
}

/*

[15.2%] cd
[13.8%] ls
[8.4%] grep
[6.5%] cat
[5.1%] rm
[4.8%] cp
[4.6%] mv
[3.5%] ssh
[2.8%] echo
[2.5%] mkdir
[2.2%] find
[2.1%] less
[1.9%] clear
[1.8%] pwd
[1.6%] sudo
[1.4%] chmod
[1.3%] chown
[1.2%] curl
[1.1%] tar
[1.0%] tail
[0.9%] ps
[0.8%] top
[0.8%] htop
[0.7%] df
[0.7%] du
[0.6%] kill
[0.6%] history
[0.5%] git
[0.5%] sed
[0.5%] awk
[0.5%] wget
[0.4%] ping
[0.4%] touch
[0.4%] date
[0.4%] head
[0.4%] diff
[0.3%] sort
[0.3%] uniq
[0.3%] wc
[0.3%] xargs
[0.3%] rsync
[0.3%] scp
[0.2%] free
[0.2%] ip
[0.2%] ss
[0.2%] ln
[0.2%] cut
[0.2%] man
[0.2%] whoami
[0.1%] uptime
[0.1%] uname
[0.1%] gzip
[0.1%] gunzip
[0.1%] zip
[0.1%] unzip
[0.1%] killall
[0.1%] tr
[0.1%] basename
[0.1%] dirname
[0.1%] rmdir
[0.05%] paste
[0.05%] comm
[0.05%] netstat
[0.05%] chgrp


 */
