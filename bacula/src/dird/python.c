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

static PyObject *job_get(PyObject *self, PyObject *args);
static PyObject *job_write(PyObject *self, PyObject *args);
static PyObject *job_set(PyObject *self, PyObject *args, PyObject *keyw);
static PyObject *set_job_events(PyObject *self, PyObject *args);
static PyObject *jcr_run(PyObject *self, PyObject *args);

/* Define Job entry points */
PyMethodDef JobMethods[] = {
    {"get", job_get, METH_VARARGS, "Get Job variables."},
    {"set", (PyCFunction)job_set, METH_VARARGS|METH_KEYWORDS,
        "Set Job variables."},
    {"set_events", set_job_events, METH_VARARGS, "Define Job events."},
    {"write", job_write, METH_VARARGS, "Write output."},
    {"run", (PyCFunction)jcr_run, METH_VARARGS, "Run a Bacula command."},
    {NULL, NULL, 0, NULL}             /* last item */
};


struct s_vars {
   const char *name;
   char *fmt;
};

/* Read-only variables */
static struct s_vars vars[] = {
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

/* Return Job variables */
static PyObject *job_get(PyObject *self, PyObject *args)
{
   JCR *jcr;
   char *item;
   bool found = false;
   int i;
   char buf[10];

   Dmsg0(100, "In job_get.\n");
   if (!PyArg_ParseTuple(args, "s:get", &item)) {
      return NULL;
   }
   jcr = get_jcr_from_PyObject(self);
   for (i=0; vars[i].name; i++) {
      if (strcmp(vars[i].name, item) == 0) {
         found = true;
         break;
      }
   }
   if (!found) {
      return NULL;
   }
   switch (i) {
   case 0:                            /* Job */
      return Py_BuildValue(vars[i].fmt, jcr->job->hdr.name);
   case 1:                            /* Director's name */
      return Py_BuildValue(vars[i].fmt, my_name);
   case 2:                            /* level */
      return Py_BuildValue(vars[i].fmt, job_level_to_str(jcr->JobLevel));
   case 3:                            /* type */
      return Py_BuildValue(vars[i].fmt, job_type_to_str(jcr->JobType));
   case 4:                            /* JobId */
      return Py_BuildValue(vars[i].fmt, jcr->JobId);
   case 5:                            /* Client */
      return Py_BuildValue(vars[i].fmt, jcr->client->hdr.name);
   case 6:                            /* NumVols */
      return Py_BuildValue(vars[i].fmt, jcr->NumVols);
   case 7:                            /* Pool */
      return Py_BuildValue(vars[i].fmt, jcr->pool->hdr.name);
   case 8:                            /* Storage */
      return Py_BuildValue(vars[i].fmt, jcr->store->hdr.name);
   case 9:
      return Py_BuildValue(vars[i].fmt, jcr->catalog->hdr.name);
   case 10:                           /* MediaType */
      return Py_BuildValue(vars[i].fmt, jcr->store->media_type);
   case 11:                           /* JobName */
      return Py_BuildValue(vars[i].fmt, jcr->Job);
   case 12:                           /* JobStatus */
      buf[1] = 0;
      buf[0] = jcr->JobStatus;
      return Py_BuildValue(vars[i].fmt, buf);
   }
   return NULL;
}

/* Set Job variables */
static PyObject *job_set(PyObject *self, PyObject *args, PyObject *keyw)
{
   JCR *jcr;
   char *msg = NULL;
   char *VolumeName = NULL;
   static char *kwlist[] = {"JobReport", "VolumeName", NULL};

   Dmsg0(100, "In job_set.\n");
   if (!PyArg_ParseTupleAndKeywords(args, keyw, "|ss:set", kwlist,
        &msg, &VolumeName)) {
      return NULL;
   }
   jcr = get_jcr_from_PyObject(self);

   if (msg) {
      Jmsg(jcr, M_INFO, 0, "%s", msg);
   }

   if (!VolumeName) {
      Dmsg1(100, "No VolumeName. Event=%s\n", jcr->event);
   } else {
      Dmsg2(100, "Vol=%s Event=%s\n", VolumeName, jcr->event);
   }
   /* Make sure VolumeName is valid and we are in VolumeName event */
   if (VolumeName && strcmp("NewVolume", jcr->event) == 0 &&
       is_volume_name_legal(NULL, VolumeName)) {
      pm_strcpy(jcr->VolumeName, VolumeName);
      Dmsg1(100, "Set Vol=%s\n", VolumeName);
   } else if (VolumeName) {
      jcr->VolumeName[0] = 0;
      Dmsg0(100, "Zap VolumeName.\n");
      return Py_BuildValue("i", 0);  /* invalid volume name */
   }
   return Py_BuildValue("i", 1);
}

static PyObject *set_job_events(PyObject *self, PyObject *args)
{
   PyObject *eObject;
   JCR *jcr;

   Dmsg0(100, "In set_job_events.\n");
   if (!PyArg_ParseTuple(args, "O:set_events", &eObject)) {
      return NULL;
   }
   jcr = get_jcr_from_PyObject(self);
   Py_XDECREF((PyObject *)jcr->Python_events);
   Py_INCREF(eObject);
   jcr->Python_events = (void *)eObject;

   Py_INCREF(Py_None);
   return Py_None;
}

/* Write text to job output */
static PyObject *job_write(PyObject *self, PyObject *args)
{
   char *text;

   if (!PyArg_ParseTuple(args, "s:write", &text)) {
      return NULL;
   }
   if (text) {
      JCR *jcr = get_jcr_from_PyObject(self);
      Jmsg(jcr, M_INFO, 0, "%s", text);
   }
        
   Py_INCREF(Py_None);
   return Py_None;
}

/* Run a Bacula command */
static PyObject *jcr_run(PyObject *self, PyObject *args)
{
   JCR *jcr;
   char *item;
   int stat;

   if (!PyArg_ParseTuple(args, "s:get", &item)) {
      return NULL;
   }
   jcr = get_jcr_from_PyObject(self);
   UAContext *ua = new_ua_context(jcr);
   ua->batch = true;
   pm_strcpy(ua->cmd, item);          /* copy command */
   parse_ua_args(ua);                 /* parse command */
   stat = run_cmd(ua, ua->cmd);
   free_ua_context(ua);
   return Py_BuildValue("i", stat);
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
