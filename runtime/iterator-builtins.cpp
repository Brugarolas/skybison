// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "iterator-builtins.h"

#include "builtins.h"
#include "type-builtins.h"

namespace py {

static const BuiltinAttribute kSeqIteratorAttributes[] = {
    {ID(_iterator__iterable), RawSeqIterator::kIterableOffset,
     AttributeFlags::kHidden},
    {ID(_iterator__index), RawSeqIterator::kIndexOffset,
     AttributeFlags::kHidden},
};

void initializeIteratorType(Thread* thread) {
  addBuiltinType(thread, ID(iterator), LayoutId::kSeqIterator,
                 /*superclass_id=*/LayoutId::kObject, kSeqIteratorAttributes,
                 SeqIterator::kSize, /*basetype=*/false);
}

}  // namespace py
