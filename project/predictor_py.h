#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <cstdlib>
#include <string>
#include <chrono>

namespace python {

/* Utility method for error handling */
    PyObject *check_py(PyObject *obj, const char *msg = "", int err_code = 1){
        if (obj == NULL) {
            if (PyErr_Occurred()) PyErr_Print();
            fputs(msg, stderr);
            fputs("\n", stderr);
            exit(1);
        }
        return obj;
    }

    void init() {
        // https://stackoverflow.com/questions/63406035/pyimport-import-cannot-find-module
        // include current directory in the PYTHONPATH - in order to find predictor.py
        std::string pp = getenv("PYTHONPATH") ?: "";
        setenv("PYTHONPATH", (".:" + pp).c_str(), 1);

        Py_Initialize();
    }

    void finalize(){
        if (Py_FinalizeEx() < 0) {
            fprintf(stderr,"Finalize failed\n");
            exit(1);
        }
    }

    class Predictor {
        // python methods
        PyObject *create_py;
        PyObject *fit_py;
        PyObject *predict_py;
        // the actual scikit model
        PyObject *pred;
        // accumulated inputs
        PyObject *list_x = PyList_New(0);
        // accumulated expected outputs
        PyObject *list_y = PyList_New(0);

    public:
        Predictor(const char *type, unsigned int size) {
            // importing predictor.py somewhere from the PYTHONPATH
            auto pModule = check_py(PyImport_ImportModule("predictor"), "Failed to load module");

            // loading the methods
            create_py  = check_py(PyObject_GetAttrString(pModule, "create"), "Failed to access 'create' method");
            fit_py     = check_py(PyObject_GetAttrString(pModule, "fit"), "Failed to access 'fit' method");
            predict_py = check_py(PyObject_GetAttrString(pModule, "predict"), "Failed to access 'predict' method");
            Py_DECREF(pModule);

            auto args = PyTuple_New(1);
            PyTuple_SetItem(args, 0, PyUnicode_FromString(type));
            // call python 'create'
            pred = check_py(PyObject_CallObject(create_py, args), "Call failed");
            Py_DECREF(args);

            // initial call to fit to enable prediction
            args = PyTuple_New(3);
            Py_INCREF(pred);
            PyTuple_SetItem(args, 0, pred);
            auto empty_list = PyList_New(size);
            for (unsigned int i = 0; i < size; i++)
                PyList_SetItem(empty_list, i, Py_BuildValue("d", 0));
            auto wrapper_list = PyList_New(1);
            PyList_SetItem(wrapper_list, 0, empty_list);
            PyTuple_SetItem(args, 1, wrapper_list);
            PyTuple_SetItem(args, 2, Py_BuildValue("[i]", 0));
            Py_DECREF(check_py(PyObject_CallObject(fit_py, args), "Call failed"));
            Py_DECREF(args);
        }

        void fit(double *inputs, unsigned int size, double output) {
            // the python fit method expects a 2d array of inputs and a 1d array of
            // expected outputs: like [[1],[2],[3]], [1,4,9]
            //
            // However, the python fit method is not incremental thus multiple calls
            // to this method are accumulated in the python lists 'list_x' and 'list_y'

            auto args = PyTuple_New(3);

            auto element_x = PyList_New(size);
            for (unsigned int i = 0; i < size; i++)
                PyList_SetItem(element_x, i, Py_BuildValue("d", inputs[i]));
            PyList_Append(list_x, element_x);
            Py_DECREF(element_x);

            auto element_y = Py_BuildValue("d", output);
            PyList_Append(list_y, element_y);
            Py_DECREF(element_y);

            Py_INCREF(pred);
            Py_INCREF(list_x);
            Py_INCREF(list_y);
            PyTuple_SetItem(args, 0, pred);
            PyTuple_SetItem(args, 1, list_x);
            PyTuple_SetItem(args, 2, list_y);

            // call python 'fit'
            auto fit_ret = check_py(PyObject_CallObject(fit_py, args), "Call failed");
            Py_DECREF(args);
            Py_DECREF(fit_ret);
        }

        double predict(double *inputs, unsigned int size) {
            auto args = PyTuple_New(2);

            auto list_x_outer = PyList_New(1);

            auto list_x = PyList_New(size);
            for (unsigned int i = 0; i < size; i++)
                PyList_SetItem(list_x, i, Py_BuildValue("d", inputs[i]));
            PyList_SetItem(list_x_outer, 0, list_x);

            Py_INCREF(pred);
            PyTuple_SetItem(args, 0, pred);
            PyTuple_SetItem(args, 1, list_x_outer);

            // call python 'predict'
            auto prediction = check_py(PyObject_CallObject(predict_py, args), "Call failed");
            Py_DECREF(args);
            return PyFloat_AsDouble(PyList_GetItem(prediction, 0));
        }

        ~Predictor() {
            Py_DECREF(create_py);
            Py_DECREF(fit_py);
            Py_DECREF(predict_py);
            Py_DECREF(pred);
            Py_DECREF(list_x);
            Py_DECREF(list_y);
        }

    };

} // end namespace python