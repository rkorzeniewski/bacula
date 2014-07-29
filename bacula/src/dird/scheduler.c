/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 *
 *   Bacula scheduler
 *     It looks at what jobs are to be run and when
 *     and waits around until it is time to
 *     fire them up.
 *
 *     Kern Sibbald, May MM, major revision December MMIII
 *
 */

#include "bacula.h"
#include "dird.h"

#if 0
#define SCHED_DEBUG
#define DBGLVL 0
#else
#undef SCHED_DEBUG
#define DBGLVL DT_SCHEDULER|200
#endif

const int dbglvl = DBGLVL;

/* Local variables */
struct job_item {
   RUN *run;
   JOB *job;
   time_t runtime;
   int Priority;
   dlink link;                        /* link for list */
};

/* List of jobs to be run. They were scheduled in this hour or the next */
static dlist *jobs_to_run;               /* list of jobs to be run */

/* Time interval in secs to sleep if nothing to be run */
static int const next_check_secs = 60;

/* Forward referenced subroutines */
static void find_runs();
static void add_job(JOB *job, RUN *run, time_t now, time_t runtime);
static void dump_job(job_item *ji, const char *msg);

/* Imported subroutines */

/* Imported variables */

/*
 * called by reload_config to tell us that the schedules
 * we may have based our next jobs to run queues have been
 * invalidated.  In fact the schedules may not have changed
 * but the run object that we have recorded the last_run time
 * on are new and no longer have a valid last_run time which
 * causes us to double run schedules that get put into the list
 * because run_nh = 1.
 */
static bool schedules_invalidated = false;
void invalidate_schedules(void) {
    schedules_invalidated = true;
}

/*********************************************************************
 *
 *         Main Bacula Scheduler
 *
 */
JCR *wait_for_next_job(char *one_shot_job_to_run)
{
   JCR *jcr;
   JOB *job;
   RUN *run;
   time_t now, prev;
   static bool first = true;
   job_item *next_job = NULL;

   Dmsg0(dbglvl, "Enter wait_for_next_job\n");
   if (first) {
      first = false;
      /* Create scheduled jobs list */
      jobs_to_run = New(dlist(next_job, &next_job->link));
      if (one_shot_job_to_run) {            /* one shot */
         job = (JOB *)GetResWithName(R_JOB, one_shot_job_to_run);
         if (!job) {
            Emsg1(M_ABORT, 0, _("Job %s not found\n"), one_shot_job_to_run);
         }
         Dmsg1(5, "Found one_shot_job_to_run %s\n", one_shot_job_to_run);
         jcr = new_jcr(sizeof(JCR), dird_free_jcr);
         set_jcr_defaults(jcr, job);
         return jcr;
      }
   }

   /* Wait until we have something in the
    * next hour or so.
    */
again:
   while (jobs_to_run->empty()) {
      find_runs();
      if (!jobs_to_run->empty()) {
         break;
      }
      bmicrosleep(next_check_secs, 0); /* recheck once per minute */
   }

#ifdef  list_chain
   job_item *je;
   foreach_dlist(je, jobs_to_run) {
      dump_job(je, _("Walk queue"));
   }
#endif
   /*
    * Pull the first job to run (already sorted by runtime and
    *  Priority, then wait around until it is time to run it.
    */
   next_job = (job_item *)jobs_to_run->first();
   jobs_to_run->remove(next_job);

   dump_job(next_job, _("Dequeued job"));

   if (!next_job) {                /* we really should have something now */
      Emsg0(M_ABORT, 0, _("Scheduler logic error\n"));
   }

   /* Now wait for the time to run the job */
   for (;;) {
      time_t twait;
      /** discard scheduled queue and rebuild with new schedule objects. **/
      lock_jobs();
      if (schedules_invalidated) {
          dump_job(next_job, "Invalidated job");
          free(next_job);
          while (!jobs_to_run->empty()) {
              next_job = (job_item *)jobs_to_run->first();
              jobs_to_run->remove(next_job);
              dump_job(next_job, "Invalidated job");
              free(next_job);
          }
          schedules_invalidated = false;
          unlock_jobs();
          goto again;
      }
      unlock_jobs();
      prev = now = time(NULL);
      twait = next_job->runtime - now;
      if (twait <= 0) {               /* time to run it */
         break;
      }
      /* Recheck at least once per minute */
      bmicrosleep((next_check_secs < twait)?next_check_secs:twait, 0);
      /* Attempt to handle clock shift (but not daylight savings time changes)
       * we allow a skew of 10 seconds before invalidating everything.
       */
      now = time(NULL);
      if (now < prev-10 || now > (prev+next_check_secs+10)) {
         schedules_invalidated = true;
      }
   }
   jcr = new_jcr(sizeof(JCR), dird_free_jcr);
   run = next_job->run;               /* pick up needed values */
   job = next_job->job;
   if (job->enabled) {
      dump_job(next_job, _("Run job"));
   }
   free(next_job);
   if (!job->enabled) {
      free_jcr(jcr);
      goto again;                     /* ignore this job */
   }
   run->last_run = now;               /* mark as run now */

   ASSERT(job);
   set_jcr_defaults(jcr, job);
   if (run->level) {
      jcr->setJobLevel(run->level);  /* override run level */
   }
   if (run->pool) {
      jcr->pool = run->pool;          /* override pool */
      jcr->run_pool_override = true;
   }
   if (run->next_pool) {
      jcr->next_pool = run->next_pool; /* override next pool */
      jcr->run_next_pool_override = true;
   }
   if (run->full_pool) {
      jcr->full_pool = run->full_pool; /* override full pool */
      jcr->run_full_pool_override = true;
   }
   if (run->inc_pool) {
      jcr->inc_pool = run->inc_pool;  /* override inc pool */
      jcr->run_inc_pool_override = true;
   }
   if (run->diff_pool) {
      jcr->diff_pool = run->diff_pool;  /* override dif pool */
      jcr->run_diff_pool_override = true;
   }
   if (run->storage) {
      USTORE store;
      store.store = run->storage;
      pm_strcpy(store.store_source, _("run override"));
      set_rwstorage(jcr, &store);     /* override storage */
   }
   if (run->msgs) {
      jcr->messages = run->msgs;      /* override messages */
   }
   if (run->Priority) {
      jcr->JobPriority = run->Priority;
   }
   if (run->spool_data_set) {
      jcr->spool_data = run->spool_data;
   }
   if (run->accurate_set) {     /* overwrite accurate mode */
      jcr->accurate = run->accurate;
   }
   if (run->write_part_after_job_set) {
      jcr->write_part_after_job = run->write_part_after_job;
   }
   if (run->MaxRunSchedTime_set) {
      jcr->MaxRunSchedTime = run->MaxRunSchedTime;
   }
   Dmsg0(dbglvl, "Leave wait_for_next_job()\n");
   return jcr;
}


/*
 * Shutdown the scheduler
 */
void term_scheduler()
{
   if (jobs_to_run) {
      delete jobs_to_run;
   }
}

/*
 * Find all jobs to be run this hour and the next hour.
 */
static void find_runs()
{
   time_t now, next_hour, runtime;
   RUN *run;
   JOB *job;
   SCHED *sched;
   struct tm tm;
   int hour, mday, wday, month, wom, woy, ldom;
   /* Items corresponding to above at the next hour */
   int nh_hour, nh_mday, nh_wday, nh_month, nh_wom, nh_woy, nh_ldom;

   Dmsg0(dbglvl, "enter find_runs()\n");

   /* compute values for time now */
   now = time(NULL);
   (void)localtime_r(&now, &tm);
   hour = tm.tm_hour;
   mday = tm.tm_mday - 1;
   wday = tm.tm_wday;
   month = tm.tm_mon;
   wom = mday / 7;
   woy = tm_woy(now);                     /* get week of year */
   ldom = tm_ldom(month, tm.tm_year + 1900);

   Dmsg7(dbglvl, "now = %x: h=%d m=%d md=%d wd=%d wom=%d woy=%d\n",
         now, hour, month, mday, wday, wom, woy);

   /*
    * Compute values for next hour from now.
    * We do this to be sure we don't miss a job while
    * sleeping.
    */
   next_hour = now + 3600;
   (void)localtime_r(&next_hour, &tm);
   nh_hour = tm.tm_hour;
   nh_mday = tm.tm_mday - 1;
   nh_wday = tm.tm_wday;
   nh_month = tm.tm_mon;
   nh_wom = nh_mday / 7;
   nh_woy = tm_woy(next_hour);              /* get week of year */
   nh_ldom = tm_ldom(nh_month, tm.tm_year + 1900);

   Dmsg7(dbglvl, "nh = %x: h=%d m=%d md=%d wd=%d wom=%d woy=%d\n",
         next_hour, nh_hour, nh_month, nh_mday, nh_wday, nh_wom, nh_woy);

   /* Loop through all jobs */
   LockRes();
   foreach_res(job, R_JOB) {
      sched = job->schedule;
      if (sched == NULL || !job->enabled) { /* scheduled? or enabled? */
         continue;                    /* no, skip this job */
      }
      Dmsg1(dbglvl, "Got job: %s\n", job->hdr.name);
      for (run=sched->run; run; run=run->next) {
         bool run_now, run_nh;
         /*
          * Find runs scheduled between now and the next hour.
          */
#ifdef xxxx
         Dmsg0(000, "\n");
         Dmsg7(000, "run h=%d m=%d md=%d wd=%d wom=%d woy=%d ldom=%d\n",
            hour, month, mday, wday, wom, woy, ldom);
         Dmsg7(000, "bitset bsh=%d bsm=%d bsmd=%d bswd=%d bswom=%d bswoy=%d bsldom=%d\n",
            bit_is_set(hour, run->hour),
            bit_is_set(month, run->month),
            bit_is_set(mday, run->mday),
            bit_is_set(wday, run->wday),
            bit_is_set(wom, run->wom),
            bit_is_set(woy, run->woy),
            bit_is_set(31, run->mday));


         Dmsg7(000, "nh_run h=%d m=%d md=%d wd=%d wom=%d woy=%d ldom=%d\n",
            nh_hour, nh_month, nh_mday, nh_wday, nh_wom, nh_woy, nh_ldom);
         Dmsg7(000, "nh_bitset bsh=%d bsm=%d bsmd=%d bswd=%d bswom=%d bswoy=%d bsldom=%d\n",
            bit_is_set(nh_hour, run->hour),
            bit_is_set(nh_month, run->month),
            bit_is_set(nh_mday, run->mday),
            bit_is_set(nh_wday, run->wday),
            bit_is_set(nh_wom, run->wom),
            bit_is_set(nh_woy, run->woy),
            bit_is_set(31, run->mday));
#endif

         run_now = bit_is_set(hour, run->hour) &&
            ((bit_is_set(mday, run->mday) &&
              bit_is_set(wday, run->wday) &&
              bit_is_set(month, run->month) &&
              bit_is_set(wom, run->wom) &&
              bit_is_set(woy, run->woy)) ||
             (bit_is_set(month, run->month) &&
              bit_is_set(31, run->mday) && mday == ldom));

         run_nh = bit_is_set(nh_hour, run->hour) &&
            ((bit_is_set(nh_mday, run->mday) &&
              bit_is_set(nh_wday, run->wday) &&
              bit_is_set(nh_month, run->month) &&
              bit_is_set(nh_wom, run->wom) &&
              bit_is_set(nh_woy, run->woy)) ||
             (bit_is_set(nh_month, run->month) &&
              bit_is_set(31, run->mday) && nh_mday == nh_ldom));

         Dmsg3(dbglvl, "run@%p: run_now=%d run_nh=%d\n", run, run_now, run_nh);

         if (run_now || run_nh) {
           /* find time (time_t) job is to be run */
           (void)localtime_r(&now, &tm);      /* reset tm structure */
           tm.tm_min = run->minute;     /* set run minute */
           tm.tm_sec = 0;               /* zero secs */
           runtime = mktime(&tm);
           if (run_now) {
             add_job(job, run, now, runtime);
           }
           /* If job is to be run in the next hour schedule it */
           if (run_nh) {
             add_job(job, run, now, runtime + 3600);
           }
         }
      }
   }
   UnlockRes();
   Dmsg0(dbglvl, "Leave find_runs()\n");
}

static void add_job(JOB *job, RUN *run, time_t now, time_t runtime)
{
   job_item *ji;
   bool inserted = false;
   /*
    * Don't run any job that ran less than a minute ago, but
    *  do run any job scheduled less than a minute ago.
    */
   if (((runtime - run->last_run) < 61) || ((runtime+59) < now)) {
#ifdef SCHED_DEBUG
      Dmsg4(000, "Drop: Job=\"%s\" run=%lld. last_run=%lld. now=%lld\n", job->hdr.name,
            (utime_t)runtime, (utime_t)run->last_run, (utime_t)now);
      fflush(stdout);
#endif
      return;
   }
#ifdef SCHED_DEBUG
   Dmsg4(000, "Add: Job=\"%s\" run=%lld last_run=%lld now=%lld\n", job->hdr.name,
            (utime_t)runtime, (utime_t)run->last_run, (utime_t)now);
#endif
   /* accept to run this job */
   job_item *je = (job_item *)malloc(sizeof(job_item));
   je->run = run;
   je->job = job;
   je->runtime = runtime;
   if (run->Priority) {
      je->Priority = run->Priority;
   } else {
      je->Priority = job->Priority;
   }

   /* Add this job to the wait queue in runtime, priority sorted order */
   foreach_dlist(ji, jobs_to_run) {
      if (ji->runtime > je->runtime ||
          (ji->runtime == je->runtime && ji->Priority > je->Priority)) {
         jobs_to_run->insert_before(je, ji);
         dump_job(je, _("Inserted job"));
         inserted = true;
         break;
      }
   }
   /* If place not found in queue, append it */
   if (!inserted) {
      jobs_to_run->append(je);
      dump_job(je, _("Appended job"));
   }
#ifdef SCHED_DEBUG
   foreach_dlist(ji, jobs_to_run) {
      dump_job(ji, _("Run queue"));
   }
   Dmsg0(000, "End run queue\n");
#endif
}

static void dump_job(job_item *ji, const char *msg)
{
#ifdef SCHED_DEBUG
   char dt[MAX_TIME_LENGTH];
   int64_t save_debug = debug_level;
   if (!chk_dbglvl(dbglvl)) {
      return;
   }
   bstrftime_nc(dt, sizeof(dt), ji->runtime);
   Dmsg4(dbglvl, "%s: Job=%s priority=%d run %s\n", msg, ji->job->hdr.name,
      ji->Priority, dt);
   fflush(stdout);
   debug_level = save_debug;
#endif
}
