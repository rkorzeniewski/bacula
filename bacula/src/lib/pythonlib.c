/*
 *
 * Bacula common code library interface to Python
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
#include "jcr.h"

#ifdef HAVE_PYTHON

#undef _POSIX_C_SOURCE
#include <Python.h>

/* Imported subroutines */
extern PyMethodDef JobMethods[];

static PyObject *bacula_module = NULL;    /* We create this */
static PyObject *StartUp_module = NULL;   /* We import this */

/* These are the daemon events or methods that are defined */
static PyObject *JobStart_method = NULL;
static PyObject *JobEnd_method = NULL;
static PyObject *Exit_method = NULL;

static PyObject *set_bacula_events(PyObject *self, PyObject *args);
static PyObject *bacula_write(PyObject *self, PyObject *args);

PyObject *find_method(PyObject *eventsObject, PyObject *method, const char *name);

/* Define Bacula daemon method entry points */
static PyMethodDef BaculaMethods[] = {
    {"set_events", set_bacula_events, METH_VARARGS, "Define Bacula events."},
    {"write", bacula_write, METH_VARARGS, "Write output."},
    {NULL, NULL, 0, NULL}             /* last item */
};


/*
 * This is a Bacula Job type as defined in Python. We store a pointer
 *   to the jcr. That is all we need, but the user's script may keep
 *   local data attached to this. 
 */
typedef struct s_JobObject {
    PyObject_HEAD
    JCR *jcr;
} JobObject;

static PyTypeObject JobType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "bacula.jcr",              /*tp_name*/
    sizeof(JobObject),         /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT, /*tp_flags*/
    "Job objects",             /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    JobMethods,                /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};

/* Return the JCR pointer from the JobObject */
JCR *get_jcr_from_PyObject(PyObject *self)
{
   return ((JobObject *)self)->jcr;
}


/* Start the interpreter */
void init_python_interpreter(const char *progname, const char *scripts,
    const char *module)
{
   char buf[MAXSTRING];

   if (!scripts || scripts[0] == 0) {
      Dmsg1(100, "No script dir. prog=%s\n", module);
      return;
   }
   Dmsg2(100, "Script dir=%s prog=%s\n", scripts, module);

   Py_SetProgramName((char *)progname);
   Py_Initialize();
   PyEval_InitThreads();
   bacula_module = Py_InitModule("bacula", BaculaMethods);
   if (!bacula_module) {
      Jmsg0(NULL, M_ERROR_TERM, 0, "Could not initialize Python\n");
   }
   bsnprintf(buf, sizeof(buf), "import sys\n"
            "sys.path.append('%s')\n", scripts);
   if (PyRun_SimpleString(buf) != 0) {
      Jmsg1(NULL, M_ERROR_TERM, 0, "Could not Run Python string %s\n", buf);
   }   
   JobType.tp_methods = JobMethods;
   if (PyType_Ready(&JobType) != 0) {
      Jmsg0(NULL, M_ERROR_TERM, 0, "Could not initialize Python Job type.\n");
      PyErr_Print();
   }   
   StartUp_module = PyImport_ImportModule((char *)module);
   if (!StartUp_module) {
      Emsg2(M_ERROR, 0, "Could not import Python script %s/%s. Python disabled.\n",
           scripts, module);
      if (PyErr_Occurred()) {
         PyErr_Print();
         Dmsg0(000, "Python Import error.\n");
      }
   }
   PyEval_ReleaseLock();
}


void term_python_interpreter()
{
   if (StartUp_module) {
      Py_XDECREF(StartUp_module);
      Py_Finalize();
   }
}

static PyObject *set_bacula_events(PyObject *self, PyObject *args)
{
   PyObject *eObject;

   Dmsg0(100, "In set_bacula_events.\n");
   if (!PyArg_ParseTuple(args, "O:set_bacula_events", &eObject)) {
      return NULL;
   }
   JobStart_method = find_method(eObject, JobStart_method, "JobStart");
   JobEnd_method = find_method(eObject, JobEnd_method, "JobEnd");
   Exit_method = find_method(eObject, Exit_method, "Exit");

   Py_XINCREF(eObject);
   Py_INCREF(Py_None);
   return Py_None;
}

/* Write text to daemon output */
static PyObject *bacula_write(PyObject *self, PyObject *args)
{
   char *text;
   if (!PyArg_ParseTuple(args, "s:write", &text)) {
      return NULL;
   }
   if (text) {
      Jmsg(NULL, M_INFO, 0, "%s", text);
   }
   Py_INCREF(Py_None);
   return Py_None;
}


/*
 * Check that a method exists and is callable.
 */
PyObject *find_method(PyObject *eventsObject, PyObject *method, const char *name)
{
   Py_XDECREF(method);             /* release old method if any */
   method = PyObject_GetAttrString(eventsObject, (char *)name);
   if (method == NULL) {
       Dmsg1(000, "Python method %s not found\n", name);
   } else if (PyCallable_Check(method) == 0) {
       Dmsg1(000, "Python object %s found but not a method.\n", name);
       Py_XDECREF(method);
       method = NULL;
   } else {
       Dmsg1(100, "Got method %s\n", name);
   }
   return method; 
}


/*
 * Generate and process a Bacula event by importing a Python
 *  module and running it.
 *
 *  Returns: 0 if Python not configured or module not found
 *          -1 on Python error
 *           1 OK
 */
int generate_daemon_event(JCR *jcr, const char *event)
{
   PyObject *pJob;
   int stat = -1;
   PyObject *result = NULL;

   if (!StartUp_module) {
      Dmsg0(100, "No startup module.\n");
      return 0;
   }

   Dmsg1(100, "event=%s\n", event);
   PyEval_AcquireLock();
   if (strcmp(event, "JobStart") == 0) {
      if (!JobStart_method) {
         stat = 0;
         goto bail_out;
      }
      /* Create JCR argument to send to function */
      pJob = (PyObject *)PyObject_New(JobObject, &JobType);
      if (!pJob) {
         Jmsg(jcr, M_ERROR, 0, "Could not create Python Job Object.\n");
         goto bail_out;
      }
      ((JobObject *)pJob)->jcr = jcr;
      bstrncpy(jcr->event, event, sizeof(jcr->event));
      result = PyObject_CallFunction(JobStart_method, "O", pJob);
      jcr->event[0] = 0;             /* no event in progress */
      if (result == NULL) {
         JobStart_method = NULL;
         if (PyErr_Occurred()) {
            PyErr_Print();
            Dmsg0(000, "Python JobStart error.\n");
         }
         Jmsg(jcr, M_ERROR, 0, "Python function \"%s\" not found.\n", event);
         Py_XDECREF(pJob);
         goto bail_out;
      }
      jcr->Python_job = (void *)pJob;
      stat = 1;                       /* OK */
      goto jobstart_ok;

   } else if (strcmp(event, "JobEnd") == 0) {
      if (!JobEnd_method || !jcr->Python_job) {
         Dmsg0(100, "No JobEnd method\n");
         stat = 0;
         goto bail_out;
      }
      bstrncpy(jcr->event, event, sizeof(jcr->event));
      Dmsg1(100, "Call event=%s\n", event);
      result = PyObject_CallFunction(JobEnd_method, "O", jcr->Python_job);
      jcr->event[0] = 0;             /* no event in progress */
      if (result == NULL) {
         JobEnd_method = NULL;
         if (PyErr_Occurred()) {
            PyErr_Print();
            Dmsg1(000, "Python JobEnd error. JobId=%d\n", jcr->JobId);
         }
         Jmsg(jcr, M_ERROR, 0, "Python function \"%s\" not found.\n", event);
         goto bail_out;
      }
      stat = 1;                    /* OK */
   } else if (strcmp(event, "Exit") == 0) {
      if (!Exit_method) {
         stat = 0;
         goto bail_out;
      }
      result = PyObject_CallFunction(JobEnd_method, NULL);
      if (result == NULL) {
         goto bail_out;
      }
      stat = 1;                    /* OK */
   } else {
      Emsg1(M_ABORT, 0, "Unknown Python daemon event %s\n", event);
   }

bail_out:
   Py_XDECREF((PyObject *)jcr->Python_job);
   jcr->Python_job = NULL;
   Py_XDECREF((PyObject *)jcr->Python_events);
   jcr->Python_events = NULL;
   /* Fall through */
jobstart_ok:
   Py_XDECREF(result);
   PyEval_ReleaseLock();
   return stat; 
}

#ifdef xxx
   PyObject *pName, *pModule, *pDict, *pFunc;
   PyObject *pArgs, *pJCR, *pCall;
   
   Dmsg1(100, "Generate event %s\n", event);
   pName = PyString_FromString(event);
   if (!pName) {
      Jmsg(jcr, M_ERROR, 0, "Could not convert \"%s\" to Python string.\n", event);
      return -1;                      /* Could not convert string */
   }

   pModule = PyImport_Import(pName);
   Py_DECREF(pName);                  /* release pName */

   if (pModule != NULL) {
      pDict = PyModule_GetDict(pModule);
      /* pDict is a borrowed reference */

      pFunc = PyDict_GetItemString(pDict, (char *)event);
      /* pFun: Borrowed reference */

      if (pFunc && PyCallable_Check(pFunc)) {
          /* Create JCR argument to send to function */
          pArgs = PyTuple_New(1);
          pJCR = (PyObject *)PyObject_New(JCRObject, &JCRType);
          ((JCRObject *)pJCR)->jcr = jcr;
          if (!pJCR) {
             Py_DECREF(pArgs);
             Py_DECREF(pModule);
             Jmsg0(jcr, M_ERROR, 0, "Could not create JCR Python Object.\n");
             return -1;
          }
          Py_INCREF(pJCR);
          /* pJCR reference stolen here: */
          PyTuple_SetItem(pArgs, 0, pJCR);

          /* Finally, we call the module here */
          bstrncpy(jcr->event, event, sizeof(jcr->event));
          pCall = PyObject_CallObject(pFunc, pArgs);
          jcr->event[0] = 0;             /* no event in progress */
          Py_DECREF(pArgs);
          Py_DECREF(pJCR);
          if (pCall != NULL) {
             Py_DECREF(pCall);
          } else {
             Py_DECREF(pModule);
             PyErr_Print();
             Jmsg1(jcr, M_ERROR, 0, "Error running Python module: %s\n", event);
             return 0;                /* error running function */
          }
          /* pDict and pFunc are borrowed and must not be Py_DECREF-ed */
      } else {
         if (PyErr_Occurred()) {
            PyErr_Print();
            Dmsg1(000, "Python event %s function not callable.\n", event);
         }
         Jmsg1(jcr, M_ERROR, 0, "Python function \"%s\" not found in module.\n", event);
         return -1;                   /* function not found */
      }
      Py_DECREF(pModule);
   } else {
      return 0;                       /* Module not present */
   }
   Dmsg0(100, "Generate event OK\n");
   return 1;
}
#endif

#else

/*
 *  No Python configured -- create external entry points and
 *    dummy routines so that library code can call this without
 *    problems even if it is not configured.
 */
int generate_daemon_event(JCR *jcr, const char *event) { return 0; }
void init_python_interpreter(const char *progname, const char *scripts,
         const char *module) { }
void term_python_interpreter() { }

#endif /* HAVE_PYTHON */
