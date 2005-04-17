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
extern PyObject *find_method(PyObject *eventsObject, PyObject *method, char *name);

static PyObject *jcr_get(PyObject *self, PyObject *args);
static PyObject *jcr_write(PyObject *self, PyObject *args);
static PyObject *jcr_set(PyObject *self, PyObject *args, PyObject *keyw);
static PyObject *set_jcr_events(PyObject *self, PyObject *args);


/* Define Job entry points */
PyMethodDef JobMethods[] = {
    {"get", jcr_get, METH_VARARGS, "Get Job variables."},
    {"set", (PyCFunction)jcr_set, METH_VARARGS|METH_KEYWORDS,
        "Set Job variables."},
    {"set_events", set_jcr_events, METH_VARARGS, "Define Job events."},
    {"write", jcr_write, METH_VARARGS, "Write output."},
    {NULL, NULL, 0, NULL}             /* last item */
};


static PyObject *open_method = NULL;
static PyObject *read_method = NULL;
static PyObject *close_method = NULL;


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
PyObject *jcr_get(PyObject *self, PyObject *args)
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
PyObject *jcr_set(PyObject *self, PyObject *args, PyObject *keyw)
{
   JCR *jcr;
   char *msg = NULL;
   PyObject *fo = NULL, *fr = NULL, *fc = NULL;
   static char *kwlist[] = {"JobReport", 
                 "FileOpen", "FileRead", "FileClose",
                 NULL};
   
   if (!PyArg_ParseTupleAndKeywords(args, keyw, "|sOOO:set", kwlist,
        &msg, &fo, &fr, &fc)) {
      return NULL;
   }
   jcr = get_jcr_from_PyObject(self);
   
   if (msg) {
      Jmsg(jcr, M_INFO, 0, "%s", msg);
   }
   if (strcmp("Reader", jcr->event) == 0) {
      if (fo) {
         if (PyCallable_Check(fo)) {
            jcr->ff->bfd.pio.fo = fo;
         } else {
            Jmsg(jcr, M_ERROR, 0, _("Python FileOpen object not callable.\n"));
         }
      }
      if (fr) {
         if (PyCallable_Check(fo)) {
            jcr->ff->bfd.pio.fr = fr;
         } else {
            Jmsg(jcr, M_ERROR, 0, _("Python FileRead object not callable.\n"));
         }
      }
      if (fc) {
         if (PyCallable_Check(fc)) {
            jcr->ff->bfd.pio.fc = fc;
         } else {
            Jmsg(jcr, M_ERROR, 0, _("Python FileClose object not callable.\n"));
         }
      }
   } 
   return Py_BuildValue("i", 1);
}


static PyObject *set_jcr_events(PyObject *self, PyObject *args)
{
   PyObject *eObject;
   JCR *jcr;
   if (!PyArg_ParseTuple(args, "O:set_events_hook", &eObject)) {
      return NULL;
   }
   Py_XINCREF(eObject);
   jcr = get_jcr_from_PyObject(self);
   jcr->ff->bfd.pio.fo = find_method(eObject, open_method, "open");
   jcr->ff->bfd.pio.fr = find_method(eObject, read_method, "read");
   jcr->ff->bfd.pio.fc = find_method(eObject, close_method, "close");
   Py_INCREF(Py_None);
   return Py_None;
}

/* Write text to job output */
static PyObject *jcr_write(PyObject *self, PyObject *args)
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

int generate_job_event(JCR *jcr, const char *event)
{
   PyEval_AcquireLock();

   PyObject *result = PyObject_CallFunction(open_method, "s", "m.py");
   if (result == NULL) {
      PyErr_Print();
      PyErr_Clear();
   }
   Py_XDECREF(result);

   PyEval_ReleaseLock();
   return 1;
}

#endif /* HAVE_PYTHON */
