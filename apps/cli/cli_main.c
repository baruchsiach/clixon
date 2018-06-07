/*
 *
  ***** BEGIN LICENSE BLOCK *****
 
  Copyright (C) 2009-2018 Olof Hagsand and Benny Holmgren

  This file is part of CLIXON.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  Alternatively, the contents of this file may be used under the terms of
  the GNU General Public License Version 3 or later (the "GPL"),
  in which case the provisions of the GPL are applicable instead
  of those above. If you wish to allow use of your version of this file only
  under the terms of the GPL, and not to allow others to
  use your version of this file under the terms of Apache License version 2, 
  indicate your decision by deleting the provisions above and replace them with
  the  notice and other provisions required by the GPL. If you do not delete
  the provisions above, a recipient may use your version of this file under
  the terms of any one of the Apache License version 2 or the GPL.

  ***** END LICENSE BLOCK *****

 * 
 */

#ifdef HAVE_CONFIG_H
#include "clixon_config.h" /* generated by config & autoconf */
#endif

#include <stdio.h>
#define __USE_GNU /* strverscmp */
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <pwd.h>
#include <assert.h>
#include <libgen.h>

/* cligen */
#include <cligen/cligen.h>

/* clicon */
#include <clixon/clixon.h>

#include "clixon_cli_api.h"

#include "cli_plugin.h"
#include "cli_generate.h"
#include "cli_common.h"
#include "cli_handle.h"

/* Command line options to be passed to getopt(3) */
#define CLI_OPTS "hD:f:xl:F:1u:d:m:qpGLy:c:U:"

/*! terminate cli application */
static int
cli_terminate(clicon_handle h)
{
    yang_spec      *yspec;

    clicon_rpc_close_session(h);
    if ((yspec = clicon_dbspec_yang(h)) != NULL)
	yspec_free(yspec);
    cli_plugin_finish(h);    
    cli_handle_exit(h);
    return 0;
}

/*! Unlink pidfile and quit
*/
static void
cli_sig_term(int arg)
{
    clicon_log(LOG_NOTICE, "%s: %u Terminated (killed by sig %d)", 
	    __PROGRAM__, getpid(), arg);
    exit(1);
}

/*! Setup signal handlers
 */
static void
cli_signal_init (clicon_handle h)
{
	cli_signal_block(h);
	set_signal(SIGTERM, cli_sig_term, NULL);
}

/*! Interactive CLI command loop
 * @param[in]  h    CLICON handle
 * @retval     0
 * @retval    -1
 * @see cligen_loop
 */
static int
cli_interactive(clicon_handle h)
{
    int     retval = -1;
    int     res;
    char   *cmd;
    char   *new_mode;
    int     eval;
    
    /* Loop through all commands */
    while(!cligen_exiting(cli_cligen(h))) {
	new_mode = cli_syntax_mode(h);
	if ((cmd = clicon_cliread(h)) == NULL) {
	    cligen_exiting_set(cli_cligen(h), 1); /* EOF */
	    goto done;
	}
	if ((res = clicon_parse(h, cmd, &new_mode, &eval)) < 0)
	    goto done;
    }
    retval = 0;
 done:
    return retval;
}

/*! Read file as configuration file and print xml file  for migrating to new fmt
 * @see clicon_option_readfile_xml
 */
static int 
dump_configfile_xml_fn(FILE       *fout,
		       const char *filename)
{
    struct stat st;
    char        opt[1024];
    char        val[1024];
    char        line[1024];
    char       *cp;
    FILE       *f = NULL;
    int         retval = -1;
    char       *suffix;

    if (filename == NULL || !strlen(filename)){
	clicon_err(OE_UNIX, 0, "Not specified");
	goto done;
    }
    if ((suffix = rindex(filename, '.')) != NULL &&
	strcmp((suffix+1), "xml") == 0){
	clicon_err(OE_CFG, 0, "Configfile %s should not be XML", filename);
	goto done;
    }
    if (stat(filename, &st) < 0){
	clicon_err(OE_UNIX, errno, "%s", filename);
	goto done;
    }
    if (!S_ISREG(st.st_mode)){
	clicon_err(OE_UNIX, 0, "%s is not a regular file", filename);
	goto done;
    }
    if ((f = fopen(filename, "r")) == NULL) {
	clicon_err(OE_UNIX, errno, "configure file: %s", filename);
	return -1;
    }
    clicon_debug(2, "Reading config file %s", __FUNCTION__, filename);
    fprintf(fout, "<config>\n");
    while (fgets(line, sizeof(line), f)) {
	if ((cp = strchr(line, '\n')) != NULL) /* strip last \n */
	    *cp = '\0';
	/* Trim out comments, strip whitespace, and remove CR */
	if ((cp = strchr(line, '#')) != NULL)
	    memcpy(cp, "\n", 2);
	if (sscanf(line, "%s %s", opt, val) < 2)
	    continue;
	fprintf(fout, "\t<%s>%s</%s>\n", opt, val, opt);
    }
    fprintf(fout, "</config>\n");
    retval = 0;
  done:
    if (f)
	fclose(f);
    return retval;
}

static void
usage(clicon_handle h,
      char         *argv0)
{
    char *confsock = clicon_sock(h);
    char *plgdir = clicon_cli_dir(h);

    fprintf(stderr, "usage:%s [options] [commands]\n"
	    "where commands is a CLI command or options passed to the main plugin\n" 
	    "where options are\n"
            "\t-h \t\tHelp\n"
    	    "\t-D <level> \tDebug\n"
	    "\t-f <file> \tConfig-file (mandatory)\n"
	    "\t-x\t\tDump configuration file as XML on stdout (migration utility)\n"
    	    "\t-F <file> \tRead commands from file (default stdin)\n"
	    "\t-1\t\tDo not enter interactive mode\n"
    	    "\t-u <sockpath>\tconfig UNIX domain path (default: %s)\n"
	    "\t-d <dir>\tSpecify plugin directory (default: %s)\n"
            "\t-m <mode>\tSpecify plugin syntax mode\n"
	    "\t-q \t\tQuiet mode, dont print greetings or prompt, terminate on ctrl-C\n"
	    "\t-p \t\tPrint database yang specification\n"
	    "\t-G \t\tPrint CLI syntax generated from dbspec (if CLICON_CLI_GENMODEL enabled)\n"
	    "\t-L \t\tDebug print dynamic CLI syntax including completions and expansions\n"
	    "\t-l <s|e|o> \tLog on (s)yslog, std(e)rr or std(o)ut (stderr is default)\n"
	    "\t-y <file>\tOverride yang spec file (dont include .yang suffix)\n"
	    "\t-c <file>\tSpecify cli spec file.\n"
	    "\t-U <user>\tOver-ride unix user with a pseudo user for NACM.\n",
	    argv0,
	    confsock ? confsock : "none",
	    plgdir ? plgdir : "none"
	);
    exit(1);
}

/*
 */
int
main(int argc, char **argv)
{
    int          retval = -1;
    char         c;    
    int          once;
    char	*tmp;
    char	*argv0 = argv[0];
    clicon_handle h;
    int          printspec = 0;
    int          printgen  = 0;
    int          logclisyntax  = 0;
    int          help = 0;
    char        *treename = NULL;
    int          len;
    int          logdst = CLICON_LOG_STDERR;
    char        *restarg = NULL; /* what remains after options */
    int          dump_configfile_xml = 0;
    yang_spec   *yspec;
    struct passwd *pw;
    
    /* Defaults */
    once = 0;

    /* In the startup, logs to stderr & debug flag set later */
    clicon_log_init(__PROGRAM__, LOG_INFO, logdst); 
    /* Initiate CLICON handle */
    if ((h = cli_handle_init()) == NULL)
	goto done;
    /* Set username to clicon handle. Use in all communication to backend 
     * Note, can be overridden by -U
     */
    if ((pw = getpwuid(getuid())) == NULL){
	clicon_err(OE_UNIX, errno, "getpwuid");
	goto done;
    }
    if (clicon_username_set(h, pw->pw_name) < 0)
	goto done;

    cligen_comment_set(cli_cligen(h), '#'); /* Default to handle #! clicon_cli scripts */

    /*
     * First-step command-line options for help, debug, config-file and log, 
     */
    optind = 1;
    opterr = 0;
    while ((c = getopt(argc, argv, CLI_OPTS)) != -1)
	switch (c) {
	case 'h':
	    /* Defer the call to usage() to later. Reason is that for helpful
	       text messages, default dirs, etc, are not set until later.
	       But this means that we need to check if 'help' is set before 
	       exiting, and then call usage() before exit.
	    */
	    help = 1; 
	    break;
	case 'D' : /* debug */
	    if (sscanf(optarg, "%d", &debug) != 1)
		usage(h, argv[0]);
	    break;
	case 'f': /* config file */
	    if (!strlen(optarg))
		usage(h, argv[0]);
	    clicon_option_str_set(h, "CLICON_CONFIGFILE", optarg);
	    break;
	case 'x': /* dump config file as xml (migration from .conf file)*/
	    dump_configfile_xml++;
	    break;
	 case 'l': /* Log destination: s|e|o */
	   switch (optarg[0]){
	   case 's':
	     logdst = CLICON_LOG_SYSLOG;
	     break;
	   case 'e':
	     logdst = CLICON_LOG_STDERR;
	     break;
	   case 'o':
	     logdst = CLICON_LOG_STDOUT;
	     break;
	   default:
	       usage(h, argv[0]);
	   }
	   break;
	}
    /* 
     * Logs, error and debug to stderr or syslog, set debug level
     */
    clicon_log_init(__PROGRAM__, debug?LOG_DEBUG:LOG_INFO, logdst);

    clicon_debug_init(debug, NULL); 

    /* Use cli as util tool to dump config file as xml for migration */
    if (dump_configfile_xml) {
	clicon_hash_t *copt = clicon_options(h);
	char *configfile = hash_value(copt, "CLICON_CONFIGFILE", NULL);	
	if (dump_configfile_xml_fn(stdout, configfile) < 0)
	    goto done;
    }

    /* Find and read configfile */
    if (clicon_options_main(h) < 0){
        if (help)
	    usage(h, argv[0]);
	return -1;
    }

    /* Now rest of options */   
    opterr = 0;
    optind = 1;
    while ((c = getopt(argc, argv, CLI_OPTS)) != -1){
	switch (c) {
	case 'D' : /* debug */
	case 'f': /* config file */
	case 'x': /* dump config file as xml */
	case 'l': /* Log destination */
	    break; /* see above */
	case 'F': /* read commands from file */
	    if (freopen(optarg, "r", stdin) == NULL){
		cli_output(stderr, "freopen: %s\n", strerror(errno));
		return -1;
	    }
	    break; 
	case '1' : /* Quit after reading database once - dont wait for events */
	    once = 1;
	    break;
	case 'u': /* config unix domain path/ ip host */
	    if (!strlen(optarg))
		usage(h, argv[0]);
	    clicon_option_str_set(h, "CLICON_SOCK", optarg);
	    break;
	case 'd':  /* Plugin directory: overrides configfile */
	    if (!strlen(optarg))
		usage(h, argv[0]);
	    clicon_option_str_set(h, "CLICON_CLI_DIR", optarg);
	    break;
	case 'm': /* CLI syntax mode */
	    if (!strlen(optarg))
		usage(h, argv[0]);
	    clicon_option_str_set(h, "CLICON_CLI_MODE", optarg);
	    break;
	case 'q' : /* Quiet mode */
	    clicon_quiet_mode_set(h, 1);
	    break;
	case 'p' : /* Print spec */
	    printspec++;
	    break;
	case 'G' : /* Print generated CLI syntax */
	    printgen++;
	    break;
	case 'L' : /* Debug print dynamic CLI syntax */
	    logclisyntax++;
	    break;
	case 'y' :{ /* Overwrite yang module or absolute filename */
	    clicon_option_str_set(h, "CLICON_YANG_MODULE_MAIN", optarg);
	    break;
	}
	case 'c' :{ /* Overwrite clispec with absolute filename */
	    clicon_option_str_set(h, "CLICON_CLISPEC_FILE", optarg);
	    break;
	}
	case 'U': /* Clixon 'pseudo' user */
	    if (!strlen(optarg))
		usage(h, argv[0]);
	    if (clicon_username_set(h, optarg) < 0)
		goto done;
	    break;
	default:
	    usage(h, argv[0]);
	    break;
	}
    }
    argc -= optind;
    argv += optind;

    /* Defer: Wait to the last minute to print help message */
    if (help)
	usage(h, argv[0]);

    /* Setup signal handlers */
    cli_signal_init(h);

    /* Backward compatible mode, do not include keys in cgv-arrays in callbacks.
       Should be 0 but default is 1 since all legacy apps use 1
       Test legacy before shifting default to 0
     */
    cv_exclude_keys(clicon_cli_varonly(h)); 

    /* Parse db specification as cli*/
    if ((yspec = yang_spec_main(h)) == NULL)
	goto done;
    if (printspec)
	yang_print(stdout, (yang_node*)yspec);

    /* Create tree generated from dataspec. If no other trees exists, this is
     * the only one.
     */
    if (clicon_cli_genmodel(h)){
	parse_tree         pt = {0,};  /* cli parse tree */

	/* Create cli command tree from dbspec */
	if (yang2cli(h, yspec, &pt, clicon_cli_genmodel_type(h)) < 0)
	    goto done;

	len = strlen("datamodel:") + strlen(clicon_dbspec_name(h)) + 1;
	if ((treename = malloc(len)) == NULL){
	    clicon_err(OE_UNIX, errno, "malloc");
	    goto done;
	}	
	snprintf(treename, len, "datamodel:%s",  clicon_dbspec_name(h));
	cligen_tree_add(cli_cligen(h), treename, pt);

	if (printgen)
	    cligen_print(stdout, pt, 1);
    }

    /* Initialize cli syntax */
    if (cli_syntax_load(h) < 0)
	goto done;

    /* Set syntax mode if specified from command-line or config-file. */
    if (clicon_option_exists(h, "CLICON_CLI_MODE"))
	if ((tmp = clicon_cli_mode(h)) != NULL)
	    if (cli_set_syntax_mode(h, tmp) == 0) {
		fprintf(stderr, "FATAL: Failed to set syntax mode '%s'\n", tmp);
		goto done;
	    }

    if (!cli_syntax_mode(h)){
	fprintf (stderr, "FATAL: No cli mode set (use -m or CLICON_CLI_MODE)\n");
	goto done;
    }
    if (cligen_tree_find(cli_cligen(h), cli_syntax_mode(h)) == NULL)
	clicon_log(LOG_WARNING, "No such cli mode: %s (Specify cli mode with CLICON_CLI_MODE in config file or -m <mode> on command line", cli_syntax_mode(h));

    if (logclisyntax)
	cli_logsyntax_set(h, logclisyntax);

    if (debug)
	clicon_option_dump(h, debug);
    
    /* Join rest of argv to a single command */
    restarg = clicon_strjoin(argc, argv, " ");

    /* If several cligen object variables match same preference, select first */
    cligen_match_cgvar_same(1);

    /* Call start function in all plugins before we go interactive 
       Pass all args after the standard options to plugin_start
     */
    tmp = *(argv-1);
    *(argv-1) = argv0;
    clixon_plugin_start(h, argc+1, argv-1);
    *(argv-1) = tmp;

    cligen_line_scrolling_set(cli_cligen(h), clicon_option_int(h,"CLICON_CLI_LINESCROLLING"));
    /* Launch interfactive event loop, unless -1 */
    if (restarg != NULL && strlen(restarg)){
	char *mode = cli_syntax_mode(h);
	int result;

	/* */
	if (clicon_parse(h, restarg, &mode, &result) != 1){
	    goto done;
	}
	if (result < 0)
	    goto done;
    }
    /* Go into event-loop unless -1 command-line */
    if (!once)
	retval = cli_interactive(h);
    else
	retval = 0;
  done:
    if (treename)
	free(treename);
    if (restarg)
	free(restarg);
    // Gets in your face if we log on stderr
    clicon_log_init(__PROGRAM__, LOG_INFO, 0); /* Log on syslog no stderr */
    clicon_log(LOG_NOTICE, "%s: %u Terminated\n", __PROGRAM__, getpid());
    if (h)
	cli_terminate(h);
    return retval;
}
