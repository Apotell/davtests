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

// Tests for 8.13--inheritance.sv (tags: 8.13)
//   module class_tb ();
//     class super_cls;
//       int s = 2;
//       function int incs();
//         ++s;
//         incs = s;
//       endfunction
//       function new(int def = 3);
//         s = def;
//       endfunction
//     endclass
//
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
//       $display(test_obj.incs());
//       $display(test_obj.s);
//     end
//   endmodule
//
// IEEE 1800-2017 8.13 "Inheritance": "test_cls extends super_cls" makes
// test_cls a subclass of super_cls -- test_cls inherits super_cls's
// properties and methods, and a test_cls-typed handle can access them
// directly (no qualification needed). This file exercises exactly that:
// "test_obj" is typed "test_cls", but "test_obj.incs()" calls a method
// declared ONLY in super_cls, and "test_obj.s" accesses a property
// declared ONLY in super_cls.
//
// Checked:
//   - design has module work@class_tb with exactly 1 net: "test_obj"
//   - the module has exactly 2 nested ClassDefns: "work@super_cls" and
//     "work@test_cls"
//   - ClassDefn "super_cls": classType vpiUserDefinedClass, has exactly 1
//     property ("s", signed IntTypespec) with initializer Constant "2",
//     and exactly 2 methods ("incs", "new") -- see the KNOWN COMPILER BUG
//     notes below for the property's visibility and the methods' "method"
//     flag
//   - "incs": a Function (not a constructor -- its return type resolves to
//     a plain IntTypespec, contrast with "new" below), public visibility,
//     2-statement body: "++s;" (Operation, vpiPreIncOp, operand "s" ->
//     property Variable) and "incs = s;" (Assignment whose lhs resolves,
//     via getActual<Function>(), to the "incs" Function itself -- the
//     same "assign to own name" idiom already confirmed in
//     chapter-8/8.10--static_methods/test_8.10--static_methods.cpp)
//   - "new" (super_cls's own constructor): its return type resolves to a
//     ClassTypespec matching super_cls itself (confirming it IS a
//     constructor, unlike "incs"); has 1 IODecl ("def", direction input,
//     IntTypespec) whose default-value expr is Constant "3"; its body is
//     a single bare Assignment (NOT wrapped in a Begin, since it is only
//     one statement) "s = def;"
//   - ClassDefn "test_cls": classType vpiUserDefinedClass, EXTENDS
//     super_cls -- getExtends() returns a non-null Extends object whose
//     single ClassTypespec resolves (via its RefTypespec) to super_cls's
//     own ClassDefn. Has exactly 1 property ("a", signed IntTypespec, no
//     initializer) and exactly 1 method ("new", its own constructor)
//   - test_cls's "new": return type resolves to a ClassTypespec matching
//     test_cls itself; 1 IODecl ("def", input, IntTypespec, default expr
//     Constant "42"); 2-statement Begin body:
//     "super.new(def + 3);" -- a bare HierPath statement (not wrapped in
//     any Assignment/ExprStmt), first path elem a RefObj "super"
//     resolving (via getActual<ClassDefn>()) to super_cls's ClassDefn --
//     the same "resolves to the enclosing/referenced ClassDefn" shape
//     already seen for "this" in
//     chapter-8/8.11--this/test_8.11--this.cpp -- second path elem a
//     FuncCall "new" whose getTaskFunc() resolves to super_cls's OWN "new"
//     Function (CONFIRMED correct, per the established KNOWN COMPILER BUG
//     #6 distinction: "super.new(...)", modeled as FuncCall, resolves
//     getTaskFunc() correctly, unlike an ordinary user "new(...)" call,
//     modeled as MethodFuncCall, which does not -- see below), with 1
//     argument, an "add" Operation ("def + 3") whose operands are
//     test_cls's own "def" IODecl and Constant "3"
//     "a = def;" -- a blocking Assignment, lhs RefObj "a" -> test_cls's
//     own property Variable, rhs RefObj "def" -> test_cls's own IODecl
//   - the initial process' Begin block has exactly 3 statements:
//     "test_obj = new(37)", "$display(test_obj.incs())",
//     "$display(test_obj.s)"
//   - "test_obj = new(37)": a blocking Assignment, rhs MethodFuncCall
//     "new" with 1 argument, Constant "37" -- per KNOWN COMPILER BUG #6,
//     this ordinary user "new(...)" call does NOT resolve getTaskFunc()
//     to the real constructor Function
//   - "$display(test_obj.incs())": a SysTaskCall whose HierPath's second
//     path elem is a MethodFuncCall "incs" resolving getTaskFunc() to
//     super_cls's "incs" Function -- CONFIRMING that an INHERITED method,
//     called through a derived-class-typed handle, resolves correctly
//     (contrast with KNOWN COMPILER BUG #6, which is specific to
//     constructor calls, not ordinary method calls)
//   - "$display(test_obj.s)": a SysTaskCall whose HierPath's second path
//     elem is a RefObj "s" resolving to super_cls's OWN property
//     Variable -- confirming INHERITED property access resolves to the
//     base class's declared Variable, not a duplicated/derived-scoped copy
//   - design-level: exactly 2 classes (work@super_cls, work@test_cls)
//
// KNOWN COMPILER BUG #2 (property visibility defaulting) and KNOWN
// COMPILER BUG #4 (a method declared directly in a class body is not
// flagged via getMethod()): already confirmed independently across other
// chapter-8 files in this suite (see
// hlc/Google/chapter-8/8.4--instantiation/test_8.4--instantiation.cpp and
// siblings). PropertySIsPublicByDefault, PropertyAIsPublicByDefault, and
// IncsFunctionIsRecognizedAsClassMethod below assert the IEEE-mandated
// behavior and will FAIL until these are fixed.
//
// KNOWN COMPILER BUG #6 (constructor call resolution): already confirmed
// across chapter-8/8.7--constructor/test_8.7--constructor.cpp and
// siblings -- an ordinary user "new(...)" call (MethodFuncCall) never
// resolves getTaskFunc() to the actual constructor, while "super.new(...)"
// (FuncCall) does. FirstStmtIsTestObjNewWithThirtySeven below asserts the
// IEEE-mandated resolution and will FAIL until this is fixed;
// TestClsNewFirstStmtIsSuperNewCall documents the "super.new(...)" side,
// which already passes.
//
// FORMERLY KNOWN COMPILER BUG #1 (class lifetime defaulting): confirmed
// fixed upstream per the ctest runs described in
// chapter-8/8.12--assignment/test_8.12--assignment.cpp and
// chapter-8/8.11--this/test_8.11--this.cpp. SuperClsIsAutomaticByDefault
// and TestClsIsAutomaticByDefault below are plain passing assertions
// accordingly.

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
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ClassInheritanceTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "8.13--inheritance.hlc"}); }
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

  static const hldb::Variable *getPropertyS() {
    const hldb::ClassDefn *const c = getSuperClsDefn();
    if (c == nullptr || c->getVariables() == nullptr || c->getVariables()->empty()) return nullptr;
    return c->getVariables()->at(0);
  }

  static const hldb::Variable *getPropertyA() {
    const hldb::ClassDefn *const c = getTestClsDefn();
    if (c == nullptr || c->getVariables() == nullptr || c->getVariables()->empty()) return nullptr;
    return c->getVariables()->at(0);
  }

  static const hldb::Function *getIncsFunction() {
    const hldb::ClassDefn *const c = getSuperClsDefn();
    if (c == nullptr || c->getMethods() == nullptr || c->getMethods()->empty()) return nullptr;
    return any_cast<hldb::Function>(c->getMethods()->at(0));
  }

  static const hldb::Function *getSuperNewFunction() {
    const hldb::ClassDefn *const c = getSuperClsDefn();
    if (c == nullptr || c->getMethods() == nullptr || c->getMethods()->size() < 2) return nullptr;
    return any_cast<hldb::Function>(c->getMethods()->at(1));
  }

  static const hldb::Function *getTestClsNewFunction() {
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

TEST_F(ClassInheritanceTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ClassInheritanceTest, ModuleHasOneNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(ClassInheritanceTest, ModuleHasTwoClassDefns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClassDefns(), nullptr);
  EXPECT_EQ(top->getClassDefns()->size(), 2u);
}

// --- class "super_cls" ---------------------------------------------------------

TEST_F(ClassInheritanceTest, SuperClsExists) { EXPECT_NE(getSuperClsDefn(), nullptr); }

TEST_F(ClassInheritanceTest, SuperClsIsUserDefinedClass) {
  const hldb::ClassDefn *const c = getSuperClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getClassType(), vpiUserDefinedClass);
}

TEST_F(ClassInheritanceTest, SuperClsIsAutomaticByDefault) {
  const hldb::ClassDefn *const c = getSuperClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->getAutomatic()) << "8.3: 'class super_cls' has no lifetime qualifier so it defaults to automatic";
}

TEST_F(ClassInheritanceTest, SuperClsHasOnePropertyS) {
  const hldb::ClassDefn *const c = getSuperClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getVariables(), nullptr);
  ASSERT_EQ(c->getVariables()->size(), 1u);
  const hldb::Variable *const s = getPropertyS();
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->getName(), "s");
  ASSERT_NE(s->getTypespec(), nullptr);
  const hldb::IntTypespec *const elem = s->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "property 's' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned());
}

TEST_F(ClassInheritanceTest, PropertySHasInitializerTwo) {
  const hldb::Variable *const s = getPropertyS();
  ASSERT_NE(s, nullptr);
  const hldb::Constant *const init = s->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "'int s = 2;' should attach '2' as the property's own initializer value";
  EXPECT_EQ(init->getDecompile(), "2");
}

TEST_F(ClassInheritanceTest, PropertySIsPublicByDefault) {
  const hldb::Variable *const s = getPropertyS();
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->getVisibility(), vpiPublicVis) << "8.14: 'int s' with no visibility qualifier defaults to public "
                                                 "(see KNOWN COMPILER BUG #2 above)";
}

TEST_F(ClassInheritanceTest, SuperClsHasTwoMethods) {
  const hldb::ClassDefn *const c = getSuperClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getMethods(), nullptr);
  ASSERT_EQ(c->getMethods()->size(), 2u);
  const hldb::Function *const incs = getIncsFunction();
  ASSERT_NE(incs, nullptr);
  EXPECT_EQ(incs->getName(), "incs");
  const hldb::Function *const ctor = getSuperNewFunction();
  ASSERT_NE(ctor, nullptr);
  EXPECT_EQ(ctor->getName(), "new");
}

TEST_F(ClassInheritanceTest, IncsFunctionIsRecognizedAsClassMethod) {
  const hldb::Function *const incs = getIncsFunction();
  ASSERT_NE(incs, nullptr);
  EXPECT_TRUE(incs->getMethod()) << "8.13: 'incs' is declared directly inside the class body and should be "
                                    "flagged as a class method (see KNOWN COMPILER BUG #4 above)";
}

TEST_F(ClassInheritanceTest, IncsFunctionIsPublicByDefault) {
  const hldb::Function *const incs = getIncsFunction();
  ASSERT_NE(incs, nullptr);
  EXPECT_EQ(incs->getVisibility(), vpiPublicVis) << "8.14: 'function int incs()' with no visibility qualifier "
                                                    "defaults to public";
}

TEST_F(ClassInheritanceTest, IncsFunctionReturnTypeIsPlainInt) {
  const hldb::Function *const incs = getIncsFunction();
  ASSERT_NE(incs, nullptr);
  ASSERT_NE(incs->getReturn(), nullptr);
  const hldb::IntTypespec *const ret = incs->getReturn()->getActual<hldb::IntTypespec>();
  ASSERT_NE(ret, nullptr) << "'incs's return type should resolve to a plain IntTypespec, confirming this is an "
                             "ordinary function and not a constructor";
  EXPECT_TRUE(ret->getSigned());
}

TEST_F(ClassInheritanceTest, IncsBodyHasTwoStmts) {
  const hldb::Function *const incs = getIncsFunction();
  ASSERT_NE(incs, nullptr);
  const hldb::Begin *const body = incs->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 2u);
}

TEST_F(ClassInheritanceTest, IncsFirstStmtIsPreIncrementOfS) {
  const hldb::Function *const incs = getIncsFunction();
  ASSERT_NE(incs, nullptr);
  const hldb::Begin *const body = incs->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_GT(body->getStmts()->size(), 0u);
  const hldb::Operation *const op = any_cast<hldb::Operation>(body->getStmts()->at(0));
  ASSERT_NE(op, nullptr) << "stmt[0] should be an Operation ('++s;')";
  EXPECT_EQ(op->getOpType(), vpiPreIncOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 1u);
  const hldb::RefObj *const sOperand = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  ASSERT_NE(sOperand, nullptr);
  EXPECT_EQ(sOperand->getName(), "s");
  EXPECT_EQ(sOperand->getActual<hldb::Variable>(), getPropertyS());
}

TEST_F(ClassInheritanceTest, IncsSecondStmtAssignsSToIncsReturnValue) {
  const hldb::Function *const incs = getIncsFunction();
  ASSERT_NE(incs, nullptr);
  const hldb::Begin *const body = incs->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_GT(body->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(1));
  ASSERT_NE(assign, nullptr) << "stmt[1] should be an Assignment ('incs = s;')";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "'incs' (return-value write target) should be a RefObj";
  EXPECT_EQ(lhs->getName(), "incs");
  EXPECT_EQ(lhs->getActual<hldb::Function>(), incs)
      << "assigning to a function's own name should resolve back to the enclosing Function itself";

  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "s");
  EXPECT_EQ(rhs->getActual<hldb::Variable>(), getPropertyS());
}

TEST_F(ClassInheritanceTest, SuperNewIsRecognizedAsConstructor) {
  const hldb::Function *const ctor = getSuperNewFunction();
  ASSERT_NE(ctor, nullptr);
  ASSERT_NE(ctor->getReturn(), nullptr);
  const hldb::ClassTypespec *const ret = ctor->getReturn()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ret, nullptr) << "'new's return type should resolve to a ClassTypespec, confirming this IS a "
                             "constructor (contrast with 'incs' above)";
  EXPECT_EQ(ret->getClassDefn(), getSuperClsDefn());
}

TEST_F(ClassInheritanceTest, SuperNewHasOneIODeclDefWithDefaultThree) {
  const hldb::Function *const ctor = getSuperNewFunction();
  ASSERT_NE(ctor, nullptr);
  ASSERT_NE(ctor->getIODecls(), nullptr);
  ASSERT_EQ(ctor->getIODecls()->size(), 1u);
  const hldb::IODecl *const io = ctor->getIODecls()->at(0);
  ASSERT_NE(io, nullptr);
  EXPECT_EQ(io->getName(), "def");
  EXPECT_EQ(io->getDirection(), vpiInput);
  const hldb::Constant *const def = io->getExpr<hldb::Constant>();
  ASSERT_NE(def, nullptr) << "'int def = 3' should attach '3' as the argument's default value";
  EXPECT_EQ(def->getDecompile(), "3");
}

TEST_F(ClassInheritanceTest, SuperNewBodyAssignsSToDef) {
  const hldb::Function *const ctor = getSuperNewFunction();
  ASSERT_NE(ctor, nullptr);
  const hldb::Assignment *const assign = ctor->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr) << "'s = def;' is super_cls's only statement, so it is NOT wrapped in a Begin";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "s");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getPropertyS());

  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "def");
  ASSERT_NE(ctor->getIODecls(), nullptr);
  ASSERT_GT(ctor->getIODecls()->size(), 0u);
  EXPECT_EQ(rhs->getActual<hldb::IODecl>(), ctor->getIODecls()->at(0));
}

// --- class "test_cls" ---------------------------------------------------------

TEST_F(ClassInheritanceTest, TestClsExists) { EXPECT_NE(getTestClsDefn(), nullptr); }

TEST_F(ClassInheritanceTest, TestClsIsUserDefinedClass) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getClassType(), vpiUserDefinedClass);
}

TEST_F(ClassInheritanceTest, TestClsIsAutomaticByDefault) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->getAutomatic()) << "8.3: 'class test_cls' has no lifetime qualifier so it defaults to automatic";
}

// THE structural crux of "extends": test_cls's Extends object must
// resolve back to super_cls's own ClassDefn.
TEST_F(ClassInheritanceTest, TestClsExtendsSuperCls) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  const hldb::Extends *const ext = c->getExtends();
  ASSERT_NE(ext, nullptr) << "'test_cls extends super_cls' should attach an Extends object";
  ASSERT_NE(ext->getClassTypespecs(), nullptr);
  ASSERT_EQ(ext->getClassTypespecs()->size(), 1u);
  const hldb::RefTypespec *const ref = ext->getClassTypespecs()->at(0);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "super_cls");
  const hldb::ClassTypespec *const ct = ref->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr);
  EXPECT_EQ(ct->getClassDefn(), getSuperClsDefn());
}

TEST_F(ClassInheritanceTest, TestClsHasOnePropertyA) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getVariables(), nullptr);
  ASSERT_EQ(c->getVariables()->size(), 1u);
  const hldb::Variable *const a = getPropertyA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getName(), "a");
  ASSERT_NE(a->getTypespec(), nullptr);
  const hldb::IntTypespec *const elem = a->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "property 'a' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned());
}

TEST_F(ClassInheritanceTest, PropertyAHasNoInitializer) {
  const hldb::Variable *const a = getPropertyA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getValue(), nullptr) << "'int a;' declares no initializer";
}

TEST_F(ClassInheritanceTest, PropertyAIsPublicByDefault) {
  const hldb::Variable *const a = getPropertyA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getVisibility(), vpiPublicVis) << "8.14: 'int a' with no visibility qualifier defaults to public "
                                                 "(see KNOWN COMPILER BUG #2 above)";
}

TEST_F(ClassInheritanceTest, TestClsHasOneMethodNew) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getMethods(), nullptr);
  ASSERT_EQ(c->getMethods()->size(), 1u);
  const hldb::Function *const ctor = getTestClsNewFunction();
  ASSERT_NE(ctor, nullptr);
  EXPECT_EQ(ctor->getName(), "new");
}

TEST_F(ClassInheritanceTest, TestClsNewIsRecognizedAsConstructor) {
  const hldb::Function *const ctor = getTestClsNewFunction();
  ASSERT_NE(ctor, nullptr);
  ASSERT_NE(ctor->getReturn(), nullptr);
  const hldb::ClassTypespec *const ret = ctor->getReturn()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ret, nullptr) << "'new's return type should resolve to a ClassTypespec";
  EXPECT_EQ(ret->getClassDefn(), getTestClsDefn());
}

TEST_F(ClassInheritanceTest, TestClsNewHasOneIODeclDefWithDefaultFortyTwo) {
  const hldb::Function *const ctor = getTestClsNewFunction();
  ASSERT_NE(ctor, nullptr);
  ASSERT_NE(ctor->getIODecls(), nullptr);
  ASSERT_EQ(ctor->getIODecls()->size(), 1u);
  const hldb::IODecl *const io = ctor->getIODecls()->at(0);
  ASSERT_NE(io, nullptr);
  EXPECT_EQ(io->getName(), "def");
  EXPECT_EQ(io->getDirection(), vpiInput);
  const hldb::Constant *const def = io->getExpr<hldb::Constant>();
  ASSERT_NE(def, nullptr) << "'int def = 42' should attach '42' as the argument's default value";
  EXPECT_EQ(def->getDecompile(), "42");
}

TEST_F(ClassInheritanceTest, TestClsNewBodyHasTwoStmts) {
  const hldb::Function *const ctor = getTestClsNewFunction();
  ASSERT_NE(ctor, nullptr);
  const hldb::Begin *const body = ctor->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 2u);
}

// THE crux of chaining: "super.new(def + 3);" must resolve "super" to
// super_cls's ClassDefn and the call itself, as a FuncCall, must resolve
// getTaskFunc() to super_cls's own "new" -- contrast with KNOWN COMPILER
// BUG #6 (an ordinary user "new(...)" call never resolves getTaskFunc()).
TEST_F(ClassInheritanceTest, TestClsNewFirstStmtIsSuperNewCall) {
  const hldb::Function *const ctor = getTestClsNewFunction();
  ASSERT_NE(ctor, nullptr);
  const hldb::Begin *const body = ctor->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_GT(body->getStmts()->size(), 0u);
  const hldb::HierPath *const path = any_cast<hldb::HierPath>(body->getStmts()->at(0));
  ASSERT_NE(path, nullptr) << "stmt[0] should be a bare HierPath ('super.new(def + 3)')";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);

  const hldb::RefObj *const superRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(superRef, nullptr);
  EXPECT_EQ(superRef->getName(), "super");
  EXPECT_EQ(superRef->getActual<hldb::ClassDefn>(), getSuperClsDefn());

  const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(path->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "'super.new(...)' second path elem should be a FuncCall";
  EXPECT_EQ(call->getName(), "new");
  EXPECT_EQ(call->getTaskFunc(), getSuperNewFunction())
      << "'super.new(...)' should resolve getTaskFunc() to super_cls's own constructor (contrast with KNOWN "
         "COMPILER BUG #6)";

  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::Operation *const arg = any_cast<hldb::Operation>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr) << "'def + 3' should be an explicit 'add' Operation";
  EXPECT_EQ(arg->getOpType(), vpiAddOp);
  ASSERT_NE(arg->getOperands(), nullptr);
  ASSERT_EQ(arg->getOperands()->size(), 2u);
  const hldb::RefObj *const defOperand = any_cast<hldb::RefObj>(arg->getOperands()->at(0));
  ASSERT_NE(defOperand, nullptr);
  EXPECT_EQ(defOperand->getName(), "def");
  ASSERT_NE(ctor->getIODecls(), nullptr);
  ASSERT_GT(ctor->getIODecls()->size(), 0u);
  EXPECT_EQ(defOperand->getActual<hldb::IODecl>(), ctor->getIODecls()->at(0));
  const hldb::Constant *const threeOperand = any_cast<hldb::Constant>(arg->getOperands()->at(1));
  ASSERT_NE(threeOperand, nullptr);
  EXPECT_EQ(threeOperand->getDecompile(), "3");
}

TEST_F(ClassInheritanceTest, TestClsNewSecondStmtAssignsAToDef) {
  const hldb::Function *const ctor = getTestClsNewFunction();
  ASSERT_NE(ctor, nullptr);
  const hldb::Begin *const body = ctor->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_GT(body->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(1));
  ASSERT_NE(assign, nullptr) << "stmt[1] should be an Assignment ('a = def;')";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "a");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getPropertyA());

  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "def");
  ASSERT_NE(ctor->getIODecls(), nullptr);
  ASSERT_GT(ctor->getIODecls()->size(), 0u);
  EXPECT_EQ(rhs->getActual<hldb::IODecl>(), ctor->getIODecls()->at(0));
}

// --- net "test_obj" ---------------------------------------------------------------

TEST_F(ClassInheritanceTest, NetTestObjExists) { EXPECT_NE(getNetTestObj(), nullptr); }

TEST_F(ClassInheritanceTest, NetTestObjTypespecResolvesToTestClsClassDefn) {
  const hldb::Net *const testObj = getNetTestObj();
  ASSERT_NE(testObj, nullptr);
  ASSERT_NE(testObj->getTypespec(), nullptr);
  const hldb::ClassTypespec *const ct = testObj->getTypespec<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr);
  EXPECT_EQ(ct->getClassDefn(), getTestClsDefn());
}

// --- initial process structure -------------------------------------------------

TEST_F(ClassInheritanceTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(ClassInheritanceTest, InitialBeginHasThreeStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 3u);
}

// See KNOWN COMPILER BUG #6 above: this assertion documents the
// IEEE-mandated resolution and is expected to FAIL until that bug is
// fixed.
TEST_F(ClassInheritanceTest, FirstStmtIsTestObjNewWithThirtySeven) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 0u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "stmt[0] should be an Assignment (test_obj = new(37))";
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "test_obj");
  EXPECT_EQ(lhs->getActual<hldb::Net>(), getNetTestObj());

  const hldb::MethodFuncCall *const newCall = assign->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(newCall, nullptr) << "'new(37)' should resolve to a MethodFuncCall";
  EXPECT_EQ(newCall->getName(), "new");
  ASSERT_NE(newCall->getArguments(), nullptr);
  ASSERT_EQ(newCall->getArguments()->size(), 1u);
  const hldb::Constant *const arg = any_cast<hldb::Constant>(newCall->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getDecompile(), "37");

  EXPECT_EQ(newCall->getTaskFunc(), getTestClsNewFunction())
      << "8.7/8.13: an ordinary 'new(...)' call should resolve getTaskFunc() to the user-written constructor "
         "(see KNOWN COMPILER BUG #6 above)";
}

// The crux of the inheritance-plus-method-call confirmation: calling an
// INHERITED method through a derived-class-typed handle must resolve
// getTaskFunc() correctly, contrasting with KNOWN COMPILER BUG #6 (which
// is specific to constructor calls).
TEST_F(ClassInheritanceTest, SecondStmtDisplaysTestObjIncs) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 1u);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(1));
  ASSERT_NE(disp, nullptr) << "stmt[1] should be a $display SysTaskCall";
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);

  const hldb::HierPath *const path = any_cast<hldb::HierPath>(disp->getArguments()->at(0));
  ASSERT_NE(path, nullptr) << "'test_obj.incs()' should be a HierPath";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);
  const hldb::RefObj *const netRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(netRef, nullptr);
  EXPECT_EQ(netRef->getName(), "test_obj");
  EXPECT_EQ(netRef->getActual<hldb::Net>(), getNetTestObj());

  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(path->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "'test_obj.incs()' second path elem should be a MethodFuncCall";
  EXPECT_EQ(call->getName(), "incs");
  EXPECT_EQ(call->getTaskFunc(), getIncsFunction())
      << "an INHERITED method called through a derived-class handle should resolve getTaskFunc() to the base "
         "class's Function";
}

// The crux of the inheritance-plus-property-access confirmation: an
// INHERITED property, accessed through a derived-class-typed handle, must
// resolve to the base class's OWN declared Variable.
TEST_F(ClassInheritanceTest, ThirdStmtDisplaysTestObjS) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 2u);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(2));
  ASSERT_NE(disp, nullptr) << "stmt[2] should be a $display SysTaskCall";
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);

  const hldb::HierPath *const path = any_cast<hldb::HierPath>(disp->getArguments()->at(0));
  ASSERT_NE(path, nullptr) << "'test_obj.s' should be a HierPath";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);
  const hldb::RefObj *const netRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(netRef, nullptr);
  EXPECT_EQ(netRef->getName(), "test_obj");
  EXPECT_EQ(netRef->getActual<hldb::Net>(), getNetTestObj());
  const hldb::RefObj *const sRef = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
  ASSERT_NE(sRef, nullptr);
  EXPECT_EQ(sRef->getName(), "s");
  EXPECT_EQ(sRef->getActual<hldb::Variable>(), getPropertyS())
      << "'test_obj.s' should resolve to super_cls's OWN declared property Variable";
}

// --- compiler diagnostics ---------------------------------------------------------

TEST_F(ClassInheritanceTest, CompilerReportsNoErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
