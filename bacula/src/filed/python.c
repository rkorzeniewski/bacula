/*
 *
 * Bacula interface to Python for the File Daemon
 *
 * Kern Sibbald, March MMV
 *
 *   Version $Id$
 *
 */

/*
   Copyright (C) 2005 Kern Sibbald

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
#include "filed.h"

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


/* Define Job entry points */
PyMethodDef JobMethods[] = {
    {"get", job_get, METH_VARARGS, "Get Job variables."},
    {"set", (PyCFunction)job_set, METH_VARARGS|METH_KEYWORDS,
        "Set Job variables."},
    {"set_events", set_job_events, METH_VARARGS, "Define Job events."},
    {"write", job_write, METH_VARARGS, "Write output."},
    {NULL, NULL, 0, NULL}             /* last item */
};

struct s_vars {
   const char *name;
   char *fmt;
};

/* Read-only variables */
static struct s_vars vars[] = {
   { N_("FDName"),     "s"},          /* 0 */
   { N_("Level"),      "s"},          /* 1 */
   { N_("Type"),       "s"},          /* 2 */
   { N_("JobId"),      "i"},          /* 3 */
   { N_("Client"),     "s"},          /* 4 */
   { N_("JobName"),    "s"},          /* 5 */
   { N_("JobStatus"),  "s"},          /* 6 */

   { NULL,             NULL}
};

/* Return Job variables */
PyObject *job_get(PyObject *self, PyObject *args)
{
   JCR *jcr;
   char *item;
   bool found = false;
   int i;
   char buf[10];

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
   case 0:                            /* FD's name */
      return Py_BuildValue(vars[i].fmt, my_name);
   case 1:                            /* level */
      return Py_BuildValue(vars[i].fmt, job_level_to_str(jcr->JobLevel));
   case 2:                            /* type */
      return Py_BuildValue(vars[i].fmt, job_type_to_str(jcr->JobType));
   case 3:                            /* JobId */
      return Py_BuildValue(vars[i].fmt, jcr->JobId);
   case 4:                            /* Client */
      return Py_BuildValue(vars[i].fmt, jcr->client_name);
   case 5:                            /* JobName */
      return Py_BuildValue(vars[i].fmt, jcr->Job);
   case 6:                            /* JobStatus */
      buf[1] = 0;
      buf[0] = jcr->JobStatus;
      return Py_BuildValue(vars[i].fmt, buf);
   }
   return NULL;
}

/* Set Job variables */
PyObject *job_set(PyObject *self, PyObject *args, PyObject *keyw)
{
   JCR *jcr;
   char *msg = NULL;
   static char *kwlist[] = {"JobReport", NULL};
   
   if (!PyArg_ParseTupleAndKeywords(args, keyw, "|s:set", kwlist,
        &msg)) {
      return NULL;
   }
   jcr = get_jcr_from_PyObject(self);
   
   if (msg) {
      Jmsg(jcr, M_INFO, 0, "%s", msg);
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
   char *text = NULL;

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


bool python_set_prog(JCR *jcr, const char *prog)
{
   PyObject *events = (PyObject *)jcr->Python_events;
   BFILE *bfd = &jcr->ff->bfd;
   char method[MAX_NAME_LENGTH];

   if (!events) {
      return false;
   }
   bstrncpy(method, prog, sizeof(method));
   bstrncat(method, "_", sizeof(method));
   bstrncat(method, "open", sizeof(method));
   bfd->pio.fo = find_method(events, bfd->pio.fo, method);
   bstrncpy(method, prog, sizeof(method));
   bstrncat(method, "_", sizeof(method));
   bstrncat(method, "read", sizeof(method));
   bfd->pio.fr = find_method(events, bfd->pio.fr, method);
   bstrncpy(method, prog, sizeof(method));
   bstrncat(method, "_", sizeof(method));
   bstrncat(method, "close", sizeof(method));
   bfd->pio.fc = find_method(events, bfd->pio.fc, method);
   return bfd->pio.fo && bfd->pio.fr && bfd->pio.fc;
}



#else

/* Dummy if Python not configured */
int generate_job_event(JCR *jcr, const char *event)
{ return 1; }


#endif /* HAVE_PYTHON */
