/*
 * Bacula RUNSCRIPT Structure definition for FileDaemon and Director
 * Eric Bollengier May 2006
 * Version $Id$
 */
/*
   Copyright (C) 2000-2006 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as amended with additional clauses defined in the
   file LICENSE in the main source directory.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
   the file LICENSE for additional details.

 */


#ifndef __RUNSCRIPT_H_
#define __RUNSCRIPT_H_ 1

/* Usage:
 *
 * #define USE_RUNSCRIPT
 * #include "lib/runscript.h"
 * 
 * RUNSCRIPT *script = new_runscript();
 * script->set_command("/bin/sleep 20");
 * script->on_failure = true;
 * script->when = SCRIPT_After;
 * 
 * script->run("Label");
 * free_runscript(script);
 */

/* 
 * RUNSCRIPT->when can take following value : 
 */
enum {
   SCRIPT_Never  = 1,
   SCRIPT_After  = 2,           /* AfterJob */
   SCRIPT_Before = 3,           /* BeforeJob */
   SCRIPT_Any    = 4            /* Before and After */
};

/*
 * Structure for RunScript ressource
 */
class RUNSCRIPT {
public:
   POOLMEM *command;            /* command string */
   POOLMEM *target;             /* host target */
   char level;                  /* Base|Full|Incr...|All (NYI) */
   bool on_success;             /* executre command on job success (After) */
   bool on_failure;             /* executre command on job failure (After) */
   bool abort_on_error;         /* abort job on error (Before) */
   int  when;                   /* SCRIPT_Before|Script_After BEFORE/AFTER JOB*/

   int run(JCR *job, const char *name="");
   bool can_run_at_level(int JobLevel) { return true;};        /* TODO */
   void set_command(const POOLMEM *cmd);
   void set_target(const POOLMEM *client_name);
   void reset_default(bool free_string = false);
   bool is_local();             /* true if running on local host */
   void debug();
};

/* create new RUNSCRIPT (set all value to 0) */
RUNSCRIPT *new_runscript();           

/* create new RUNSCRIPT from an other */
RUNSCRIPT *copy_runscript(RUNSCRIPT *src);

/* launch each script from runscripts*/
int run_scripts(JCR *jcr, alist *runscripts, const char *name); 

/* free RUNSCRIPT (and all POOLMEM) */
void free_runscript(RUNSCRIPT *script);

/* foreach_alist free RUNSCRIPT */
void free_runscripts(alist *runscripts); /* you have to free alist */

#endif /* __RUNSCRIPT_H_ */
