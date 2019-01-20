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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>


#include "options.h"
#include "symboltable.h"
#include "property.h"
#include "plugin.h"
#include "buffers.h"

/*
  properties used
	HOST
	PORT
	RESPONSE - if present, the read only continues until this string appears collected data

 */

#define MESSAGE_BUFFER msg_buf
#define ADDTOBUFFER  if (newmsg) { MESSAGE_BUFFER = append_buffer(MESSAGE_BUFFER, newmsg); }
#define PRINT(msg) { char *newmsg = NULL; asprintf(&newmsg, (msg) ); ADDTOBUFFER }
#define PRINT2(msg,a) { char *newmsg = NULL; asprintf(&newmsg, (msg),(a) ); ADDTOBUFFER }
#define PRINT3(msg,a,b) { char *newmsg = NULL; asprintf(&newmsg, (msg),(a),(b) ); ADDTOBUFFER }
#define PRINT4(msg,a,b,c) { char *newmsg = NULL; asprintf(&newmsg, (msg),(a),(b),(c) ); ADDTOBUFFER }

/* display a string which may contain non-printable characters */
static void display_string(const char *str)
{
  const char *p = str;
  while (*p)
  {
    if (*p>=' ' && *p<127)
      printf("%c", *p);
    else if (*p == '\r')
      printf("\\r");
    else if (*p == '\n')
      printf("\\n");
    else if (*p == '\t')
      printf("\\t");
    else
      printf("\\%o", *p);
    p++;
  }
}

/* struct message_data is used to build the fields of a message from
  the entries of a property group. See properties.each_property for details
  on how the iteration works.
 */

struct message_data
{
  const char *property_group;
  symbol_table properties;
  char *buffer;
  const char *field_header;
  const char *field_separator;
  const char *field_tail;
  const char *message_header;
  const char *message_separator;
  const char *message_tail;
};

static void build_message(const char *property_group, const char *name,
                          const char *value, void *user_data)
{
  struct message_data *data = (struct message_data *)user_data;
  char *local_buf;
  asprintf(&local_buf, "%s%s%s%s%s", data->field_header, name, data->field_separator, value, data->field_tail);
  data->buffer = append_buffer(data->buffer, local_buf);
}

#define ABORT goto error_exit

#ifdef BUILD_PLUGIN
EXPORT
#else
static
#endif
char *plugin_func(symbol_table variables, char *user_buf, int user_buflen, int argc, const char *argv[])
{
  char *MESSAGE_BUFFER = NULL; /* used to catch error messages */
  char *buf = user_buf;
  struct message_data message;
  int buflen = user_buflen;
  const char *property_group;
  const char *command;
  int sock;

  memset(&message, 0, sizeof(struct message_data));

  if (argc > 2)
  {
    const char *lookup;
    property_group = argv[1];
    command = argv[2];
    lookup = get_string_value(variables, command);
    if (lookup)
      command = lookup;

    if (verbose())
      printf("connection properties: %s\n", property_group);
  }

  /* parse and check command parameters */
  if (argc==3 && strcmp(argv[2], "CLOSE") == 0)
  {
    sock = lookup_int_property(variables, property_group, "SOCKET", 0);
    if (sock)
    {
      if (verbose())
        printf("Closing persistent connection\n");
      shutdown(sock, SHUT_RDWR);
      close(sock);
      set_int_property(variables, property_group, "SOCKET", 0);
    }
    ABORT;
  }
  else if (argc==4 && strcmp(argv[2], "USING") == 0)
  {
    message.property_group = argv[3];
  }
  else if (argc != 3)
  {
    PRINT3("usage: %s property_name request \n" 
      "%s property_name USING request_template", argv[0], argv[0]);
    ABORT;
  }
  else
  {
    message.property_group = NULL;
  }
  /*set_verbose(1); set this to 0 to stop verbose output */

  /* send request and receive response */
  {
    int err; /* used to detect stdlibc errors */
    int num_bytes; /* utility variable for counting bytes read and written and some string calculations */
    char *dest_address = "127.0.0.1";  /* default values. these may be overridden by properties */
    int dest_port = 4000;
    int timeout = 1;
    const char *host_name;
    const char *response_terminator;
    fd_set read_ready;
    fd_set write_ready;
    fd_set stream_error;
    int is_persistent = 0;

    is_persistent = lookup_boolean_property(variables, property_group, "PERSISTENT", 0);
    host_name = lookup_string_property(variables, property_group, "HOST", dest_address);

    dest_port = lookup_int_property(variables, property_group, "PORT", dest_port);
    if (verbose())
      printf("using host: %s and port %d\n", host_name, dest_port);

    timeout = lookup_int_property(variables, property_group, "TIMEOUT", timeout);
    if (verbose())
      printf("using timeout %d\n", timeout);

    response_terminator = lookup_string_property(variables, property_group, "RESPONSE_TERMINATOR", NULL);
    if (verbose() && response_terminator)
    {
      printf("reading until reponse: ");
      display_string(response_terminator);
      printf("\n");
    }

    /*
     DEFINE PERSISTENT = YES; # when connected the value TESTSOCK_SOCKET will be nonzero
     */
    /* look for an existing socket for the connection and use it if we find one */
    sock = lookup_int_property(variables, property_group, "SOCKET", 0);
    if (sock == 0)
    {
      char *response = NULL;
      int flags;
      struct sockaddr_in address;

      set_int_property(variables, property_group, "SOCKET", 0);
      if ( inet_aton(host_name, &(address.sin_addr)) != INADDR_NONE
           && address.sin_addr.s_addr != 0 )
      {
        /* the request is already an IP address */
        address.sin_family = AF_INET;
        if (verbose()) printf("host: %s has address %s (%x)\n", host_name,
                                inet_ntoa(address.sin_addr), address.sin_addr.s_addr);
      }
      else /* number provided was not an ip address, lookup the name...*/
      {
        int retries = 2;
        while (retries > 0)
        {
          /*
              Find host details. This code may block.
           */

          struct hostent *host_entry;

          memset(&address, 0, sizeof(struct sockaddr_in));

          errno = 0;
          host_entry = gethostbyname(host_name);
          if (errno == EINTR)   /* timeout */
          {
            asprintf(&response, "gethostbyname: timeout %s (%d)", strerror(errno), errno);
            return response;
          }
          else
          {
            if (host_entry)
            {
              memmove(&address.sin_addr, host_entry->h_addr, host_entry->h_length);
              /*len = dest_host_entry->h_length;*/
              address.sin_family = host_entry->h_addrtype;
              break;
            }
            else
            {
              if (h_errno == HOST_NOT_FOUND ) /* no such host */
              {
                if (verbose()) printf("host: %s not found\n", host_name);
                retries = 0;
              }
              else if (h_errno == TRY_AGAIN ) /* request timed out, try again maybe */
              {
                if (verbose()) printf("temporary failure looking up host %s, trying again\n", host_name);
                usleep(500000L); /* little sleep and try again*/
                retries--;
              }
              else if (h_errno == NO_RECOVERY ) /* unknown error */
              {
                if (verbose()) printf("error %s looking up host: %s\n", hstrerror(h_errno), host_name);
                retries = 0;
              }
              else if (h_errno == NO_DATA ) /* known host with no ip address */
              {
                if (verbose()) printf("no ip address for host: %s (%s)\n", host_name, hstrerror(h_errno));
                retries = 0;
              }
            }
          }
        }
        if (address.sin_addr.s_addr == 0) /* no host found */
        {
          asprintf(&response, "failed to resolve host name %s, aborting.\n", host_name);
          return response;
        }
      }
      address.sin_port = htons(dest_port);
      if (verbose())
        printf("host: %s (%x)\n", inet_ntoa(address.sin_addr), address.sin_addr.s_addr);

      sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
      if (sock == -1)
      {
        asprintf(&response, "socket: %s (%d)", strerror(errno), errno);
        return response;
      }

      if ( (flags =  fcntl(sock, F_GETFL, NULL)) < 0)
      {
        asprintf(&response, "fcntl F_GETFL: %s (%d)", strerror(errno), errno);
        close(sock);
        return response;
      }
      if ( fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0)
      {
        asprintf(&response, "fcntl F_SETFL: %s (%d)", strerror(errno), errno);
        close(sock);
        return response;
      }

      err = connect(sock, (struct sockaddr *)&address, sizeof(struct sockaddr_in));
      if (err == -1 && errno != EINPROGRESS)
      {
        asprintf(&response, "connect: %s (%d)", strerror(errno), errno);
        close(sock);
        return response;
      }
      else
      {
        struct timeval select_timeout;
        select_timeout.tv_sec = timeout;
        select_timeout.tv_usec = 0;

        FD_ZERO(&write_ready);
        FD_SET(sock, &write_ready);
        err = select(sock+1, NULL, &write_ready, NULL, &select_timeout);
        if (err == -1)
        {
          asprintf(&response, "select: %s (%d)", strerror(errno), errno);
          shutdown(sock, SHUT_RDWR);
          close(sock);
          return response;
        }
        else if (err == 0)
        {
          shutdown(sock, SHUT_RDWR);
          close(sock);
          return strdup("connection timeout");
        }
      }

      if ( fcntl(sock, F_SETFL, flags) < 0)
      {
        PRINT3("fcntl F_SETFL (clear O_NONBLOCK): %s (%d)\n", strerror(errno), errno);
        shutdown(sock, SHUT_RDWR);
        close(sock);
        set_int_property(variables, property_group, "SOCKET", 0);
        return NULL;
      }

      set_int_property(variables, property_group, "SOCKET", sock);
    }

    if (message.property_group)
    {
      message.properties = collect_properties(variables, property_group);
      message.buffer = NULL;
      /*
      fprintf(stderr, "\nMessage Properties: ");
      dump_symbol_table(message.properties);
      */
      message.message_header = lookup_string_property(message.properties, property_group, "MESSAGE_HEADER", "");
      message.message_separator = lookup_string_property(message.properties, property_group, "MESSAGE_SEPARATOR", "\r\n");
      message.message_tail = lookup_string_property(message.properties, property_group, "MESSAGE_TERMINATOR", "");
      message.field_header = lookup_string_property(message.properties, property_group, "FIELD_HEADER", "");
      message.field_separator = lookup_string_property(message.properties, property_group, "FIELD_SEPARATOR", "|");
      message.field_tail = lookup_string_property(message.properties, property_group, "FIELD_TERMINATOR", "");
    }

    FD_ZERO(&read_ready);
    FD_SET(sock, &read_ready);

    /* if we are using a message template, collect the fields for that property group and
        build a message */
    if (message.property_group)
    {
      symbol_table fields = collect_properties(variables, message.property_group);
      /*
      fprintf(stderr, "Message Fields:");
      dump_symbol_table(fields);
      */
      each_property(fields, message.property_group, build_message, &message);
      free_symbol_table(fields);
      if (message.buffer == NULL)
      {
        fprintf(stderr, "Failed to find property list %s. Aborting send\n", message.property_group );
        if (!is_persistent)
        {
          err = shutdown(sock, SHUT_RDWR);
          if (err <0)
            PRINT3("shutdown socket %s (%d)\n", strerror(errno), errno);
        }
        if (message.properties)
        {
          free_symbol_table(message.properties);
          free((char *)command);
        }
        return NULL;
      }
      /* remove the extra field separator on the last field */
      {
        int nbytes = strlen(message.buffer);
        if (nbytes > strlen(message.field_separator))
          message.buffer[nbytes - strlen(message.field_separator)] = 0;
      }
      message.buffer = append_buffer(message.buffer, strdup(message.message_tail));
      command = message.buffer;
    }
    if (verbose())
    {
      printf("writing %s\n", command);
      fflush(stdout);
    }
    num_bytes = write(sock, command, strlen(command));
    if (num_bytes == -1)
    {
      int err;
      PRINT3("write: %s (%d)\n", strerror(errno), errno);
      err = shutdown(sock, SHUT_RDWR);
      if (err != 0) PRINT3("shutdown: %s (%d)\n", strerror(errno), errno);
      err = close(sock);
      if (err != 0) PRINT3("shutdown: %s (%d)\n", strerror(errno), errno);
      set_int_property(variables, property_group, "SOCKET", 0);
      if (message.properties)
      {
        free_symbol_table(message.properties);
        free((char *)command);
      }
      return NULL;
    }
    if (!is_persistent)
    {
      err = shutdown(sock, SHUT_WR);
      if (err <0)
        PRINT3("shutdown socket writer: %s (%d)\n", strerror(errno), errno);
    }

    if (num_bytes != strlen(command))
    {
      fprintf(stderr, "warning: write() to socket failed to write all bytes\n");
    }
    if (!buf)
    {
      if (!buflen)
        buflen = lookup_int_property(variables, argv[0], "MAXBUFSIZE", 0);
      if (!buflen)
        buflen = 5000; /*TBD*/
      buf = malloc(buflen);
    }

    {
      int read_bytes = 0;
      int found_terminator = 0;
      int buf_size = buflen;
      char *tmp_buf = malloc(buf_size);
      time_t start_time = time(NULL);
      time_t now = start_time;
      struct timeval select_timeout;
      select_timeout.tv_sec = timeout;
      select_timeout.tv_usec = 0;
      num_bytes = 0;
      err = select(sock+1, &read_ready, NULL, &stream_error, &select_timeout);
      while (err >= 0 && !found_terminator && now-timeout-1 < start_time)
      {
        if (FD_ISSET(sock, &read_ready) )
        {
          read_bytes = read(sock, tmp_buf, buf_size-1); /* leaving space to null terminate the buffer */
          if (read_bytes < 0)
            err = read_bytes;
          else if (read_bytes > 0)
          {
            if (verbose())
            {
              tmp_buf[read_bytes] = 0;
              printf("socket_script read: %s (%d bytes)\n", tmp_buf, read_bytes);
            }
            /* copy the most recent buf_size bytes in the temporary buffer to our processing buffer */
            num_bytes += read_bytes;
            if (num_bytes >= buf_size)
            {
              /* slide bytes in our buffer over to leave space for the new data
                  so we have to slide by num_bytes-buf_size
              */
              memmove(buf, buf + num_bytes-buf_size+1, num_bytes-buf_size-1);
              num_bytes = buf_size-1;
            }
            /* note. buf + num_bytes is the address of the last byte of our final buffer */
            memcpy(buf + (num_bytes - read_bytes), tmp_buf, read_bytes);
            if (found_terminator)
              found_terminator = (strstr(buf, response_terminator) != NULL);
          }
          else
            break;
        }
        if (!found_terminator)
          err = select(sock+1, &read_ready, NULL, &stream_error, &select_timeout);
        now = time(NULL);
      }
      free(tmp_buf);
      if (err < 0)
      {
        /* with socket operations, we treat any error as fatal, failing this run of the condition */
        /* note that reading no data is not regarded as an error */
        PRINT3("read: %s (%d)\n", strerror(errno), errno);
        free(buf);
        buf = NULL;
        err = shutdown(sock, SHUT_RDWR);
        if (err != 0) PRINT3("shutdown: %s (%d)\n", strerror(errno), errno);
        err = close(sock);
        if (err != 0) PRINT3("shutdown: %s (%d)\n", strerror(errno), errno);
        set_int_property(variables, property_group, "SOCKET", 0);
        if (message.properties)
        {
          free_symbol_table(message.properties);
          free((char *)command);
        }
        return NULL;
      }
      buf[num_bytes] = 0;

      /* the data may be in a key-value form, attempt to analyse it if the user wants us to */
      if (num_bytes && message.property_group && lookup_boolean_property(variables, property_group, "INTERPRET_RESULT", 0))
      {
        /* Note, this code was the template for interpret_text() in splitstring.c  That
            version should now be called from here instead. TBD
         */
        char *next = buf;
        char *fldsep = strstr(next, message.field_separator);
        char *fldterm = strstr(next, message.field_tail);
        int seplen = strlen(message.field_separator);
        int termlen = strlen(message.field_tail);
        /* if there are no more field terminators, we are done */
        while (fldterm)
        {
          if (!fldsep) break;

          /* normally a field terminator wouldn't appear before the next separator
          	but this can happen if not all sections of the output are in the
          	key-value form (eg if the response has header text)
           */
          if (fldterm - next > fldsep - next)
          {
            int keylen = fldsep - next;
            int valuelen = fldterm - fldsep - seplen;
            char *key = malloc(keylen + 1);
            char *value = malloc(valuelen + 1);
            memmove(key, next, keylen);
            key[keylen] = 0;
            memmove(value, fldsep + seplen, valuelen);
            value[valuelen] = 0;
            if (verbose())
              fprintf(stderr, "returning result parameter: %s: %s\n", key, value);
            set_string_property(variables, "RESULT", key, value);
            free(key);
            free(value);
          }
          next = fldterm + termlen;
          fldsep = strstr(next, message.field_separator);
          fldterm = strstr(next, message.field_tail);
        }
      }
    }

    /*is_persistent = lookup_boolean_property(variables, property_group, "PERSISTENT", 0);*/
    if (num_bytes == 0 || !is_persistent)
    {
      if (num_bytes == 0)
        fprintf(stderr, "Connection lost\n");
      else
        fprintf(stderr, "Closing non-persistent connection\n");
      close(sock);
      set_int_property(variables, property_group, "SOCKET", 0);
    }
    if (verbose())
      printf("Buffer: %s\n", buf);
  }
  if (message.property_group)
  {
    free_symbol_table(message.properties);
    free((char *)command);
  }
  return buf;
error_exit:
  if (MESSAGE_BUFFER != NULL)
  {
    printf("%s\n", MESSAGE_BUFFER);
    free(MESSAGE_BUFFER);
  }
  return NULL;
}

int socket_script(symbol_table variables, char *buf, int buflen, int argc, const char *argv[])
{
  return (plugin_func(variables, buf, buflen, argc, argv)) ? 0 : -1;
}

#ifdef TESTING_PLUGIN
int main(int argc, const char *argv[])
{
  int buflen = 500;
  char *buf = malloc(buflen);
  char *res;
  symbol_table symbols = init_symbol_table();

  set_verbose(1); /* set this to 0 to stop verbose output */

  set_string_value(symbols, "TEST_HOST", "127.0.0.1");
  set_integer_value(symbols, "TEST_PORT", 80);

  res = plugin_func(symbols, buf, buflen, argc, argv);

  if (!res) return 1;
  printf("%s\n", res);

  free_symbol_table(symbols);
  free(buf);
  return 0;
}
#endif

