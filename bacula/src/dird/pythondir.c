/*
 *
 * Bacula interface to Python for the Director
 *
 * Kern Sibbald, November MMIV
 *
 *   Version $Id$
 *
 */

/*
   Copyright (C) 2004-2005 Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

 */

#include "bacula.h"
#include "dird.h"

#ifdef HAVE_PYTHON
#undef _POSIX_C_SOURCE
#include <Python.h>

extern JCR *get_jcr_from_PyObject(PyObject *self);
extern PyObject *find_method(PyObject *eventsObject, PyObject *method, 
         const char *name);


static int set_job_events(PyObject *self, PyObject *arg);
static int job_run(PyObject *self, PyObject *arg);

#ifdef needed
static PyObject *set_bacula_job_events(PyObject *self, PyObject *arg)
{
   Dmsg2(000, "In set_bacula_job_events self=%p arg=%p\n",
      self, arg);
   Py_INCREF(Py_None);
   return Py_None;
}
PyMethodDef JobMethods[] = {
    {"set_events", set_bacula_job_events, METH_VARARGS, "Define Bacula events."},
    {NULL, NULL, 0, NULL}             /* last item */
};
#endif
 

struct s_vars {
   const char *name;
   char *fmt;
};

/* Read-only variables */
static struct s_vars getvars[] = {
   { N_("Job"),        "s"},
   { N_("DirName"),    "s"},
   { N_("Level"),      "s"},
   { N_("Type"),       "s"},
   { N_("JobId"),      "i"},
   { N_("Client"),     "s"},
   { N_("NumVols"),    "i"},
   { N_("Pool"),       "s"},
   { N_("Storage"),    "s"},
   { N_("Catalog"),    "s"},
   { N_("MediaType"),  "s"},
   { N_("JobName"),    "s"},
   { N_("JobStatus"),  "s"},

   { NULL,             NULL}
};

/* Writable variables */
static struct s_vars setvars[] = {
   { N_("set_events"), NULL},
   { N_("run"),        NULL},
   { N_("JobReport"),   "s"},
   { N_("write"),       "s"},
   { N_("VolumeName") , "s"},

   { NULL,             NULL}
};


/* Return Job variables */
/* Returns:  NULL if error
 *           PyObject * return value if OK
 */
PyObject *job_getattr(PyObject *self, char *attrname)
{
   JCR *jcr;
   bool found = false;
   int i;
   char buf[10];
   char errmsg[200];

   Dmsg0(100, "In job_getattr.\n");
   jcr = get_jcr_from_PyObject(self);
   if (!jcr) {
      bstrncpy(errmsg, "Job pointer not found.", sizeof(errmsg));
      goto bail_out;
   }
   for (i=0; getvars[i].name; i++) {
      if (strcmp(getvars[i].name, attrname) == 0) {
         found = true;
         break;
      }
   }
   if (!found) {
      goto not_found;
   }
   switch (i) {
   case 0:                            /* Job */
      return Py_BuildValue(getvars[i].fmt, jcr->job->hdr.name);
   case 1:                            /* Director's name */
      return Py_BuildValue(getvars[i].fmt, my_name);
   case 2:                            /* level */
      return Py_BuildValue(getvars[i].fmt, job_level_to_str(jcr->JobLevel));
   case 3:                            /* type */
      return Py_BuildValue(getvars[i].fmt, job_type_to_str(jcr->JobType));
   case 4:                            /* JobId */
      return Py_BuildValue(getvars[i].fmt, jcr->JobId);
   case 5:                            /* Client */
      return Py_BuildValue(getvars[i].fmt, jcr->client->hdr.name);
   case 6:                            /* NumVols */
      return Py_BuildValue(getvars[i].fmt, jcr->NumVols);
   case 7:                            /* Pool */
      return Py_BuildValue(getvars[i].fmt, jcr->pool->hdr.name);
   case 8:                            /* Storage */
      return Py_BuildValue(getvars[i].fmt, jcr->store->hdr.name);
   case 9:
      return Py_BuildValue(getvars[i].fmt, jcr->catalog->hdr.name);
   case 10:                           /* MediaType */
      return Py_BuildValue(getvars[i].fmt, jcr->store->media_type);
   case 11:                           /* JobName */
      return Py_BuildValue(getvars[i].fmt, jcr->Job);
   case 12:                           /* JobStatus */
      buf[1] = 0;
      buf[0] = jcr->JobStatus;
      return Py_BuildValue(getvars[i].fmt, buf);
   }
not_found:
   bsnprintf(errmsg, sizeof(errmsg), "Attribute %s not found.", attrname);
bail_out:
   PyErr_SetString(PyExc_AttributeError, errmsg);
   return NULL;
}


/* Set Job variables */
/*  Returns:   0 for OK
 *            -1 for error
 */
int job_setattr(PyObject *self, char *attrname, PyObject *value)
{
   JCR *jcr;
   bool found = false;
   char *strval = NULL;
   int i;

   Dmsg2(100, "In job_setattr=%s val=%p.\n", attrname, value);
   if (value == NULL) {                /* Cannot delete variables */
       goto bail_out;
   }
   jcr = get_jcr_from_PyObject(self);
   if (!jcr) {
      goto bail_out;
   }

   /* Find attribute name in list */
   for (i=0; setvars[i].name; i++) {
      if (strcmp(setvars[i].name, attrname) == 0) {
         found = true;
         break;
      }
   }
   if (!found) {
      goto bail_out;
   }
   /* Get argument value ***FIXME*** handle other formats */
   if (setvars[i].fmt != NULL) {
      if (!PyArg_Parse(value, setvars[i].fmt, &strval)) {
         PyErr_SetString(PyExc_TypeError, "Read-only attribute");
         return -1;
      }
   }   
   switch (i) {
   case 0:                            /* set_events */
      return set_job_events(self, value);
   case 1:                            /* run */
      return job_run(self, value);
   case 2:                            /* JobReport */
   case 3:                            /* write */
      Jmsg(jcr, M_INFO, 0, "%s", strval);
      return 0;
   case 4:                            /* VolumeName */
      /* Make sure VolumeName is valid and we are in VolumeName event */
      if (strcmp("NewVolume", jcr->event) == 0 &&
          is_volume_name_legal(NULL, strval)) {
         pm_strcpy(jcr->VolumeName, strval);
         Dmsg1(100, "Set Vol=%s\n", strval);
         return 0;
      } else {
         jcr->VolumeName[0] = 0;
      }
   }
bail_out:
   PyErr_SetString(PyExc_AttributeError, attrname);
   return -1;
}

#ifdef needed
static PyObject *set_bacula_job_events(PyObject *self, PyObject *arg)
{
   Dmsg2(000, "In set_bacula_job_events self=%p arg=%p\n",
      self, arg);
   Py_INCREF(Py_None);
   return Py_None;
}
#endif


static int set_job_events(PyObject *self, PyObject *arg)
{
   PyObject *eObject;
   JCR *jcr;

   Dmsg0(100, "In set_job_events.\n");
   if (!PyArg_Parse(arg, "O", &eObject)) {
      return -1;
   }
   jcr = get_jcr_from_PyObject(self);
   Py_XDECREF((PyObject *)jcr->Python_events);
   Py_INCREF(eObject);
   jcr->Python_events = (void *)eObject;
   return 0;                    /* good return */
}

/* Run a Bacula command */
static int job_run(PyObject *self, PyObject *arg)
{
   JCR *jcr;
   char *item;
   int stat;

   if (!PyArg_Parse(arg, "s", &item)) {
      return -1;
   }
   jcr = get_jcr_from_PyObject(self);
   UAContext *ua = new_ua_context(jcr);
   ua->batch = true;
   pm_strcpy(ua->cmd, item);          /* copy command */
   parse_ua_args(ua);                 /* parse command */
   stat = run_cmd(ua, ua->cmd);
   free_ua_context(ua);
   /* ***FIXME*** check stat */
   return 0;
}


int generate_job_event(JCR *jcr, const char *event)
{
   PyObject *method = NULL;
   PyObject *Job = (PyObject *)jcr->Python_job;
   PyObject *result = NULL;
   int stat = 0;

   if (!Job) {
      return 0;
   }

   PyEval_AcquireLock();

   PyObject *events = (PyObject *)jcr->Python_events;
   method = find_method(events, method, event);
   if (!method) {
      goto bail_out;
   }

   bstrncpy(jcr->event, event, sizeof(jcr->event));
   result = PyObject_CallFunction(method, "O", Job);
   jcr->event[0] = 0;             /* no event in progress */
   if (result == NULL) {
      if (PyErr_Occurred()) {
         PyErr_Print();
         Dmsg1(000, "Error in Python method %s\n", event);
      }
   } else {
      stat = 1;
   }
   Py_XDECREF(result);

bail_out:
   PyEval_ReleaseLock();
   return stat;
}

bool python_set_prog(JCR*, char const*) { return false; }

#else

/* Dummy if Python not configured */
int generate_job_event(JCR *jcr, const char *event) { return 1; }
   

#endif /* HAVE_PYTHON */
