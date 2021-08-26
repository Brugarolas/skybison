#include "api-handle.h"

#include <cstdint>

#include "cpython-data.h"
#include "cpython-func.h"
#include "cpython-types.h"

#include "capi-state.h"
#include "capi.h"
#include "debugging.h"
#include "event.h"
#include "globals.h"
#include "object-builtins.h"
#include "objects.h"
#include "runtime.h"
#include "scavenger.h"
#include "thread.h"
#include "visitor.h"

namespace py {

static const int32_t kEmptyIndex = -1;
static const int32_t kTombstoneIndex = -2;

struct IndexProbe {
  word index;
  word mask;
  uword perturb;
};

// Compute hash value suitable for `RawObject::operator==` (aka `a is b`)
// equality tests.
static inline uword handleHash(RawObject obj) {
  if (obj.isHeapObject()) {
    return obj.raw() >> kObjectAlignmentLog2;
  }
  return obj.raw();
}

static int32_t indexAt(int32_t* indices, word index) { return indices[index]; }

static void indexAtPut(int32_t* indices, word index, int32_t item_index) {
  indices[index] = item_index;
}

static void indexAtPutTombstone(int32_t* indices, word index) {
  indices[index] = kTombstoneIndex;
}

static void itemAtPut(RawObject* keys, void** values, int32_t index,
                      RawObject key, void* value) {
  DCHECK(key != SmallInt::fromWord(0), "0 represents empty and tombstone");
  DCHECK(value != nullptr, "key must be associated with a C-API handle");
  keys[index] = key;
  values[index] = value;
}

static void itemAtPutTombstone(RawObject* keys, void** values, int32_t index) {
  keys[index] = SmallInt::fromWord(0);
  values[index] = nullptr;
}

static RawObject itemKeyAt(RawObject* keys, int32_t index) {
  return keys[index];
}

static void* itemValueAt(void** values, int32_t index) { return values[index]; }

static int32_t maxCapacity(word num_indices) {
  DCHECK(num_indices <= kMaxInt32, "cannot fit %ld indices into 4-byte int",
         num_indices);
  return static_cast<int32_t>((num_indices * 2) / 3);
}

static int32_t* newIndices(word num_indices) {
  word size = num_indices * sizeof(int32_t);
  void* result = std::malloc(size);
  DCHECK(result != nullptr, "malloc failed");
  std::memset(result, -1, size);  // fill with kEmptyIndex
  return reinterpret_cast<int32_t*>(result);
}

static RawObject* newKeys(int32_t capacity) {
  void* result = std::calloc(capacity, sizeof(RawObject));
  DCHECK(result != nullptr, "malloc failed");
  return reinterpret_cast<RawObject*>(result);
}

static void** newValues(int32_t capacity) {
  void* result = std::malloc(static_cast<size_t>(capacity) * kPointerSize);
  DCHECK(result != nullptr, "malloc failed");
  return reinterpret_cast<void**>(result);
}

static bool nextItem(RawObject* keys, void** values, int32_t* idx, int32_t end,
                     RawObject* key_out, void** value_out) {
  for (int32_t i = *idx; i < end; i++) {
    RawObject key = itemKeyAt(keys, i);
    if (key == SmallInt::fromWord(0)) continue;
    *key_out = key;
    *value_out = itemValueAt(values, i);
    *idx = i + 1;
    return true;
  }
  *idx = end;
  return false;
}

static IndexProbe probeBegin(word num_indices, uword hash) {
  DCHECK(num_indices > 0 && Utils::isPowerOfTwo(num_indices),
         "number of indices must be a power of two, got %ld", num_indices);
  word mask = num_indices - 1;
  return {static_cast<word>(hash) & mask, mask, hash};
}

static void probeNext(IndexProbe* probe) {
  // Note that repeated calls to this function guarantee a permutation of all
  // indices when the number of indices is power of two. See
  // https://en.wikipedia.org/wiki/Linear_congruential_generator#c_%E2%89%A0_0.
  probe->perturb >>= 5;
  probe->index = (probe->index * 5 + 1 + probe->perturb) & probe->mask;
}

void* ApiHandleDict::at(RawObject key) {
  word index;
  int32_t item_index;
  if (lookup(key, &index, &item_index)) {
    return itemValueAt(values(), item_index);
  }
  return nullptr;
}

inline void* ApiHandleDict::atIndex(int32_t item_index) {
  return itemValueAt(values(), item_index);
}

void ApiHandleDict::atPut(RawObject key, void* value) {
  int32_t item_index;
  atPutLookup(key, &item_index);
  atPutValue(item_index, value);
}

ALWAYS_INLINE bool ApiHandleDict::atPutLookup(RawObject key,
                                              int32_t* item_index) {
  DCHECK(key != SmallInt::fromWord(0),
         "0 key not allowed (used for tombstone)");
  uword hash = handleHash(key);
  int32_t* indices = this->indices();
  RawObject* keys = this->keys();
  word num_indices = this->numIndices();

  word next_free_index = -1;
  for (IndexProbe probe = probeBegin(num_indices, hash);; probeNext(&probe)) {
    int32_t current_item_index = indexAt(indices, probe.index);
    if (current_item_index >= 0) {
      if (itemKeyAt(keys, current_item_index) == key) {
        *item_index = current_item_index;
        return false;
      }
      continue;
    }
    if (next_free_index == -1) {
      next_free_index = probe.index;
    }
    if (current_item_index == kEmptyIndex) {
      word new_item_index = nextIndex();
      indexAtPut(indices, next_free_index, new_item_index);
      keys[new_item_index] = key;
      setNextIndex(new_item_index + 1);
      incrementNumItems();
      *item_index = new_item_index;
      return true;
    }
  }
}

ALWAYS_INLINE void ApiHandleDict::atPutValue(int32_t item_index, void* value) {
  DCHECK(value != nullptr, "key cannot be associated with nullptr");
  values()[item_index] = value;

  // Maintain the invariant that we have space for at least one more item.
  if (!hasUsableItem()) {
    grow();
  }
}

NEVER_INLINE void ApiHandleDict::grow() {
  // If at least half of the items in the dense array are tombstones, removing
  // them will free up plenty of space. Otherwise, the dict must be grown.
  word growth_factor = (numItems() < capacity() / 2) ? 1 : kGrowthFactor;
  word new_num_indices = numIndices() * growth_factor;
  rehash(new_num_indices);
  DCHECK(hasUsableItem(), "dict must have space for another item");
}

void ApiHandleDict::initialize(word num_indices) {
  setIndices(newIndices(num_indices));
  setNumIndices(num_indices);

  int32_t capacity = maxCapacity(num_indices);
  setCapacity(capacity);
  setKeys(newKeys(capacity));
  setValues(newValues(capacity));
}

bool ApiHandleDict::lookup(RawObject key, word* sparse, int32_t* dense) {
  uword hash = handleHash(key);
  int32_t* indices = this->indices();
  RawObject* keys = this->keys();
  word num_indices = this->numIndices();

  for (IndexProbe probe = probeBegin(num_indices, hash);; probeNext(&probe)) {
    int32_t item_index = indexAt(indices, probe.index);
    if (item_index >= 0) {
      if (itemKeyAt(keys, item_index) == key) {
        *sparse = probe.index;
        *dense = item_index;
        return true;
      }
      continue;
    }
    if (item_index == kEmptyIndex) {
      return false;
    }
  }
}

void ApiHandleDict::rehash(word new_num_indices) {
  int32_t end = nextIndex();
  int32_t* indices = this->indices();
  RawObject* keys = this->keys();
  void** values = this->values();

  int32_t new_capacity = maxCapacity(new_num_indices);
  int32_t* new_indices = newIndices(new_num_indices);
  RawObject* new_keys = newKeys(new_capacity);
  void** new_values = newValues(new_capacity);

  // Re-insert items
  RawObject key = NoneType::object();
  void* value;
  for (int32_t i = 0, count = 0; nextItem(keys, values, &i, end, &key, &value);
       count++) {
    uword hash = handleHash(key);
    for (IndexProbe probe = probeBegin(new_num_indices, hash);;
         probeNext(&probe)) {
      if (indexAt(new_indices, probe.index) == kEmptyIndex) {
        indexAtPut(new_indices, probe.index, count);
        itemAtPut(new_keys, new_values, count, key, value);
        break;
      }
    }
  }

  setCapacity(new_capacity);
  setIndices(new_indices);
  setKeys(new_keys);
  setNextIndex(numItems());
  setNumIndices(new_num_indices);
  setValues(new_values);

  std::free(indices);
  std::free(keys);
  std::free(values);
}

void* ApiHandleDict::remove(RawObject key) {
  word index;
  int32_t item_index;
  if (!lookup(key, &index, &item_index)) {
    return nullptr;
  }

  void** values = this->values();
  void* result = itemValueAt(values, item_index);
  indexAtPutTombstone(indices(), index);
  itemAtPutTombstone(keys(), values, item_index);
  decrementNumItems();
  return result;
}

void ApiHandleDict::visitKeys(PointerVisitor* visitor) {
  RawObject* keys = this->keys();
  if (keys == nullptr) return;
  word keys_length = capacity();
  for (word i = 0; i < keys_length; i++) {
    visitor->visitPointer(&keys[i], PointerKind::kRuntime);
  }
}

// Reserves a new handle in the given runtime's handle buffer.
static ApiHandle* allocateHandle(Runtime* runtime) {
  FreeListNode** free_handles = capiFreeHandles(runtime);
  ApiHandle* result = reinterpret_cast<ApiHandle*>(*free_handles);

  FreeListNode* next = (*free_handles)->next;
  if (next != nullptr) {
    *free_handles = next;
  } else {
    // No handles left to recycle; advance the frontier
    *free_handles = reinterpret_cast<FreeListNode*>(result + 1);
  }

  return result;
}

// Frees the handle for future re-use by the given runtime.
static void freeHandle(Runtime* runtime, ApiHandle* handle) {
  FreeListNode** free_handles = capiFreeHandles(runtime);
  FreeListNode* node = reinterpret_cast<FreeListNode*>(handle);
  node->next = *free_handles;
  *free_handles = node;
}

RawNativeProxy ApiHandle::asNativeProxy() {
  DCHECK(!isImmediate() && reference_ != 0, "expected extension object handle");
  return RawObject{reference_}.rawCast<RawNativeProxy>();
}

ApiHandle* ApiHandle::newReference(Runtime* runtime, RawObject obj) {
  if (isEncodeableAsImmediate(obj)) {
    return handleFromImmediate(obj);
  }
  if (runtime->isInstanceOfNativeProxy(obj)) {
    ApiHandle* result = static_cast<ApiHandle*>(
        Int::cast(obj.rawCast<RawNativeProxy>().native()).asCPtr());
    result->increfNoImmediate();
    return result;
  }
  return ApiHandle::newReferenceWithManaged(runtime, obj);
}

ApiHandle* ApiHandle::newReferenceWithManaged(Runtime* runtime, RawObject obj) {
  DCHECK(!isEncodeableAsImmediate(obj), "immediates not handled here");
  DCHECK(!runtime->isInstanceOfNativeProxy(obj),
         "native proxy not handled here");

  // Get the handle of a builtin instance
  ApiHandleDict* handles = capiHandles(runtime);
  int32_t index;
  if (!handles->atPutLookup(obj, &index)) {
    ApiHandle* result = reinterpret_cast<ApiHandle*>(handles->atIndex(index));
    result->increfNoImmediate();
    return result;
  }

  // Initialize an ApiHandle for a builtin object or runtime instance
  EVENT_ID(AllocateCAPIHandle, obj.layoutId());
  ApiHandle* handle = allocateHandle(runtime);
  handle->reference_ = SmallInt::fromWord(0).raw();
  handle->ob_refcnt = 1;

  handles->atPutValue(index, handle);
  handle->reference_ = obj.raw();
  return handle;
}

ApiHandle* ApiHandle::borrowedReference(Runtime* runtime, RawObject obj) {
  if (isEncodeableAsImmediate(obj)) {
    return handleFromImmediate(obj);
  }
  if (runtime->isInstanceOfNativeProxy(obj)) {
    ApiHandle* result = static_cast<ApiHandle*>(
        Int::cast(obj.rawCast<RawNativeProxy>().native()).asCPtr());
    result->ob_refcnt |= kBorrowedBit;
    return result;
  }
  ApiHandle* result = ApiHandle::newReferenceWithManaged(runtime, obj);
  result->ob_refcnt |= kBorrowedBit;
  result->ob_refcnt--;
  return result;
}

RawObject ApiHandle::checkFunctionResult(Thread* thread, PyObject* result) {
  bool has_pending_exception = thread->hasPendingException();
  if (result == nullptr) {
    if (has_pending_exception) return Error::exception();
    return thread->raiseWithFmt(LayoutId::kSystemError,
                                "NULL return without exception set");
  }
  RawObject result_obj = stealReference(result);
  if (has_pending_exception) {
    // TODO(T53569173): set the currently pending exception as the cause of the
    // newly raised SystemError
    thread->clearPendingException();
    return thread->raiseWithFmt(LayoutId::kSystemError,
                                "non-NULL return with exception set");
  }
  return result_obj;
}

void* ApiHandle::cache(Runtime* runtime) {
  // Only managed objects can have a cached value
  DCHECK(!isImmediate(), "immediate handles do not have caches");

  ApiHandleDict* caches = capiCaches(runtime);
  RawObject obj = asObjectNoImmediate();
  DCHECK(!runtime->isInstanceOfNativeProxy(obj),
         "cache must not be called on extension object");
  return caches->at(obj);
}

NEVER_INLINE void ApiHandle::dispose() {
  disposeWithRuntime(Thread::current()->runtime());
}

void ApiHandle::disposeWithRuntime(Runtime* runtime) {
  // TODO(T46009838): If a module handle is being disposed, this should register
  // a weakref to call the module's m_free once's the module is collected

  RawObject obj = asObjectNoImmediate();
  DCHECK(!runtime->isInstanceOfNativeProxy(obj),
         "Dispose must not be called on extension object");
  capiHandles(runtime)->remove(obj);

  void* cache = capiCaches(runtime)->remove(obj);
  std::free(cache);
  freeHandle(runtime, this);
}

// TODO(T58710656): Allow immediate handles for SmallStr
// TODO(T58710677): Allow immediate handles for SmallBytes
bool ApiHandle::isEncodeableAsImmediate(RawObject obj) {
  // SmallStr and SmallBytes require solutions for C-API functions that read
  // out char* whose lifetimes depend on the lifetimes of the PyObject*s.
  return !obj.isHeapObject() && !obj.isSmallStr() && !obj.isSmallBytes();
}

void ApiHandle::setCache(Runtime* runtime, void* value) {
  ApiHandleDict* caches = capiCaches(runtime);
  RawObject obj = asObjectNoImmediate();
  caches->atPut(obj, value);
}

void ApiHandle::setRefcnt(Py_ssize_t count) {
  if (isImmediate()) return;
  DCHECK((count & kBorrowedBit) == 0, "count must not have high bits set");
  Py_ssize_t flags = ob_refcnt & kBorrowedBit;
  ob_refcnt = count | flags;
}

void disposeApiHandles(Runtime* runtime) {
  ApiHandleDict* handles = capiHandles(runtime);
  int32_t end = handles->nextIndex();
  RawObject* keys = handles->keys();
  void** values = handles->values();

  RawObject key = NoneType::object();
  void* value;
  for (int32_t i = 0; nextItem(keys, values, &i, end, &key, &value);) {
    ApiHandle* handle = reinterpret_cast<ApiHandle*>(value);
    handle->disposeWithRuntime(runtime);
  }
}

word numApiHandles(Runtime* runtime) {
  return capiHandles(runtime)->numItems();
}

void visitApiHandles(Runtime* runtime, HandleVisitor* visitor) {
  ApiHandleDict* handles = capiHandles(runtime);
  int32_t end = handles->nextIndex();
  RawObject* keys = handles->keys();
  void** values = handles->values();

  RawObject key = NoneType::object();
  void* value;
  for (int32_t i = 0; nextItem(keys, values, &i, end, &key, &value);) {
    visitor->visitHandle(value, key);
  }
}

void visitIncrementedApiHandles(Runtime* runtime, PointerVisitor* visitor) {
  // Report handles with a refcount > 0 as roots. We deliberately do not visit
  // other handles and do not update dictionary keys yet.
  ApiHandleDict* handles = capiHandles(runtime);
  int32_t end = handles->nextIndex();
  RawObject* keys = handles->keys();
  void** values = handles->values();

  RawObject key = NoneType::object();
  void* value;
  for (int32_t i = 0; nextItem(keys, values, &i, end, &key, &value);) {
    ApiHandle* handle = reinterpret_cast<ApiHandle*>(value);
    if (handle->refcntNoImmediate() > 0) {
      visitor->visitPointer(&key, PointerKind::kApiHandle);
      // We do not write back the changed `key` to the dictionary yet but leave
      // that to `visitNotIncrementedBorrowedApiHandles` because we still need
      // the old `key` to access `capiCaches` there).
    }
  }
}

void visitNotIncrementedBorrowedApiHandles(Runtime* runtime,
                                           Scavenger* scavenger,
                                           PointerVisitor* visitor) {
  // This function:
  // - Rebuilds the handle dictionary: The GC may have moved object around so
  //   we have to adjust the dictionary keys to the new references and updated
  //   hash values. As a side effect this also clears tombstones and shrinks
  //   the dictionary if possible.
  // - Remove (or rather not insert into the new dictionary) entries with
  //   refcount zero, that are not referenced from any other live object
  //   (object is "white" after GC tri-coloring).
  // - Rebuild cache dictionary to adjust for moved `key` addresses.

  ApiHandleDict* caches = capiCaches(runtime);
  ApiHandleDict* handles = capiHandles(runtime);
  int32_t end = handles->nextIndex();
  int32_t* indices = handles->indices();
  RawObject* keys = handles->keys();
  void** values = handles->values();

  word old_num_items = handles->numItems();
  word new_num_indices = Utils::nextPowerOfTwo((old_num_items * 3) / 2 + 1);
  int32_t new_capacity = maxCapacity(new_num_indices);
  int32_t* new_indices = newIndices(new_num_indices);
  RawObject* new_keys = newKeys(new_capacity);
  void** new_values = newValues(new_capacity);

  RawObject key = NoneType::object();
  void* value;
  int32_t count = 0;
  for (int32_t i = 0; nextItem(keys, values, &i, end, &key, &value);) {
    ApiHandle* handle = reinterpret_cast<ApiHandle*>(value);
    if (handle->refcntNoImmediate() == 0) {
      DCHECK(handle->isBorrowedNoImmediate(),
             "non-borrowed object should already be disposed");
      if (key.isHeapObject() &&
          isWhiteObject(scavenger, HeapObject::cast(key))) {
        // Lookup associated cache data. Note that `key` and the keys in the
        // `caches` array both use addressed from before GC movement.
        // `caches.rehash()` happens is delayed until the end of this function.
        void* cache = caches->remove(key);
        freeHandle(runtime, handle);
        std::free(cache);
        continue;
      }
    }
    visitor->visitPointer(&key, PointerKind::kApiHandle);
    handle->reference_ = reinterpret_cast<uintptr_t>(key.raw());
    // Insert into new handle dictionary.
    uword hash = handleHash(key);
    for (IndexProbe probe = probeBegin(new_num_indices, hash);;
         probeNext(&probe)) {
      if (indexAt(new_indices, probe.index) == kEmptyIndex) {
        indexAtPut(new_indices, probe.index, count);
        itemAtPut(new_keys, new_values, count, key, value);
        break;
      }
    }
    count++;
  }

  handles->setCapacity(new_capacity);
  handles->setIndices(new_indices);
  handles->setKeys(new_keys);
  handles->setNextIndex(count);
  handles->setNumIndices(new_num_indices);
  handles->setValues(new_values);

  std::free(indices);
  std::free(keys);
  std::free(values);

  // Re-hash caches dictionary.
  caches->visitKeys(visitor);
  caches->rehash(caches->numIndices());
}

RawObject objectGetMember(Thread* thread, RawObject ptr, RawObject name) {
  ApiHandle* value = *reinterpret_cast<ApiHandle**>(Int::cast(ptr).asCPtr());
  if (value != nullptr) {
    return value->asObject();
  }
  if (name.isNoneType()) {
    return NoneType::object();
  }
  HandleScope scope(thread);
  Str name_str(&scope, name);
  return thread->raiseWithFmt(LayoutId::kAttributeError,
                              "Object attribute '%S' is nullptr", &name_str);
}

bool objectHasHandleCache(Runtime* runtime, RawObject obj) {
  return ApiHandle::borrowedReference(runtime, obj)->cache(runtime) != nullptr;
}

void* objectNewReference(Runtime* runtime, RawObject obj) {
  return ApiHandle::newReference(runtime, obj);
}

void objectSetMember(Runtime* runtime, RawObject old_ptr, RawObject new_val) {
  ApiHandle** old = reinterpret_cast<ApiHandle**>(Int::cast(old_ptr).asCPtr());
  (*old)->decref();
  *old = ApiHandle::newReference(runtime, new_val);
}

void dump(PyObject* obj) {
  if (obj == nullptr) {
    std::fprintf(stderr, "<nullptr>\n");
    return;
  }
  dump(ApiHandle::fromPyObject(obj)->asObject());
}

}  // namespace py
