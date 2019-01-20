
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
/*

This program uses libcurl which is copyrighted:

COPYRIGHT AND PERMISSION NOTICE
 
Copyright (c) 1996 - 2009, Daniel Stenberg, <daniel@haxx.se>.
 
All rights reserved.
 
Permission to use, copy, modify, and distribute this software for any purpose
with or without fee is hereby granted, provided that the above copyright
notice and this permission notice appear in all copies.
 
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS. IN
NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
OR OTHER DEALINGS IN THE SOFTWARE.
 
Except as contained in this notice, the name of a copyright holder shall not
be used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization of the copyright holder.

*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "plugin.h"

struct buffer_info {
    size_t size; /* allocated size */
    size_t len;  /* amount of data */
    char *buffer; /* null terminated */
};

size_t receive_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
    int n = size * nmemb;
    struct buffer_info *bufp = (struct buffer_info *) userp;
    if (!bufp)
 	return 0;
    if (bufp->len + n + 1 > bufp->size)
    {
         int newsize = bufp->len + n + 1;
         char *newbuf = malloc(newsize);
         memmove(newbuf, bufp->buffer, bufp->len);
	     memmove(newbuf + bufp->len, buffer, n);
         newbuf[bufp->len + size*nmemb] = 0;
         if (bufp->buffer)
             free(bufp->buffer);
         bufp->buffer = newbuf;
         bufp->size = newsize;
         bufp->len = newsize-1;
    }
    else
    {
         memmove(bufp->buffer + bufp->len, buffer, n);
         bufp->len += n;
         if (bufp->size > bufp->len)
              bufp->buffer[bufp->len] = 0;
    }
    return size * nmemb;
}

EXPORT
char *plugin_func(symbol_table variables, char *plugin_buffer, int buflen, int argc, char *argv[])
{
    CURLcode result = 0;
	CURLcode curl;
	CURL *curl_handle;
    char *data="";
    char *base_url = "http://192.168.2.199/";
    struct buffer_info *buf = malloc(sizeof(struct buffer_info));
  
    curl = curl_global_init(CURL_GLOBAL_NOTHING);
    if (curl != 0)
    {
		fprintf(stderr, "error %d initialising cURL\n", curl);
		return NULL;
    }
    curl_handle = curl_easy_init();

    if (!buf) {
        fprintf(stderr, "out of memory allocating buffer\n");
		return plugin_buffer;
    }
    buf->size = 0;
    buf->len = 0;
    buf->buffer = NULL;

    if (argc > 1)
	base_url = argv[1];
    if (argc > 2)
        data = argv[2];
    
    if (data && strlen(data))
		curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(curl_handle, CURLOPT_URL, base_url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, receive_data);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, buf);


    result = curl_easy_perform(curl_handle);
    if (result != 0)
		fprintf(stderr, "Error %d received from curl\n", result);
    if (buf->buffer)
    {
		if (plugin_buffer)
		{
			int n = buf->len;
			if (n >= buflen) n = buflen-1;
			strncpy(plugin_buffer, buf->buffer, n);
			plugin_buffer[n] = 0;
			free(buf->buffer);
		}
		else
			plugin_buffer = buf->buffer;
	}
	else if (plugin_buffer)
		plugin_buffer[0] = 0;
    free(buf);
    curl_easy_cleanup(curl_handle);

    return plugin_buffer;
}


#ifdef TEST_PLUGIN
int main(int argc, char *argv[])
{
	symbol_table variables = init_symbol_table();
	char *buf = malloc(1000);
	char *res = plugin_func(variables, buf, 1000, argc, argv);
	free_symbol_table(variables);
	if (res)
	{
		printf("%s", res);
		free(res);
		return 1;
	}
	return 0;
}
#endif
