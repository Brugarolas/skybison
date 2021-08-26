/* Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com) */
#pragma once

#include "api-handle.h"
#include "handles-decl.h"
#include "runtime.h"

namespace py {

// Returns the handle's cache buffer, if it already exists. Otherwise,
// allocates a buffer, copies the bytes's contents, and caches it on the handle.
char* bytesAsString(Runtime* runtime, ApiHandle* handle, const Bytes& bytes);

}  // namespace py
