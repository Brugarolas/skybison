#!/usr/bin/env python3
# $builtin-init-module$

from _builtins import _builtin, _unimplemented


def alarm(seconds):
    _builtin()


def default_int_handler(*args, **kwargs):
    _builtin()


def getitimer(which):
    _unimplemented()


def getsignal(signalnum):
    _builtin()


def pause():
    _unimplemented()


def pthread_kill(thread_id, signalnum):
    _unimplemented()


def set_wakeup_fd(fd):
    # TODO(T43321769) Return -1 until signals are fully implemented
    return -1


def setitimer(which, seconds, interval=0.0):
    _unimplemented()


def siginterrupt(signalnum, flag):
    _builtin()


def signal(signalnum, handler):
    _builtin()


def sigpending():
    _unimplemented()


def sigwait(sigset):
    _unimplemented()


def valid_signals():
    _builtin()
