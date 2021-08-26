/* Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com) */
#pragma once

#include "globals.h"
#include "runtime.h"

namespace py {

RawObject instanceDelAttr(Thread* thread, const Instance& instance,
                          const Object& name);

RawObject instanceGetAttribute(Thread* thread, const Instance& instance,
                               const Object& name);

RawObject instanceGetAttributeSetLocation(Thread* thread,
                                          const Instance& instance,
                                          const Object& name_interned,
                                          Object* location_out);

void instanceGrowOverflow(Thread* thread, const Instance& instance,
                          word length);

RawObject instanceSetAttr(Thread* thread, const Instance& instance,
                          const Object& name, const Object& value);

RawObject objectDeleteAttribute(Thread* thread, const Object& receiver,
                                const Object& name);

RawObject objectGetAttribute(Thread* thread, const Object& object,
                             const Object& name);

// Same as `objectGetAttribute()` but stores a value into `location_out` that is
// used by the inline cache and interpreted by
// `Interpreter::loadAttrWithLocation()`.
RawObject objectGetAttributeSetLocation(Thread* thread, const Object& object,
                                        const Object& name,
                                        Object* location_out,
                                        LoadAttrKind* kind);

// Raise an AttributeError that `object` has no attribute named `name`.
RawObject objectRaiseAttributeError(Thread* thread, const Object& object,
                                    const Object& name);

RawObject objectSetAttr(Thread* thread, const Object& object,
                        const Object& name, const Object& value);

RawObject objectSetAttrSetLocation(Thread* thread, const Object& object,
                                   const Object& name, const Object& value,
                                   Object* location_out);

RawObject objectNew(Thread* thread, const Type& type);

RawObject objectDelItem(Thread* thread, const Object& object,
                        const Object& key);

RawObject objectGetItem(Thread* thread, const Object& object,
                        const Object& key);

RawObject objectSetItem(Thread* thread, const Object& object, const Object& key,
                        const Object& value);

void initializeObjectTypes(Thread* thread);

}  // namespace py
