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

// Tests for 8.6--methods.sv (tags: 8.6)
//   module class_tb ();
//     class test_cls;
//       int a;
//       task test_method(int val);
//         $display("test_method");
//         a += val;
//       endtask
//     endclass
//
//     test_cls test_obj;
//
//     initial begin
//       test_obj = new;
//       test_obj.a = 12;
//       $display(test_obj.a);
//       test_obj.test_method(9);
//       $display(test_obj.a);
//     end
//   endmodule
//
// IEEE 1800-2017 8.6 "Methods": a class may declare tasks/functions as
// methods; they are invoked through a handle with the "." operator
// ("test_obj.test_method(9)") the same way a property is accessed, and
// (unlike the implicit "new" constructor in chapter-8/8.4--instantiation.sv)
// this method is user-written, so the call's resolution back to the
// specific Task declared in the class IS verifiable ground truth here.
//
// Checked:
//   - design has module work@class_tb with exactly 1 net: "test_obj" (the
//     class handle)
//   - the module has exactly 1 nested ClassDefn: "work@test_cls"
//   - ClassDefn "test_cls": classType vpiUserDefinedClass, has exactly 1
//     property ("a") and exactly 1 method ("test_method") -- see the KNOWN
//     COMPILER BUG notes below for the class's lifetime and the property's
//     visibility (both default-related bugs already confirmed elsewhere)
//   - method "test_method": resolves to a Task, IS correctly marked public
//     by default (getVisibility() == vpiPublicVis) -- contrast this with
//     the KNOWN COMPILER BUG note below: methods default their visibility
//     correctly where properties do not
//   - scope: "test_method" is flagged getMethod() == true (correctly
//     classified as belonging to a class, not a free-standing task), and
//     does NOT also appear in the enclosing module's own getTaskFuncs()
//     list (mirrors the enum/typedef scope-containment check in
//     chapter-8/8.5--properties_enum/test_8.5--properties_enum.cpp)
//   - "test_method" has exactly 1 IODecl ("val", direction input, signed
//     IntTypespec) and a 2-statement body: "$display("test_method")" and
//     "a += val"
//   - "a += val" lowers to a blocking Assignment: lhs RefObj "a" resolved
//     to the SAME Variable as the class's property; rhs is a 2-operand add
//     Operation (vpiAddOp) whose operands are RefObj "a" (same Variable
//     again) and RefObj "val" (resolved to the task's own IODecl)
//   - net "test_obj": its typespec resolves (RefTypespec -> ClassTypespec)
//     to the SAME ClassDefn as "work@test_cls"
//   - the initial process' Begin block has exactly 5 statements:
//     "test_obj = new", "test_obj.a = 12", "$display(test_obj.a)",
//     "test_obj.test_method(9)", "$display(test_obj.a)"
//   - "test_obj.test_method(9)" is itself a bare HierPath statement (not
//     wrapped in a SysFuncCall or Assignment): 2 path elems (RefObj
//     "test_obj" resolved to the Net; MethodFuncCall "test_method" taking
//     1 Constant argument "9") -- and crucially, that MethodFuncCall's
//     getTaskFunc() resolves back to the SAME Task object declared as
//     "test_cls"'s method, confirming the call is tied to its declaration
//   - design-level: exactly 1 class (work@test_cls)
//
// KNOWN COMPILER BUG #1 (class lifetime defaulting, not a defect in this
// file): IEEE 1800-2017 8.3 says a class declared with no lifetime
// qualifier must default to automatic lifetime (getAutomatic() == true).
// This HLC build never sets the automatic flag to true for the unqualified
// case. Already confirmed independently via
// hlc/Google/generic/class/test_class_test_1.cpp,
// hlc/Google/chapter-8/8.4--instantiation/test_8.4--instantiation.cpp,
// hlc/Google/chapter-8/8.5--parameters/test_8.5--parameters.cpp and
// hlc/Google/chapter-8/8.5--properties_enum/test_8.5--properties_enum.cpp
// (all fail the analogous check). ClassIsAutomaticByDefault below asserts
// the IEEE-mandated behavior and will FAIL until this is fixed.
//
// KNOWN COMPILER BUG #2 (property visibility defaulting, not a defect in
// this file): IEEE 1800-2017 8.14 says a property with no explicit
// "local"/"protected" qualifier defaults to public visibility. Confirmed
// independently via
// hlc/Google/chapter-8/8.5--properties/test_8.5--properties.cpp: property
// 'a' returns getVisibility() == 0, not vpiPublicVis. PropertyAIsPublic-
// ByDefault below asserts the IEEE-mandated behavior and will FAIL until
// this is fixed. Notably, TestMethodIsPublicByDefault below is expected to
// PASS for the method 'test_method' -- this build's .log shows
// "vpiVisibility: public (1)" for it, so the defaulting bug appears
// specific to properties (Variable), not methods (TaskFunc).
//
// KNOWN COMPILER BUG #3 (method classification, new finding, confirmed via
// ctest): 'test_method' is declared directly inside test_cls's body, is
// found via ClassDefn::getMethods(), and resolves correctly when called
// through a handle ("test_obj.test_method(9)") -- structurally it is a
// class method in every observable way. Yet TaskFunc::getMethod(), the
// flag specifically meant to record "this task/function belongs to a
// class," returns false for it. Same root pattern as #1 and #2: a flag
// that should be explicitly set based on where the declaration appears is
// instead left at its unset default. TestMethodIsFlaggedAsAClassMethod
// below asserts the correct classification and FAILS until this is fixed.

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
#include <hldb/task.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ClassMethodsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "8.6--methods.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("work@class_tb", m_design->getAllModules());
  }

  static const hldb::ClassDefn *getTestClsDefn() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::ClassDefn>("work@test_cls", top->getClassDefns());
  }

  static const hldb::Net *getNetTestObj() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Net>("test_obj", top->getNets());
  }

  static const hldb::Variable *getPropertyA() {
    const hldb::ClassDefn *const c = getTestClsDefn();
    if (c == nullptr || c->getVariables() == nullptr || c->getVariables()->empty()) return nullptr;
    return c->getVariables()->at(0);
  }

  static const hldb::Task *getTestMethodTask() {
    const hldb::ClassDefn *const c = getTestClsDefn();
    if (c == nullptr || c->getMethods() == nullptr || c->getMethods()->empty()) return nullptr;
    return any_cast<hldb::Task>(c->getMethods()->at(0));
  }

  static const hldb::Begin *getInitialBegin() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (init == nullptr) return nullptr;
    return init->getStmt<hldb::Begin>();
  }

  // Verifies stmt[index] is "$display(<value>)" with exactly 1 HierPath
  // argument "test_obj.a" resolving to the class's property Variable.
  static void ExpectDisplayOfTestObjA(size_t index) {
    const hldb::Begin *const begin = getInitialBegin();
    ASSERT_NE(begin, nullptr);
    ASSERT_GT(begin->getStmts()->size(), index);
    const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(index));
    ASSERT_NE(disp, nullptr) << "stmt[" << index << "] should be a $display SysFuncCall";
    EXPECT_EQ(disp->getName(), "$display");
    ASSERT_NE(disp->getArguments(), nullptr);
    ASSERT_EQ(disp->getArguments()->size(), 1u);

    const hldb::HierPath *const path = any_cast<hldb::HierPath>(disp->getArguments()->at(0));
    ASSERT_NE(path, nullptr) << "'test_obj.a' should be a HierPath";
    ASSERT_NE(path->getPathElems(), nullptr);
    ASSERT_EQ(path->getPathElems()->size(), 2u);

    const hldb::RefObj *const testObjRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
    ASSERT_NE(testObjRef, nullptr);
    EXPECT_EQ(testObjRef->getName(), "test_obj");
    EXPECT_EQ(testObjRef->getActual<hldb::Net>(), getNetTestObj());

    const hldb::RefObj *const aRef = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
    ASSERT_NE(aRef, nullptr);
    EXPECT_EQ(aRef->getName(), "a");
    EXPECT_EQ(aRef->getActual<hldb::Variable>(), getPropertyA());
  }
};

// --- module / design shape ---------------------------------------------------

TEST_F(ClassMethodsTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ClassMethodsTest, ModuleHasOneNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(ClassMethodsTest, ModuleHasOneClassDefn) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClassDefns(), nullptr);
  EXPECT_EQ(top->getClassDefns()->size(), 1u);
}

// --- class "test_cls" ---------------------------------------------------------

TEST_F(ClassMethodsTest, ClassTestClsExists) { EXPECT_NE(getTestClsDefn(), nullptr); }

TEST_F(ClassMethodsTest, ClassIsUserDefinedClass) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getClassType(), vpiUserDefinedClass);
}

TEST_F(ClassMethodsTest, ClassIsAutomaticByDefault) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->getAutomatic()) << "8.3: 'class test_cls' has no lifetime qualifier so it defaults to automatic; "
                                    "getAutomatic() must return true (see KNOWN COMPILER BUG #1 above)";
}

TEST_F(ClassMethodsTest, ClassHasOnePropertyA) {
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

TEST_F(ClassMethodsTest, PropertyAIsPublicByDefault) {
  const hldb::Variable *const a = getPropertyA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getVisibility(), vpiPublicVis) << "8.14: 'int a;' with no visibility qualifier defaults to public "
                                                 "(see KNOWN COMPILER BUG #2 above)";
}

TEST_F(ClassMethodsTest, ClassHasOneMethodTestMethod) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getMethods(), nullptr);
  ASSERT_EQ(c->getMethods()->size(), 1u);
  const hldb::Task *const t = getTestMethodTask();
  ASSERT_NE(t, nullptr) << "'task test_method(...)' should resolve to a Task";
  EXPECT_EQ(t->getName(), "test_method");
}

TEST_F(ClassMethodsTest, TestMethodIsPublicByDefault) {
  const hldb::Task *const t = getTestMethodTask();
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(t->getVisibility(), vpiPublicVis)
      << "8.14: 'task test_method(...)' with no visibility qualifier defaults to public -- unlike property 'a' "
         "(see KNOWN COMPILER BUG #2 above), this is expected to PASS for methods";
}

// "getMethod()" is the direct signal for "is this task correctly classified
// as belonging to a class" (as opposed to a free-standing, non-method
// task/function elsewhere in the design) -- the crux of "is the scope of
// the task being correctly interpreted." See KNOWN COMPILER BUG #3 above.
TEST_F(ClassMethodsTest, TestMethodIsFlaggedAsAClassMethod) {
  const hldb::Task *const t = getTestMethodTask();
  ASSERT_NE(t, nullptr);
  EXPECT_TRUE(t->getMethod()) << "'task test_method(...)' is declared inside test_cls's body, so it must be "
                                 "flagged as a class method, not a free-standing task (see KNOWN COMPILER BUG "
                                 "#3 above)";
}

// Mirrors the enum/typedef scope-containment check in
// chapter-8/8.5--properties_enum/test_8.5--properties_enum.cpp: a method
// declared inside a class must stay nested under the ClassDefn and must
// not also appear in the enclosing module's own scope.
TEST_F(ClassMethodsTest, ModuleScopeDoesNotContainTestMethod) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Task *const t = getTestMethodTask();
  ASSERT_NE(t, nullptr);
  if (top->getTaskFuncs() == nullptr) return;
  for (const hldb::TaskFunc *const tf : *top->getTaskFuncs()) {
    EXPECT_NE(tf, t) << "the class-scoped method must not also appear in the module's own tasks/functions";
  }
}

TEST_F(ClassMethodsTest, TestMethodHasOneIODeclVal) {
  const hldb::Task *const t = getTestMethodTask();
  ASSERT_NE(t, nullptr);
  ASSERT_NE(t->getIODecls(), nullptr);
  ASSERT_EQ(t->getIODecls()->size(), 1u);
  const hldb::IODecl *const val = t->getIODecls()->at(0);
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getName(), "val");
  EXPECT_EQ(val->getDirection(), vpiInput);
  ASSERT_NE(val->getTypespec(), nullptr);
  const hldb::IntTypespec *const elem = val->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "IODecl 'val' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned());
}

TEST_F(ClassMethodsTest, TestMethodBodyHasTwoStmts) {
  const hldb::Task *const t = getTestMethodTask();
  ASSERT_NE(t, nullptr);
  const hldb::Begin *const body = t->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "task body should be a Begin block";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 2u);
}

TEST_F(ClassMethodsTest, TestMethodFirstStmtDisplaysItsOwnName) {
  const hldb::Task *const t = getTestMethodTask();
  ASSERT_NE(t, nullptr);
  const hldb::Begin *const body = t->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_GT(body->getStmts()->size(), 0u);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(body->getStmts()->at(0));
  ASSERT_NE(disp, nullptr) << "'$display(\"test_method\")' should be a $display SysFuncCall";
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);
  const hldb::Constant *const arg = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getValue(), "test_method");
}

TEST_F(ClassMethodsTest, TestMethodSecondStmtIsAPlusEqualsVal) {
  const hldb::Task *const t = getTestMethodTask();
  ASSERT_NE(t, nullptr);
  const hldb::Begin *const body = t->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_GT(body->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(1));
  ASSERT_NE(assign, nullptr) << "'a += val' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "a");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getPropertyA());

  const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'a += val' should lower to 'a = a + val', an add Operation";
  EXPECT_EQ(rhs->getOpType(), vpiAddOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);

  const hldb::RefObj *const aOperand = any_cast<hldb::RefObj>(rhs->getOperands()->at(0));
  ASSERT_NE(aOperand, nullptr);
  EXPECT_EQ(aOperand->getName(), "a");
  EXPECT_EQ(aOperand->getActual<hldb::Variable>(), getPropertyA());

  const hldb::RefObj *const valOperand = any_cast<hldb::RefObj>(rhs->getOperands()->at(1));
  ASSERT_NE(valOperand, nullptr);
  EXPECT_EQ(valOperand->getName(), "val");
  ASSERT_NE(t->getIODecls(), nullptr);
  ASSERT_GT(t->getIODecls()->size(), 0u);
  EXPECT_EQ(valOperand->getActual<hldb::IODecl>(), t->getIODecls()->at(0))
      << "'val' inside the task body must resolve to the SAME IODecl as the task's own parameter";
}

// --- net "test_obj": the class handle ------------------------------------------

TEST_F(ClassMethodsTest, NetTestObjExists) { EXPECT_NE(getNetTestObj(), nullptr); }

TEST_F(ClassMethodsTest, NetTestObjTypespecResolvesToTestClsClassDefn) {
  const hldb::Net *const testObj = getNetTestObj();
  ASSERT_NE(testObj, nullptr);
  ASSERT_NE(testObj->getTypespec(), nullptr);
  const hldb::ClassTypespec *const ct = testObj->getTypespec<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr) << "handle 'test_obj' must resolve to a ClassTypespec";
  EXPECT_EQ(ct->getClassDefn(), getTestClsDefn())
      << "the handle's ClassTypespec must point back to the SAME ClassDefn as 'test_cls'";
}

// --- initial process structure -------------------------------------------------

TEST_F(ClassMethodsTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(ClassMethodsTest, InitialBeginHasFiveStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 5u);
}

// --- test_obj = new (stmt[0]) -----------------------------------------------------

TEST_F(ClassMethodsTest, FirstStmtIsBlockingAssignment) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 0u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "'test_obj = new' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());
}

TEST_F(ClassMethodsTest, FirstStmtLhsIsTestObjHandle) {
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

TEST_F(ClassMethodsTest, FirstStmtRhsIsNewMethodFuncCall) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 0u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::MethodFuncCall *const newCall = assign->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(newCall, nullptr) << "'new' should resolve to a MethodFuncCall";
  EXPECT_EQ(newCall->getName(), "new");
  EXPECT_EQ(newCall->getArguments(), nullptr);
}

// --- test_obj.a = 12 (stmt[1]) -----------------------------------------------------

TEST_F(ClassMethodsTest, SecondStmtLhsIsTestObjDotA) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(1));
  ASSERT_NE(assign, nullptr) << "'test_obj.a = 12' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());
  const hldb::HierPath *const lhs = assign->getLhs<hldb::HierPath>();
  ASSERT_NE(lhs, nullptr) << "'test_obj.a' (write target) should be a HierPath";
  ASSERT_NE(lhs->getPathElems(), nullptr);
  ASSERT_EQ(lhs->getPathElems()->size(), 2u);

  const hldb::RefObj *const testObjRef = any_cast<hldb::RefObj>(lhs->getPathElems()->at(0));
  ASSERT_NE(testObjRef, nullptr);
  EXPECT_EQ(testObjRef->getName(), "test_obj");
  EXPECT_EQ(testObjRef->getActual<hldb::Net>(), getNetTestObj());

  const hldb::RefObj *const aRef = any_cast<hldb::RefObj>(lhs->getPathElems()->at(1));
  ASSERT_NE(aRef, nullptr);
  EXPECT_EQ(aRef->getName(), "a");
  EXPECT_EQ(aRef->getActual<hldb::Variable>(), getPropertyA());
}

TEST_F(ClassMethodsTest, SecondStmtRhsIsTwelve) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "12");
}

// --- $display(test_obj.a) before the method call (stmt[2]) -------------------------

TEST_F(ClassMethodsTest, ThirdStmtDisplaysTestObjA) { ExpectDisplayOfTestObjA(2); }

// --- test_obj.test_method(9) (stmt[3]) -----------------------------------------------

TEST_F(ClassMethodsTest, FourthStmtIsTestMethodCallHierPath) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 3u);
  const hldb::HierPath *const path = any_cast<hldb::HierPath>(begin->getStmts()->at(3));
  ASSERT_NE(path, nullptr) << "'test_obj.test_method(9)' should itself be a bare HierPath statement";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);

  const hldb::RefObj *const testObjRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(testObjRef, nullptr);
  EXPECT_EQ(testObjRef->getName(), "test_obj");
  EXPECT_EQ(testObjRef->getActual<hldb::Net>(), getNetTestObj());

  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(path->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "'test_method(9)' should resolve to a MethodFuncCall";
  EXPECT_EQ(call->getName(), "test_method");
  const hldb::Task *const t = getTestMethodTask();
  EXPECT_EQ(call->getTaskFunc<hldb::Task>(), t) << "the call must resolve back to the SAME Task declared as "
                                                   "test_cls's method";
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  ASSERT_NE(t, nullptr);
  ASSERT_NE(t->getIODecls(), nullptr);
  EXPECT_EQ(call->getArguments()->size(), t->getIODecls()->size())
      << "the number of actual arguments at the call site (\"(9)\") must match the number of formal "
         "parameters declared on test_method (\"val\"), confirming positional binding lines up";
  const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getDecompile(), "9");
}

// --- $display(test_obj.a) after the method call (stmt[4]) --------------------------

TEST_F(ClassMethodsTest, FifthStmtDisplaysTestObjA) { ExpectDisplayOfTestObjA(4); }

// --- compiler diagnostics ---------------------------------------------------------

TEST_F(ClassMethodsTest, CompilerReportsNoErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
