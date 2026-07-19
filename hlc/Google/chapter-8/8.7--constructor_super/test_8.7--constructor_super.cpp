/*
 Copyright 2020 Apotell

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

// Tests for 8.7--constructor_super.sv (tags: 8.7 8.17)
//   module class_tb ();
//     class super_cls;
//       int s = 2;
//       function new(int def = 3);
//         s = def;
//       endfunction
//     endclass
//     class test_cls extends super_cls;
//       int a;
//       function new(int def = 42);
//         super.new(def + 3);
//         a = def;
//       endfunction
//     endclass
//
//     test_cls test_obj;
//
//     initial begin
//       test_obj = new(37);
//
//       $display(test_obj.a);
//       $display(test_obj.s);
//     end
//   endmodule
//
// IEEE 1800-2017 8.7 "Constructors" and 8.17 "This and super": a derived
// class's constructor may chain to its base class's constructor via
// "super.new(...)"; once constructed, a handle of the derived type can
// access properties inherited from the base ("test_obj.s", declared on
// super_cls, not test_cls) the same way it accesses its own ("test_obj.a").
//
// Checked:
//   - design has module work@class_tb with exactly 1 net: "test_obj"
//   - the module has exactly 2 nested ClassDefns: "work@super_cls" and
//     "work@test_cls"
//   - ClassDefn "super_cls": classType vpiUserDefinedClass, has exactly 1
//     property ("s") WITH an initializer -- getValue() is a Constant "2"
//     (unlike every other chapter-8 property so far, which had no
//     initializer) -- and exactly 1 constructor ("new(int def = 3)"),
//     public by default, returning super_cls's own type, body "s = def;"
//   - ClassDefn "test_cls": classType vpiUserDefinedClass, getExtends()
//     resolves (via its ClassTypespecs) to the SAME ClassDefn as
//     "super_cls" -- confirms "extends super_cls" is correctly recognised
//   - "test_cls" has exactly 1 property ("a", no initializer) and exactly
//     1 constructor ("new(int def = 42)"), public by default, returning
//     test_cls's OWN type (not super_cls's) -- confirming a derived
//     class's constructor still returns its own type, not the base's
//   - the derived constructor's body is a 2-statement Begin:
//     "super.new(def + 3);" and "a = def;"
//   - "super.new(def + 3)" is a HierPath with 2 path elems: RefObj "super"
//     resolving DIRECTLY to the SAME ClassDefn as "super_cls" (not to some
//     intermediate wrapper object), and a FuncCall (NOT a MethodFuncCall)
//     named "new" taking 1 argument (an add Operation, "def + 3") -- and
//     CRUCIALLY, this FuncCall's getTaskFunc() DOES resolve back to the
//     SAME Function as super_cls's constructor -- see the KNOWN COMPILER
//     BUG note below for why this contrasts with ordinary "new" calls
//   - "a = def;" is a blocking Assignment: lhs RefObj "a" resolved to
//     test_cls's own property; rhs RefObj "def" resolved to the derived
//     constructor's own IODecl
//   - net "test_obj": its typespec resolves to the SAME ClassDefn as
//     "test_cls"
//   - the initial process' Begin block has exactly 3 statements:
//     "test_obj = new(37)", "$display(test_obj.a)", "$display(test_obj.s)"
//   - "test_obj = new(37)": blocking Assignment, lhs RefObj "test_obj"
//     resolved to the Net, rhs MethodFuncCall "new" taking 1 Constant
//     argument "37" -- see the KNOWN COMPILER BUG note below for whether
//     THIS call resolves to the constructor (expected to fail, unlike the
//     "super.new" call above)
//   - "$display(test_obj.a)": HierPath, "a" resolves to test_cls's own
//     property
//   - "$display(test_obj.s)": HierPath, "s" resolves to the SAME Variable
//     found on super_cls -- confirms a derived-class handle correctly
//     reaches an INHERITED property, not just its own class's members
//   - design-level: exactly 2 classes
//
// KNOWN COMPILER BUG #1 (class lifetime defaulting, not a defect in this
// file): IEEE 1800-2017 8.3 says a class declared with no lifetime
// qualifier must default to automatic lifetime (getAutomatic() == true).
// Already confirmed independently via multiple other chapter-8 files.
// SuperClsIsAutomaticByDefault and TestClsIsAutomaticByDefault below both
// assert the IEEE-mandated behavior and will FAIL until this is fixed.
//
// KNOWN COMPILER BUG #2 (property visibility defaulting, not a defect in
// this file): IEEE 1800-2017 8.14 says a property with no explicit
// "local"/"protected" qualifier defaults to public visibility. Already
// confirmed independently via
// hlc/Google/chapter-8/8.5--properties/test_8.5--properties.cpp and
// siblings. SuperClsPropertySIsPublicByDefault and
// TestClsPropertyAIsPublicByDefault below both assert the IEEE-mandated
// behavior and will FAIL until this is fixed.
//
// KNOWN COMPILER BUG #3 (constructor call resolution, refined finding):
// confirmed via ctest in
// hlc/Google/chapter-8/8.7--constructor/test_8.7--constructor.cpp that a
// top-level "new" call's getTaskFunc() never resolves back to the actual
// constructor. This file adds an important refinement: "super.new(...)"
// (modeled as a FuncCall, not a MethodFuncCall) DOES correctly resolve
// getTaskFunc() to super_cls's constructor (see the .log's
// "vpiTaskFunc: Function name:new" under the FuncCall) -- so the gap is
// NOT that "new" resolution is broken everywhere; it appears specific to
// the MethodFuncCall shape used for ordinary object-construction "new"
// calls ("test_obj = new(37)"), not the FuncCall shape used for explicit
// super-constructor chaining. FirstStmtNewCallResolvesToConstructor below
// is expected to FAIL (same as the sibling files) while
// TestConstructorFirstStmtIsSuperNewCall's getTaskFunc() check is expected
// to PASS.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/extends.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ClassConstructorSuperTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "8.7--constructor_super.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("work@class_tb", m_design->getAllModules());
  }

  static const hldb::ClassDefn *getSuperClsDefn() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::ClassDefn>("work@super_cls", top->getClassDefns());
  }

  static const hldb::ClassDefn *getTestClsDefn() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::ClassDefn>("work@test_cls", top->getClassDefns());
  }

  static const hldb::Variable *getSuperPropertyS() {
    const hldb::ClassDefn *const c = getSuperClsDefn();
    if (c == nullptr || c->getVariables() == nullptr || c->getVariables()->empty()) return nullptr;
    return c->getVariables()->at(0);
  }

  static const hldb::Function *getSuperConstructor() {
    const hldb::ClassDefn *const c = getSuperClsDefn();
    if (c == nullptr || c->getMethods() == nullptr || c->getMethods()->empty()) return nullptr;
    return any_cast<hldb::Function>(c->getMethods()->at(0));
  }

  static const hldb::Variable *getTestPropertyA() {
    const hldb::ClassDefn *const c = getTestClsDefn();
    if (c == nullptr || c->getVariables() == nullptr || c->getVariables()->empty()) return nullptr;
    return c->getVariables()->at(0);
  }

  static const hldb::Function *getTestConstructor() {
    const hldb::ClassDefn *const c = getTestClsDefn();
    if (c == nullptr || c->getMethods() == nullptr || c->getMethods()->empty()) return nullptr;
    return any_cast<hldb::Function>(c->getMethods()->at(0));
  }

  static const hldb::Net *getNetTestObj() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Net>("test_obj", top->getNets());
  }

  static const hldb::Begin *getInitialBegin() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (init == nullptr) return nullptr;
    return init->getStmt<hldb::Begin>();
  }
};

// --- module / design shape ---------------------------------------------------

TEST_F(ClassConstructorSuperTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ClassConstructorSuperTest, ModuleHasOneNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(ClassConstructorSuperTest, ModuleHasTwoClassDefns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClassDefns(), nullptr);
  EXPECT_EQ(top->getClassDefns()->size(), 2u);
}

// --- class "super_cls" ---------------------------------------------------------

TEST_F(ClassConstructorSuperTest, SuperClsExists) { EXPECT_NE(getSuperClsDefn(), nullptr); }

TEST_F(ClassConstructorSuperTest, SuperClsIsUserDefinedClass) {
  const hldb::ClassDefn *const c = getSuperClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getClassType(), vpiUserDefinedClass);
}

TEST_F(ClassConstructorSuperTest, SuperClsIsAutomaticByDefault) {
  const hldb::ClassDefn *const c = getSuperClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->getAutomatic()) << "8.3: 'class super_cls' has no lifetime qualifier so it defaults to "
                                    "automatic (see KNOWN COMPILER BUG #1 above)";
}

TEST_F(ClassConstructorSuperTest, SuperClsHasOnePropertySWithInitializerTwo) {
  const hldb::ClassDefn *const c = getSuperClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getVariables(), nullptr);
  ASSERT_EQ(c->getVariables()->size(), 1u);
  const hldb::Variable *const s = getSuperPropertyS();
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->getName(), "s");
  ASSERT_NE(s->getTypespec(), nullptr);
  const hldb::IntTypespec *const elem = s->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "property 's' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned());

  const hldb::Constant *const init = s->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "'int s = 2;' should attach '2' as the property's own initializer value";
  EXPECT_EQ(init->getDecompile(), "2");
}

TEST_F(ClassConstructorSuperTest, SuperClsPropertySIsPublicByDefault) {
  const hldb::Variable *const s = getSuperPropertyS();
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->getVisibility(), vpiPublicVis) << "8.14: 'int s = 2;' with no visibility qualifier defaults to "
                                                 "public (see KNOWN COMPILER BUG #2 above)";
}

TEST_F(ClassConstructorSuperTest, SuperClsHasOneConstructor) {
  const hldb::ClassDefn *const c = getSuperClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getMethods(), nullptr);
  ASSERT_EQ(c->getMethods()->size(), 1u);
  const hldb::Function *const ctor = getSuperConstructor();
  ASSERT_NE(ctor, nullptr) << "'function new(int def = 3)' should resolve to a Function";
  EXPECT_EQ(ctor->getName(), "new");
}

TEST_F(ClassConstructorSuperTest, SuperConstructorReturnsSuperClsType) {
  const hldb::Function *const ctor = getSuperConstructor();
  ASSERT_NE(ctor, nullptr);
  ASSERT_NE(ctor->getReturn(), nullptr);
  const hldb::ClassTypespec *const ct = ctor->getReturn<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr);
  EXPECT_EQ(ct->getClassDefn(), getSuperClsDefn());
}

TEST_F(ClassConstructorSuperTest, SuperConstructorHasOneIODeclDefWithDefaultValueThree) {
  const hldb::Function *const ctor = getSuperConstructor();
  ASSERT_NE(ctor, nullptr);
  ASSERT_NE(ctor->getIODecls(), nullptr);
  ASSERT_EQ(ctor->getIODecls()->size(), 1u);
  const hldb::IODecl *const def = ctor->getIODecls()->at(0);
  ASSERT_NE(def, nullptr);
  EXPECT_EQ(def->getName(), "def");
  EXPECT_EQ(def->getDirection(), vpiInput);
  const hldb::Constant *const defaultValue = def->getExpr<hldb::Constant>();
  ASSERT_NE(defaultValue, nullptr);
  EXPECT_EQ(defaultValue->getDecompile(), "3");
}

TEST_F(ClassConstructorSuperTest, SuperConstructorBodyAssignsPropertySFromDef) {
  const hldb::Function *const ctor = getSuperConstructor();
  ASSERT_NE(ctor, nullptr);
  const hldb::Assignment *const assign = ctor->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr) << "'s = def;' should be a single Assignment, not wrapped in a Begin";
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "s");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getSuperPropertyS());
  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "def");
  ASSERT_NE(ctor->getIODecls(), nullptr);
  ASSERT_GT(ctor->getIODecls()->size(), 0u);
  EXPECT_EQ(rhs->getActual<hldb::IODecl>(), ctor->getIODecls()->at(0));
}

// --- class "test_cls extends super_cls" -----------------------------------------

TEST_F(ClassConstructorSuperTest, TestClsExists) { EXPECT_NE(getTestClsDefn(), nullptr); }

TEST_F(ClassConstructorSuperTest, TestClsIsUserDefinedClass) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getClassType(), vpiUserDefinedClass);
}

TEST_F(ClassConstructorSuperTest, TestClsIsAutomaticByDefault) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->getAutomatic()) << "8.3: 'class test_cls extends super_cls' has no lifetime qualifier so it "
                                    "defaults to automatic (see KNOWN COMPILER BUG #1 above)";
}

TEST_F(ClassConstructorSuperTest, TestClsExtendsSuperCls) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  const hldb::Extends *const ext = c->getExtends();
  ASSERT_NE(ext, nullptr) << "'extends super_cls' should attach an Extends node";
  ASSERT_NE(ext->getClassTypespecs(), nullptr);
  ASSERT_EQ(ext->getClassTypespecs()->size(), 1u);
  const hldb::RefTypespec *const baseRef = ext->getClassTypespecs()->at(0);
  ASSERT_NE(baseRef, nullptr);
  const hldb::ClassTypespec *const baseCt = baseRef->getActual<hldb::ClassTypespec>();
  ASSERT_NE(baseCt, nullptr);
  EXPECT_EQ(baseCt->getClassDefn(), getSuperClsDefn())
      << "'extends super_cls' must resolve back to the SAME ClassDefn as 'super_cls'";
}

TEST_F(ClassConstructorSuperTest, TestClsHasOnePropertyA) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getVariables(), nullptr);
  ASSERT_EQ(c->getVariables()->size(), 1u);
  const hldb::Variable *const a = getTestPropertyA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getName(), "a");
  ASSERT_NE(a->getTypespec(), nullptr);
  const hldb::IntTypespec *const elem = a->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "property 'a' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned());
  EXPECT_EQ(a->getValue(), nullptr) << "'int a;' has no initializer, unlike 'int s = 2;' on super_cls";
}

TEST_F(ClassConstructorSuperTest, TestClsPropertyAIsPublicByDefault) {
  const hldb::Variable *const a = getTestPropertyA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getVisibility(), vpiPublicVis) << "8.14: 'int a;' with no visibility qualifier defaults to public "
                                                 "(see KNOWN COMPILER BUG #2 above)";
}

TEST_F(ClassConstructorSuperTest, TestClsHasOneConstructor) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getMethods(), nullptr);
  ASSERT_EQ(c->getMethods()->size(), 1u);
  const hldb::Function *const ctor = getTestConstructor();
  ASSERT_NE(ctor, nullptr) << "'function new(int def = 42)' should resolve to a Function";
  EXPECT_EQ(ctor->getName(), "new");
}

TEST_F(ClassConstructorSuperTest, TestConstructorReturnsTestClsTypeNotSuperCls) {
  const hldb::Function *const ctor = getTestConstructor();
  ASSERT_NE(ctor, nullptr);
  ASSERT_NE(ctor->getReturn(), nullptr);
  const hldb::ClassTypespec *const ct = ctor->getReturn<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr);
  EXPECT_EQ(ct->getClassDefn(), getTestClsDefn())
      << "8.7: a derived class's constructor returns its OWN type, not the base's";
}

TEST_F(ClassConstructorSuperTest, TestConstructorHasOneIODeclDefWithDefaultValue42) {
  const hldb::Function *const ctor = getTestConstructor();
  ASSERT_NE(ctor, nullptr);
  ASSERT_NE(ctor->getIODecls(), nullptr);
  ASSERT_EQ(ctor->getIODecls()->size(), 1u);
  const hldb::IODecl *const def = ctor->getIODecls()->at(0);
  ASSERT_NE(def, nullptr);
  EXPECT_EQ(def->getName(), "def");
  const hldb::Constant *const defaultValue = def->getExpr<hldb::Constant>();
  ASSERT_NE(defaultValue, nullptr);
  EXPECT_EQ(defaultValue->getDecompile(), "42");
}

TEST_F(ClassConstructorSuperTest, TestConstructorBodyHasTwoStmts) {
  const hldb::Function *const ctor = getTestConstructor();
  ASSERT_NE(ctor, nullptr);
  const hldb::Begin *const body = ctor->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "a 2-statement constructor body should be wrapped in a Begin";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 2u);
}

TEST_F(ClassConstructorSuperTest, TestConstructorFirstStmtIsSuperNewCall) {
  const hldb::Function *const ctor = getTestConstructor();
  ASSERT_NE(ctor, nullptr);
  const hldb::Begin *const body = ctor->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_GT(body->getStmts()->size(), 0u);
  const hldb::HierPath *const path = any_cast<hldb::HierPath>(body->getStmts()->at(0));
  ASSERT_NE(path, nullptr) << "'super.new(def + 3)' should be a bare HierPath statement";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);

  const hldb::RefObj *const superRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(superRef, nullptr) << "'super' should be a RefObj";
  EXPECT_EQ(superRef->getName(), "super");
  EXPECT_EQ(superRef->getActual<hldb::ClassDefn>(), getSuperClsDefn())
      << "'super' must resolve directly to the SAME ClassDefn as 'super_cls'";

  const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(path->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "'super.new(...)' should resolve to a FuncCall, not a MethodFuncCall";
  EXPECT_EQ(call->getName(), "new");
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), getSuperConstructor())
      << "unlike ordinary 'new(...)' calls (see KNOWN COMPILER BUG #3 above), 'super.new(...)' IS expected to "
         "resolve back to the base class's constructor";

  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::Operation *const arg = any_cast<hldb::Operation>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr) << "'def + 3' should be an add Operation";
  EXPECT_EQ(arg->getOpType(), vpiAddOp);
  ASSERT_NE(arg->getOperands(), nullptr);
  ASSERT_EQ(arg->getOperands()->size(), 2u);

  const hldb::RefObj *const defOperand = any_cast<hldb::RefObj>(arg->getOperands()->at(0));
  ASSERT_NE(defOperand, nullptr);
  EXPECT_EQ(defOperand->getName(), "def");
  ASSERT_NE(ctor->getIODecls(), nullptr);
  ASSERT_GT(ctor->getIODecls()->size(), 0u);
  EXPECT_EQ(defOperand->getActual<hldb::IODecl>(), ctor->getIODecls()->at(0))
      << "'def' here must resolve to test_cls's OWN constructor parameter, not super_cls's";

  const hldb::Constant *const threeOperand = any_cast<hldb::Constant>(arg->getOperands()->at(1));
  ASSERT_NE(threeOperand, nullptr);
  EXPECT_EQ(threeOperand->getDecompile(), "3");
}

TEST_F(ClassConstructorSuperTest, TestConstructorSecondStmtAssignsPropertyAFromDef) {
  const hldb::Function *const ctor = getTestConstructor();
  ASSERT_NE(ctor, nullptr);
  const hldb::Begin *const body = ctor->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_GT(body->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(1));
  ASSERT_NE(assign, nullptr) << "'a = def;' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "a");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getTestPropertyA());

  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "def");
  ASSERT_NE(ctor->getIODecls(), nullptr);
  ASSERT_GT(ctor->getIODecls()->size(), 0u);
  EXPECT_EQ(rhs->getActual<hldb::IODecl>(), ctor->getIODecls()->at(0));
}

// --- net "test_obj" --------------------------------------------------------------

TEST_F(ClassConstructorSuperTest, NetTestObjExists) { EXPECT_NE(getNetTestObj(), nullptr); }

TEST_F(ClassConstructorSuperTest, NetTestObjTypespecResolvesToTestClsClassDefn) {
  const hldb::Net *const testObj = getNetTestObj();
  ASSERT_NE(testObj, nullptr);
  ASSERT_NE(testObj->getTypespec(), nullptr);
  const hldb::ClassTypespec *const ct = testObj->getTypespec<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr);
  EXPECT_EQ(ct->getClassDefn(), getTestClsDefn());
}

// --- initial process structure -------------------------------------------------

TEST_F(ClassConstructorSuperTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(ClassConstructorSuperTest, InitialBeginHasThreeStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 3u);
}

// --- test_obj = new(37) (stmt[0]) -------------------------------------------------

TEST_F(ClassConstructorSuperTest, FirstStmtIsBlockingAssignment) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 0u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "'test_obj = new(37)' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());
}

TEST_F(ClassConstructorSuperTest, FirstStmtLhsIsTestObjHandle) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 0u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "test_obj");
  EXPECT_EQ(lhs->getActual<hldb::Net>(), getNetTestObj());
}

TEST_F(ClassConstructorSuperTest, FirstStmtRhsIsNewCallWithArg37) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 0u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::MethodFuncCall *const newCall = assign->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(newCall, nullptr) << "'new(37)' should resolve to a MethodFuncCall";
  EXPECT_EQ(newCall->getName(), "new");
  ASSERT_NE(newCall->getArguments(), nullptr);
  ASSERT_EQ(newCall->getArguments()->size(), 1u);
  const hldb::Constant *const arg = any_cast<hldb::Constant>(newCall->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getDecompile(), "37");
}

TEST_F(ClassConstructorSuperTest, FirstStmtNewCallResolvesToConstructor) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 0u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::MethodFuncCall *const newCall = assign->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(newCall, nullptr);
  EXPECT_EQ(newCall->getTaskFunc<hldb::Function>(), getTestConstructor())
      << "8.7: 'new(37)' must resolve back to the SAME user-written constructor declared on test_cls (see "
         "KNOWN COMPILER BUG #3 above -- contrast with the 'super.new' call, which DOES resolve)";
}

// --- $display(test_obj.a) (stmt[1]) -----------------------------------------------

TEST_F(ClassConstructorSuperTest, SecondStmtDisplaysTestObjA) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 1u);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(1));
  ASSERT_NE(disp, nullptr) << "stmt[1] should be a $display SysFuncCall";
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);
  const hldb::HierPath *const path = any_cast<hldb::HierPath>(disp->getArguments()->at(0));
  ASSERT_NE(path, nullptr) << "'test_obj.a' should be a HierPath";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);
  const hldb::RefObj *const testObjRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(testObjRef, nullptr);
  EXPECT_EQ(testObjRef->getActual<hldb::Net>(), getNetTestObj());
  const hldb::RefObj *const aRef = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
  ASSERT_NE(aRef, nullptr);
  EXPECT_EQ(aRef->getName(), "a");
  EXPECT_EQ(aRef->getActual<hldb::Variable>(), getTestPropertyA());
}

// --- $display(test_obj.s) (stmt[2]) -- inherited property access -----------------

TEST_F(ClassConstructorSuperTest, ThirdStmtDisplaysInheritedTestObjS) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 2u);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(2));
  ASSERT_NE(disp, nullptr) << "stmt[2] should be a $display SysFuncCall";
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);
  const hldb::HierPath *const path = any_cast<hldb::HierPath>(disp->getArguments()->at(0));
  ASSERT_NE(path, nullptr) << "'test_obj.s' should be a HierPath";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);
  const hldb::RefObj *const testObjRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(testObjRef, nullptr);
  EXPECT_EQ(testObjRef->getActual<hldb::Net>(), getNetTestObj());
  const hldb::RefObj *const sRef = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
  ASSERT_NE(sRef, nullptr);
  EXPECT_EQ(sRef->getName(), "s");
  EXPECT_EQ(sRef->getActual<hldb::Variable>(), getSuperPropertyS())
      << "8.17: 'test_obj.s', accessed through a test_cls handle, must resolve to the SAME Variable 's' "
         "inherited from super_cls -- not a dangling/unresolved reference";
}

// --- compiler diagnostics ---------------------------------------------------------

TEST_F(ClassConstructorSuperTest, CompilerReportsNoErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
