#include <Python.h>
#include <ev.h>

typedef struct libevwrapper_Loop {
    PyObject_HEAD
    struct ev_loop *loop;
} libevwrapper_Loop;

static void
Loop_dealloc(libevwrapper_Loop *self) {
    self->ob_type->tp_free((PyObject *)self);
};

static PyObject*
Loop_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
    libevwrapper_Loop *self;

    self = (libevwrapper_Loop *)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->loop = ev_default_loop(0);
        if (!self->loop) {
            PyErr_SetString(PyExc_Exception, "Error getting default ev loop");
            return NULL;
        }
    }
    return (PyObject *)self;
};

static int
Loop_init(libevwrapper_Loop *self, PyObject *args, PyObject *kwds) {
    if (!PyArg_ParseTuple(args, "")) {
        PyErr_SetString(PyExc_TypeError, "Loop.__init__() takes no arguments");
        return -1;
    }
    return 0;
};

static PyObject *
Loop_start(libevwrapper_Loop *self) {
    Py_BEGIN_ALLOW_THREADS
    ev_run(self->loop, 0);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
};

static PyObject *
Loop_unref(libevwrapper_Loop *self) {
    ev_unref(self->loop);
    Py_RETURN_NONE;
}

static PyMethodDef Loop_methods[] = {
    {"start", (PyCFunction)Loop_start, METH_NOARGS, "Start the event loop"},
    {"unref", (PyCFunction)Loop_unref, METH_NOARGS, "Unrefrence the event loop"},
    {NULL} /* Sentinel */
};

static PyTypeObject libevwrapper_LoopType = {
    PyObject_HEAD_INIT(NULL)
    0,                               /*ob_size*/
    "cassandra.io.libevwrapper.Loop",/*tp_name*/
    sizeof(libevwrapper_Loop),       /*tp_basicsize*/
    0,                               /*tp_itemsize*/
    (destructor)Loop_dealloc,        /*tp_dealloc*/
    0,                               /*tp_print*/
    0,                               /*tp_getattr*/
    0,                               /*tp_setattr*/
    0,                               /*tp_compare*/
    0,                               /*tp_repr*/
    0,                               /*tp_as_number*/
    0,                               /*tp_as_sequence*/
    0,                               /*tp_as_mapping*/
    0,                               /*tp_hash */
    0,                               /*tp_call*/
    0,                               /*tp_str*/
    0,                               /*tp_getattro*/
    0,                               /*tp_setattro*/
    0,                               /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "Loop objects",                  /* tp_doc */
    0,                               /* tp_traverse */
    0,                               /* tp_clear */
    0,                               /* tp_richcompare */
    0,                               /* tp_weaklistoffset */
    0,                               /* tp_iter */
    0,                               /* tp_iternext */
    Loop_methods,                    /* tp_methods */
    0,                               /* tp_members */
    0,                               /* tp_getset */
    0,                               /* tp_base */
    0,                               /* tp_dict */
    0,                               /* tp_descr_get */
    0,                               /* tp_descr_set */
    0,                               /* tp_dictoffset */
    (initproc)Loop_init,             /* tp_init */
    0,                               /* tp_alloc */
    Loop_new,                        /* tp_new */
};

typedef struct libevwrapper_IO {
    PyObject_HEAD
    struct ev_io io;
    struct libevwrapper_Loop *loop;
    PyObject *callback;
} libevwrapper_IO;

static void
IO_dealloc(libevwrapper_IO *self) {
    Py_XDECREF(self->loop);
    Py_XDECREF(self->callback);
    self->ob_type->tp_free((PyObject *)self);
};

static void io_callback(struct ev_loop *loop, ev_io *watcher, int revents) {
    if (revents & EV_ERROR) {
        if (errno) {
            PyErr_SetFromErrno(PyExc_IOError);
        } else {
            PyErr_SetString(PyExc_IOError, "libev errored");
        }
    }

    libevwrapper_IO *self = watcher->data;

    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();

    PyObject *result = PyObject_CallFunction(self->callback, "Ob", self, revents);
    if (!result) {
        PyErr_WriteUnraisable(self->callback);
    }
    Py_XDECREF(result);

    PyGILState_Release(gstate);
};

static int
IO_init(libevwrapper_IO *self, PyObject *args, PyObject *kwds) {
    PyObject *socket;
    PyObject *callback;
    libevwrapper_Loop *loop;
    int io_flags = 0;

    if (!PyArg_ParseTuple(args, "ObOO", &socket, &io_flags, &loop, &callback)) {
        return -1;
    }

    if (loop) {
        Py_INCREF(loop);
        self->loop = (libevwrapper_Loop *)loop;
    }

    if (callback) {
        if (!PyCallable_Check(callback)) {
            PyErr_SetString(PyExc_TypeError, "callback parameter must be callable");
            Py_XDECREF(loop);
            return -1;
        }
        Py_INCREF(callback);
        self->callback = callback;
    }

    int fd = PyObject_AsFileDescriptor(socket);
    if (fd == -1) {
        PyErr_SetString(PyExc_TypeError, "unable to get file descriptor from socket");
        Py_XDECREF(callback);
        Py_XDECREF(loop);
        return -1;
    }
    ev_io_init(&self->io, io_callback, fd, io_flags);
    self->io.data = self;
    return 0;
}

static PyObject*
IO_start(libevwrapper_IO *self) {
    ev_io_start(self->loop->loop, &self->io);
    Py_RETURN_NONE;
}

static PyObject*
IO_stop(libevwrapper_IO *self) {
    ev_io_stop(self->loop->loop, &self->io);
    Py_RETURN_NONE;
}

static PyObject*
IO_is_active(libevwrapper_IO *self) {
    return PyBool_FromLong(ev_is_active(&self->io));
}

static PyMethodDef IO_methods[] = {
    {"start", (PyCFunction)IO_start, METH_NOARGS, "Start the watcher"},
    {"stop", (PyCFunction)IO_stop, METH_NOARGS, "Stop the watcher"},
    {"is_active", (PyCFunction)IO_is_active, METH_NOARGS, "Is the watcher active?"},
    {NULL}  /* Sentinal */
};

static PyTypeObject libevwrapper_IOType = {
    PyObject_HEAD_INIT(NULL)
    0,                               /*ob_size*/
    "cassandra.io.libevwrapper.IO",  /*tp_name*/
    sizeof(libevwrapper_IO),         /*tp_basicsize*/
    0,                               /*tp_itemsize*/
    (destructor)IO_dealloc,          /*tp_dealloc*/
    0,                               /*tp_print*/
    0,                               /*tp_getattr*/
    0,                               /*tp_setattr*/
    0,                               /*tp_compare*/
    0,                               /*tp_repr*/
    0,                               /*tp_as_number*/
    0,                               /*tp_as_sequence*/
    0,                               /*tp_as_mapping*/
    0,                               /*tp_hash */
    0,                               /*tp_call*/
    0,                               /*tp_str*/
    0,                               /*tp_getattro*/
    0,                               /*tp_setattro*/
    0,                               /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "IO objects",                    /* tp_doc */
    0,                               /* tp_traverse */
    0,                               /* tp_clear */
    0,                               /* tp_richcompare */
    0,                               /* tp_weaklistoffset */
    0,                               /* tp_iter */
    0,                               /* tp_iternext */
    IO_methods,                      /* tp_methods */
    0,                               /* tp_members */
    0,                               /* tp_getset */
    0,                               /* tp_base */
    0,                               /* tp_dict */
    0,                               /* tp_descr_get */
    0,                               /* tp_descr_set */
    0,                               /* tp_dictoffset */
    (initproc)IO_init,               /* tp_init */
};

typedef struct libevwrapper_Async {
    PyObject_HEAD
    struct ev_async async;
    struct libevwrapper_Loop *loop;
} libevwrapper_Async;

static void
Async_dealloc(libevwrapper_Async *self) {
    self->ob_type->tp_free((PyObject *)self);
};

static void async_callback(EV_P_ ev_async *watcher, int revents) {};

static int
Async_init(libevwrapper_Async *self, PyObject *args, PyObject *kwds) {
    libevwrapper_Loop *loop;

    static char *kwlist[] = {"loop", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &loop)) {
        PyErr_SetString(PyExc_TypeError, "unable to get file descriptor from socket");
        return -1;
    }

    if (loop) {
        Py_INCREF(loop);
        self->loop = loop;
    } else {
        return -1;
    }
    ev_async_init(&self->async, async_callback);
    return 0;
};

static PyObject *
Async_start(libevwrapper_Async *self) {
    ev_async_start(self->loop->loop, &self->async);
    Py_RETURN_NONE;
}

static PyObject *
Async_send(libevwrapper_Async *self) {
    ev_async_send(self->loop->loop, &self->async);
    Py_RETURN_NONE;
};

static PyMethodDef Async_methods[] = {
    {"start", (PyCFunction)Async_start, METH_NOARGS, "Start the watcher"},
    {"send", (PyCFunction)Async_send, METH_NOARGS, "Notify the event loop"},
    {NULL} /* Sentinel */
};

static PyTypeObject libevwrapper_AsyncType = {
    PyObject_HEAD_INIT(NULL)
    0,                               /*ob_size*/
    "cassandra.io.libevwrapper.Async", /*tp_name*/
    sizeof(libevwrapper_Async),      /*tp_basicsize*/
    0,                               /*tp_itemsize*/
    (destructor)Async_dealloc,       /*tp_dealloc*/
    0,                               /*tp_print*/
    0,                               /*tp_getattr*/
    0,                               /*tp_setattr*/
    0,                               /*tp_compare*/
    0,                               /*tp_repr*/
    0,                               /*tp_as_number*/
    0,                               /*tp_as_sequence*/
    0,                               /*tp_as_mapping*/
    0,                               /*tp_hash */
    0,                               /*tp_call*/
    0,                               /*tp_str*/
    0,                               /*tp_getattro*/
    0,                               /*tp_setattro*/
    0,                               /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "Async objects",                 /* tp_doc */
    0,                               /* tp_traverse */
    0,                               /* tp_clear */
    0,                               /* tp_richcompare */
    0,                               /* tp_weaklistoffset */
    0,                               /* tp_iter */
    0,                               /* tp_iternext */
    Async_methods,                   /* tp_methods */
    0,                               /* tp_members */
    0,                               /* tp_getset */
    0,                               /* tp_base */
    0,                               /* tp_dict */
    0,                               /* tp_descr_get */
    0,                               /* tp_descr_set */
    0,                               /* tp_dictoffset */
    (initproc)Async_init,            /* tp_init */
};

static PyMethodDef module_methods[] = {
    {NULL}  /* Sentinal */
};


#ifndef PyMODINIT_FUNC  /* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
initlibevwrapper(void)
{
    PyObject *m;

    if (PyType_Ready(&libevwrapper_LoopType) < 0)
        return;

    libevwrapper_IOType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&libevwrapper_IOType) < 0)
        return;

    libevwrapper_AsyncType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&libevwrapper_AsyncType) < 0)
        return;

    m = Py_InitModule3("libevwrapper", module_methods, "libev wrapper methods");
    PyModule_AddIntConstant(m, "EV_READ", EV_READ);
    PyModule_AddIntConstant(m, "EV_WRITE", EV_WRITE);

    Py_INCREF(&libevwrapper_LoopType);
    PyModule_AddObject(m, "Loop", (PyObject *)&libevwrapper_LoopType);

    Py_INCREF(&libevwrapper_IOType);
    PyModule_AddObject(m, "IO", (PyObject *)&libevwrapper_IOType);

    Py_INCREF(&libevwrapper_AsyncType);
    PyModule_AddObject(m, "Async", (PyObject *)&libevwrapper_AsyncType);

    if (!PyEval_ThreadsInitialized()) {
        PyEval_InitThreads();
    }
}
