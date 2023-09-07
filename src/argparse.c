
/* to speed this up gendocstrings can generate something like this
   that uses the string length as a hash

    switch(strlen(kwname))
    {
        case 7:
          if(0==strcmp(kwname, "hkjdshfkjd")) return 4;
          if(0==strcmp(kwname, "sdsdshfkjd")) return 2;
          return -1;

        case 2:
          if(0==strcmp(kwname, "ab")) return 1;
          return -1;

        default: return -1;
    }
*/
static int
ARG_WHICH_KEYWORD(PyObject *item, const char *kwlist[], size_t n_kwlist, const char **kwname)
{
    *kwname = PyUnicode_AsUTF8(item);
    size_t cmp;
    for (cmp = 0; cmp < n_kwlist; cmp++)
    {
        if (0 == strcmp(*kwname, kwlist[cmp]))
            return cmp;
    }
    return -1;
}

#define ARG_PROLOG(maxpos_args, kwname_list)                                              \
    static const char *kwlist[] = {kwname_list};                                          \
    const int maxpos = maxpos_args;                                                       \
    const char *unknown_keyword = NULL;                                                   \
    const int maxargs = Py_ARRAY_LENGTH(kwlist);                                          \
    PyObject *myargs[maxargs];                                                            \
    PyObject **useargs = (PyObject **)fast_args;                                          \
    size_t actual_nargs = PyVectorcall_NARGS(fast_nargs);                                 \
    if (actual_nargs > maxpos)                                                            \
        goto too_many_args;                                                               \
    if (fast_kwnames)                                                                     \
    {                                                                                     \
        useargs = myargs;                                                                 \
        memcpy(useargs, fast_args, sizeof(PyObject *) * actual_nargs);                    \
        memset(useargs + actual_nargs, 0, sizeof(PyObject *) * (maxargs - actual_nargs)); \
        for (int i = 0; i < PyTuple_GET_SIZE(fast_kwnames); i++)                          \
        {                                                                                 \
            PyObject *item = PyTuple_GET_ITEM(fast_kwnames, i);                           \
            int which = ARG_WHICH_KEYWORD(item, kwlist, maxargs, &unknown_keyword);       \
            if (which == -1)                                                              \
                goto unknown_keyword_arg;                                                 \
            if (useargs[which])                                                           \
                goto pos_and_keyword;                                                     \
            useargs[which] = fast_args[actual_nargs + i];                                 \
        }                                                                                 \
    }                                                                                     \
    int optind = 0;

#define ARG_MANDATORY                                       \
    if ((size_t)optind >= actual_nargs || !useargs[optind]) \
        goto missing_required;

#define ARG_OPTIONAL      \
    if (!useargs[optind]) \
        optind++;         \
    else

#define ARG_EPILOG(retval, usage)                                                                                                     \
    assert((size_t)optind == actual_nargs);                                                                                           \
    goto success;                                                                                                                     \
    /* this wont be hit but is here to stop warnings about unused label */                                                            \
    goto missing_required;                                                                                                            \
    too_many_args:                                                                                                                    \
    PyErr_Format(PyExc_TypeError, "Too many arguments %d (min %d max %d) provided to %s", (int)actual_nargs, maxpos, maxargs, usage); \
    goto error_return;                                                                                                                \
    missing_required:                                                                                                                 \
    PyErr_Format(PyExc_TypeError, "Missing required parameter #%d '%s' of %s", optind + 1, kwlist[optind], usage);                    \
    goto error_return;                                                                                                                \
    unknown_keyword_arg:                                                                                                              \
    PyErr_Format(PyExc_TypeError, "'%s' is an invalid keyword argument for %s", unknown_keyword, usage);                              \
    goto error_return;                                                                                                                \
    pos_and_keyword:                                                                                                                  \
    PyErr_Format(PyExc_TypeError, "argument '%s' given by name and position for %s", unknown_keyword, usage);                         \
    goto error_return;                                                                                                                \
    param_error:                                                                                                                      \
    /* ::TODO:: add note about kwlist[optind] */                                                                                      \
    goto error_return;                                                                                                                \
    error_return:                                                                                                                     \
    assert(PyErr_Occurred());                                                                                                         \
    return retval;                                                                                                                    \
    success:

#define ARG_pyobject(varname)                                                                    \
    do                                                                                           \
    {                                                                                            \
        if (useargs[optind])                                                                     \
        {                                                                                        \
            varname = useargs[optind];                                                           \
            optind++;                                                                            \
        }                                                                                        \
        else /* this won't be hit, and is here to ensure the label is used to avoid a warning */ \
            goto param_error;                                                                    \
    } while (0)

#define ARG_pointer(varname)                         \
    do                                               \
    {                                                \
        varname = PyLong_AsVoidPtr(useargs[optind]); \
        if (PyErr_Occurred())                        \
            goto param_error;                        \
        optind++;                                    \
    } while (0)

#define ARG_str(varname)                             \
    do                                               \
    {                                                \
        varname = PyUnicode_AsUTF8(useargs[optind]); \
        if (!varname)                                \
            goto param_error;                        \
        optind++;                                    \
    } while (0)

#define ARG_PyUnicode(varname)                                                                    \
    do                                                                                            \
    {                                                                                             \
        if (PyUnicode_Check(useargs[optind]))                                                     \
        {                                                                                         \
            varname = useargs[optind];                                                            \
            optind++;                                                                             \
        }                                                                                         \
        else                                                                                      \
        {                                                                                         \
            PyErr_Format(PyExc_TypeError, "Expected a str not %s", Py_TypeName(useargs[optind])); \
            goto param_error;                                                                     \
        }                                                                                         \
    } while (0)

#define ARG_optional_str(varname)       \
    do                                  \
    {                                   \
        if (Py_IsNone(useargs[optind])) \
        {                               \
            varname = NULL;             \
            optind++;                   \
        }                               \
        else                            \
            ARG_str(varname);           \
    } while (0)

#define ARG_Callable(varname)                                                                          \
    do                                                                                                 \
    {                                                                                                  \
        if (PyCallable_Check(useargs[optind]))                                                         \
        {                                                                                              \
            varname = useargs[optind];                                                                 \
            optind++;                                                                                  \
        }                                                                                              \
        else                                                                                           \
        {                                                                                              \
            PyErr_Format(PyExc_TypeError, "Expected a callable not %s", Py_TypeName(useargs[optind])); \
            goto param_error;                                                                          \
        }                                                                                              \
    } while (0)

#define ARG_optional_Callable(varname)  \
    do                                  \
    {                                   \
        if (Py_IsNone(useargs[optind])) \
        {                               \
            varname = NULL;             \
            optind++;                   \
        }                               \
        else                            \
            ARG_Callable(varname);      \
    } while (0)

#define ARG_bool(varname)                                 \
    do                                                    \
    {                                                     \
        varname = PyObject_IsTrueStrict(useargs[optind]); \
        if (varname == -1)                                \
            goto param_error;                             \
        optind++;                                         \
    } while (0)

#define ARG_int(varname)                         \
    do                                           \
    {                                            \
        varname = PyLong_AsInt(useargs[optind]); \
        if (varname == -1 && PyErr_Occurred())   \
            goto param_error;                    \
        optind++;                                \
    } while (0)

#define ARG_int64(varname)                            \
    do                                                \
    {                                                 \
        varname = PyLong_AsLongLong(useargs[optind]); \
        if (varname == -1 && PyErr_Occurred())        \
            goto param_error;                         \
        optind++;                                     \
    } while (0)

#define ARG_TYPE_CHECK(varname, type, cast)                                                                       \
    do                                                                                                            \
    {                                                                                                             \
        switch (PyObject_IsInstance(useargs[optind], type))                                                       \
        {                                                                                                         \
        case 1:                                                                                                   \
            varname = (cast)useargs[optind];                                                                      \
            optind++;                                                                                             \
            break;                                                                                                \
        case 0:                                                                                                   \
            PyErr_Format(PyExc_TypeError, "Expected %s not %s", Py_TypeName(type), Py_TypeName(useargs[optind])); \
            /* fallthru */                                                                                        \
        case -1:                                                                                                  \
            goto param_error;                                                                                     \
        }                                                                                                         \
    } while (0)

#define ARG_Connection(varname) ARG_TYPE_CHECK(varname, (PyObject *)&ConnectionType, Connection *)

/* PySequence_Check is too strict and rejects things that are
    accepted by PySequence_Fast like sets and generators,
    so everything is accepted */
#define ARG_optional_Bindings(varname)  \
    do                                  \
    {                                   \
        if (Py_IsNone(useargs[optind])) \
            varname = NULL;             \
        else                            \
            varname = useargs[optind];  \
        optind++;                       \
    } while (0)

#define ARG_optional_str_URIFilename(varname)                                                                                                         \
    do                                                                                                                                                \
    {                                                                                                                                                 \
        if (Py_IsNone(useargs[optind]) || PyUnicode_Check(useargs[optind]) || PyObject_IsInstance(useargs[optind], (PyObject *)&APSWURIFilenameType)) \
        {                                                                                                                                             \
            varname = useargs[optind];                                                                                                                \
            optind++;                                                                                                                                 \
        }                                                                                                                                             \
        else                                                                                                                                          \
        {                                                                                                                                             \
            PyErr_Format(PyExc_TypeError, "Expected None | str | apsw.URIFilename, not %s", Py_TypeName(useargs[optind]));                            \
            goto param_error;                                                                                                                         \
        }                                                                                                                                             \
    } while (0)

#define ARG_List_int_int(varname)                                                                                                            \
    do                                                                                                                                       \
    {                                                                                                                                        \
        if (!PyList_Check(useargs[optind]) || PyList_Size(useargs[optind]) != 2)                                                             \
        {                                                                                                                                    \
            PyErr_Format(PyExc_TypeError, "Expected a two item list of int");                                                                \
            goto param_error;                                                                                                                \
        }                                                                                                                                    \
        for (int i = 0; i < 2; i++)                                                                                                          \
        {                                                                                                                                    \
            PyObject *list_item = PyList_GetItem(useargs[optind], i);                                                                        \
            if (!list_item)                                                                                                                  \
                goto param_error;                                                                                                            \
            if (!PyLong_Check(list_item))                                                                                                    \
            {                                                                                                                                \
                PyErr_Format(PyExc_TypeError, "Function argument list[int,int] expected int for item %d not %s", i, Py_TypeName(list_item)); \
                goto param_error;                                                                                                            \
            }                                                                                                                                \
        }                                                                                                                                    \
        varname = useargs[optind];                                                                                                           \
        optind++;                                                                                                                            \
    } while (0)

#define ARG_optional_set(varname)                                                                        \
    do                                                                                                   \
    {                                                                                                    \
        if (Py_IsNone(useargs[optind]))                                                                  \
            varname = NULL;                                                                              \
        else if (PySet_Check(useargs[optind]))                                                           \
            varname = useargs[optind];                                                                   \
        else                                                                                             \
        {                                                                                                \
            PyErr_Format(PyExc_TypeError, "Expected None or set, not %s", Py_TypeName(useargs[optind])); \
            goto param_error;                                                                            \
        }                                                                                                \
        optind++;                                                                                        \
    } while (0)

#define ARG_py_buffer(varname)                                                                                                                   \
    do                                                                                                                                           \
    {                                                                                                                                            \
        if (!PyObject_CheckBuffer(useargs[optind]))                                                                                              \
        {                                                                                                                                        \
            PyErr_Format(PyExc_TypeError, "Expected bytes or similar type that supports buffer protocol, not %s", Py_TypeName(useargs[optind])); \
            goto param_error;                                                                                                                    \
        }                                                                                                                                        \
        varname = useargs[optind];                                                                                                               \
        optind++;                                                                                                                                \
    } while (0)
