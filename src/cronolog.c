/* ====================================================================
 * Copyright (c) 1995-1999 The Apache Group.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * 4. The names "Apache Server" and "Apache Group" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * THIS SOFTWARE IS PROVIDED BY THE APACHE GROUP ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE APACHE GROUP OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Group and was originally based
 * on public domain software written at the National Center for
 * Supercomputing Applications, University of Illinois, Urbana-Champaign.
 * For more information on the Apache Group and the Apache HTTP server
 * project, please see <http://www.apache.org/>.
 *
 */
/*
 * cronolog -- simple Apache log rotation program
 *
 * Copyright (c) 1996-1999 by Ford & Mason Ltd
 *
 * This software was submitted by Ford & Mason Ltd to the Apache
 * Software Foundation in December 1999.  Future revisions and
 * derivatives of this source code must acknowledge Ford & Mason Ltd
 * as the original contributor of this module.  All other licensing
 * and usage conditions are those of the Apache Software Foundation.
 *
 * Originally written by Andrew Ford <A.Ford@ford-mason.co.uk>
 *
 * cronolog is loosly based on the rotatelogs program, which is part of the
 * Apache package written by Ben Laurie <ben@algroup.co.uk>
 *
 * The argument to this program is the log file name template as an
 * strftime format string.  For example to generate new error and
 * access logs each day stored in subdirectories by year, month and day add
 * the following lines to the httpd.conf:
 *
 *	TransferLog "|/www/etc/cronolog /www/logs/%Y/%m/%d/access.log"
 *	ErrorLog    "|/www/etc/cronolog /www/logs/%Y/%m/%d/error.log"
 *
 * The option "-x file" specifies that debugging messages should be
 * written to "file" (e.g. /dev/console) or to stderr if "file" is "-".
 */

#include "cronoutils.h"
#include "../lib/getopt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <zlib.h>
#include <libgearman/gearman.h>

#include "iniparser.h"

/* Forward function declaration */
void DEBUGLOG( int lv, const char *buf, ... );
char *gzcompress( char *buffer, int length, int *comprLen );
int	new_log_file(const char *, const char *, mode_t, const char *, PERIODICITY, int, int, char *, size_t, time_t, time_t *);

//global var
gearman_client_st __GCLIENT__;

int __GEARMAN_TIMEOUT__;

bool __USEGZIP__;
bool __DEBUG__;
bool __GEARMAN_STATUS__;
bool __GEARMAN_ENABLE__;

char *__SERVERS__;
char *__JOB_HANDLE__;
char *__GM_WORKER__;
char *__DEBUG_LOG_FILE__;

/* Definition of version and usage messages */

#define ZLIB_MODIFIER 			1000

#define efree(ptr)          	free(ptr)
#define emalloc(size)       	malloc(size)
#define erealloc(ptr, size)		__realloc(ptr,size)
#define safe_emalloc(nmemb, size, offset)          malloc((nmemb) * (size) + (offset))

#ifndef _WIN32
#define VERSION_MSG   	PACKAGE " version " VERSION "\n"
#else
#define VERSION_MSG      "cronolog_gearman version 0.1\n"
#endif


#define USAGE_MSG 	"usage: %s [OPTIONS] logfile-spec\n" \
			"\n" \
			"   -H NAME,   --hardlink=NAME maintain a hard link from NAME to current log\n" \
			"   -S NAME,   --symlink=NAME  maintain a symbolic link from NAME to current log\n" \
			"   -P NAME,   --prev-symlink=NAME  maintain a symbolic link from NAME to previous log\n" \
			"   -l NAME,   --link=NAME     same as -S/--symlink\n" \
			"   -h,        --help          print this help, then exit\n" \
			"   -p PERIOD, --period=PERIOD set the rotation period explicitly\n" \
			"   -d DELAY,  --delay=DELAY   set the rotation period delay\n" \
			"   -o,        --once-only     create single output log from template (not rotated)\n" \
			"   -x FILE,   --debug=FILE    write debug messages to FILE\n" \
			"                              ( or to standard error if FILE is \"-\")\n" \
			"   -a,        --american         American date formats\n" \
			"   -e,        --european         European date formats (default)\n" \
			"   -s,    --start-time=TIME   starting time\n" \
			"   -z TZ, --time-zone=TZ      use TZ for timezone\n" \
			"   -V,      --version         print version number, then exit\n"

/* Definition of the short and long program options */

char          *short_options = "ad:eop:s:z:H:P:S:l:hVx:";

#ifndef _WIN32
struct option long_options[] =
{
    { "american",	no_argument,		NULL, 'a' },
    { "european",	no_argument,		NULL, 'e' },
    { "start-time", 	required_argument,	NULL, 's' },
    { "time-zone",  	required_argument,	NULL, 'z' },
    { "hardlink",  	required_argument, 	NULL, 'H' },
    { "symlink",   	required_argument, 	NULL, 'S' },
    { "prev-symlink",  	required_argument, 	NULL, 'P' },
    { "link",      	required_argument, 	NULL, 'l' },
    { "period",		required_argument,	NULL, 'p' },
    { "delay",		required_argument,	NULL, 'd' },
    { "once-only", 	no_argument,       	NULL, 'o' },
    { "help",      	no_argument,       	NULL, 'h' },
    { "version",   	no_argument,       	NULL, 'V' }
};
#endif

inline static void * __realloc(void *p, size_t len) {
  p = realloc(p, len);
  if (p) {
    return p;
  }
  fprintf(stderr, "Out of memory\n");
  exit(1);
}

static voidpf zlib_alloc(voidpf opaque, uInt items, uInt size) {
    return (voidpf)safe_emalloc(items, size, 0);
}

static void zlib_free(voidpf opaque, voidpf address) {
    efree((void*)address);
}

void error_handling(char* message) {
	fputs(message, stdout);
	fputc('\n', stdout);
	exit(1);
}

void DEBUGLOG( int lv, const char *buf, ... ) {

	if ( __DEBUG_LOG_FILE__ == NULL ) { return; }

	if ( __DEBUG__ == true ) {

		char buffer[8192];

	 	va_list ap;
    	va_start( ap, buf );
	    vsnprintf( buffer + strlen( buffer ), sizeof( buffer ) - strlen( buffer ), buf, ap );
    	va_end( ap );

		FILE *fildes = fopen( __DEBUG_LOG_FILE__, "a");
		if ( lv == 0 ) {
			fprintf(fildes, "[cronolog] %s\n\n", buffer);
		} else {
			fprintf(fildes, "[gearman_client_error] %s\n", buffer);
		}
		fclose(fildes);
	}

}

char *gzcompress( char *buffer, int length, int *comprLen ) {

	int status;
	long level = Z_DEFAULT_COMPRESSION;
	z_stream stream;
    char *zipdata;

    stream.data_type = Z_ASCII;
    stream.zalloc = zlib_alloc;
    stream.zfree  = zlib_free;
    stream.opaque = (voidpf) Z_NULL;

    stream.next_in = (Bytef *) buffer;
    stream.avail_in = length;
	stream.avail_out = stream.avail_in + (stream.avail_in / ZLIB_MODIFIER) + 15 + 1; /* room for \0 */

    zipdata = (char *) emalloc(stream.avail_out);
    if (!zipdata) {
		return 0;
    }
	stream.next_out = zipdata;

    /* init with -MAX_WBITS disables the zlib internal headers */
    status = deflateInit2(&stream, level, Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL, 0);
    if (status == Z_OK) {
        status = deflate(&stream, Z_FINISH);
        if (status != Z_STREAM_END) {
            deflateEnd(&stream);
            if (status == Z_OK) {
                status = Z_BUF_ERROR;
            }
        } else {
            status = deflateEnd(&stream);
        }
    }

	*comprLen = stream.total_out;

    if (status == Z_OK) {
        zipdata = erealloc(zipdata,stream.total_out + 1); /* resize to buffer to the "right" size */
        zipdata[ stream.total_out ] = '\0';
		return zipdata;
    } else {
        efree(zipdata);
		return NULL;
    }

}

int createGearmanClient() {

	gearman_return_t ret;

    if (gearman_client_create(&__GCLIENT__) == NULL) {
        DEBUGLOG(1, "gearman_client_create 1");
		return false;
    } else {

    	char * server  = strdup( __SERVERS__ );
	    char * host    = str_token( &server, ':' );
    	in_port_t port = ( in_port_t ) atoi( str_token( &server, 0 ) );

	    gearman_client_set_timeout( &__GCLIENT__, __GEARMAN_TIMEOUT__ );
	    ret = gearman_client_add_server(&__GCLIENT__, host, port);
    	if (ret != GEARMAN_SUCCESS) {
        	DEBUGLOG(1, "gearman_client_add_server 2");
			return false;
    	}

	}

	return true;
}

/* Main function.
 */
int main(int argc, char **argv) {
    PERIODICITY	periodicity = UNKNOWN;
    PERIODICITY	period_delay_units = UNKNOWN;
    int		period_multiple = 1;
    int		period_delay  = 0;
    int		use_american_date_formats = 0;
    char 	read_buf[BUFSIZE];
    char 	tzbuf[BUFSIZE];
    char	filename[MAX_PATH];
    char	*start_time = NULL;
    char	*template;
    char	*linkname = NULL;
    char	*prevlinkname = NULL;
    mode_t	linktype = 0;
    int 	n_bytes_read;
    int		ch;
    time_t	time_now;
    time_t	time_offset = 0;
    time_t	next_period = 0;
    int 	log_fd = -1;

    char 	*gzip_buf;
	int 	gzip_buf_len;

    gearman_return_t gearman_ret;

    dictionary *ini     = iniparser_load("/etc/cronolog_gm.ini");
    __SERVERS__         = iniparser_getstring(ini, "gearman:servers", NULL);
    __GM_WORKER__       = iniparser_getstring(ini, "gearman:workercommand", NULL);

    __USEGZIP__         = iniparser_getboolean(ini, "gearman:usegzip", false);
    __GEARMAN_ENABLE__	= iniparser_getboolean(ini, "gearman:enable", true);
    __GEARMAN_TIMEOUT__ = iniparser_getint(ini, "gearman:timeout", 3000);

	__DEBUG__			= iniparser_getboolean(ini, "gearman:debug", false);
	__DEBUG_LOG_FILE__	= iniparser_getstring(ini, "gearman:debugfile", NULL);

#ifndef _WIN32
    while ((ch = getopt_long(argc, argv, short_options, long_options, NULL)) != EOF)
#else
    while ((ch = getopt(argc, argv, short_options)) != EOF)
#endif        
    {
	switch (ch)
	{
	case 'a':
	    use_american_date_formats = 1;
	    break;
	    
	case 'e':
	    use_american_date_formats = 0;
	    break;
	    
	case 's':
	    start_time = optarg;
	    break;

	case 'z':
	    sprintf(tzbuf, "TZ=%s", optarg);
	    putenv(tzbuf);
	    break;

	case 'H':
	    linkname = optarg;
	    linktype = S_IFREG;
	    break;

	case 'l':
	case 'S':
	    linkname = optarg;
#ifndef _WIN32
	    linktype = S_IFLNK;
#endif        
	    break;
	    
	case 'P':
	    if (linkname == NULL)
	    {
		fprintf(stderr, "A current log symlink is needed to mantain a symlink to the previous log\n", argv[0]);
		exit(1);
	    }
	    prevlinkname = optarg;
	    break;
	    

	case 'd':
	    period_delay_units = parse_timespec(optarg, &period_delay);
	    break;

	case 'p':
	    periodicity = parse_timespec(optarg, &period_multiple);
	    if (   (periodicity == INVALID_PERIOD)
		|| (periodicity == PER_SECOND) && (60 % period_multiple)
		|| (periodicity == PER_MINUTE) && (60 % period_multiple)
		|| (periodicity == HOURLY)     && (24 % period_multiple)
		|| (periodicity == DAILY)      && (period_multiple > 365)
		|| (periodicity == WEEKLY)     && (period_multiple > 52)
		|| (periodicity == MONTHLY)    && (12 % period_multiple)) {
		fprintf(stderr, "%s: invalid explicit period specification (%s)\n", argv[0], start_time);
		exit(1);
	    }		
	    break;
	    
	case 'o':
	    periodicity = ONCE_ONLY;
	    break;
	    
	case 'x':
	    if (strcmp(optarg, "-") == 0)
	    {
		debug_file = stderr;
	    }
	    else
	    {
		debug_file = fopen(optarg, "a+");
	    }
	    break;
	    
	case 'V':
	    fprintf(stderr, VERSION_MSG);
	    exit(0);
	    
	case 'h':
	case '?':
	    fprintf(stderr, USAGE_MSG, argv[0]);
	    exit(1);
	}
    }

    if ((argc - optind) != 1) {
		fprintf(stderr, USAGE_MSG, argv[0]);
		exit(1);
    }

    if (start_time) {
		time_now = parse_time(start_time, use_american_date_formats);
		if (time_now == -1) {
		    fprintf(stderr, "%s: invalid start time (%s)\n", argv[0], start_time);
		    exit(1);
		}
		time_offset = time_now - time(NULL);
		DEBUGLOG(0, "Using offset of %d seconds from real time", time_offset);
    }

    /* The template should be the only argument.
     * Unless the -o option was specified, determine the periodicity.
     */
    
    template = argv[optind];
    if (periodicity == UNKNOWN) {
		periodicity = determine_periodicity(template);
    }
    DEBUGLOG(0, "periodicity = %d %s", period_multiple, periods[periodicity]);

    if (period_delay) {
		if ( (period_delay_units > periodicity) || (   period_delay_units == periodicity && abs(period_delay)  >= period_multiple)) {
		    fprintf(stderr, "%s: period delay cannot be larger than the rollover period\n", argv[0], start_time);
	    	exit(1);
		}		
		period_delay *= period_seconds[period_delay_units];
    }
    DEBUGLOG(0, "Rotation period is per %d %s", period_multiple, periods[periodicity]);

	//gearman client create
	if ( __GEARMAN_ENABLE__ ) {

		__GEARMAN_STATUS__ = createGearmanClient();
		if ( !__GEARMAN_STATUS__ ) {
    		DEBUGLOG(1, "createGearmanClient Error");
		}

	}

    /* Loop, waiting for data on standard input */
    for (;;) {

		/* Read a buffer's worth of log file data, exiting on errors
		 * or end of file.
		 */
		n_bytes_read = read(0, read_buf, sizeof read_buf);
		if (n_bytes_read == 0) {
	    	exit(3);
		}
		if (errno == EINTR) {
	    	continue;
		} else if (n_bytes_read < 0) {
	    	exit(4);
		}

		time_now = time(NULL) + time_offset;
	
		/* If the current period has finished and there is a log file
		 * open, close the log file
		 */
		if ((time_now >= next_period) && (log_fd >= 0)) {
		    close(log_fd);
		    log_fd = -1;
		}
	
		/* If there is no log file open then open a new one.
		 */
		if (log_fd < 0) {
		    log_fd = new_log_file(template, linkname, linktype, prevlinkname, periodicity, period_multiple, period_delay, filename, sizeof (filename), time_now, &next_period);
		}

		//DEBUGLOG("%s (%d): wrote message; next period starts at %s (%d) in %d secs\n", timestamp(time_now), time_now,  timestamp(next_period), next_period, next_period - time_now);

		/* Write out the log data to the current log file.
		 */
		if (write(log_fd, read_buf, n_bytes_read) != n_bytes_read) {
		    perror(filename);
		    exit(5);
		}

		if ( __GEARMAN_ENABLE__ && __GEARMAN_STATUS__ && n_bytes_read > 10 ) {

			//send gearman-server
			__JOB_HANDLE__		= emalloc(GEARMAN_JOB_HANDLE_SIZE);
			if ( __USEGZIP__ ) {
				gzip_buf		= gzcompress( read_buf, n_bytes_read, &gzip_buf_len ); 
	    		gearman_ret = gearman_client_do_background(&__GCLIENT__, __GM_WORKER__, NULL, ( void * )gzip_buf, gzip_buf_len, __JOB_HANDLE__ );
			} else {
	    		gearman_ret = gearman_client_do_background(&__GCLIENT__, __GM_WORKER__, NULL, ( void * )read_buf, n_bytes_read, __JOB_HANDLE__ );
			}
			gearman_client_run_tasks( &__GCLIENT__ );
			efree(__JOB_HANDLE__);
			efree(gzip_buf);

		}

		//gearman client end

    }

	gearman_client_free(&__GCLIENT__);

	iniparser_freedict(ini);

    /* NOTREACHED */
    return 1;
}

/* Open a new log file: determine the start of the current
 * period, generate the log file name from the template,
 * determine the end of the period and open the new log file.
 *
 * Returns the file descriptor of the new log file and also sets the
 * name of the file and the start time of the next period via pointers
 * supplied.
 */
int new_log_file(const char *template, const char *linkname, mode_t linktype, const char *prevlinkname,
	     PERIODICITY periodicity, int period_multiple, int period_delay,
	     char *pfilename, size_t pfilename_len,
	     time_t time_now, time_t *pnext_period)
{
    time_t 	start_of_period;
    struct tm 	*tm;
    int 	log_fd;

    start_of_period = start_of_this_period(time_now, periodicity, period_multiple);
    tm = localtime(&start_of_period);
    strftime(pfilename, BUFSIZE, template, tm);
    *pnext_period = start_of_next_period(start_of_period, periodicity, period_multiple) + period_delay;
    
    DEBUGLOG(0, "%s (%d): using log file \"%s\" from %s (%d) until %s (%d) (for %d secs)",
	   timestamp(time_now), time_now, pfilename, timestamp(start_of_period), start_of_period, timestamp(*pnext_period), *pnext_period, *pnext_period - time_now);
    
    log_fd = open(pfilename, O_WRONLY|O_CREAT|O_APPEND, FILE_MODE);
    
#ifndef DONT_CREATE_SUBDIRS
    if ((log_fd < 0) && (errno == ENOENT))
    {
	create_subdirs(pfilename);
	log_fd = open(pfilename, O_WRONLY|O_CREAT|O_APPEND, FILE_MODE);
    }
#endif	    

    if (log_fd < 0)
    {
	perror(pfilename);
	exit(2);
    }

    if (linkname)
    {
	create_link(pfilename, linkname, linktype, prevlinkname);
    }
    return log_fd;
}

