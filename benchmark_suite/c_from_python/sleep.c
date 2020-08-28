#include <unistd.h>
#include <Python.h>

static PyObject* sleeper_c(PyObject* self, PyObject* args){
	int input;

	if(!PyArg_ParseTuple(args, "i", &input))
			return NULL;

	sleep(input);
	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef sleeper_funcs[] = {
	{ "sleep_c", sleeper_c, METH_VARARGS, "sleeps"},
	{ NULL, NULL, 0, NULL}
};

static struct PyModuleDef sleepermodule = {
    PyModuleDef_HEAD_INIT,  /* m_base */
    "sleep_c",              /* m_name */
    NULL,                   /* m_doc */
    -1,                     /* m_size */
    sleeper_funcs           /* m_methods */
};

PyMODINIT_FUNC PyInit_sleeper(void) {
	return PyModule_Create(&sleepermodule);
}
