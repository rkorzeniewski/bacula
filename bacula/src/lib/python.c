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

EVENT_HANDLER *generate_event;

#ifdef HAVE_PYTHON

#undef _POSIX_C_SOURCE
#include <Python.h>


PyObject *bacula_get(PyObject *self, PyObject *args);
PyObject *bacula_set(PyObject *self, PyObject *args, PyObject *keyw);
int _generate_event(JCR *jcr, const char *event);

/* Pull in Bacula entry points */
extern PyMethodDef BaculaMethods[];

typedef struct bJCRObject {
    PyObject_HEAD
    JCR *jcr;
} bJCRObject;

static PyTypeObject JCRType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "bacula.jcr",              /*tp_name*/
    sizeof(bJCRObject),        /*tp_basicsize*/
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
    "JCR objects",             /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    0,                         /* tp_methods */
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

JCR *get_jcr_from_PyObject(PyObject *self)
{
   return ((bJCRObject *)self)->jcr;
}

/* Start the interpreter */
void init_python_interpreter(const char *progname, const char *scripts)
{
   char buf[MAXSTRING];
   Py_SetProgramName((char *)progname);
   Py_Initialize();
   PyEval_InitThreads();
   Py_InitModule("bacula", BaculaMethods);
   bsnprintf(buf, sizeof(buf), "import sys\n"
            "sys.path.append('%s')\n", scripts);
   PyRun_SimpleString(buf);
   JCRType.tp_methods = BaculaMethods;
   PyType_Ready(&JCRType);
   PyEval_ReleaseLock();
   generate_event = _generate_event;
}

void term_python_interpreter()
{
   Py_Finalize();
}


/*
 * Generate and process a Bacula event by importing a Python
 *  module and running it.
 *
 *  Returns: 0 if Python not configured or module not found
 *          -1 on Python error
 *           1 OK
 */
int _generate_event(JCR *jcr, const char *event)
{
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
          pJCR = (PyObject *)PyObject_New(bJCRObject, &JCRType);
          ((bJCRObject *)pJCR)->jcr = jcr;
//        pValue = PyCObject_FromVoidPtr((void *)jcr, NULL);
          if (!pJCR) {
             Py_DECREF(pArgs);
             Py_DECREF(pModule);
             Jmsg(jcr, M_ERROR, 0, "Could not create JCR Python Object.\n");
             return -1;
          }
          Py_INCREF(pJCR);
          /* pJCR reference stolen here: */
          PyTuple_SetItem(pArgs, 0, pJCR);

          /* Finally, we call the module here */
          pCall = PyObject_CallObject(pFunc, pArgs);
          Py_DECREF(pArgs);
          Py_DECREF(pJCR);
          if (pCall != NULL) {
             Py_DECREF(pCall);
          } else {
             Py_DECREF(pModule);
             PyErr_Print();
             Jmsg(jcr, M_ERROR, 0, "Error running Python module: %s\n", event);
             return 0;                /* error running function */
          }
          /* pDict and pFunc are borrowed and must not be Py_DECREF-ed */
      } else {
         if (PyErr_Occurred()) {
            PyErr_Print();
         }
         Jmsg(jcr, M_ERROR, 0, "Python function \"%s\" not found in module.\n", event);
         return -1;                   /* function not found */
      }
      Py_DECREF(pModule);
   } else {
      return 0;                       /* Module not present */
   }
   Dmsg0(100, "Generate event OK\n");
   return 1;
}

#else

/*
 *  No Python configured
 */

int _generate_event(JCR *jcr, const char *event) { return 0; }
void init_python_interpreter(const char *progname, const char *scripts)
{
   generate_event = _generate_event;   
}
void term_python_interpreter() { }


#endif /* HAVE_PYTHON */
