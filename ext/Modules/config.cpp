// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "cpython-data.h"
#include "cpython-func.h"
#include "cpython-types.h"

extern "C" PyObject* PyInit__ast();
extern "C" PyObject* PyInit__bisect();
extern "C" PyObject* PyInit__blake2();
extern "C" PyObject* PyInit__bz2();
extern "C" PyObject* PyInit__curses();
extern "C" PyObject* PyInit__csv();
extern "C" PyObject* PyInit__datetime();
extern "C" PyObject* PyInit__hashlib();
extern "C" PyObject* PyInit__locale();
extern "C" PyObject* PyInit__lzma();
extern "C" PyObject* PyInit__multiprocessing();
extern "C" PyObject* PyInit__parser();
extern "C" PyObject* PyInit__posixsubprocess();
extern "C" PyObject* PyInit__random();
#ifdef __APPLE__
extern "C" PyObject* PyInit__scproxy();
#endif
extern "C" PyObject* PyInit__sha3();
extern "C" PyObject* PyInit__socket();
extern "C" PyObject* PyInit__sqlite3();
extern "C" PyObject* PyInit__sre();
extern "C" PyObject* PyInit__ssl();
extern "C" PyObject* PyInit__stat();
extern "C" PyObject* PyInit__struct();
extern "C" PyObject* PyInit__symtable();
extern "C" PyObject* PyInit_atexit();
extern "C" PyObject* PyInit_binascii();
extern "C" PyObject* PyInit_errno();
extern "C" PyObject* PyInit_fcntl();
extern "C" PyObject* PyInit_grp();
extern "C" PyObject* PyInit_math();
extern "C" PyObject* PyInit_posix();
extern "C" PyObject* PyInit_pwd();
extern "C" PyObject* PyInit_pyexpat();
extern "C" PyObject* PyInit_readline();
extern "C" PyObject* PyInit_resource();
extern "C" PyObject* PyInit_select();
extern "C" PyObject* PyInit_syslog();
extern "C" PyObject* PyInit_termios();
extern "C" PyObject* PyInit_time();
extern "C" PyObject* PyInit_zlib();

namespace py {

// _empty module to test loading from init tab
PyObject* PyInit__empty() {
  static PyModuleDef def;
  def = {};
  def.m_name = "_empty";
  return PyModule_Create(&def);
}

// clang-format off
struct _inittab _PyImport_Inittab[] = {
    {"_ast", PyInit__ast},
    {"_bisect", PyInit__bisect},
    {"_blake2", PyInit__blake2},
    {"_bz2", PyInit__bz2},
    {"_curses", PyInit__curses},
    {"_csv", PyInit__csv},
    {"_datetime", PyInit__datetime},
    {"_empty", PyInit__empty},
    {"_hashlib", PyInit__hashlib},
    {"_locale", PyInit__locale},
    {"_lzma", PyInit__lzma},
    {"_multiprocessing", PyInit__multiprocessing},
    {"_parser", PyInit__parser},
    {"_posixsubprocess", PyInit__posixsubprocess},
    {"_random", PyInit__random},
#ifdef __APPLE__
    {"_scproxy", PyInit__scproxy},
#endif
    {"_sha3", PyInit__sha3},
    {"_socket", PyInit__socket},
    {"_sqlite3", PyInit__sqlite3},
    {"_sre", PyInit__sre},
    {"_ssl", PyInit__ssl},
    {"_stat", PyInit__stat},
    {"_struct", PyInit__struct},
    {"_symtable", PyInit__symtable},
    {"atexit", PyInit_atexit},
    {"binascii", PyInit_binascii},
    {"errno", PyInit_errno},
    {"fcntl", PyInit_fcntl},
    {"grp", PyInit_grp},
    {"math", PyInit_math},
    {"posix", PyInit_posix},
    {"pwd", PyInit_pwd},
    {"pyexpat", PyInit_pyexpat},
    {"readline", PyInit_readline},
    {"resource", PyInit_resource},
    {"select", PyInit_select},
    {"syslog", PyInit_syslog},
    {"termios", PyInit_termios},
    {"time", PyInit_time},
    {"zlib", PyInit_zlib},
    {nullptr, nullptr},
};
// clang-format on

}  // namespace py
