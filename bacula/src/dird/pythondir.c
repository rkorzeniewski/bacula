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


static PyObject *set_job_events(PyObject *self, PyObject *arg);
static PyObject *job_run(PyObject *self, PyObject *arg);
static PyObject *job_write(PyObject *self, PyObject *arg);

PyMethodDef JobMethods[] = {
    {"set_events", set_job_events, METH_VARARGS, "Set Job events"},
    {"run", job_run, METH_VARARGS, "Run a Job"},
    {"write", job_write, METH_VARARGS, "Write to output"},
    {NULL, NULL, 0, NULL}             /* last item */
};
 

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
   { N_("Priority"),   "i"},

   { NULL,             NULL}
};

/* Writable variables */
static struct s_vars setvars[] = {
   { N_("JobReport"),   "s"},
   { N_("VolumeName"),  "s"},
   { N_("Priority"),    "i"},

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
      /* Try our methods */
      return Py_FindMethod(JobMethods, self, attrname);
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
   case 13:                           /* Priority */
      return Py_BuildValue(getvars[i].fmt, jcr->JobPriority);
   }
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
   int intval = 0;
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
   /* Get argument value */
   if (setvars[i].fmt != NULL) {
      switch (setvars[i].fmt[0]) {
      case 's':
         if (!PyArg_Parse(value, setvars[i].fmt, &strval)) {
            PyErr_SetString(PyExc_TypeError, "Read-only attribute");
            return -1;
         }
         break;
      case 'i':
         if (!PyArg_Parse(value, setvars[i].fmt, &intval)) {
            PyErr_SetString(PyExc_TypeError, "Read-only attribute");
            return -1;
         }
         break;
      }
   }   
   switch (i) {
   case 0:                            /* JobReport */
      Jmsg(jcr, M_INFO, 0, "%s", strval);
      return 0;
   case 1:                            /* VolumeName */
      /* Make sure VolumeName is valid and we are in VolumeName event */
      if (strcmp("NewVolume", jcr->event) == 0 &&
          is_volume_name_legal(NULL, strval)) {
         pm_strcpy(jcr->VolumeName, strval);
         Dmsg1(100, "Set Vol=%s\n", strval);
         return 0;
      } else {
         jcr->VolumeName[0] = 0;
      }
      break;
   case 2:                            /* Priority */
      Dmsg1(000, "Set priority=%d\n", intval);
      return 0;
   }
bail_out:
   PyErr_SetString(PyExc_AttributeError, attrname);
   return -1;
}


static PyObject *set_job_events(PyObject *self, PyObject *arg)
{
   PyObject *eObject;
   JCR *jcr;

   Dmsg0(100, "In set_job_events.\n");
   if (!PyArg_ParseTuple(arg, "O:set_events", &eObject)) {
      Dmsg0(000, "Error in ParseTuple\n");
      return NULL;
   }
   jcr = get_jcr_from_PyObject(self);
   Py_XDECREF((PyObject *)jcr->Python_events);
   Py_INCREF(eObject);
   jcr->Python_events = (void *)eObject;
   Py_INCREF(Py_None);
   return Py_None;
}

/* Run a Bacula command */
static PyObject *job_run(PyObject *self, PyObject *arg)
{
   JCR *jcr;
   char *item;
   int stat;

   if (!PyArg_ParseTuple(arg, "s:run", &item)) {
      Dmsg0(000, "Error in ParseTuple\n");
      return NULL;
   }
   /* Release lock due to recursion */
   PyEval_ReleaseLock();
   jcr = get_jcr_from_PyObject(self);
   UAContext *ua = new_ua_context(jcr);
   ua->batch = true;
   pm_strcpy(ua->cmd, item);          /* copy command */
   parse_ua_args(ua);                 /* parse command */
   stat = run_cmd(ua, ua->cmd);
   free_ua_context(ua);
   PyEval_AcquireLock();
   return PyInt_FromLong((long)stat);
}

static PyObject *job_write(PyObject *self, PyObject *args)
{
   char *text = NULL;

   if (!PyArg_ParseTuple(args, "s:write", &text)) {
      Dmsg0(000, "Parse tuple error in job_write\n");
      return NULL;
   }
   if (text) {
      Jmsg(NULL, M_INFO, 0, "%s", text);
   }
   Py_INCREF(Py_None);
   return Py_None;
}


/*
 * Generate a Job event, which means look up the event
 *  method defined by the user, and if it exists, 
 *  call it.
 */
int generate_job_event(JCR *jcr, const char *event)
{
   PyObject *method = NULL;
   PyObject *Job = (PyObject *)jcr->Python_job;
   PyObject *events = (PyObject *)jcr->Python_events;
   PyObject *result = NULL;
   int stat = 0;

   if (!Job || !events) {
      return 0;
   }

   PyEval_AcquireLock();

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
