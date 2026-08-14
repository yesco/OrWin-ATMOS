// OrWIN Shell pipeline execute

char* cmdnames=
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

cmdf commands[]= {
};

typedef char* (*cmdf)(void* state, char* line);



int system(char* cmd) {
  
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
