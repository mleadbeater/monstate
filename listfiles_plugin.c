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
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "regular_expressions.h"
#include "symboltable.h"
#include "plugin.h"
#include "buffers.h"
#include "property.h"

struct traversal_info {
	char *current_path_prefix;
	char *output;
	int buffer_size;
	int chunk_size;
	char *buffer_end;
    const char *match;
    rexp_info *rexp;
};

static int show_hidden = 1;
static int show_link_targets = 0;
static int show_file_size = 0;
static int show_dir_header = 0;

typedef int (handler)(struct traversal_info *, struct dirent *dp, struct stat *fs);
typedef int (dir_handler)(struct traversal_info *, char *dirname);

void process_dir(struct traversal_info *, const char *path, handler *handle, dir_handler *handle_dir);

int get_file_stat(const char *filepath, struct stat *fs)
{
	int res = lstat(filepath, fs);
	if (res == -1) {
		perror(filepath);
		return res;
	}

	if ((fs->st_mode & S_IFMT) != S_IFLNK) {
		res = stat(filepath, fs);
		if (res == -1) {
			perror("stat");
			return res;
		}
	}
    return res;
}

char *save_current_path()
{
	int pathsize = 1024;
	char *cwd = malloc(pathsize);
	while ( (cwd = getcwd(cwd, pathsize)) == NULL && errno == ERANGE)
	{
	    free(cwd);
	    pathsize += 1024;
	    cwd = malloc(pathsize);
	}
	if (cwd == NULL)
		fprintf(stderr, "unable to get currrent directory path");
	return cwd;
}

struct traversal_info *new_traversal()
{
	struct traversal_info *ti = malloc(sizeof(struct traversal_info));
	ti->current_path_prefix = NULL;
	ti->output = strdup("");
    ti->match = NULL;
    ti->rexp = NULL;
	return ti;
}


struct traversal_info *dup_traversal(struct traversal_info *orig)
{
	struct traversal_info *ti = malloc(sizeof(struct traversal_info));
	memcpy(ti, orig, sizeof(struct traversal_info));
    if (ti->match)
        ti->rexp = create_pattern(ti->match);
    else
        ti->rexp = NULL;
	return ti;
}

struct traversal_info *clone_traversal(struct traversal_info *orig)
{
    /* warning. this does not clone the rexp field correctly */
	struct traversal_info *ti = malloc(sizeof(struct traversal_info));
	memset(ti, 0, sizeof(struct traversal_info));
	if (orig->current_path_prefix)
		ti->current_path_prefix = strdup(orig->current_path_prefix);
	if (orig->output)
		ti->output = strdup(orig->output);
    if (orig->match) {
        ti->match = orig->match;
        ti->rexp = create_pattern(ti->match);
    }
	return ti;
}

void release_traversal(struct traversal_info *ti)
{
	if (ti->current_path_prefix)
		free(ti->current_path_prefix);
	if (ti->output)
		free(ti->output);
    /* match is not owned by the traversal structure. do not free it here */
    if (ti->rexp)
        release_pattern(ti->rexp);
	free(ti);
}

void set_traversal_buffer()
{
	
}

/* display directory headers with total usage information.

   if the traversal_info structure is not null, output will be appended to the 
   traversal buffer, otherwise it is printed to stdout.
*/
static int dir_header(struct traversal_info *ti, char *dirname) 
{
	char *prefix = NULL;
	char *buf = strdup("");
	char *new_buf;
	if (ti)
		prefix = ti->current_path_prefix;
	if (prefix == NULL) prefix=".";

	asprintf(&new_buf, "%s------- %s/%s -------\n", buf, prefix, dirname);
	buf = replace_buffer(buf, new_buf);
	if (buf)
	{
		if (ti)
			ti->output = append_buffer(ti->output, buf);
		else 
		{
			printf("%s", buf);
			free(buf);
		}
	}
	return 0;
}

static int show(struct traversal_info *ti, struct dirent *dp, struct stat *fs) 
{
	
	char *prefix = NULL; 
	char *fname = dp->d_name;
	char *buf = NULL;
	char *new_buf;
	if (!show_hidden && fname[0] == '.') return 0; /* support shell conventions for hidden files */

	if (strcmp(fname, ".") == 0 || strcmp(fname, "..") == 0)     
        return 0; /* do not list . and .. */
    buf = strdup("");
	
	if (ti) {
		prefix = ti->current_path_prefix;
        /* do nothing if a pattern is supplied and the filename does not match */
        if (ti->rexp) {
            if ( execute_pattern(ti->rexp, fname) != 0 ) return 0;
        }
    }
	
    if (show_file_size)
	{
		asprintf(&new_buf, "%s%ld %ld ", buf, (long)fs->st_blocks / 2, (long)fs->st_size);
		buf = replace_buffer(buf, new_buf);
	}

	if (show_link_targets && (fs->st_mode & S_IFMT) == S_IFLNK)
	{
		char *link_name;
		int offset;
		int len;
        link_name = malloc(strlen(dp->d_name) + strlen(" -> ") + fs->st_size + 1);
        offset = sprintf(link_name, "%s -> ", dp->d_name);
        len = readlink(dp->d_name, link_name + offset, fs->st_size);
        if (len > 0) link_name[len + offset] = 0;

		if (prefix)
			asprintf(&new_buf, "%s%s/%s\n", buf, prefix, link_name);
		else
			asprintf(&new_buf, "%s%s\n", buf, link_name);
		buf = replace_buffer(buf, new_buf);
		free(link_name);
	}
	else 
	{
		if (prefix)
			asprintf(&new_buf, "%s%s/%s\n", buf, prefix, dp->d_name);
		else
			asprintf(&new_buf, "%s%s\n", buf, dp->d_name);
		buf = replace_buffer(buf, new_buf);
	}
	if (buf)
	{
		
		if (ti)
			ti->output = append_buffer(ti->output, buf);
		else {
			printf("%s", buf);
			free(buf);
		}
	}
	return 0;
}

/* directory traversal */
void process_dir(struct traversal_info *orig_ti, const char *path, handler *handle, dir_handler *handle_dir)
{
	int list_chunk_size = 64;
	DIR *saved_wd = opendir(".");
	int list_size = 0;
	int num_entries = 0;
	char **dirlist = NULL;
	char *prefix = NULL;
	int res; /* general purpose variable to catch errno values */
	struct traversal_info *ti = orig_ti;
	struct stat fs;
	
	if (!ti)
		ti = new_traversal();
    else if (orig_ti->match) {
        ti->match = orig_ti->match;
        ti->rexp = create_pattern(ti->match);
    }

	prefix = ti->current_path_prefix;

	/* check if we have a file and if so, just handle it and return*/
	res = get_file_stat(path, &fs);
	if (res == -1)
	{
		/* unexpected error here (TBD) skip this directory */
		if (saved_wd)
		{
			closedir(saved_wd);
			saved_wd = NULL;
		}
	}
	if (( fs.st_mode & S_IFMT)!= S_IFDIR )
	{
		/* dummy up a dirent structure so we can use our handler to display the file details */
		struct dirent *d = malloc(sizeof(struct dirent) + strlen(path) + 1);
		strcpy(d->d_name, path);
		handle(ti, d, &fs);
		free(d);
	}
	else
	{
		/* traverse a directory */
	    DIR *dir = opendir(path);
		if (dir) 
		{
            char *new_prefix;
			struct dirent *dp;
			struct traversal_info *saved_ti; /* optimisation to reduce buffer copies */
			saved_ti = ti;
			ti = new_traversal();
            if (saved_ti && saved_ti->match)
            {
                ti->match = saved_ti->match;
                ti->rexp = create_pattern(ti->match);
            }
            ti->current_path_prefix = NULL;
			if (prefix == NULL) 
			{
				new_prefix = malloc(strlen(path) + 1);
				strcpy(new_prefix, path);
			}
			else
			{
				new_prefix = malloc(strlen(prefix) + strlen(path) + 2);
				sprintf(new_prefix, "%s/%s", prefix, path);
			}
			ti->current_path_prefix = new_prefix; /* new_prefix will be managed by the traversal info structure */
			res = fchdir( dirfd(dir) );
			if (res == -1) {
				perror("cd");
			}
			else {
                while ( (dp = readdir(dir)) )
                {
                    struct stat fs;
                    char *filepath = strdup(dp->d_name);
                    memset(&fs, 0, sizeof(struct stat));
                    if (show_link_targets)
                        get_file_stat(filepath, &fs);
                    if ((dp->d_type & DT_DIR)
                        && strcmp(dp->d_name, ".") != 0 
                        && strcmp(dp->d_name, "..") != 0 ) 
                    {
                        if (num_entries == list_size)
                        {
                            char **newlist;
                            list_size += list_chunk_size;
                            newlist = malloc(sizeof(char *) * list_size);
                            if (dirlist != NULL)
                            {
                                memcpy(newlist, dirlist, sizeof(char *) * (list_size - list_chunk_size));
                                free(dirlist);
                            }
                            dirlist = newlist;
                        }
                        dirlist[num_entries++] = filepath;
                        handle(ti, dp, &fs);
                    }
                    else
                    {
                        handle(ti, dp, &fs);
                        free(filepath);
                    }
                }  
                {
                    int d_num;
                    for (d_num = 0; d_num <  num_entries; d_num++)
                    {
                        if (handle_dir != NULL)
                            handle_dir(ti, dirlist[d_num]);
                        process_dir(ti, dirlist[d_num], handle, handle_dir );
                        free(dirlist[d_num]);
                        dirlist[d_num] = NULL;
                    }
                }
                saved_ti->output = append_buffer(saved_ti->output, ti->output);
			}
			ti->output = NULL;
			release_traversal(ti);
			ti = saved_ti;
			closedir(dir);
	    }
		else
		{
			if (errno != EACCES)  /* do not report permission errors when traversing directories */
			{
				perror("path");
				fprintf(stderr, "unable to open path: %s\n", path);
			}
		}
	}
	if (dirlist != NULL)
		free(dirlist);
	if (saved_wd)
	{	
		int res = fchdir( dirfd(saved_wd) );
		if (res == -1)
		{
			perror("fchdir");
		}
		closedir(saved_wd);
	}
	if (ti && !orig_ti)
	{
		printf("%s", ti->output);
		release_traversal(ti);
	}
}

EXPORT
char *plugin_func(symbol_table variables, char *buf, int buflen, int argc, char *argv[])
{
	struct traversal_info *ti;
	int i;
	const char *filename_pattern = lookup_string_property(variables, argv[0], "MATCH", "");
	ti = new_traversal();
	if (strlen(filename_pattern))
	{
		ti->match = filename_pattern;
		ti->rexp = create_pattern(filename_pattern);
	}
    
	for (i=1; i<argc; i++)
		if (show_dir_header)
			process_dir(ti, name_lookup(variables, argv[i]), show, dir_header);
		else
			process_dir(ti, name_lookup(variables, argv[i]), show, NULL);

	if (ti->output)
	{
		if (buf)
		{
			int n = strlen(ti->output);
			if (n >= buflen) n = buflen-1;
			strncpy(buf, ti->output, n);
			buf[n] = 0;
		}
		else
		{
			buf = ti->output;
			ti->output = NULL; /* buf now owns the output, do not want release_traversal to free() it. */
		}
	}
	else if (buf)
		buf[0] = 0;
	release_traversal(ti);
	return buf;
}

#ifdef TESTING_PLUGIN
int main( int argc, char *argv[])
{
	int i;
	struct traversal_info *ti;

	ti = new_traversal();
    ti->match = "^[a-zA-Z.]*$";
    ti->rexp = create_pattern(ti->match);
	for (i=1; i<argc; i++)
		process_dir(ti, argv[i], show, NULL);

	if (ti->output) 
		printf("%s", ti->output);
	release_traversal(ti);

	return 0;
}
#endif
