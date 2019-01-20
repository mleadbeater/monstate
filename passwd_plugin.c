/*
Copyright (c) 2009-2019, Martin Leadbeater
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include "symboltable.h"
#include "options.h"
#include "plugin.h"
#include "property.h"
#include "buffers.h"

#define DEFAULT_PASSWORDFILE "/etc/passwd"
#define FINISH goto finished
#define ABORT goto exit_program
#define USER_FLAG "-u"
#define FILE_FLAG "-f"

#define LOWERCASE_ "abcdefghijklmnopqrstuvwxyz"
#define UPPERCASE_ "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define NUMBER_ "0123456789"
#define SALT_CHARSET (LOWERCASE_ UPPERCASE_ NUMBER_".")
#define SALT_SET_SIZE 63
#define SALT_HEADER "$1$"
#define SALT_TEMPLATE SALT_HEADER"XXXXXXXX$"
#define TEMPLATE_PASSWORD SALT_TEMPLATE"xxxxxxxxxxxxxxxxxxxxxx"

#define NUM_LOCK_TRYS 3
#define FALSE 0
#define TRUE 1
typedef int BOOL;

#ifdef MACOSX
/* dummy routines to enable compile on mac os x */
static int getline(char**buf,size_t*buflen,FILE*f)
{
  *buf= NULL,*buflen= 0;
  return 0;
}
static struct passwd*fgetpwent(FILE*f)
{
  return NULL;
}
static int putpwent(struct passwd*ent,FILE*f)
{
  return-1;
}
#endif


static int writePasswordToFile(struct passwd*pwe,char*encrypted_password,FILE*pwf)
{
  struct passwd new_pwe;
  int update_err;
  memcpy(&new_pwe,pwe,sizeof(struct passwd));
  new_pwe.pw_passwd= encrypted_password;
  update_err= putpwent(&new_pwe,pwf);
  return update_err;
}

#define ADDTOBUFFER  if (newmsg) { buf = append_buffer(buf, newmsg); }
#define PRINT(msg) { char *newmsg = NULL; asprintf(&newmsg, (msg) ); ADDTOBUFFER }
#define PRINT2(msg,a) { char *newmsg = NULL; asprintf(&newmsg, (msg),(a) ); ADDTOBUFFER }
#define PRINT3(msg,a,b) { char *newmsg = NULL; asprintf(&newmsg, (msg),(a),(b) ); ADDTOBUFFER }
#define PRINT4(msg,a,b,c) { char *newmsg = NULL; asprintf(&newmsg, (msg),(a),(b),(c) ); ADDTOBUFFER }

/*
    usage: PASSWD [-u username] [-f passwordfile]  secretfile
 */


#ifdef BUILD_PLUGIN
EXPORT
#else
static
#endif
char *plugin_func(symbol_table variables, char *user_buf, int user_buflen, int argc, const char *argv[])
{
  char *result= NULL;
  long found_pos= -1;
  struct passwd*pwe;
  char*username= NULL;
  char*password_filename= NULL;
  char*secret_filename= NULL;
  char*root_user;
  char*sample_password= NULL;
  int original_pw_len= 0;
  char*new_password= NULL;
  char*encrypted_password;
  struct stat fsi;
  FILE*pwf= NULL;
  char tmpfilename[24];
  FILE*tmpfile= NULL;
  
  char *buf = user_buf;
  int buflen = user_buflen;
  
  if (!buf) {
    if (!buflen)
      buflen = lookup_int_property(variables, argv[0], "MAXBUFSIZE", 5000);
    buf = malloc(buflen);
    if (!buf) ABORT; /* no memory? */
  }
  
  /* lookup the name of the root user */
  pwe= getpwuid(0);
  if (pwe&&pwe->pw_name)
    root_user= strdup(pwe->pw_name);
  else
    root_user= strdup("root");
  endpwent();
  /* seed the random number generator */
  {
    struct timeval now;
    int res= gettimeofday(&now,NULL);
    if (res==-1)
    {
      PRINT2("gettimeofday: %s", strerror(errno));
      srandom(time(NULL));
    }
    else
      srandom(now.tv_sec+now.tv_usec);
  }
  /* interpret and validate commandline parameters */
  tmpfilename[0]= 0;
  {
    int arg= 1;
    char*login_name= getlogin();/* temporary variable pointing to static data*/
    if (login_name==NULL)
    {
      login_name= "root";
    }
    if (argc<2)
    {
      PRINT("secretfile not provided\n");
      {
        PRINT4("Usage: %s [-u user ] [ -f passwordfile ] secretfile\n"
                "\nnote: user defaults to %s, passwordfile defaults to %s\n",
                argv[0],login_name,DEFAULT_PASSWORDFILE);
        ABORT;
      }
    }
    while (arg<argc)
    {
      if (strcmp(USER_FLAG,argv[arg])==0&&arg+1<argc)
        username= strdup(argv[++arg]);
      if (strcmp(FILE_FLAG,argv[arg])==0&&arg+1<argc)
        password_filename= strdup(argv[++arg]);
      else if (argv[arg][0]==0||argv[arg][0]=='-')
      {
        PRINT4("Usage: %s [-u user ] [ -f passwordfile ] secretfile\n"
                "\nnote: user defaults to %s, passwordfile defaults to %s\n",
                argv[0],login_name,DEFAULT_PASSWORDFILE);
        ABORT;
      }
      else
        break;
      ++arg;
    }
    if (arg!=argc-1) /* no filename given for the secret file */
    {
      PRINT("secretfile not provided\n");
      {
        PRINT4("Usage: %s [-u user ] [ -f passwordfile ] secretfile\n"
                "\nnote: user defaults to %s, passwordfile defaults to %s\n",
                argv[0],login_name,DEFAULT_PASSWORDFILE);
        ABORT;
      }
    }
    else
      secret_filename= strdup(argv[arg++]);
    
    /* apply defaults where optional parameters were not supplied */
    if (username==NULL&&login_name)
      username= strdup(login_name);
    if (password_filename==NULL)
      password_filename= strdup(DEFAULT_PASSWORDFILE);
    if (username==NULL||password_filename==NULL)
    {
      PRINT("unrecoverable error (memory?) during initialisation\n");
      ABORT;
    }
  }
  /* only the system administrator can change other peoples password */
  {
    char*current_user= getlogin();
    if (getuid()!=0&&strcmp(username,current_user)!=0)
    {
      PRINT2("Error: only %s can use this program\n",root_user);
      ABORT;
    }
  }
  /* load the new password from the first line of the secret file */
  {
    FILE*secretfile= fopen(secret_filename,"r");
    char*linebuf= NULL;
    size_t len;
    if (!secretfile)
    {
      perror("fopen");
      PRINT2("opening file %s\n",secret_filename);
      ABORT;
    }
    len= getline(&linebuf,&len,secretfile);
    if (len>=0)
    {
      if (
        TRUE
      )
      {
        if (len> 0&&linebuf[len-1]=='\n')linebuf[len-1]= 0;
        new_password= linebuf;
      }
      else
      {
        PRINT("please choose a more complex password");
        ABORT;
      }
    }
    else
    {
      PRINT2("getline: %s",strerror(errno));
      ABORT;
    }
    fclose(secretfile);
  }
  /* change the password*/
  pwf= fopen(password_filename,"r+");
  if (!pwf)
  {
    PRINT2("fopen: %s", strerror(errno));
    PRINT2("opening file %s for read and write\n",password_filename);
    ABORT;
  }
  /* lock the password file for writing */
  {
    struct flock lock;
    int trys= NUM_LOCK_TRYS;
    int res;
    lock.l_type= F_WRLCK;
    lock.l_whence= SEEK_SET;
    lock.l_start= 0;
    lock.l_len= 0;
    res= -1;
    while (res==-1&&trys)
    {
      res= fcntl(fileno(pwf),F_SETLK,&lock);
      if (res==-1&&errno!=EAGAIN)break;
      if (trys--<NUM_LOCK_TRYS)sleep(1);
    }
    if (res==-1)
    {
      PRINT("fcntl()[set lock]");
      PRINT("failed to obtain password file lock\n");
#ifdef DEBUG
      PRINT3("%s, %d\n",__FILE__,__LINE__);
#endif
      fclose(pwf);
      pwf= NULL;
      ABORT;
    }
  }
  /* loop through the file, searching for the user id to be changes */
  {
    long file_pos= 0;
    while ((pwe= fgetpwent(pwf))!=NULL)
    {
      if (pwe->pw_name&&strcmp(pwe->pw_name,username)==0)
      {
        found_pos= file_pos;
        if (pwe->pw_passwd)
        {
          int pw_len= strlen(pwe->pw_passwd);
          if (pw_len==1)
          {
            PRINT("Shadow passwords seem to be in use, aborting\n");
            ABORT;
          }
          else if (pwe->pw_passwd[0]!=0&&pwe->pw_passwd[0]!='$'&&pw_len!=13)
          {
            PRINT("An unknown password scheme appears to be active,"
                    " aborting\n");
            ABORT;
          }
          if (pwe->pw_passwd&&pw_len> 2)
          {
            original_pw_len= pw_len;
            if (sample_password)free(sample_password);
            sample_password= strdup(pwe->pw_passwd);
          }
        }
        if (sample_password)break;
      }
      if (!sample_password&&pwe->pw_passwd&&strlen(pwe->pw_passwd)> 2)
      {
        sample_password= strdup(pwe->pw_passwd);
        if (found_pos!=-1)break;
      }
      file_pos= ftell(pwf);
    }
    if (sample_password==NULL)
      sample_password= strdup(TEMPLATE_PASSWORD);
  }
  /* user entry was not found, abort */
  if (found_pos==-1)
    FINISH;
  
  /* generate a salt which matches the current encryption method */
  {
    char*salt= malloc(strlen(sample_password)+1);
    int start;
    int count;
    strcpy(salt,sample_password);
    if (salt[0]!='$')
    {
      start= 0;
      count= 2;
    }
    else
    {
      start= 1;
      while (salt[start]&&salt[start]!='$')
        start++;
      if (salt[start])start++;
      count= start;
      while (salt[count]&&salt[count]!='$')
        count++;
      count-= start;
    }
    {
      int i;
      int maxidx= start+count;
      for (i= start;i<maxidx;i++)
      {
        int idx= random()%SALT_SET_SIZE;
        salt[i]= SALT_CHARSET[idx];
      }
    }
    
    /* encrypt the password using the salt */
    encrypted_password= crypt(new_password,salt);
    free(salt);
    if (encrypted_password)
    {
      int expected_length;
      if (original_pw_len==0)
        expected_length= strlen(sample_password);
      else
        expected_length= original_pw_len;
#ifdef DEBUG
      PRINT3("old password: %s (%d)\n",pwe->pw_passwd,strlen(pwe->pw_passwd));
      PRINT3("new password: %s (%d)\n",encrypted_password,strlen(encrypted_password));
#endif
      if (expected_length!=strlen(encrypted_password))
      {
        PRINT3("password lengths differ (%d vs %d). password update failed.\n",
               expected_length,(int)strlen(encrypted_password));
        ABORT;
      }
    }
  }
  if (
    (original_pw_len> 0)
  )
  /* if the user entry had a password, write the new password to the file, in place
   */
  {
    struct passwd*pwe;
    fseek(pwf,found_pos,SEEK_SET);
    pwe= fgetpwent(pwf);
    if (!pwe)
    {
      PRINT2("fgetpwent: %s", strerror(errno));
      ABORT;
    }
    fseek(pwf,found_pos,SEEK_SET);
    if (ftell(pwf)==found_pos&&strcmp(pwe->pw_name,username)==0)
    {
      if (writePasswordToFile(pwe,encrypted_password,pwf)==-1) {
        PRINT2("putpwent: %s", strerror(errno));
        ABORT;
      }
    }
    else
      PRINT("crosscheck failed. file is not in the correct position\n");
  }
  else
  /* the user entry did not have a password, copy all entries to a new password file and
     update the password on the user's entry as we go
  */
  {
    sprintf(tmpfilename,"/tmp/pwtmp.%04xd",getpid());
    tmpfile= fopen(tmpfilename,"w");
    if (!tmpfile)
    {
      PRINT2("Failed to create temporary file %s\n",tmpfilename);
      exit(1);
    }
    if ((fstat(fileno(pwf),&fsi)==-1))
    {
      PRINT2("fstat: %s", strerror(errno));
      ABORT;
    }
    if (fchown(fileno(tmpfile),fsi.st_uid,fsi.st_gid)==-1)
    {
      PRINT2("fchown: %s", strerror(errno));
      ABORT;
    }
    if (fchmod(fileno(tmpfile),fsi.st_mode)==-1)
    {
      PRINT2("fchmod: %s", strerror(errno));
      ABORT;
    }
    rewind(pwf);
    while ((pwe= fgetpwent(pwf)))
    {
      if (pwe->pw_name&&strcmp(pwe->pw_name,username)==0)
      {
        if (writePasswordToFile(pwe,encrypted_password,tmpfile)==-1) {
          PRINT2("putpwent: %s", strerror(errno));
          ABORT;
        }
      }
      else
        putpwent(pwe,tmpfile);
    }
    {
      int res= rename(tmpfilename,password_filename);
      if (res==-1)
      {
        PRINT2("rename: %s", strerror(errno));
        ABORT;
      }
    }
  }
finished:

exit_program:
  result= buf;
  if (root_user)
  {
    free(root_user);
    root_user= NULL;
  }
  if (username)
  {
    free(username);
    username= NULL;
  }
  if (password_filename)
  {
    free(password_filename);
    password_filename= NULL;
  }
  if (secret_filename)
  {
    free(secret_filename);
    secret_filename= NULL;
  }
  if (new_password)
  {
    free(new_password);
    new_password= NULL;
  }
  /* release the lock on the password file */
  {
    if (pwf)
    {
      struct flock lock;
      int res;
      lock.l_type= F_UNLCK;
      lock.l_whence= SEEK_SET;
      lock.l_start= 0;
      lock.l_len= 0;
      res= fcntl(fileno(pwf),F_SETLK,&lock);
      if (res==-1)
      {
        PRINT2("fcntl: %s", strerror(errno));
#ifdef DEBUG
        PRINT3("%s, %d\n",__FILE__,__LINE__);
#endif
        ABORT;
      }
      fclose(pwf);
      pwf= NULL;
    }
  }
  if (tmpfile)
  {
    fclose(tmpfile);
    tmpfile= NULL;
  }
  return result;
}

#ifdef TESTING_PLUGIN
int main (int argc, const char * argv[]) {
	char *buf = NULL;
    symbol_table variables = init_symbol_table();
	buf = plugin_func(variables, buf, 0, argc, argv);
	if (buf) printf("result: %s\n", buf);
    printf("symbols:\n");
    dump_symbol_table(variables);
    free_symbol_table(variables);
    fflush(stdout);
    return 0;
}
#endif

