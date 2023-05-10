// Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
#include "ic.h"

#include "gtest/gtest.h"

#include "attributedict.h"
#include "dict-builtins.h"
#include "str-builtins.h"
#include "test-utils.h"
#include "type-builtins.h"

namespace py {
namespace testing {

using IcTest = RuntimeFixture;

TEST_F(
    IcTest,
    icLookupMonomorphicWithEmptyCacheReturnsErrorNotFoundAndSetIsFoundToFalse) {
  HandleScope scope(thread_);
  MutableTuple caches(&scope,
                      runtime_->newMutableTuple(2 * kIcPointersPerEntry));
  caches.fill(NoneType::object());
  bool is_found;
  EXPECT_TRUE(icLookupMonomorphic(*caches, 1, LayoutId::kSmallInt, &is_found)
                  .isErrorNotFound());
  EXPECT_FALSE(is_found);
}

TEST_F(IcTest, IcLookupBinaryOpReturnsErrorNotFound) {
  HandleScope scope(thread_);

  MutableTuple caches(&scope, runtime_->newMutableTuple(kIcPointersPerEntry));
  caches.fill(NoneType::object());
  BinaryOpFlags flags;
  EXPECT_TRUE(icLookupBinaryOp(*caches, 0, LayoutId::kSmallInt,
                               LayoutId::kSmallInt, &flags)
                  .isErrorNotFound());
}

TEST_F(IcTest, IcLookupGlobalVar) {
  HandleScope scope(thread_);
  MutableTuple caches(&scope, runtime_->newMutableTuple(2));
  caches.fill(NoneType::object());
  ValueCell cache(&scope, runtime_->newValueCell());
  cache.setValue(SmallInt::fromWord(99));
  caches.atPut(0, *cache);
  EXPECT_TRUE(
      isIntEqualsWord(valueCellValue(icLookupGlobalVar(*caches, 0)), 99));
  EXPECT_TRUE(icLookupGlobalVar(*caches, 1).isNoneType());
}

TEST_F(IcTest, IcUpdateAttrSetsMonomorphicEntry) {
  HandleScope scope(thread_);
  MutableTuple caches(&scope,
                      runtime_->newMutableTuple(1 * kIcPointersPerEntry));
  caches.fill(NoneType::object());
  Object value(&scope, runtime_->newInt(88));
  Object name(&scope, Str::empty());
  Function dependent(&scope, newEmptyFunction());
  EXPECT_EQ(icUpdateAttr(thread_, caches, 0, LayoutId::kSmallInt, value, name,
                         dependent),
            ICState::kMonomorphic);

  bool is_found;
  EXPECT_EQ(icLookupMonomorphic(*caches, 0, LayoutId::kSmallInt, &is_found),
            *value);
}

TEST_F(IcTest, IcUpdateAttrUpdatesExistingMonomorphicEntry) {
  HandleScope scope(thread_);
  MutableTuple caches(&scope,
                      runtime_->newMutableTuple(1 * kIcPointersPerEntry));
  caches.fill(NoneType::object());
  Object value(&scope, runtime_->newInt(88));
  Object name(&scope, Str::empty());
  Function dependent(&scope, newEmptyFunction());
  ASSERT_EQ(icUpdateAttr(thread_, caches, 0, LayoutId::kSmallInt, value, name,
                         dependent),
            ICState::kMonomorphic);
  bool is_found;
  EXPECT_EQ(icLookupMonomorphic(*caches, 0, LayoutId::kSmallInt, &is_found),
            *value);
  EXPECT_TRUE(is_found);

  Object new_value(&scope, runtime_->newInt(99));
  EXPECT_EQ(icUpdateAttr(thread_, caches, 0, LayoutId::kSmallInt, new_value,
                         name, dependent),
            ICState::kMonomorphic);
  EXPECT_EQ(icLookupMonomorphic(*caches, 0, LayoutId::kSmallInt, &is_found),
            *new_value);
  EXPECT_TRUE(is_found);
}

TEST_F(IcTest, IcUpdateAttrSetsPolymorphicEntry) {
  HandleScope scope(thread_);
  MutableTuple caches(&scope,
                      runtime_->newMutableTuple(1 * kIcPointersPerEntry));
  caches.fill(NoneType::object());
  Object int_value(&scope, runtime_->newInt(88));
  Object str_value(&scope, runtime_->newInt(99));
  Object name(&scope, Str::empty());
  Function dependent(&scope, newEmptyFunction());
  ASSERT_EQ(icUpdateAttr(thread_, caches, 0, LayoutId::kSmallInt, int_value,
                         name, dependent),
            ICState::kMonomorphic);
  EXPECT_EQ(icUpdateAttr(thread_, caches, 0, LayoutId::kSmallStr, str_value,
                         name, dependent),
            ICState::kPolymorphic);
  bool is_found;
  EXPECT_EQ(icLookupPolymorphic(*caches, 0, LayoutId::kSmallInt, &is_found),
            *int_value);
  EXPECT_TRUE(is_found);
  EXPECT_EQ(icLookupPolymorphic(*caches, 0, LayoutId::kSmallStr, &is_found),
            *str_value);
  EXPECT_TRUE(is_found);
}

TEST_F(IcTest, IcUpdateAttrUpdatesPolymorphicEntry) {
  HandleScope scope(thread_);
  MutableTuple caches(&scope,
                      runtime_->newMutableTuple(1 * kIcPointersPerEntry));
  caches.fill(NoneType::object());
  Object int_value(&scope, runtime_->newInt(88));
  Object str_value(&scope, runtime_->newInt(99));
  Object name(&scope, Str::empty());
  Function dependent(&scope, newEmptyFunction());
  ASSERT_EQ(icUpdateAttr(thread_, caches, 0, LayoutId::kSmallInt, int_value,
                         name, dependent),
            ICState::kMonomorphic);
  ASSERT_EQ(icUpdateAttr(thread_, caches, 0, LayoutId::kSmallStr, str_value,
                         name, dependent),
            ICState::kPolymorphic);
  bool is_found;
  ASSERT_EQ(icLookupPolymorphic(*caches, 0, LayoutId::kSmallInt, &is_found),
            *int_value);
  ASSERT_TRUE(is_found);
  ASSERT_EQ(icLookupPolymorphic(*caches, 0, LayoutId::kSmallStr, &is_found),
            *str_value);
  ASSERT_TRUE(is_found);

  Object new_value(&scope, runtime_->newInt(101));
  EXPECT_EQ(icUpdateAttr(thread_, caches, 0, LayoutId::kSmallStr, new_value,
                         name, dependent),
            ICState::kPolymorphic);
  EXPECT_EQ(icLookupPolymorphic(*caches, 0, LayoutId::kSmallStr, &is_found),
            *new_value);
  EXPECT_TRUE(is_found);
}

TEST_F(IcTest, IcUpdateAttrInsertsDependencyUpToDefiningType) {
  HandleScope scope(thread_);
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class A:
  pass

class B(A):
  foo = "class B"

class C(B):
  bar = "class C"

c = C()
)")
                   .isError());
  // Inserting dependent adds dependent to a new Placeholder in C for 'foo', and
  // to the existing ValueCell in B. A won't be affected since it's not visited
  // during MRO traversal.
  MutableTuple caches(&scope, runtime_->newMutableTuple(4));
  caches.fill(NoneType::object());
  Object c(&scope, mainModuleAt(runtime_, "c"));
  Object value(&scope, SmallInt::fromWord(1234));
  Object foo(&scope, Runtime::internStrFromCStr(thread_, "foo"));
  Function dependent(&scope, newEmptyFunction());
  icUpdateAttr(thread_, caches, 0, c.layoutId(), value, foo, dependent);

  Type type_a(&scope, mainModuleAt(runtime_, "A"));
  RawObject unused = NoneType::object();
  EXPECT_FALSE(attributeValueCellAt(*type_a, *foo, &unused));

  Type type_b(&scope, mainModuleAt(runtime_, "B"));
  ValueCell b_entry(&scope, typeValueCellAt(*type_b, *foo));
  EXPECT_FALSE(b_entry.isPlaceholder());
  WeakLink b_link(&scope, b_entry.dependencyLink());
  EXPECT_EQ(b_link.referent(), dependent);
  EXPECT_TRUE(b_link.next().isNoneType());

  Type type_c(&scope, mainModuleAt(runtime_, "C"));
  ValueCell c_entry(&scope, typeValueCellAt(*type_c, *foo));
  EXPECT_TRUE(c_entry.isPlaceholder());
  WeakLink c_link(&scope, c_entry.dependencyLink());
  EXPECT_EQ(c_link.referent(), dependent);
  EXPECT_TRUE(c_link.next().isNoneType());
}

TEST_F(IcTest, IcUpdateAttrDoesNotInsertsDependencyToSealedType) {
  HandleScope scope(thread_);
  Str instance(&scope, runtime_->newStrFromCStr("str instance"));
  MutableTuple caches(&scope, runtime_->newMutableTuple(4));
  caches.fill(NoneType::object());
  Object value(&scope, SmallInt::fromWord(1234));
  Object dunder_add(&scope, runtime_->symbols()->at(ID(__add__)));
  Function dependent(&scope, newEmptyFunction());
  icUpdateAttr(thread_, caches, 0, instance.layoutId(), value, dunder_add,
               dependent);

  Type type_str(&scope, runtime_->typeAt(LayoutId::kStr));
  ValueCell dunder_add_entry(&scope, typeValueCellAt(*type_str, *dunder_add));
  EXPECT_TRUE(dunder_add_entry.dependencyLink().isNoneType());
}

static RawObject dependencyLinkOfTypeAttr(Thread* thread, const Type& type,
                                          const char* attribute_name) {
  HandleScope scope(thread);
  Object attribute_name_str(&scope,
                            Runtime::internStrFromCStr(thread, attribute_name));
  ValueCell value_cell(&scope, typeValueCellAt(*type, *attribute_name_str));
  return value_cell.dependencyLink();
}

static bool icDependentIncluded(RawObject dependent, RawObject link) {
  for (; !link.isNoneType(); link = WeakLink::cast(link).next()) {
    if (WeakLink::cast(link).referent() == dependent) {
      return true;
    }
  }
  return false;
}

TEST_F(IcTest, IcEvictAttr) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class A:
  def __init__(self):
    self.foo = 4

def cache_a_foo(a):
  return a.foo

a = A()
cache_a_foo(a)

class B:
  pass
)")
                   .isError());

  HandleScope scope(thread_);
  Type type_a(&scope, mainModuleAt(runtime_, "A"));
  Function cache_a_foo(&scope, mainModuleAt(runtime_, "cache_a_foo"));
  MutableTuple caches(&scope, cache_a_foo.caches());
  Object cached_object(&scope, mainModuleAt(runtime_, "a"));
  // Precondition check that the A.foo attribute lookup has been cached.
  ASSERT_FALSE(
      icLookupAttr(*caches, 1, cached_object.layoutId()).isErrorNotFound());
  ASSERT_EQ(WeakLink::cast(dependencyLinkOfTypeAttr(thread_, type_a, "foo"))
                .referent(),
            *cache_a_foo);

  // Try evicting caches with an attribute name that is not in the cache.  This
  // should have no effect.
  Type cached_type(&scope, mainModuleAt(runtime_, "A"));
  IcIterator it(&scope, runtime_, *cache_a_foo);
  Object not_cached_attr_name(&scope,
                              Runtime::internStrFromCStr(thread_, "random"));
  icEvictAttr(thread_, it, cached_type, not_cached_attr_name,
              AttributeKind::kNotADataDescriptor, cache_a_foo);
  EXPECT_FALSE(
      icLookupAttr(*caches, 1, cached_object.layoutId()).isErrorNotFound());
  EXPECT_EQ(WeakLink::cast(dependencyLinkOfTypeAttr(thread_, type_a, "foo"))
                .referent(),
            *cache_a_foo);

  // Try evicting instance attribute caches for a non-data descriptor
  // assignment.  Because instance attributes have a higher priority than
  // non-data descriptors, nothing should be evicted.
  Object foo(&scope, Runtime::internStrFromCStr(thread_, "foo"));
  icEvictAttr(thread_, it, cached_type, foo, AttributeKind::kNotADataDescriptor,
              cache_a_foo);
  EXPECT_FALSE(
      icLookupAttr(*caches, 1, cached_object.layoutId()).isErrorNotFound());
  EXPECT_EQ(WeakLink::cast(dependencyLinkOfTypeAttr(thread_, type_a, "foo"))
                .referent(),
            *cache_a_foo);

  // Try evicting caches with a type that is not being cached.  This should have
  // no effect.
  Type not_cached_type(&scope, mainModuleAt(runtime_, "B"));
  icEvictAttr(thread_, it, not_cached_type, foo, AttributeKind::kDataDescriptor,
              cache_a_foo);
  EXPECT_FALSE(
      icLookupAttr(*caches, 1, cached_object.layoutId()).isErrorNotFound());
  EXPECT_EQ(WeakLink::cast(dependencyLinkOfTypeAttr(thread_, type_a, "foo"))
                .referent(),
            *cache_a_foo);

  // An update to a type attribute whose type, Attribute name with a data
  // desciptor value invalidates an instance attribute cache.
  icEvictAttr(thread_, it, cached_type, foo, AttributeKind::kDataDescriptor,
              cache_a_foo);
  EXPECT_TRUE(
      icLookupAttr(*caches, 1, cached_object.layoutId()).isErrorNotFound());
  // The dependency for cache_a_foo gets deleted.
  EXPECT_FALSE(icDependentIncluded(
      *cache_a_foo, dependencyLinkOfTypeAttr(thread_, type_a, "foo")));
}

TEST_F(IcTest, IcEvictBinaryOpEvictsCacheForUpdateToLeftOperandType) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class A:
  def __ge__(self, other):
    return True

class B:
  def __le__(self, other):
    return True

def cache_binop(a, b):
  return a >= b

a = A()
b = B()

cache_binop(a, b)
)")
                   .isError());
  HandleScope scope(thread_);
  Function cache_binop(&scope, mainModuleAt(runtime_, "cache_binop"));
  MutableTuple caches(&scope, cache_binop.caches());
  Object left_operand(&scope, mainModuleAt(runtime_, "a"));
  Object right_operand(&scope, mainModuleAt(runtime_, "b"));
  Type left_operand_type(&scope, mainModuleAt(runtime_, "A"));
  BinaryOpFlags flags_out;
  // Precondition check that the A.__ge__ attribute lookup has been cached.
  ASSERT_FALSE(icLookupBinaryOp(*caches, 0, left_operand.layoutId(),
                                right_operand.layoutId(), &flags_out)
                   .isErrorNotFound());

  IcIterator it(&scope, runtime_, *cache_binop);

  // An update to A.__ge__ invalidates the binop cache for a >= b.
  Object dunder_ge(&scope, Runtime::internStrFromCStr(thread_, "__ge__"));
  icEvictBinaryOp(thread_, it, left_operand_type, dunder_ge, cache_binop);
  EXPECT_TRUE(icLookupBinaryOp(*caches, 0, left_operand.layoutId(),
                               right_operand.layoutId(), &flags_out)
                  .isErrorNotFound());
}

TEST_F(IcTest, IcEvictBinaryOpEvictsCacheForUpdateToRightOperand) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class A:
  def __ge__(self, other):
    return True

class B:
  def __le__(self, other):
    return True

def cache_binop(a, b):
  return a >= b

a = A()
b = B()

cache_binop(a, b)
)")
                   .isError());
  HandleScope scope(thread_);
  Function cache_binop(&scope, mainModuleAt(runtime_, "cache_binop"));
  MutableTuple caches(&scope, cache_binop.caches());
  Object left_operand(&scope, mainModuleAt(runtime_, "a"));
  Object right_operand(&scope, mainModuleAt(runtime_, "b"));
  Type right_operand_type(&scope, mainModuleAt(runtime_, "B"));
  BinaryOpFlags flags_out;
  // Precondition check that the A.__ge__ attribute lookup has been cached.
  ASSERT_FALSE(icLookupBinaryOp(*caches, 0, left_operand.layoutId(),
                                right_operand.layoutId(), &flags_out)
                   .isErrorNotFound());

  IcIterator it(&scope, runtime_, *cache_binop);
  Object dunder_le(&scope, Runtime::internStrFromCStr(thread_, "__le__"));
  // An update to B.__le__ invalidates the binop cache for a >= b.
  icEvictBinaryOp(thread_, it, right_operand_type, dunder_le, cache_binop);
  EXPECT_TRUE(icLookupBinaryOp(*caches, 0, left_operand.layoutId(),
                               right_operand.layoutId(), &flags_out)
                  .isErrorNotFound());
}

TEST_F(IcTest, IcEvictBinaryOpDoesnNotDeleteDependenciesFromCachedTypes) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class A:
  def __ge__(self, other): return True

class B:
  def __le__(self, other): return True

def cache_compare_op(a, b):
  t0 = a >= b
  t1 = b <= 5

a = A()
b = B()

cache_compare_op(a, b)

A__ge__ = A.__ge__
B__le__ = B.__le__
)")
                   .isError());
  HandleScope scope(thread_);
  Object a(&scope, mainModuleAt(runtime_, "a"));
  Object b(&scope, mainModuleAt(runtime_, "b"));

  Object type_a_dunder_ge(&scope, mainModuleAt(runtime_, "A__ge__"));
  Object type_b_dunder_le(&scope, mainModuleAt(runtime_, "B__le__"));
  Function cache_compare_op(&scope, mainModuleAt(runtime_, "cache_compare_op"));
  MutableTuple caches(&scope, cache_compare_op.caches());
  BinaryOpFlags flags_out;
  // Ensure that A.__ge__ is cached for t0 = a >= b.
  ASSERT_EQ(
      icLookupBinaryOp(*caches, 0, a.layoutId(), b.layoutId(), &flags_out),
      *type_a_dunder_ge);
  // Ensure that B.__le__ is cached for t1 = b >= 5.
  ASSERT_EQ(icLookupBinaryOp(*caches, 1, b.layoutId(),
                             SmallInt::fromWord(0).layoutId(), &flags_out),
            *type_b_dunder_le);

  Type type_a(&scope, mainModuleAt(runtime_, "A"));
  // Ensure cache_compare_op is a dependent of A.__ge__.
  ASSERT_TRUE(icDependentIncluded(
      *cache_compare_op, dependencyLinkOfTypeAttr(thread_, type_a, "__ge__")));

  Type type_b(&scope, mainModuleAt(runtime_, "B"));
  // Ensure cache_compare_op is a dependent of B.__le__.
  ASSERT_TRUE(icDependentIncluded(
      *cache_compare_op, dependencyLinkOfTypeAttr(thread_, type_b, "__le__")));

  // Update A.__ge__ to invalidate cache for t0 = a >= b.
  Object dunder_ge_name(&scope, Runtime::internStrFromCStr(thread_, "__ge__"));
  icEvictCache(thread_, cache_compare_op, type_a, dunder_ge_name,
               AttributeKind::kNotADataDescriptor);
  // The invalidation removes dependency from cache_compare_op to A.__ge__.
  EXPECT_FALSE(icDependentIncluded(
      *cache_compare_op, dependencyLinkOfTypeAttr(thread_, type_a, "__ge__")));
  // However, cache_compare_op still depends on B.__le__ since b >= 5 is cached.
  EXPECT_TRUE(icDependentIncluded(
      *cache_compare_op, dependencyLinkOfTypeAttr(thread_, type_b, "__le__")));
}

TEST_F(IcTest, IcDeleteDependentInValueCellDependencyLinkDeletesDependent) {
  HandleScope scope(thread_);
  ValueCell value_cell(&scope, runtime_->newValueCell());
  Object dependent0(&scope, newTupleWithNone(4));
  Object dependent1(&scope, newTupleWithNone(5));
  Object dependent2(&scope, newTupleWithNone(6));
  Object dependent3(&scope, newTupleWithNone(7));
  icInsertDependentToValueCellDependencyLink(thread_, dependent3, value_cell);
  icInsertDependentToValueCellDependencyLink(thread_, dependent2, value_cell);
  icInsertDependentToValueCellDependencyLink(thread_, dependent1, value_cell);
  icInsertDependentToValueCellDependencyLink(thread_, dependent0, value_cell);

  // Delete the head.
  icDeleteDependentInValueCell(thread_, value_cell, dependent0);

  WeakLink link(&scope, value_cell.dependencyLink());
  EXPECT_EQ(link.referent(), *dependent1);
  EXPECT_TRUE(link.prev().isNoneType());
  EXPECT_EQ(WeakLink::cast(link.next()).referent(), *dependent2);
  EXPECT_EQ(WeakLink::cast(link.next()).prev(), *link);

  // Delete the dependent in the middle.
  icDeleteDependentInValueCell(thread_, value_cell, dependent2);

  link = value_cell.dependencyLink();
  EXPECT_EQ(link.referent(), *dependent1);
  EXPECT_EQ(WeakLink::cast(link.next()).referent(), *dependent3);
  EXPECT_EQ(WeakLink::cast(link.next()).prev(), *link);

  // Delete the tail.
  icDeleteDependentInValueCell(thread_, value_cell, dependent3);

  link = value_cell.dependencyLink();
  EXPECT_EQ(link.referent(), *dependent1);
  EXPECT_TRUE(link.next().isNoneType());

  // Delete the last node.
  icDeleteDependentInValueCell(thread_, value_cell, dependent1);
  EXPECT_TRUE(value_cell.dependencyLink().isNoneType());
}

TEST_F(
    IcTest,
    IcDeleteDependentFromCachedAttributeDeletesDependentUnderAttributeNameInMro) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class A:
  def foo(self): return 1
  def bar(self): return 1

def x(a):
  return a.foo()

def y(a):
  return a.bar()

a = A()

x(a)
y(a)
)")
                   .isError());

  HandleScope scope(thread_);
  Type type_a(&scope, mainModuleAt(runtime_, "A"));
  Object foo_name(&scope, Runtime::internStrFromCStr(thread_, "foo"));
  Object bar_name(&scope, Runtime::internStrFromCStr(thread_, "bar"));
  Function dependent_x(&scope, mainModuleAt(runtime_, "x"));
  Function dependent_y(&scope, mainModuleAt(runtime_, "y"));

  // A.foo -> x
  ValueCell foo_in_a(&scope, typeValueCellAt(*type_a, *foo_name));
  ASSERT_EQ(WeakLink::cast(foo_in_a.dependencyLink()).referent(), *dependent_x);

  // A.bar -> y
  ValueCell bar_in_a(&scope, typeValueCellAt(*type_a, *bar_name));
  ASSERT_EQ(WeakLink::cast(bar_in_a.dependencyLink()).referent(), *dependent_y);

  LayoutId type_a_instance_layout_id = type_a.instanceLayoutId();
  // Try to delete dependent_y under name "foo". Nothing happens.
  icDeleteDependentFromInheritingTypes(thread_, type_a_instance_layout_id,
                                       foo_name, type_a, dependent_y);
  EXPECT_EQ(WeakLink::cast(foo_in_a.dependencyLink()).referent(), *dependent_x);
  EXPECT_EQ(WeakLink::cast(bar_in_a.dependencyLink()).referent(), *dependent_y);

  // Try to delete dependent_x under name "bar". Nothing happens.
  icDeleteDependentFromInheritingTypes(thread_, type_a_instance_layout_id,
                                       bar_name, type_a, dependent_x);
  EXPECT_EQ(WeakLink::cast(foo_in_a.dependencyLink()).referent(), *dependent_x);
  EXPECT_EQ(WeakLink::cast(bar_in_a.dependencyLink()).referent(), *dependent_y);

  icDeleteDependentFromInheritingTypes(thread_, type_a_instance_layout_id,
                                       foo_name, type_a, dependent_x);
  EXPECT_TRUE(foo_in_a.dependencyLink().isNoneType());
  EXPECT_EQ(WeakLink::cast(bar_in_a.dependencyLink()).referent(), *dependent_y);

  icDeleteDependentFromInheritingTypes(thread_, type_a_instance_layout_id,
                                       bar_name, type_a, dependent_y);
  EXPECT_TRUE(foo_in_a.dependencyLink().isNoneType());
  EXPECT_TRUE(bar_in_a.dependencyLink().isNoneType());
}

TEST_F(IcTest,
       IcDeleteDependentFromCachedAttributeDeletesDependentUpToUpdatedType) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class A:
  def foo(self): return 1

class B(A):
  def foo(self): return 2

class C(B):
  pass

def x(c):
  return c.foo()

c = C()
x(c)

a = A()
x(a)
)")
                   .isError());

  HandleScope scope(thread_);
  Type a(&scope, mainModuleAt(runtime_, "A"));
  Type b(&scope, mainModuleAt(runtime_, "B"));
  Type c(&scope, mainModuleAt(runtime_, "C"));
  Object dependent_x(&scope, mainModuleAt(runtime_, "x"));
  Object foo_name(&scope, Runtime::internStrFromCStr(thread_, "foo"));

  // A.foo -> x
  ValueCell foo_in_a(&scope, typeValueCellAt(*a, *foo_name));
  ASSERT_FALSE(foo_in_a.isPlaceholder());
  ASSERT_EQ(WeakLink::cast(foo_in_a.dependencyLink()).referent(), *dependent_x);

  // B.foo -> x
  ValueCell foo_in_b(&scope, typeValueCellAt(*b, *foo_name));
  ASSERT_FALSE(foo_in_b.isPlaceholder());
  ASSERT_EQ(WeakLink::cast(foo_in_b.dependencyLink()).referent(), *dependent_x);

  // C.foo -> x
  // Note that this dependency is a placeholder.
  ValueCell foo_in_c(&scope, typeValueCellAt(*c, *foo_name));
  ASSERT_TRUE(foo_in_c.isPlaceholder());
  ASSERT_EQ(WeakLink::cast(foo_in_c.dependencyLink()).referent(), *dependent_x);

  Object c_obj(&scope, mainModuleAt(runtime_, "c"));
  // Delete dependent_x for an update to B.foo.
  icDeleteDependentFromInheritingTypes(thread_, c_obj.layoutId(), foo_name, b,
                                       dependent_x);

  // B.foo's update doesn't affect the cache for A.foo since the update does not
  // shadow a.foo where type(a) == A.
  EXPECT_TRUE(foo_in_c.dependencyLink().isNoneType());
  EXPECT_TRUE(foo_in_b.dependencyLink().isNoneType());
  // Didn't delete this since type lookup cannot reach A by successful attribute
  // lookup for "foo" in B.
  EXPECT_EQ(WeakLink::cast(foo_in_a.dependencyLink()).referent(), *dependent_x);
}

TEST_F(
    IcTest,
    IcHighestSuperTypeNotInMroOfOtherCachedTypesReturnsHighestNotCachedSuperType) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class A:
  def foo(self):
    return 4

class B(A):
  pass

def cache_foo(x):
  return x.foo

a_foo = A.foo
b = B()
cache_foo(b)
)")
                   .isError());
  HandleScope scope(thread_);
  Function cache_foo(&scope, mainModuleAt(runtime_, "cache_foo"));
  Object a_foo(&scope, mainModuleAt(runtime_, "a_foo"));
  Object b_obj(&scope, mainModuleAt(runtime_, "b"));
  Type a_type(&scope, mainModuleAt(runtime_, "A"));
  MutableTuple caches(&scope, cache_foo.caches());
  ASSERT_EQ(icLookupAttr(*caches, 1, b_obj.layoutId()), *a_foo);
  // Manually delete the cache for B.foo in cache_foo.
  caches.atPut(1 * kIcPointersPerEntry + kIcEntryKeyOffset, NoneType::object());
  caches.atPut(1 * kIcPointersPerEntry + kIcEntryValueOffset,
               NoneType::object());
  ASSERT_TRUE(icLookupAttr(*caches, 1, b_obj.layoutId()).isErrorNotFound());

  // Now cache_foo doesn't depend on neither A.foo nor B.foo, so this should
  // return A.
  Object foo(&scope, Runtime::internStrFromCStr(thread_, "foo"));
  Object result(&scope, icHighestSuperTypeNotInMroOfOtherCachedTypes(
                            thread_, b_obj.layoutId(), foo, cache_foo));
  EXPECT_EQ(result, *a_type);
}

TEST_F(IcTest, IcIsCachedAttributeAffectedByUpdatedType) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class A:
  def foo(self): return 1

class B(A):
  def foo(self): return 2

class C(B):
  pass


def x(c):
  return c.foo()

c = C()
x(c)
)")
                   .isError());
  HandleScope scope(thread_);
  Type type_a(&scope, mainModuleAt(runtime_, "A"));
  Type type_b(&scope, mainModuleAt(runtime_, "B"));
  Type type_c(&scope, mainModuleAt(runtime_, "C"));
  Object foo_name(&scope, Runtime::internStrFromCStr(thread_, "foo"));

  LayoutId type_c_instance_layout_id = type_c.instanceLayoutId();
  // Check if A.foo is not retrived from C.foo.
  EXPECT_FALSE(icIsCachedAttributeAffectedByUpdatedType(
      thread_, type_c_instance_layout_id, foo_name, type_a));
  // Check if B.foo is retrieved from C.foo.
  EXPECT_TRUE(icIsCachedAttributeAffectedByUpdatedType(
      thread_, type_c_instance_layout_id, foo_name, type_b));

  // Assign C.foo to a real value.
  ValueCell foo_in_c(&scope, typeValueCellAt(*type_c, *foo_name));
  foo_in_c.setValue(NoneType::object());
  // Check if B.foo is not retrived from C.foo from now on.
  EXPECT_FALSE(icIsCachedAttributeAffectedByUpdatedType(
      thread_, type_c_instance_layout_id, foo_name, type_b));
  // Instead, C.foo is retrieved.
  EXPECT_TRUE(icIsCachedAttributeAffectedByUpdatedType(
      thread_, type_c_instance_layout_id, foo_name, type_c));
}

// Create a function that maps cache index 1 to the given attribute name.
static RawObject testingFunctionCachingAttributes(
    Thread* thread, const Object& attribute_name) {
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Tuple consts(&scope, runtime->emptyTuple());
  Object name(&scope, Str::empty());
  Tuple names(&scope, runtime->newTupleWith2(attribute_name, name));
  Code code(&scope,
            newCodeWithBytesConstsNames(View<byte>(nullptr, 0), consts, names));

  MutableBytes rewritten_bytecode(&scope,
                                  runtime->newMutableBytesUninitialized(8));
  rewrittenBytecodeOpAtPut(rewritten_bytecode, 0, LOAD_ATTR_ANAMORPHIC);
  rewrittenBytecodeArgAtPut(rewritten_bytecode, 0, 0);
  rewrittenBytecodeCacheAtPut(rewritten_bytecode, 0, 1);

  Module module(&scope, findMainModule(runtime));
  Function function(&scope,
                    runtime->newFunctionWithCode(thread, name, code, module));
  function.setRewrittenBytecode(*rewritten_bytecode);

  MutableTuple caches(&scope,
                      runtime->newMutableTuple(2 * kIcPointersPerEntry));
  caches.fill(NoneType::object());
  function.setCaches(*caches);

  return *function;
}

TEST_F(IcTest, IcEvictCacheEvictsCachesForMatchingAttributeName) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class C: pass

c = C()
)")
                   .isError());
  HandleScope scope(thread_);
  Type type(&scope, mainModuleAt(runtime_, "C"));
  Object foo_name(&scope, Runtime::internStrFromCStr(thread_, "foo"));
  Object bar_name(&scope, Runtime::internStrFromCStr(thread_, "bar"));
  Function dependent(&scope,
                     testingFunctionCachingAttributes(thread_, foo_name));

  // foo -> dependent.
  ValueCell foo(&scope, attributeValueCellAtPut(thread_, type, foo_name));
  ASSERT_TRUE(
      icInsertDependentToValueCellDependencyLink(thread_, dependent, foo));

  // Create an attribute cache for an instance of C, under name "foo".
  Object instance(&scope, mainModuleAt(runtime_, "c"));
  MutableTuple caches(&scope, dependent.caches());
  Object value(&scope, SmallInt::fromWord(1234));
  Object name(&scope, Str::empty());
  icUpdateAttr(thread_, caches, 1, instance.layoutId(), value, name, dependent);
  ASSERT_EQ(icLookupAttr(*caches, 1, instance.layoutId()),
            SmallInt::fromWord(1234));

  // Deleting caches for "bar" doesn't affect the cache for "foo".
  icEvictCache(thread_, dependent, type, bar_name,
               AttributeKind::kDataDescriptor);
  EXPECT_EQ(icLookupAttr(*caches, 1, instance.layoutId()),
            SmallInt::fromWord(1234));

  // Deleting caches for "foo".
  icEvictCache(thread_, dependent, type, foo_name,
               AttributeKind::kDataDescriptor);
  EXPECT_TRUE(icLookupAttr(*caches, 1, instance.layoutId()).isErrorNotFound());
}

TEST_F(IcTest,
       IcEvictCacheEvictsCachesForInstanceOffsetOnlyWhenDataDesciptorIsTrue) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class C: pass

c = C()
)")
                   .isError());
  HandleScope scope(thread_);
  Type type(&scope, mainModuleAt(runtime_, "C"));
  Object foo_name(&scope, Runtime::internStrFromCStr(thread_, "foo"));
  Function dependent(&scope,
                     testingFunctionCachingAttributes(thread_, foo_name));

  // foo -> dependent.
  ValueCell foo(&scope, attributeValueCellAtPut(thread_, type, foo_name));
  ASSERT_TRUE(
      icInsertDependentToValueCellDependencyLink(thread_, dependent, foo));

  // Create an instance offset cache for an instance of C, under name "foo".
  Object instance(&scope, mainModuleAt(runtime_, "c"));
  MutableTuple caches(&scope, dependent.caches());
  Object value(&scope, SmallInt::fromWord(1234));
  Object name(&scope, Str::empty());
  icUpdateAttr(thread_, caches, 1, instance.layoutId(), value, name, dependent);
  ASSERT_EQ(icLookupAttr(*caches, 1, instance.layoutId()),
            SmallInt::fromWord(1234));

  // An attempt to delete caches for "foo" with data_descriptor == false doesn't
  // affect it.
  icEvictCache(thread_, dependent, type, foo_name,
               AttributeKind::kNotADataDescriptor);
  EXPECT_EQ(icLookupAttr(*caches, 1, instance.layoutId()),
            SmallInt::fromWord(1234));

  // Delete caches for "foo" with data_descriptor == true actually deletes it.
  icEvictCache(thread_, dependent, type, foo_name,
               AttributeKind::kDataDescriptor);
  EXPECT_TRUE(icLookupAttr(*caches, 1, instance.layoutId()).isErrorNotFound());
}

TEST_F(IcTest, IcEvictCacheEvictsOnlyAffectedCaches) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class A:
  def foo(self): return  1

class B(A):
  def foo(self): return  2

class C(B): pass

a = A()
b = B()
c = C()
)")
                   .isError());
  HandleScope scope(thread_);
  Type a_type(&scope, mainModuleAt(runtime_, "A"));
  Type b_type(&scope, mainModuleAt(runtime_, "B"));
  Type c_type(&scope, mainModuleAt(runtime_, "C"));
  Object foo_name(&scope, Runtime::internStrFromCStr(thread_, "foo"));
  Function dependent(&scope,
                     testingFunctionCachingAttributes(thread_, foo_name));

  // The following lines simulate that dependent caches a.foo, b.foo, c.foo, and
  // x.foo. A.foo -> dependent.
  ValueCell a_foo(&scope, typeValueCellAt(*a_type, *foo_name));
  ASSERT_TRUE(
      icInsertDependentToValueCellDependencyLink(thread_, dependent, a_foo));
  // B.foo -> dependent.
  ValueCell b_foo(&scope, typeValueCellAt(*b_type, *foo_name));
  ASSERT_TRUE(
      icInsertDependentToValueCellDependencyLink(thread_, dependent, b_foo));
  // C.foo -> dependent.
  ValueCell c_foo(&scope, attributeValueCellAtPut(thread_, c_type, foo_name));
  // This is a placeholder since C.foo is resolved to B.foo.
  c_foo.makePlaceholder();
  ASSERT_TRUE(
      icInsertDependentToValueCellDependencyLink(thread_, dependent, c_foo));

  // Create a cache for a.foo in dependent.
  Object a(&scope, mainModuleAt(runtime_, "a"));
  MutableTuple caches(&scope, dependent.caches());
  Object value_100(&scope, SmallInt::fromWord(100));
  Object name(&scope, Str::empty());
  icUpdateAttr(thread_, caches, 1, a.layoutId(), value_100, name, dependent);
  ASSERT_EQ(icLookupAttr(*caches, 1, a.layoutId()), SmallInt::fromWord(100));
  // Create a cache for b.foo in dependent.
  Object b(&scope, mainModuleAt(runtime_, "b"));
  Object value_200(&scope, SmallInt::fromWord(200));
  icUpdateAttr(thread_, caches, 1, b.layoutId(), value_200, name, dependent);
  ASSERT_EQ(icLookupAttr(*caches, 1, b.layoutId()), SmallInt::fromWord(200));
  // Create a cache for c.foo in dependent.
  Object c(&scope, mainModuleAt(runtime_, "c"));
  Object value_300(&scope, SmallInt::fromWord(300));
  icUpdateAttr(thread_, caches, 1, c.layoutId(), value_300, name, dependent);
  ASSERT_EQ(icLookupAttr(*caches, 1, c.layoutId()), SmallInt::fromWord(300));

  // Trigger invalidation by updating B.foo.
  icEvictCache(thread_, dependent, b_type, foo_name,
               AttributeKind::kDataDescriptor);
  // Note that only caches made for the type attribute are evincted, and
  // dependent is dropped from them.
  EXPECT_EQ(icLookupAttr(*caches, 1, a.layoutId()), SmallInt::fromWord(100));
  EXPECT_EQ(WeakLink::cast(a_foo.dependencyLink()).referent(), *dependent);
  EXPECT_TRUE(icLookupAttr(*caches, 1, b.layoutId()).isErrorNotFound());
  EXPECT_TRUE(b_foo.dependencyLink().isNoneType());
  EXPECT_TRUE(icLookupAttr(*caches, 1, c.layoutId()).isErrorNotFound());
  EXPECT_TRUE(c_foo.dependencyLink().isNoneType());

  // Trigger invalidation by updating A.foo.
  icEvictCache(thread_, dependent, a_type, foo_name,
               AttributeKind::kDataDescriptor);
  EXPECT_TRUE(icLookupAttr(*caches, 1, a.layoutId()).isErrorNotFound());
  EXPECT_TRUE(a_foo.dependencyLink().isNoneType());
}

TEST_F(IcTest, IcEvictCacheWithPolymorphicCacheEvictsCache) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class A: pass

class B: pass

a = A()
b = B()
)")
                   .isError());
  HandleScope scope(thread_);
  Type a_type(&scope, mainModuleAt(runtime_, "A"));
  Object a(&scope, mainModuleAt(runtime_, "a"));
  Type b_type(&scope, mainModuleAt(runtime_, "B"));
  Object b(&scope, mainModuleAt(runtime_, "b"));

  Object a_value(&scope, runtime_->newInt(88));
  Object b_value(&scope, runtime_->newInt(99));
  Object name(&scope, Str::empty());
  Function dependent(&scope, testingFunctionCachingAttributes(thread_, name));
  MutableTuple caches(&scope, dependent.caches());
  ASSERT_EQ(
      icUpdateAttr(thread_, caches, 1, a.layoutId(), a_value, name, dependent),
      ICState::kMonomorphic);
  EXPECT_EQ(
      icUpdateAttr(thread_, caches, 1, b.layoutId(), b_value, name, dependent),
      ICState::kPolymorphic);
  bool is_found;
  EXPECT_EQ(icLookupPolymorphic(*caches, 1, a.layoutId(), &is_found), *a_value);
  EXPECT_TRUE(is_found);
  EXPECT_EQ(icLookupPolymorphic(*caches, 1, b.layoutId(), &is_found), *b_value);
  EXPECT_TRUE(is_found);
  icEvictCache(thread_, dependent, a_type, name,
               AttributeKind::kDataDescriptor);
  EXPECT_TRUE(icLookupPolymorphic(*caches, 1, a.layoutId(), &is_found)
                  .isErrorNotFound());
  EXPECT_FALSE(is_found);
  icEvictCache(thread_, dependent, b_type, name,
               AttributeKind::kDataDescriptor);
  EXPECT_TRUE(icLookupPolymorphic(*caches, 1, b.layoutId(), &is_found)
                  .isErrorNotFound());
  EXPECT_FALSE(is_found);
}

// Verify if IcInvalidateCachesForTypeAttr calls
// DeleteCachesForTypeAttrInDependent with all dependents.
TEST_F(IcTest, IcInvalidateCachesForTypeAttrProcessesAllDependents) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class C: pass

c = C()
)")
                   .isError());
  HandleScope scope(thread_);
  Type type(&scope, mainModuleAt(runtime_, "C"));
  Object foo_name(&scope, Runtime::internStrFromCStr(thread_, "foo"));
  Object bar_name(&scope, Runtime::internStrFromCStr(thread_, "bar"));
  Function dependent0(&scope,
                      testingFunctionCachingAttributes(thread_, foo_name));
  Function dependent1(&scope,
                      testingFunctionCachingAttributes(thread_, bar_name));

  // Create a property so these value cells look like data descriptor attributes
  Object none(&scope, NoneType::object());
  Object data_descriptor(&scope, runtime_->newProperty(none, none, none));

  // foo -> dependent0.
  ValueCell foo(&scope, attributeValueCellAtPut(thread_, type, foo_name));
  foo.setValue(*data_descriptor);
  ASSERT_TRUE(
      icInsertDependentToValueCellDependencyLink(thread_, dependent0, foo));

  // bar -> dependent1.
  ValueCell bar(&scope, attributeValueCellAtPut(thread_, type, bar_name));
  bar.setValue(*data_descriptor);

  ASSERT_TRUE(
      icInsertDependentToValueCellDependencyLink(thread_, dependent1, bar));

  MutableTuple dependent0_caches(&scope, dependent0.caches());
  Object instance(&scope, mainModuleAt(runtime_, "c"));
  {
    // Create an attribute cache for an instance of C, under name "foo" in
    // dependent0.
    Object name(&scope, Str::empty());
    Object value(&scope, SmallInt::fromWord(1234));
    icUpdateAttr(thread_, dependent0_caches, 1, instance.layoutId(), value,
                 name, dependent0);
    ASSERT_EQ(icLookupAttr(*dependent0_caches, 1, instance.layoutId()),
              SmallInt::fromWord(1234));
  }

  MutableTuple dependent1_caches(&scope, dependent1.caches());
  {
    // Create an attribute cache for an instance of C, under name "bar" in
    // dependent1.
    Object name(&scope, Str::empty());
    Object value(&scope, SmallInt::fromWord(5678));
    icUpdateAttr(thread_, dependent1_caches, 1, instance.layoutId(), value,
                 name, dependent1);
    ASSERT_EQ(icLookupAttr(*dependent1_caches, 1, instance.layoutId()),
              SmallInt::fromWord(5678));
  }

  icInvalidateAttr(thread_, type, foo_name, foo);
  EXPECT_TRUE(icLookupAttr(*dependent0_caches, 1, instance.layoutId())
                  .isErrorNotFound());
  EXPECT_EQ(icLookupAttr(*dependent1_caches, 1, instance.layoutId()),
            SmallInt::fromWord(5678));

  icInvalidateAttr(thread_, type, bar_name, bar);
  EXPECT_TRUE(icLookupAttr(*dependent0_caches, 1, instance.layoutId())
                  .isErrorNotFound());
  EXPECT_TRUE(icLookupAttr(*dependent1_caches, 1, instance.layoutId())
                  .isErrorNotFound());
}

TEST_F(IcTest,
       BinarySubscrUpdateCacheWithRaisingDescriptorPropagatesException) {
  EXPECT_TRUE(raisedWithStr(runFromCStr(runtime_, R"(
class Desc:
  def __get__(self, instance, type):
    raise UserWarning("foo")

class C:
  __getitem__ = Desc()

container = C()
result = container[0]
)"),
                            LayoutId::kUserWarning, "foo"));
}

TEST_F(IcTest, IcIsAttrCachedInDependentReturnsTrueForAttrCaches) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class X:
  def foo(self): return 4

class Y(X):
  pass

class A:
  def foo(self): return 4

class B(A):
  pass

def cache_Y_foo():
  return Y().foo()

cache_Y_foo()
)")
                   .isError());
  HandleScope scope(thread_);
  Type type_a(&scope, mainModuleAt(runtime_, "A"));
  Type type_b(&scope, mainModuleAt(runtime_, "B"));
  Type type_x(&scope, mainModuleAt(runtime_, "X"));
  Type type_y(&scope, mainModuleAt(runtime_, "Y"));
  Object foo(&scope, Runtime::internStrFromCStr(thread_, "foo"));
  Object bar(&scope, Runtime::internStrFromCStr(thread_, "bar"));
  Function cache_y_foo(&scope, mainModuleAt(runtime_, "cache_Y_foo"));

  // Note that cache_y_foo depends both on X.foo and Y.foo since an
  // update to either one of them flows to Y().foo().
  EXPECT_TRUE(icIsAttrCachedInDependent(thread_, type_x, foo, cache_y_foo));
  EXPECT_TRUE(icIsAttrCachedInDependent(thread_, type_y, foo, cache_y_foo));
  EXPECT_FALSE(icIsAttrCachedInDependent(thread_, type_x, bar, cache_y_foo));
  EXPECT_FALSE(icIsAttrCachedInDependent(thread_, type_a, foo, cache_y_foo));
  EXPECT_FALSE(icIsAttrCachedInDependent(thread_, type_b, foo, cache_y_foo));
}

TEST_F(IcTest, IcIsAttrCachedInDependentReturnsTrueForBinaryOpCaches) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class X:
  def __ge__(self, other): return 5

class Y(X):
  pass

class A:
  def foo(self): return 4

class B(A):
  pass

def cache_Y_ge():
  return Y() >= B()

cache_Y_ge()
)")
                   .isError());
  HandleScope scope(thread_);
  Type type_x(&scope, mainModuleAt(runtime_, "X"));
  Type type_y(&scope, mainModuleAt(runtime_, "Y"));
  Type type_a(&scope, mainModuleAt(runtime_, "A"));
  Type type_b(&scope, mainModuleAt(runtime_, "B"));
  Object dunder_ge(&scope, Runtime::internStrFromCStr(thread_, "__ge__"));
  Object dunder_le(&scope, Runtime::internStrFromCStr(thread_, "__le__"));
  Function cache_ge(&scope, mainModuleAt(runtime_, "cache_Y_ge"));

  // Note that cache_ge indirectly depends on X, but directly on Y since both
  // X.__ge__ and Y.__ge__ affect Y() >= sth.
  EXPECT_TRUE(icIsAttrCachedInDependent(thread_, type_x, dunder_ge, cache_ge));
  EXPECT_TRUE(icIsAttrCachedInDependent(thread_, type_y, dunder_ge, cache_ge));
  // Note that cache_ge indirectly depends on A, but directly on B since both
  // B.__le__ and C.__le__ affect sth >= B().
  EXPECT_TRUE(icIsAttrCachedInDependent(thread_, type_a, dunder_le, cache_ge));
  EXPECT_TRUE(icIsAttrCachedInDependent(thread_, type_b, dunder_le, cache_ge));

  EXPECT_FALSE(icIsAttrCachedInDependent(thread_, type_x, dunder_le, cache_ge));
  EXPECT_FALSE(icIsAttrCachedInDependent(thread_, type_y, dunder_le, cache_ge));
  EXPECT_FALSE(icIsAttrCachedInDependent(thread_, type_a, dunder_ge, cache_ge));
  EXPECT_FALSE(icIsAttrCachedInDependent(thread_, type_b, dunder_ge, cache_ge));
}

TEST_F(IcTest, IcDependentIncludedWithNoneLinkReturnsFalse) {
  EXPECT_FALSE(icDependentIncluded(Unbound::object(), NoneType::object()));
}

TEST_F(IcTest, IcDependentIncludedWithDependentInChainReturnsTrue) {
  HandleScope scope(thread_);
  Object none(&scope, NoneType::object());
  Object one(&scope, runtime_->newSet());
  Object two(&scope, runtime_->newSet());
  Object three(&scope, runtime_->newSet());
  // Set up None <- link0 <-> link1 <-> link2 -> None
  WeakLink link0(&scope, runtime_->newWeakLink(thread_, one, none, none));
  WeakLink link1(&scope, runtime_->newWeakLink(thread_, two, link0, none));
  WeakLink link2(&scope, runtime_->newWeakLink(thread_, three, link1, none));
  link0.setNext(*link1);
  link1.setNext(*link2);
  EXPECT_TRUE(icDependentIncluded(*one, *link0));
  EXPECT_TRUE(icDependentIncluded(*two, *link0));
  EXPECT_TRUE(icDependentIncluded(*three, *link0));

  EXPECT_FALSE(icDependentIncluded(*one, *link1));
  EXPECT_TRUE(icDependentIncluded(*two, *link1));
  EXPECT_TRUE(icDependentIncluded(*three, *link1));

  EXPECT_FALSE(icDependentIncluded(*one, *link2));
  EXPECT_FALSE(icDependentIncluded(*two, *link2));
  EXPECT_TRUE(icDependentIncluded(*three, *link2));

  EXPECT_FALSE(icDependentIncluded(Unbound::object(), *link0));
  EXPECT_FALSE(icDependentIncluded(Unbound::object(), *link1));
  EXPECT_FALSE(icDependentIncluded(Unbound::object(), *link2));
}

TEST_F(IcTest, IcEvictCacheEvictsCompareOpCaches) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class A:
  def __ge__(self, other): return True

class B: pass

def cache_compare_op(a, b):
  return a >= b

a = A()
b = B()
A__ge__ = A.__ge__

cache_compare_op(a, b)
)")
                   .isError());
  HandleScope scope(thread_);
  Object a(&scope, mainModuleAt(runtime_, "a"));
  Object b(&scope, mainModuleAt(runtime_, "b"));
  Object type_a_dunder_ge(&scope, mainModuleAt(runtime_, "A__ge__"));
  Function cache_compare_op(&scope, mainModuleAt(runtime_, "cache_compare_op"));
  MutableTuple caches(&scope, cache_compare_op.caches());
  BinaryOpFlags flags_out;
  Object cached(&scope, icLookupBinaryOp(*caches, 0, a.layoutId(), b.layoutId(),
                                         &flags_out));
  // Precondition check that the A.__ge__ lookup has been cached.
  ASSERT_EQ(*cached, *type_a_dunder_ge);
  Type type_a(&scope, mainModuleAt(runtime_, "A"));
  Object dunder_ge_name(&scope, Runtime::internStrFromCStr(thread_, "__ge__"));
  ValueCell dunder_ge(&scope, typeValueCellAt(*type_a, *dunder_ge_name));
  WeakLink dunder_ge_link(&scope, dunder_ge.dependencyLink());
  // Precondition check that cache_compare_op is a dependent of A.__ge__.
  ASSERT_EQ(dunder_ge_link.referent(), *cache_compare_op);
  Type type_b(&scope, mainModuleAt(runtime_, "B"));
  Object dunder_le_name(&scope, Runtime::internStrFromCStr(thread_, "__le__"));
  ValueCell dunder_le(&scope, typeValueCellAt(*type_b, *dunder_le_name));
  WeakLink dunder_le_link(&scope, dunder_le.dependencyLink());
  // Precondition check that cache_compare_op is a dependent of B.__le__.
  ASSERT_EQ(dunder_le_link.referent(), *cache_compare_op);

  // Updating A.__ge__ triggers cache invalidation.
  icEvictCache(thread_, cache_compare_op, type_a, dunder_ge_name,
               AttributeKind::kNotADataDescriptor);
  EXPECT_TRUE(
      icLookupBinaryOp(*caches, 0, a.layoutId(), b.layoutId(), &flags_out)
          .isErrorNotFound());
  EXPECT_FALSE(
      icDependentIncluded(*cache_compare_op, dunder_ge.dependencyLink()));
  EXPECT_TRUE(dunder_le.dependencyLink().isNoneType());
}

TEST_F(IcTest,
       ForIterUpdateCacheWithRaisingDescriptorDunderNextPropagatesException) {
  EXPECT_TRUE(raisedWithStr(runFromCStr(runtime_, R"(
class Desc:
  def __get__(self, instance, type):
    raise UserWarning("foo")

class C:
  def __iter__(self):
    return self
  __next__ = Desc()

container = C()
result = [x for x in container]
)"),
                            LayoutId::kUserWarning, "foo"));
}

TEST_F(IcTest, BinarySubscrUpdateCacheWithFunctionUpdatesCache) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class Container:
  def __getitem__(self, index):
    return index + 1

def f(c, k):
  return c[k]

container = Container()
getitem = type(container).__getitem__
result = f(container, 0)
)")
                   .isError());

  HandleScope scope(thread_);
  Object result(&scope, mainModuleAt(runtime_, "result"));
  EXPECT_TRUE(isIntEqualsWord(*result, 1));

  Object container(&scope, mainModuleAt(runtime_, "container"));
  Object getitem(&scope, mainModuleAt(runtime_, "getitem"));
  Function f(&scope, mainModuleAt(runtime_, "f"));
  MutableTuple caches(&scope, f.caches());
  // Expect that BINARY_SUBSCR is the only cached opcode in f().
  ASSERT_EQ(caches.length(), 1 * kIcPointersPerEntry);
  EXPECT_EQ(icLookupAttr(*caches, 0, container.layoutId()), *getitem);

  ASSERT_FALSE(runFromCStr(runtime_, R"(
container2 = Container()
result2 = f(container2, 1)
)")
                   .isError());
  Object container2(&scope, mainModuleAt(runtime_, "container2"));
  Object result2(&scope, mainModuleAt(runtime_, "result2"));
  EXPECT_EQ(container2.layoutId(), container.layoutId());
  EXPECT_TRUE(isIntEqualsWord(*result2, 2));
}

TEST_F(IcTest, BinarySubscrUpdateCacheWithNonFunctionDoesntUpdateCache) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
def f(c, k):
  return c[k]
class Container:
  def get(self):
    def getitem(key):
      return key
    return getitem

  __getitem__ = property(get)

container = Container()
result = f(container, "hi")
)")
                   .isError());

  HandleScope scope(thread_);
  Object result(&scope, mainModuleAt(runtime_, "result"));
  EXPECT_TRUE(isStrEqualsCStr(*result, "hi"));

  Object container(&scope, mainModuleAt(runtime_, "container"));
  Function f(&scope, mainModuleAt(runtime_, "f"));
  MutableTuple caches(&scope, f.caches());
  // Expect that BINARY_SUBSCR is the only cached opcode in f().
  ASSERT_EQ(caches.length(), 1 * kIcPointersPerEntry);
  EXPECT_TRUE(icLookupAttr(*caches, 0, container.layoutId()).isErrorNotFound());

  ASSERT_FALSE(runFromCStr(runtime_, R"(
container2 = Container()
result2 = f(container, "hello there!")
)")
                   .isError());
  Object container2(&scope, mainModuleAt(runtime_, "container2"));
  Object result2(&scope, mainModuleAt(runtime_, "result2"));
  ASSERT_EQ(container2.layoutId(), container.layoutId());
  EXPECT_TRUE(isStrEqualsCStr(*result2, "hello there!"));
}

TEST_F(IcTest, IcUpdateBinaryOpSetsEmptyEntry) {
  HandleScope scope(thread_);
  MutableTuple caches(&scope,
                      runtime_->newMutableTuple(2 * kIcPointersPerEntry));
  caches.fill(NoneType::object());
  Object value(&scope, runtime_->newStrFromCStr("this is a random value"));
  EXPECT_EQ(icUpdateBinOp(thread_, caches, 1, LayoutId::kLargeInt,
                          LayoutId::kSmallInt, value, kBinaryOpNone),
            ICState::kMonomorphic);
  BinaryOpFlags flags;
  EXPECT_EQ(icLookupBinOpMonomorphic(*caches, 1, LayoutId::kLargeInt,
                                     LayoutId::kSmallInt, &flags),
            *value);
}

TEST_F(IcTest, IcUpdateBinaryOpSetsExistingMonomorphicEntry) {
  HandleScope scope(thread_);

  MutableTuple caches(&scope,
                      runtime_->newMutableTuple(2 * kIcPointersPerEntry));
  caches.fill(NoneType::object());
  Object value(&scope, runtime_->newStrFromCStr("xxx"));
  ASSERT_EQ(icUpdateBinOp(thread_, caches, 1, LayoutId::kLargeInt,
                          LayoutId::kSmallInt, value, kBinaryOpNone),
            ICState::kMonomorphic);
  Object new_value(&scope, runtime_->newStrFromCStr("yyy"));
  EXPECT_EQ(icUpdateBinOp(thread_, caches, 1, LayoutId::kLargeInt,
                          LayoutId::kSmallInt, new_value, kBinaryOpNone),
            ICState::kMonomorphic);
  BinaryOpFlags flags;
  EXPECT_EQ(icLookupBinOpMonomorphic(*caches, 1, LayoutId::kLargeInt,
                                     LayoutId::kSmallInt, &flags),
            *new_value);
}

TEST_F(IcTest, IcUpdateBinaryOpSetsExistingPolymorphicEntry) {
  HandleScope scope(thread_);

  MutableTuple caches(&scope,
                      runtime_->newMutableTuple(2 * kIcPointersPerEntry));
  caches.fill(NoneType::object());
  Object value(&scope, runtime_->newStrFromCStr("xxx"));
  ASSERT_EQ(icUpdateBinOp(thread_, caches, 1, LayoutId::kLargeInt,
                          LayoutId::kSmallInt, value, kBinaryOpNone),
            ICState::kMonomorphic);
  BinaryOpFlags flags;
  ASSERT_EQ(icLookupBinOpMonomorphic(*caches, 1, LayoutId::kLargeInt,
                                     LayoutId::kSmallInt, &flags),
            *value);

  ASSERT_EQ(icUpdateBinOp(thread_, caches, 1, LayoutId::kSmallInt,
                          LayoutId::kLargeInt, value, kBinaryOpNone),
            ICState::kPolymorphic);
  ASSERT_EQ(icLookupBinOpPolymorphic(*caches, 1, LayoutId::kSmallInt,
                                     LayoutId::kLargeInt, &flags),
            *value);

  Object new_value(&scope, runtime_->newStrFromCStr("yyy"));
  EXPECT_EQ(icUpdateBinOp(thread_, caches, 1, LayoutId::kLargeInt,
                          LayoutId::kSmallInt, new_value, kBinaryOpNone),
            ICState::kPolymorphic);
  EXPECT_EQ(icLookupBinOpPolymorphic(*caches, 1, LayoutId::kLargeInt,
                                     LayoutId::kSmallInt, &flags),
            *new_value);
}

TEST_F(IcTest, ForIterUpdateCacheWithFunctionUpdatesCache) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
def f(container):
  for i in container:
    return i

class C:
  def __iter__(self):
    return Iterator()

class Iterator:
  def __init__(self):
    self.next_called = False

  def __next__(self):
    if self.next_called:
      raise StopIteration
    return 1

container = C()
iterator = iter(container)
iter_next = Iterator.__next__
result = f(container)
)")
                   .isError());

  HandleScope scope(thread_);
  Object result(&scope, mainModuleAt(runtime_, "result"));
  EXPECT_TRUE(isIntEqualsWord(*result, 1));

  Object iterator(&scope, mainModuleAt(runtime_, "iterator"));
  Object iter_next(&scope, mainModuleAt(runtime_, "iter_next"));
  Function f(&scope, mainModuleAt(runtime_, "f"));
  MutableTuple caches(&scope, f.caches());
  // Expect that FOR_ITER is the only cached opcode in f().
  ASSERT_EQ(caches.length(), 1 * kIcPointersPerEntry);
  EXPECT_EQ(icLookupAttr(*caches, 0, iterator.layoutId()), *iter_next);
}

TEST_F(IcTest, ForIterUpdateCacheWithNonFunctionDoesntUpdateCache) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
def f(container):
  for i in container:
    return i

class Iter:
  def get(self):
    def next():
      return 123
    return next
  __next__ = property(get)

class Container:
  def __iter__(self):
    return Iter()

container = Container()
iterator = iter(container)
result = f(container)
)")
                   .isError());

  HandleScope scope(thread_);
  Object result(&scope, mainModuleAt(runtime_, "result"));
  EXPECT_TRUE(isIntEqualsWord(*result, 123));

  Object iterator(&scope, mainModuleAt(runtime_, "iterator"));
  Function f(&scope, mainModuleAt(runtime_, "f"));
  MutableTuple caches(&scope, f.caches());
  // Expect that FOR_ITER is the only cached opcode in f().
  ASSERT_EQ(caches.length(), 1 * kIcPointersPerEntry);
  EXPECT_TRUE(icLookupAttr(*caches, 0, iterator.layoutId()).isErrorNotFound());
}

static RawObject testingFunction(Thread* thread) {
  Runtime* runtime = thread->runtime();
  HandleScope scope(thread);
  Object name(&scope, Str::empty());
  Tuple consts(&scope, runtime->emptyTuple());
  Tuple names(&scope, runtime->newTupleWith2(name, name));
  Code code(&scope,
            newCodeWithBytesConstsNames(View<byte>(nullptr, 0), consts, names));
  MutableBytes rewritten_bytecode(
      &scope, runtime->newMutableBytesUninitialized(4 * kCodeUnitSize));
  rewrittenBytecodeOpAtPut(rewritten_bytecode, 0, LOAD_GLOBAL);
  rewrittenBytecodeArgAtPut(rewritten_bytecode, 0, 0);
  rewrittenBytecodeOpAtPut(rewritten_bytecode, 1, STORE_GLOBAL);
  rewrittenBytecodeArgAtPut(rewritten_bytecode, 1, 1);
  rewrittenBytecodeOpAtPut(rewritten_bytecode, 2, LOAD_GLOBAL);
  rewrittenBytecodeArgAtPut(rewritten_bytecode, 2, 0);
  rewrittenBytecodeOpAtPut(rewritten_bytecode, 3, STORE_GLOBAL);
  rewrittenBytecodeArgAtPut(rewritten_bytecode, 3, 1);

  Module module(&scope, findMainModule(runtime));
  Function function(&scope,
                    runtime->newFunctionWithCode(thread, name, code, module));
  function.setRewrittenBytecode(*rewritten_bytecode);

  MutableTuple caches(&scope, runtime->newMutableTuple(2));
  caches.fill(NoneType::object());
  function.setCaches(*caches);
  return *function;
}

TEST_F(IcTest,
       IcInsertDependentToValueCellDependencyLinkInsertsDependentAsHead) {
  HandleScope scope(thread_);
  Function function0(&scope, testingFunction(thread_));
  Function function1(&scope, testingFunction(thread_));

  ValueCell cache(&scope, runtime_->newValueCell());
  ASSERT_TRUE(cache.dependencyLink().isNoneType());

  EXPECT_TRUE(
      icInsertDependentToValueCellDependencyLink(thread_, function0, cache));
  WeakLink link0(&scope, cache.dependencyLink());
  EXPECT_EQ(link0.referent(), *function0);
  EXPECT_TRUE(link0.prev().isNoneType());
  EXPECT_TRUE(link0.next().isNoneType());

  EXPECT_TRUE(
      icInsertDependentToValueCellDependencyLink(thread_, function1, cache));
  WeakLink link1(&scope, cache.dependencyLink());
  EXPECT_EQ(link1.referent(), *function1);
  EXPECT_TRUE(link1.prev().isNoneType());
  EXPECT_EQ(link1.next(), *link0);
}

TEST_F(
    IcTest,
    IcInsertDependentToValueCellDependencyLinkDoesNotInsertExistingDependent) {
  HandleScope scope(thread_);
  Function function0(&scope, testingFunction(thread_));
  Function function1(&scope, testingFunction(thread_));

  ValueCell cache(&scope, runtime_->newValueCell());
  EXPECT_TRUE(
      icInsertDependentToValueCellDependencyLink(thread_, function0, cache));
  EXPECT_TRUE(
      icInsertDependentToValueCellDependencyLink(thread_, function1, cache));
  EXPECT_FALSE(
      icInsertDependentToValueCellDependencyLink(thread_, function0, cache));

  WeakLink link(&scope, cache.dependencyLink());
  EXPECT_EQ(link.referent(), *function1);
  EXPECT_TRUE(link.prev().isNoneType());
  EXPECT_EQ(WeakLink::cast(link.next()).referent(), *function0);
  EXPECT_TRUE(WeakLink::cast(link.next()).next().isNoneType());
}

TEST_F(IcTest, IcUpdateGlobalVarFillsCacheLineAndReplaceOpcode) {
  HandleScope scope(thread_);
  Function function(&scope, testingFunction(thread_));
  MutableTuple caches(&scope, function.caches());
  MutableBytes rewritten_bytecode(&scope, function.rewrittenBytecode());

  ValueCell cache(&scope, runtime_->newValueCell());
  cache.setValue(SmallInt::fromWord(99));
  ValueCell another_cache(&scope, runtime_->newValueCell());
  another_cache.setValue(SmallInt::fromWord(123));

  icUpdateGlobalVar(thread_, function, 0, cache);

  EXPECT_EQ(caches.at(0), cache);
  EXPECT_EQ(rewrittenBytecodeOpAt(rewritten_bytecode, 0), LOAD_GLOBAL_CACHED);
  EXPECT_EQ(rewrittenBytecodeOpAt(rewritten_bytecode, 1), STORE_GLOBAL);

  icUpdateGlobalVar(thread_, function, 1, another_cache);

  EXPECT_EQ(caches.at(0), cache);
  EXPECT_EQ(rewrittenBytecodeOpAt(rewritten_bytecode, 0), LOAD_GLOBAL_CACHED);
  EXPECT_EQ(rewrittenBytecodeOpAt(rewritten_bytecode, 1), STORE_GLOBAL_CACHED);
}

TEST_F(IcTest, IcUpdateGlobalVarFillsCacheLineAndReplaceOpcodeWithExtendedArg) {
  HandleScope scope(thread_);
  Function function(&scope, testingFunction(thread_));
  MutableTuple caches(&scope, function.caches());

  MutableBytes rewritten_bytecode(
      &scope, runtime_->newMutableBytesUninitialized(4 * kCodeUnitSize));
  // TODO(T45440363): Replace the argument of EXTENDED_ARG for a non-zero value.
  rewrittenBytecodeOpAtPut(rewritten_bytecode, 0, EXTENDED_ARG);
  rewrittenBytecodeArgAtPut(rewritten_bytecode, 0, 0);
  rewrittenBytecodeOpAtPut(rewritten_bytecode, 1, LOAD_GLOBAL);
  rewrittenBytecodeArgAtPut(rewritten_bytecode, 1, 0);
  rewrittenBytecodeOpAtPut(rewritten_bytecode, 2, EXTENDED_ARG);
  rewrittenBytecodeArgAtPut(rewritten_bytecode, 2, 0);
  rewrittenBytecodeOpAtPut(rewritten_bytecode, 3, STORE_GLOBAL);
  rewrittenBytecodeArgAtPut(rewritten_bytecode, 3, 1);
  function.setRewrittenBytecode(*rewritten_bytecode);

  ValueCell cache(&scope, runtime_->newValueCell());
  cache.setValue(SmallInt::fromWord(99));
  ValueCell another_cache(&scope, runtime_->newValueCell());
  another_cache.setValue(SmallInt::fromWord(123));

  icUpdateGlobalVar(thread_, function, 0, cache);

  EXPECT_EQ(caches.at(0), cache);
  EXPECT_EQ(rewrittenBytecodeOpAt(rewritten_bytecode, 1), LOAD_GLOBAL_CACHED);
  EXPECT_EQ(rewrittenBytecodeOpAt(rewritten_bytecode, 3), STORE_GLOBAL);

  icUpdateGlobalVar(thread_, function, 1, another_cache);

  EXPECT_EQ(caches.at(0), cache);
  EXPECT_EQ(rewrittenBytecodeOpAt(rewritten_bytecode, 1), LOAD_GLOBAL_CACHED);
  EXPECT_EQ(rewrittenBytecodeOpAt(rewritten_bytecode, 3), STORE_GLOBAL_CACHED);
}

TEST_F(IcTest, IcUpdateGlobalVarCreatesDependencyLink) {
  HandleScope scope(thread_);
  Function function(&scope, testingFunction(thread_));
  ValueCell cache(&scope, runtime_->newValueCell());
  cache.setValue(SmallInt::fromWord(99));
  icUpdateGlobalVar(thread_, function, 0, cache);

  ASSERT_TRUE(cache.dependencyLink().isWeakLink());
  WeakLink link(&scope, cache.dependencyLink());
  EXPECT_EQ(link.referent(), *function);
  EXPECT_EQ(link.prev(), NoneType::object());
  EXPECT_EQ(link.next(), NoneType::object());
}

TEST_F(IcTest, IcUpdateGlobalVarInsertsHeadOfDependencyLink) {
  HandleScope scope(thread_);
  Function function0(&scope, testingFunction(thread_));
  Function function1(&scope, testingFunction(thread_));

  // Adds cache into function0's caches first, then to function1's.
  ValueCell cache(&scope, runtime_->newValueCell());
  cache.setValue(SmallInt::fromWord(99));
  icUpdateGlobalVar(thread_, function0, 0, cache);
  icUpdateGlobalVar(thread_, function1, 0, cache);

  ASSERT_TRUE(cache.dependencyLink().isWeakLink());
  WeakLink link(&scope, cache.dependencyLink());
  EXPECT_EQ(link.referent(), *function1);
  EXPECT_TRUE(link.prev().isNoneType());

  WeakLink next_link(&scope, link.next());
  EXPECT_EQ(next_link.referent(), *function0);
  EXPECT_EQ(next_link.prev(), *link);
  EXPECT_TRUE(next_link.next().isNoneType());
}

TEST_F(IcTest,
       IcInvalidateGlobalVarRemovesInvalidatedCacheFromReferencedFunctions) {
  HandleScope scope(thread_);
  Function function0(&scope, testingFunction(thread_));
  Function function1(&scope, testingFunction(thread_));
  MutableTuple caches0(&scope, function0.caches());
  MutableTuple caches1(&scope, function1.caches());

  // Both caches of Function0 & 1 caches the same cache value.
  ValueCell cache(&scope, runtime_->newValueCell());
  cache.setValue(SmallInt::fromWord(99));
  ValueCell another_cache(&scope, runtime_->newValueCell());
  another_cache.setValue(SmallInt::fromWord(123));

  icUpdateGlobalVar(thread_, function0, 0, cache);
  icUpdateGlobalVar(thread_, function0, 1, another_cache);
  icUpdateGlobalVar(thread_, function1, 0, another_cache);
  icUpdateGlobalVar(thread_, function1, 1, cache);

  EXPECT_TRUE(
      isIntEqualsWord(valueCellValue(icLookupGlobalVar(*caches0, 0)), 99));
  EXPECT_TRUE(
      isIntEqualsWord(valueCellValue(icLookupGlobalVar(*caches0, 1)), 123));
  EXPECT_TRUE(
      isIntEqualsWord(valueCellValue(icLookupGlobalVar(*caches1, 0)), 123));
  EXPECT_TRUE(
      isIntEqualsWord(valueCellValue(icLookupGlobalVar(*caches1, 1)), 99));

  // Invalidating cache makes it removed from both caches, and nobody depends on
  // it anymore.
  icInvalidateGlobalVar(thread_, cache);

  EXPECT_TRUE(icLookupGlobalVar(*caches0, 0).isNoneType());
  EXPECT_TRUE(
      isIntEqualsWord(valueCellValue(icLookupGlobalVar(*caches0, 1)), 123));
  EXPECT_TRUE(
      isIntEqualsWord(valueCellValue(icLookupGlobalVar(*caches1, 0)), 123));
  EXPECT_TRUE(icLookupGlobalVar(*caches1, 1).isNoneType());
  EXPECT_TRUE(cache.dependencyLink().isNoneType());
}

TEST_F(IcTest, IcInvalidateGlobalVarDoNotDeferenceDeallocatedReferent) {
  HandleScope scope(thread_);
  Function function0(&scope, testingFunction(thread_));
  Function function1(&scope, testingFunction(thread_));
  MutableTuple caches0(&scope, function0.caches());
  MutableTuple caches1(&scope, function1.caches());

  // Both caches of Function0 & 1 caches the same cache value.
  ValueCell cache(&scope, runtime_->newValueCell());
  cache.setValue(SmallInt::fromWord(99));
  ValueCell another_cache(&scope, runtime_->newValueCell());
  another_cache.setValue(SmallInt::fromWord(123));

  icUpdateGlobalVar(thread_, function0, 0, cache);
  icUpdateGlobalVar(thread_, function0, 1, another_cache);
  icUpdateGlobalVar(thread_, function1, 0, another_cache);
  icUpdateGlobalVar(thread_, function1, 1, cache);

  ASSERT_TRUE(
      isIntEqualsWord(valueCellValue(icLookupGlobalVar(*caches0, 0)), 99));
  ASSERT_TRUE(
      isIntEqualsWord(valueCellValue(icLookupGlobalVar(*caches0, 1)), 123));
  ASSERT_TRUE(
      isIntEqualsWord(valueCellValue(icLookupGlobalVar(*caches1, 0)), 123));
  ASSERT_TRUE(
      isIntEqualsWord(valueCellValue(icLookupGlobalVar(*caches1, 1)), 99));

  // Simulate GCing function1.
  WeakLink link(&scope, cache.dependencyLink());
  ASSERT_EQ(link.referent(), *function1);
  link.setReferent(NoneType::object());

  // Invalidation cannot touch function1 anymore.
  icInvalidateGlobalVar(thread_, cache);

  EXPECT_TRUE(icLookupGlobalVar(*caches0, 0).isNoneType());
  EXPECT_TRUE(
      isIntEqualsWord(valueCellValue(icLookupGlobalVar(*caches0, 1)), 123));
  EXPECT_TRUE(
      isIntEqualsWord(valueCellValue(icLookupGlobalVar(*caches1, 0)), 123));
  EXPECT_TRUE(
      isIntEqualsWord(valueCellValue(icLookupGlobalVar(*caches1, 1)), 99));
  EXPECT_TRUE(cache.dependencyLink().isNoneType());
}

TEST_F(IcTest, IcInvalidateGlobalVarRevertsOpCodeToOriginalOnes) {
  HandleScope scope(thread_);
  Function function(&scope, testingFunction(thread_));
  MutableBytes bytecode(&scope, function.rewrittenBytecode());
  ValueCell cache(&scope, runtime_->newValueCell());
  cache.setValue(SmallInt::fromWord(99));
  ValueCell another_cache(&scope, runtime_->newValueCell());
  another_cache.setValue(SmallInt::fromWord(123));

  byte original_expected[] = {LOAD_GLOBAL, 0, 0, 0, STORE_GLOBAL, 1, 0, 0,
                              LOAD_GLOBAL, 0, 0, 0, STORE_GLOBAL, 1, 0, 0};
  ASSERT_TRUE(isMutableBytesEqualsBytes(bytecode, original_expected));

  icUpdateGlobalVar(thread_, function, 0, cache);
  byte cached_expected0[] = {
      LOAD_GLOBAL_CACHED, 0, 0, 0, STORE_GLOBAL, 1, 0, 0,
      LOAD_GLOBAL_CACHED, 0, 0, 0, STORE_GLOBAL, 1, 0, 0};
  EXPECT_TRUE(isMutableBytesEqualsBytes(bytecode, cached_expected0));

  icUpdateGlobalVar(thread_, function, 1, another_cache);
  byte cached_expected1[] = {
      LOAD_GLOBAL_CACHED, 0, 0, 0, STORE_GLOBAL_CACHED, 1, 0, 0,
      LOAD_GLOBAL_CACHED, 0, 0, 0, STORE_GLOBAL_CACHED, 1, 0, 0};
  EXPECT_TRUE(isMutableBytesEqualsBytes(bytecode, cached_expected1));

  icInvalidateGlobalVar(thread_, cache);

  // Only invalidated cache's opcode gets reverted to the original one.
  byte invalidated_expected[] = {
      LOAD_GLOBAL, 0, 0, 0, STORE_GLOBAL_CACHED, 1, 0, 0,
      LOAD_GLOBAL, 0, 0, 0, STORE_GLOBAL_CACHED, 1, 0, 0,
  };
  EXPECT_TRUE(isMutableBytesEqualsBytes(bytecode, invalidated_expected));
}

TEST_F(IcTest, IcIteratorIteratesOverAttrCaches) {
  HandleScope scope(thread_);
  MutableBytes bytecode(
      &scope, runtime_->newMutableBytesUninitialized(10 * kCodeUnitSize));
  rewrittenBytecodeOpAtPut(bytecode, 0, LOAD_GLOBAL);
  rewrittenBytecodeArgAtPut(bytecode, 0, 100);
  rewrittenBytecodeCacheAtPut(bytecode, 0, 0);
  rewrittenBytecodeOpAtPut(bytecode, 1, LOAD_ATTR_ANAMORPHIC);
  rewrittenBytecodeArgAtPut(bytecode, 1, 0);
  rewrittenBytecodeCacheAtPut(bytecode, 1, 0);
  rewrittenBytecodeOpAtPut(bytecode, 2, LOAD_GLOBAL);
  rewrittenBytecodeArgAtPut(bytecode, 2, 100);
  rewrittenBytecodeCacheAtPut(bytecode, 2, 0);
  rewrittenBytecodeOpAtPut(bytecode, 3, LOAD_METHOD_ANAMORPHIC);
  rewrittenBytecodeArgAtPut(bytecode, 3, 1);
  rewrittenBytecodeCacheAtPut(bytecode, 3, 1);
  rewrittenBytecodeOpAtPut(bytecode, 4, LOAD_GLOBAL);
  rewrittenBytecodeArgAtPut(bytecode, 4, 100);
  rewrittenBytecodeCacheAtPut(bytecode, 4, 0);
  rewrittenBytecodeOpAtPut(bytecode, 5, LOAD_ATTR_ANAMORPHIC);
  rewrittenBytecodeArgAtPut(bytecode, 5, 2);
  rewrittenBytecodeCacheAtPut(bytecode, 5, 2);
  rewrittenBytecodeOpAtPut(bytecode, 6, STORE_ATTR_ANAMORPHIC);
  rewrittenBytecodeArgAtPut(bytecode, 6, 3);
  rewrittenBytecodeCacheAtPut(bytecode, 6, 3);
  rewrittenBytecodeOpAtPut(bytecode, 7, FOR_ITER_ANAMORPHIC);
  rewrittenBytecodeArgAtPut(bytecode, 7, -1);
  rewrittenBytecodeCacheAtPut(bytecode, 7, 4);
  rewrittenBytecodeOpAtPut(bytecode, 8, BINARY_SUBSCR_ANAMORPHIC);
  rewrittenBytecodeArgAtPut(bytecode, 8, -1);
  rewrittenBytecodeCacheAtPut(bytecode, 8, 5);
  rewrittenBytecodeOpAtPut(bytecode, 9, LOAD_GLOBAL);
  rewrittenBytecodeArgAtPut(bytecode, 9, 100);
  rewrittenBytecodeCacheAtPut(bytecode, 9, 0);

  word num_caches = 6;

  Object name1(&scope, Runtime::internStrFromCStr(
                           thread_, "load_attr_cached_attr_name"));
  Object name2(&scope, Runtime::internStrFromCStr(
                           thread_, "load_method_cached_attr_name"));
  Object name3(&scope, Runtime::internStrFromCStr(
                           thread_, "load_attr_cached_attr_name2"));
  Object name4(&scope, Runtime::internStrFromCStr(
                           thread_, "store_attr_cached_attr_name"));
  Tuple names(&scope, runtime_->newTupleWith4(name1, name2, name3, name4));

  Object name(&scope, runtime_->newStrFromCStr("name"));
  Function dependent(&scope, newEmptyFunction());
  Object value(&scope, NoneType::object());
  MutableTuple caches(
      &scope, runtime_->newMutableTuple(num_caches * kIcPointersPerEntry));
  caches.fill(NoneType::object());
  // Caches for LOAD_ATTR_ANAMORPHIC at PC 2.
  value = SmallInt::fromWord(10);
  icUpdateAttr(thread_, caches, 0, LayoutId::kBool, value, name, dependent);
  value = SmallInt::fromWord(20);
  icUpdateAttr(thread_, caches, 0, LayoutId::kSmallInt, value, name, dependent);

  // Caches for LOAD_METHOD_ANAMORPHIC at PC 6.
  value = SmallInt::fromWord(30);
  icUpdateAttr(thread_, caches, 1, LayoutId::kSmallInt, value, name, dependent);

  // Caches are empty for LOAD_ATTR_ANAMORPHIC at PC 10.

  // Caches for STORE_ATTR_ANAMORPHIC at PC 12.
  value = SmallInt::fromWord(40);
  icUpdateAttr(thread_, caches, 3, LayoutId::kNoneType, value, name, dependent);

  // Caches for FOR_ITER_ANAMORPHIC at PC 14.
  value = SmallInt::fromWord(50);
  icUpdateAttr(thread_, caches, 4, LayoutId::kStr, value, name, dependent);

  // Caches for BINARY_SUBSCR_ANAMORPHIC at PC 16.
  value = SmallInt::fromWord(60);
  icUpdateAttr(thread_, caches, 5, LayoutId::kTuple, value, name, dependent);

  Function function(&scope, newEmptyFunction());
  function.setRewrittenBytecode(*bytecode);
  function.setCaches(*caches);
  Code::cast(function.code()).setNames(*names);

  IcIterator it(&scope, runtime_, *function);
  ASSERT_TRUE(it.hasNext());
  ASSERT_TRUE(it.isAttrCache());
  EXPECT_FALSE(it.isBinaryOpCache());
  Object load_attr_cached_attr_name(
      &scope,
      Runtime::internStrFromCStr(thread_, "load_attr_cached_attr_name"));
  EXPECT_TRUE(it.isAttrNameEqualTo(load_attr_cached_attr_name));
  EXPECT_EQ(it.layoutId(), LayoutId::kBool);
  EXPECT_TRUE(it.isInstanceAttr());

  it.next();
  ASSERT_TRUE(it.hasNext());
  ASSERT_TRUE(it.isAttrCache());
  EXPECT_FALSE(it.isBinaryOpCache());
  EXPECT_TRUE(it.isAttrNameEqualTo(load_attr_cached_attr_name));
  EXPECT_EQ(it.layoutId(), LayoutId::kSmallInt);
  EXPECT_TRUE(it.isInstanceAttr());

  it.next();
  ASSERT_TRUE(it.hasNext());
  ASSERT_TRUE(it.isAttrCache());
  EXPECT_FALSE(it.isBinaryOpCache());
  Object load_method_cached_attr_name(
      &scope,
      Runtime::internStrFromCStr(thread_, "load_method_cached_attr_name"));
  EXPECT_TRUE(it.isAttrNameEqualTo(load_method_cached_attr_name));
  EXPECT_EQ(it.layoutId(), SmallInt::fromWord(100).layoutId());
  EXPECT_TRUE(it.isInstanceAttr());

  it.next();
  ASSERT_TRUE(it.hasNext());
  ASSERT_TRUE(it.isAttrCache());
  EXPECT_FALSE(it.isBinaryOpCache());
  Object store_attr_cached_attr_name(
      &scope,
      Runtime::internStrFromCStr(thread_, "store_attr_cached_attr_name"));
  EXPECT_TRUE(it.isAttrNameEqualTo(store_attr_cached_attr_name));
  EXPECT_EQ(it.layoutId(), NoneType::object().layoutId());
  EXPECT_TRUE(it.isInstanceAttr());

  ASSERT_EQ(
      caches.at(3 * kIcPointersPerEntry + kIcEntryKeyOffset),
      SmallInt::fromWord(static_cast<word>(NoneType::object().layoutId())));
  ASSERT_FALSE(
      caches.at(3 * kIcPointersPerEntry + kIcEntryValueOffset).isNoneType());

  it.evict();

  EXPECT_TRUE(
      caches.at(3 * kIcPointersPerEntry + kIcEntryKeyOffset).isNoneType());
  EXPECT_TRUE(
      caches.at(3 * kIcPointersPerEntry + kIcEntryValueOffset).isNoneType());

  it.next();
  ASSERT_TRUE(it.hasNext());
  ASSERT_TRUE(it.isAttrCache());
  EXPECT_FALSE(it.isBinaryOpCache());
  Object for_iter_cached_attr_name(
      &scope, Runtime::internStrFromCStr(thread_, "__next__"));
  EXPECT_TRUE(it.isAttrNameEqualTo(for_iter_cached_attr_name));
  EXPECT_EQ(it.layoutId(), LayoutId::kStr);
  EXPECT_TRUE(it.isInstanceAttr());

  it.next();
  ASSERT_TRUE(it.hasNext());
  ASSERT_TRUE(it.isAttrCache());
  EXPECT_FALSE(it.isBinaryOpCache());
  Object binary_subscr_cached_attr_name(
      &scope, Runtime::internStrFromCStr(thread_, "__getitem__"));
  EXPECT_TRUE(it.isAttrNameEqualTo(binary_subscr_cached_attr_name));
  EXPECT_EQ(it.layoutId(), LayoutId::kTuple);
  EXPECT_TRUE(it.isInstanceAttr());

  it.next();
  EXPECT_FALSE(it.hasNext());
}

TEST_F(IcTest, IcIteratorIteratesOverBinaryOpCaches) {
  HandleScope scope(thread_);
  MutableBytes bytecode(
      &scope, runtime_->newMutableBytesUninitialized(4 * kCodeUnitSize));
  rewrittenBytecodeOpAtPut(bytecode, 0, LOAD_GLOBAL);
  rewrittenBytecodeArgAtPut(bytecode, 0, 100);
  rewrittenBytecodeCacheAtPut(bytecode, 0, 0);
  rewrittenBytecodeOpAtPut(bytecode, 1, COMPARE_OP_ANAMORPHIC);
  rewrittenBytecodeArgAtPut(bytecode, 1, CompareOp::GE);
  rewrittenBytecodeCacheAtPut(bytecode, 1, 0);
  rewrittenBytecodeOpAtPut(bytecode, 2, BINARY_OP_ANAMORPHIC);
  rewrittenBytecodeArgAtPut(bytecode, 2,
                            static_cast<word>(Interpreter::BinaryOp::ADD));
  rewrittenBytecodeCacheAtPut(bytecode, 2, 1);
  rewrittenBytecodeOpAtPut(bytecode, 3, LOAD_GLOBAL);
  rewrittenBytecodeArgAtPut(bytecode, 3, 100);
  rewrittenBytecodeCacheAtPut(bytecode, 3, 0);

  word num_caches = 2;
  MutableTuple caches(
      &scope, runtime_->newMutableTuple(num_caches * kIcPointersPerEntry));

  // Caches for COMPARE_OP_ANAMORPHIC at 2.
  word compare_op_cached_index =
      0 * kIcPointersPerEntry + 0 * kIcPointersPerEntry;
  word compare_op_key_high_bits =
      static_cast<word>(SmallInt::fromWord(0).layoutId())
          << Header::kLayoutIdBits |
      static_cast<word>(SmallStr::fromCStr("test").layoutId());
  caches.atPut(compare_op_cached_index + kIcEntryKeyOffset,
               SmallInt::fromWord(compare_op_key_high_bits << kBitsPerByte |
                                  static_cast<word>(kBinaryOpReflected)));
  caches.atPut(compare_op_cached_index + kIcEntryValueOffset,
               SmallInt::fromWord(50));

  // Caches for BINARY_OP_ANAMORPHIC at 4.
  word binary_op_cached_index =
      1 * kIcPointersPerEntry + 0 * kIcPointersPerEntry;
  word binary_op_key_high_bits =
      static_cast<word>(SmallStr::fromCStr("").layoutId())
          << Header::kLayoutIdBits |
      static_cast<word>(SmallInt::fromWord(0).layoutId());
  caches.atPut(binary_op_cached_index + kIcEntryKeyOffset,
               SmallInt::fromWord(binary_op_key_high_bits << kBitsPerByte |
                                  static_cast<word>(kBinaryOpReflected)));
  caches.atPut(binary_op_cached_index + kIcEntryValueOffset,
               SmallInt::fromWord(60));

  Function function(&scope, newEmptyFunction());
  function.setRewrittenBytecode(*bytecode);
  function.setCaches(*caches);

  IcIterator it(&scope, runtime_, *function);
  ASSERT_TRUE(it.hasNext());
  ASSERT_TRUE(it.isBinaryOpCache());
  EXPECT_FALSE(it.isAttrCache());
  EXPECT_EQ(it.leftLayoutId(), SmallInt::fromWord(-1).layoutId());
  EXPECT_EQ(it.rightLayoutId(), SmallStr::fromCStr("").layoutId());
  {
    Object left_operator_name(&scope,
                              Runtime::internStrFromCStr(thread_, "__ge__"));
    EXPECT_EQ(left_operator_name, it.leftMethodName());
    Object right_operator_name(&scope,
                               Runtime::internStrFromCStr(thread_, "__le__"));
    EXPECT_EQ(right_operator_name, it.rightMethodName());
  }

  it.next();
  ASSERT_TRUE(it.hasNext());
  ASSERT_TRUE(it.isBinaryOpCache());
  EXPECT_FALSE(it.isAttrCache());
  EXPECT_EQ(it.leftLayoutId(), SmallStr::fromCStr("").layoutId());
  EXPECT_EQ(it.rightLayoutId(), SmallInt::fromWord(-1).layoutId());
  {
    Object left_operator_name(&scope,
                              Runtime::internStrFromCStr(thread_, "__add__"));
    EXPECT_EQ(left_operator_name, it.leftMethodName());
    Object right_operator_name(&scope,
                               Runtime::internStrFromCStr(thread_, "__radd__"));
    EXPECT_EQ(right_operator_name, it.rightMethodName());
  }

  it.next();
  EXPECT_FALSE(it.hasNext());
}

TEST_F(IcTest, IcIteratorIteratesOverInplaceOpCaches) {
  HandleScope scope(thread_);
  MutableBytes bytecode(
      &scope, runtime_->newMutableBytesUninitialized(4 * kCodeUnitSize));
  rewrittenBytecodeOpAtPut(bytecode, 0, LOAD_GLOBAL);
  rewrittenBytecodeArgAtPut(bytecode, 0, 100);
  rewrittenBytecodeCacheAtPut(bytecode, 0, 0);
  rewrittenBytecodeOpAtPut(bytecode, 1, INPLACE_OP_ANAMORPHIC);
  rewrittenBytecodeArgAtPut(bytecode, 1,
                            static_cast<word>(Interpreter::BinaryOp::MUL));
  rewrittenBytecodeCacheAtPut(bytecode, 1, 0);
  rewrittenBytecodeOpAtPut(bytecode, 2, LOAD_GLOBAL);
  rewrittenBytecodeArgAtPut(bytecode, 2, 100);
  rewrittenBytecodeCacheAtPut(bytecode, 2, 0);

  word num_caches = 1;
  MutableTuple caches(
      &scope, runtime_->newMutableTuple(num_caches * kIcPointersPerEntry));

  // Caches for BINARY_OP_ANAMORPHIC at 2.
  word inplace_op_cached_index =
      0 * kIcPointersPerEntry + 0 * kIcPointersPerEntry;
  word inplace_op_key_high_bits =
      static_cast<word>(SmallStr::fromCStr("a").layoutId())
          << Header::kLayoutIdBits |
      static_cast<word>(SmallInt::fromWord(3).layoutId());
  caches.atPut(inplace_op_cached_index + kIcEntryKeyOffset,
               SmallInt::fromWord(inplace_op_key_high_bits << kBitsPerByte |
                                  static_cast<word>(kBinaryOpReflected)));
  caches.atPut(inplace_op_cached_index + kIcEntryValueOffset,
               SmallInt::fromWord(70));

  Function function(&scope, newEmptyFunction());
  function.setRewrittenBytecode(*bytecode);
  function.setCaches(*caches);

  IcIterator it(&scope, runtime_, *function);
  ASSERT_TRUE(it.hasNext());
  ASSERT_TRUE(it.isInplaceOpCache());
  EXPECT_FALSE(it.isBinaryOpCache());
  EXPECT_FALSE(it.isAttrCache());
  EXPECT_EQ(it.leftLayoutId(), SmallStr::fromCStr("").layoutId());
  EXPECT_EQ(it.rightLayoutId(), SmallInt::fromWord(-1).layoutId());
  {
    Object inplace_operator_name(
        &scope, Runtime::internStrFromCStr(thread_, "__imul__"));
    EXPECT_EQ(inplace_operator_name, it.inplaceMethodName());
    Object left_operator_name(&scope,
                              Runtime::internStrFromCStr(thread_, "__mul__"));
    EXPECT_EQ(left_operator_name, it.leftMethodName());
    Object right_operator_name(&scope,
                               Runtime::internStrFromCStr(thread_, "__rmul__"));
    EXPECT_EQ(right_operator_name, it.rightMethodName());
  }

  it.next();
  EXPECT_FALSE(it.hasNext());
}

TEST_F(IcTest, IcRemoveDeadWeakLinksRemoveRemovesDeadHead) {
  HandleScope scope(thread_);
  ValueCell value_cell(&scope, runtime_->newValueCell());
  Object dependent1(&scope, newTupleWithNone(1));
  Object dependent2(&scope, newTupleWithNone(2));
  Object dependent3(&scope, newTupleWithNone(3));
  icInsertDependentToValueCellDependencyLink(thread_, dependent1, value_cell);
  icInsertDependentToValueCellDependencyLink(thread_, dependent2, value_cell);
  icInsertDependentToValueCellDependencyLink(thread_, dependent3, value_cell);
  // The dependenty link looks like dependent3 -> dependent2 -> dependent1.
  // Kill dependent3.
  WeakLink head(&scope, value_cell.dependencyLink());
  head.setReferent(NoneType::object());

  icRemoveDeadWeakLinks(*value_cell);

  ASSERT_TRUE(value_cell.dependencyLink().isWeakLink());
  WeakLink new_head(&scope, value_cell.dependencyLink());
  EXPECT_EQ(new_head.referent(), *dependent2);
  EXPECT_TRUE(new_head.prev().isNoneType());
  WeakLink new_next(&scope, new_head.next());
  EXPECT_EQ(new_next.referent(), *dependent1);
  EXPECT_EQ(new_next.prev(), *new_head);
  EXPECT_TRUE(new_next.next().isNoneType());
}

TEST_F(IcTest, IcRemoveDeadWeakLinksRemoveRemovesDeadMiddleNode) {
  HandleScope scope(thread_);
  ValueCell value_cell(&scope, runtime_->newValueCell());
  Object dependent1(&scope, newTupleWithNone(1));
  Object dependent2(&scope, newTupleWithNone(2));
  Object dependent3(&scope, newTupleWithNone(3));
  icInsertDependentToValueCellDependencyLink(thread_, dependent1, value_cell);
  icInsertDependentToValueCellDependencyLink(thread_, dependent2, value_cell);
  icInsertDependentToValueCellDependencyLink(thread_, dependent3, value_cell);
  // The dependenty link looks like dependent3 -> dependent2 -> dependent1.
  WeakLink head(&scope, value_cell.dependencyLink());
  // Kill dependent2.
  WeakLink next(&scope, head.next());
  next.setReferent(NoneType::object());

  icRemoveDeadWeakLinks(*value_cell);

  ASSERT_TRUE(value_cell.dependencyLink().isWeakLink());
  WeakLink new_head(&scope, value_cell.dependencyLink());
  EXPECT_EQ(new_head.referent(), *dependent3);
  EXPECT_TRUE(new_head.prev().isNoneType());
  WeakLink new_next(&scope, new_head.next());
  EXPECT_EQ(new_next.referent(), *dependent1);
  EXPECT_EQ(new_next.prev(), *new_head);
  EXPECT_TRUE(new_next.next().isNoneType());
}

TEST_F(IcTest, IcRemoveDeadWeakLinksRemoveRemovesDeadTailNode) {
  HandleScope scope(thread_);
  ValueCell value_cell(&scope, runtime_->newValueCell());
  Object dependent1(&scope, newTupleWithNone(1));
  Object dependent2(&scope, newTupleWithNone(2));
  Object dependent3(&scope, newTupleWithNone(3));
  icInsertDependentToValueCellDependencyLink(thread_, dependent1, value_cell);
  icInsertDependentToValueCellDependencyLink(thread_, dependent2, value_cell);
  icInsertDependentToValueCellDependencyLink(thread_, dependent3, value_cell);
  // The dependenty link looks like dependent3 -> dependent2 -> dependent1.
  WeakLink head(&scope, value_cell.dependencyLink());
  // Kill dependent1.
  WeakLink next_next(&scope, WeakLink::cast(head.next()).next());
  next_next.setReferent(NoneType::object());

  icRemoveDeadWeakLinks(*value_cell);

  ASSERT_TRUE(value_cell.dependencyLink().isWeakLink());
  WeakLink new_head(&scope, value_cell.dependencyLink());
  EXPECT_EQ(new_head.referent(), *dependent3);
  EXPECT_TRUE(new_head.prev().isNoneType());
  WeakLink new_next(&scope, new_head.next());
  EXPECT_EQ(new_next.referent(), *dependent2);
  EXPECT_EQ(new_next.prev(), *new_head);
  EXPECT_TRUE(new_next.next().isNoneType());
}

TEST_F(IcTest,
       EncodeBinaryOpKeyEntryReturnsKeyAccessedByLookupBinOpMonomorphic) {
  HandleScope scope(thread_);
  SmallInt entry_key(&scope, encodeBinaryOpKey(LayoutId::kStr, LayoutId::kInt,
                                               kBinaryOpReflected));
  Object entry_value(&scope, runtime_->newStrFromCStr("value"));
  MutableTuple caches(&scope, runtime_->newMutableTuple(kIcPointersPerEntry));
  caches.fill(NoneType::object());
  caches.atPut(kIcEntryKeyOffset, *entry_key);
  caches.atPut(kIcEntryValueOffset, *entry_value);

  BinaryOpFlags flags_out;
  Object result(&scope, icLookupBinOpMonomorphic(*caches, 0, LayoutId::kStr,
                                                 LayoutId::kInt, &flags_out));
  EXPECT_EQ(result, entry_value);
  EXPECT_TRUE(icLookupBinOpMonomorphic(*caches, 0, LayoutId::kInt,
                                       LayoutId::kStr, &flags_out)
                  .isErrorNotFound());
}

TEST_F(
    IcTest,
    IcInvalidateAttrWithDunderFunctionsUpdatesCorrespondingAttributeTypeFlags) {
  HandleScope scope(thread_);
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class A:
  pass

class B(A):
  pass

class C(B):
  pass

class X:
  pass

class D(X, C):
  pass

class E(D):
  pass

def custom_getattribute(self, name):
  return "bogus"

object_getattribute = object.__getattribute__
)")
                   .isError());

  Type a(&scope, mainModuleAt(runtime_, "A"));
  Type b(&scope, mainModuleAt(runtime_, "B"));
  Type c(&scope, mainModuleAt(runtime_, "C"));
  Type d(&scope, mainModuleAt(runtime_, "D"));
  Type e(&scope, mainModuleAt(runtime_, "E"));
  Type x(&scope, mainModuleAt(runtime_, "X"));

  ASSERT_TRUE(a.hasFlag(Type::Flag::kHasObjectDunderGetattribute));
  ASSERT_TRUE(b.hasFlag(Type::Flag::kHasObjectDunderGetattribute));
  ASSERT_TRUE(c.hasFlag(Type::Flag::kHasObjectDunderGetattribute));
  ASSERT_TRUE(d.hasFlag(Type::Flag::kHasObjectDunderGetattribute));
  ASSERT_TRUE(e.hasFlag(Type::Flag::kHasObjectDunderGetattribute));
  ASSERT_TRUE(x.hasFlag(Type::Flag::kHasObjectDunderGetattribute));

  Object custom_getattribute(&scope,
                             mainModuleAt(runtime_, "custom_getattribute"));
  typeAtPutById(thread_, c, ID(__getattribute__), custom_getattribute);

  EXPECT_TRUE(a.hasFlag(Type::Flag::kHasObjectDunderGetattribute));
  EXPECT_TRUE(b.hasFlag(Type::Flag::kHasObjectDunderGetattribute));
  EXPECT_FALSE(c.hasFlag(Type::Flag::kHasObjectDunderGetattribute));
  EXPECT_FALSE(d.hasFlag(Type::Flag::kHasObjectDunderGetattribute));
  EXPECT_FALSE(e.hasFlag(Type::Flag::kHasObjectDunderGetattribute));
  EXPECT_TRUE(x.hasFlag(Type::Flag::kHasObjectDunderGetattribute));

  Object object_getattribute(&scope,
                             mainModuleAt(runtime_, "object_getattribute"));
  typeAtPutById(thread_, c, ID(__getattribute__), object_getattribute);

  EXPECT_TRUE(a.hasFlag(Type::Flag::kHasObjectDunderGetattribute));
  EXPECT_TRUE(b.hasFlag(Type::Flag::kHasObjectDunderGetattribute));
  EXPECT_TRUE(c.hasFlag(Type::Flag::kHasObjectDunderGetattribute));
  EXPECT_TRUE(d.hasFlag(Type::Flag::kHasObjectDunderGetattribute));
  EXPECT_TRUE(e.hasFlag(Type::Flag::kHasObjectDunderGetattribute));
  EXPECT_TRUE(x.hasFlag(Type::Flag::kHasObjectDunderGetattribute));
}

TEST_F(IcTest, IcInvalidateTypeHierarchy) {
  ASSERT_FALSE(runFromCStr(runtime_, R"(
class A:
  def __init__(self):
    self.foo = 400

class B(A):
  pass

def cache_attribute(c):
  return c.foo

def invalidate():
  A.foo = property(lambda self: 123)

a = A()
b = B()
a_init = A.__init__
)")
                   .isError());
  HandleScope scope(thread_);
  Function a_init(&scope, mainModuleAt(runtime_, "a_init"));
  Function cache_attribute(&scope, mainModuleAt(runtime_, "cache_attribute"));
  Type type_a(&scope, mainModuleAt(runtime_, "A"));
  Type type_b(&scope, mainModuleAt(runtime_, "B"));
  Object obj_a(&scope, mainModuleAt(runtime_, "a"));
  Object obj_b(&scope, mainModuleAt(runtime_, "b"));
  Object foo_name(&scope, Runtime::internStrFromCStr(thread_, "foo"));
  ValueCell foo_in_a(&scope, typeValueCellAt(*type_a, *foo_name));
  ValueCell foo_in_b(&scope, typeValueCellAt(*type_b, *foo_name));

  // We've called __init__ already so it should be a dependent.
  EXPECT_TRUE(icDependentIncluded(*a_init, foo_in_a.dependencyLink()));
  EXPECT_TRUE(icDependentIncluded(*a_init, foo_in_b.dependencyLink()));
  EXPECT_FALSE(icDependentIncluded(*cache_attribute, foo_in_a.dependencyLink()));
  EXPECT_FALSE(icDependentIncluded(*cache_attribute, foo_in_b.dependencyLink()));

  // Cache an attribute elsewhere.
  ASSERT_TRUE(isIntEqualsWord(Interpreter::call1(thread_, cache_attribute, obj_a), 400));
  ASSERT_TRUE(isIntEqualsWord(Interpreter::call1(thread_, cache_attribute, obj_b), 400));

  // That function should be a dependent too.
  EXPECT_TRUE(icDependentIncluded(*a_init, foo_in_a.dependencyLink()));
  EXPECT_TRUE(icDependentIncluded(*a_init, foo_in_b.dependencyLink()));
  EXPECT_TRUE(icDependentIncluded(*cache_attribute, foo_in_a.dependencyLink()));
  EXPECT_TRUE(icDependentIncluded(*cache_attribute, foo_in_b.dependencyLink()));

  // Invalidate the attribute.
  Function invalidate(&scope, mainModuleAt(runtime_, "invalidate"));
  ASSERT_TRUE(Interpreter::call0(thread_, invalidate).isNoneType());

  EXPECT_FALSE(icDependentIncluded(*a_init, foo_in_a.dependencyLink()));
  EXPECT_FALSE(icDependentIncluded(*a_init, foo_in_b.dependencyLink()));
  // TODO(#269): Change to EXPECT_FALSE.
  EXPECT_TRUE(icDependentIncluded(*cache_attribute, foo_in_a.dependencyLink()));
  EXPECT_TRUE(icDependentIncluded(*cache_attribute, foo_in_b.dependencyLink()));
}

}  // namespace testing
}  // namespace py
