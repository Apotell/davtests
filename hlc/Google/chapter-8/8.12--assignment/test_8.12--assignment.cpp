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

// Tests for 8.12--assignment.sv (tags: 8.12)
//   module class_tb ();
//     class test_cls;
//       int a;
//       task test_method(int val);
//         $display("test_method");
//         a += val;
//       endtask
//     endclass
//
//     test_cls test_obj0;
//     test_cls test_obj1;
//
//     initial begin
//       test_obj0 = new;
//       test_obj1 = test_obj0;
//
//       test_obj0.a = 12;
//
//       $display(test_obj0.a);
//
//       test_obj0.test_method(9);
//
//       $display(test_obj1.a);
//     end
//   endmodule
//
// IEEE 1800-2017 8.12 "Assignment, copying, and cloning": assigning one
// class handle to another ("test_obj1 = test_obj0;") copies the HANDLE,
// not the object -- both variables end up referring to the SAME object,
// with no new object created. This is structurally distinct from object
// construction ("test_obj0 = new;"), which DOES construct a new object.
// The defining thing to check statically is that these two kinds of
// assignment are NOT conflated: a handle-copy assignment must NOT show a
// "new" call anywhere, and must simply resolve its rhs to the source
// handle's own declaration.
//
// HLC is a static parser/elaborator, not a simulator: it cannot observe
// that a write through "test_obj0.a" becomes visible through "test_obj1.a"
// after the handle-copy (that is a runtime/simulation fact). What it CAN
// and should model statically is (a) that "test_obj1 = test_obj0;" is a
// plain handle assignment with no implicit construction, and (b) that
// "test_obj0.a" and "test_obj1.a" both resolve to the exact same declared
// property Variable regardless of which handle is used to reach it (the
// same "single declaration, multiple access paths" pattern already
// established for the STATIC property in
// chapter-8/8.9--static_properties/test_8.9--static_properties.cpp -- here
// it holds for an ordinary, non-static property too, since HLC never
// allocates per-object storage at all).
//
// Checked:
//   - design has module work@class_tb with exactly 2 nets: "test_obj0" and
//     "test_obj1", both typed as test_cls
//   - the module has exactly 1 nested ClassDefn: "work@test_cls"
//   - ClassDefn "test_cls": classType vpiUserDefinedClass, has exactly 1
//     property ("a", signed IntTypespec, no initializer) and exactly 1
//     method ("test_method", a Task) -- see the KNOWN COMPILER BUG notes
//     below for the class's lifetime, the property's visibility, and the
//     method's "method" flag
//   - "test_method": a Task, public visibility, with exactly 1 IODecl
//     ("val", direction input, signed IntTypespec)
//   - "test_method"'s body is a 2-statement Begin:
//     `$display("test_method");` -- here (inside a TASK body) this is a
//     SysTaskCall, NOT a SysFuncCall as seen for the same-looking call
//     inside an "initial" procedural block in other chapter-8 files (see
//     chapter-8/8.9--static_properties/test_8.9--static_properties.cpp)
//     `a += val;` -- a blocking Assignment desugared to an explicit "add"
//     Operation, lhs RefObj "a" resolving to the property Variable "a",
//     rhs operands [RefObj "a" -> Variable "a", RefObj "val" -> IODecl
//     "val"]
//   - the initial process' Begin block has exactly 6 statements:
//     "test_obj0 = new", "test_obj1 = test_obj0", "test_obj0.a = 12",
//     "$display(test_obj0.a)", "test_obj0.test_method(9)",
//     "$display(test_obj1.a)"
//   - "test_obj0 = new": a blocking Assignment, lhs RefObj resolved to Net
//     "test_obj0", rhs MethodFuncCall "new" taking no arguments
//   - "test_obj1 = test_obj0": THE CRUX of this file -- a blocking
//     Assignment whose lhs RefObj resolves to Net "test_obj1" and whose
//     rhs is an ordinary RefObj resolving to Net "test_obj0" -- NOT a
//     MethodFuncCall "new". This is the structural signature that
//     distinguishes a handle-copy assignment from object construction.
//   - "test_obj0.a = 12": a blocking Assignment whose lhs is a HierPath
//     resolving "a" to the class's property Variable, rhs Constant "12"
//   - "$display(test_obj0.a)": a SysTaskCall (not SysFuncCall, as this is
//     a bare statement, not an expression argument) whose argument is a
//     HierPath resolving "a" the same way
//   - "test_obj0.test_method(9)": appears directly as a bare HierPath
//     statement (no enclosing Assignment/SysTaskCall), whose second path
//     element is a MethodTaskCall "test_method" -- the task-call
//     counterpart of MethodFuncCall, used because "test_method" is a Task
//     -- resolving getTaskFunc() to the class's "test_method" Task, with
//     exactly 1 argument, Constant "9"
//   - "$display(test_obj1.a)": a SysTaskCall whose HierPath's FIRST path
//     elem resolves to the OTHER net (test_obj1, not test_obj0) but whose
//     SECOND path elem ("a") resolves to the exact SAME Variable object as
//     every other "a" access in this file
//   - design-level: exactly 1 class (work@test_cls)
//
// KNOWN COMPILER BUG #1 (class lifetime defaulting) and KNOWN COMPILER BUG
// #2 (property visibility defaulting) and KNOWN COMPILER BUG #4 (a method
// declared directly in a class body is not flagged via getMethod()):
// already confirmed independently across other chapter-8 files in this
// suite (see hlc/Google/chapter-8/8.4--instantiation/test_8.4--instantiation.cpp
// and siblings). ClassIsAutomaticByDefault, PropertyAIsPublicByDefault, and
// TestMethodIsRecognizedAsClassMethod below assert the IEEE-mandated
// behavior and will FAIL until these are fixed. NOTE: this file's own
// .log text dump prints "vpiAutomatic: true" for the ClassDefn (as do the
// committed .log files for 8.4--instantiation and 8.10--static_methods),
// which appears to disagree with the "false" actually returned by
// ClassDefn::getAutomatic() when queried live via ctest for those two
// files -- ClassIsAutomaticByDefault below is written to trust the live
// getter, not the .log text, per this suite's standing rule that logs are
// reference-only.

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
#include <hldb/method_task_call.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_task_call.h>
#include <hldb/task.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ClassAssignmentTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "8.12--assignment.hlc"}); }
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

  static const hldb::Net *getNetTestObj0() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Net>("test_obj0", top->getNets());
  }

  static const hldb::Net *getNetTestObj1() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Net>("test_obj1", top->getNets());
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

TEST_F(ClassAssignmentTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ClassAssignmentTest, ModuleHasTwoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 2u);
}

TEST_F(ClassAssignmentTest, ModuleHasOneClassDefn) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClassDefns(), nullptr);
  EXPECT_EQ(top->getClassDefns()->size(), 1u);
}

// --- class "test_cls" ---------------------------------------------------------

TEST_F(ClassAssignmentTest, ClassTestClsExists) { EXPECT_NE(getTestClsDefn(), nullptr); }

TEST_F(ClassAssignmentTest, ClassIsUserDefinedClass) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getClassType(), vpiUserDefinedClass);
}

TEST_F(ClassAssignmentTest, ClassIsAutomaticByDefault) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->getAutomatic()) << "8.3: 'class test_cls' has no lifetime qualifier so it defaults to "
                                    "automatic (see KNOWN COMPILER BUG #1 above)";
}

TEST_F(ClassAssignmentTest, ClassHasOnePropertyA) {
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

TEST_F(ClassAssignmentTest, PropertyAHasNoInitializer) {
  const hldb::Variable *const a = getPropertyA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getValue(), nullptr) << "'int a;' declares no initializer";
}

TEST_F(ClassAssignmentTest, PropertyAIsPublicByDefault) {
  const hldb::Variable *const a = getPropertyA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getVisibility(), vpiPublicVis) << "8.14: 'int a' with no visibility qualifier defaults to public "
                                                 "(see KNOWN COMPILER BUG #2 above)";
}

TEST_F(ClassAssignmentTest, ClassHasOneMethodTestMethod) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getMethods(), nullptr);
  ASSERT_EQ(c->getMethods()->size(), 1u);
  const hldb::Task *const t = getTestMethodTask();
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(t->getName(), "test_method");
}

TEST_F(ClassAssignmentTest, TestMethodIsRecognizedAsClassMethod) {
  const hldb::Task *const t = getTestMethodTask();
  ASSERT_NE(t, nullptr);
  EXPECT_TRUE(t->getMethod()) << "8.12: 'test_method' is declared directly inside the class body and should be "
                                 "flagged as a class method (see KNOWN COMPILER BUG #4 above)";
}

TEST_F(ClassAssignmentTest, TestMethodIsPublicByDefault) {
  const hldb::Task *const t = getTestMethodTask();
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(t->getVisibility(), vpiPublicVis) << "8.14: 'task test_method(...)' with no visibility qualifier "
                                                 "defaults to public";
}

TEST_F(ClassAssignmentTest, TestMethodHasOneIODeclVal) {
  const hldb::Task *const t = getTestMethodTask();
  ASSERT_NE(t, nullptr);
  ASSERT_NE(t->getIODecls(), nullptr);
  ASSERT_EQ(t->getIODecls()->size(), 1u);
  const hldb::IODecl *const io = t->getIODecls()->at(0);
  ASSERT_NE(io, nullptr);
  EXPECT_EQ(io->getName(), "val");
  EXPECT_EQ(io->getDirection(), vpiInput);
  ASSERT_NE(io->getTypespec(), nullptr);
  const hldb::IntTypespec *const elem = io->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "argument 'val' should resolve to IntTypespec";
}

// --- "test_method" body: '$display("test_method"); a += val;' ------------------

TEST_F(ClassAssignmentTest, TestMethodBodyHasTwoStmts) {
  const hldb::Task *const t = getTestMethodTask();
  ASSERT_NE(t, nullptr);
  const hldb::Begin *const body = t->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 2u);
}

TEST_F(ClassAssignmentTest, FirstMethodStmtDisplaysTestMethod) {
  const hldb::Task *const t = getTestMethodTask();
  ASSERT_NE(t, nullptr);
  const hldb::Begin *const body = t->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_GT(body->getStmts()->size(), 0u);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(body->getStmts()->at(0));
  ASSERT_NE(disp, nullptr) << "stmt[0] should be a $display SysTaskCall (task-body context)";
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);
  const hldb::Constant *const arg = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getDecompile(), "\"test_method\"");
}

TEST_F(ClassAssignmentTest, SecondMethodStmtIsAPlusEqualsVal) {
  const hldb::Task *const t = getTestMethodTask();
  ASSERT_NE(t, nullptr);
  const hldb::Begin *const body = t->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_GT(body->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(1));
  ASSERT_NE(assign, nullptr) << "stmt[1] should be an Assignment ('a += val;')";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "a");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getPropertyA());

  const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'+=' should desugar to an explicit 'add' Operation on the rhs";
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
  EXPECT_EQ(valOperand->getActual<hldb::IODecl>(), t->getIODecls()->at(0));
}

// --- nets "test_obj0" / "test_obj1" ---------------------------------------------

TEST_F(ClassAssignmentTest, NetTestObj0Exists) { EXPECT_NE(getNetTestObj0(), nullptr); }

TEST_F(ClassAssignmentTest, NetTestObj1Exists) { EXPECT_NE(getNetTestObj1(), nullptr); }

TEST_F(ClassAssignmentTest, NetTestObj0TypespecResolvesToTestClsClassDefn) {
  const hldb::Net *const testObj0 = getNetTestObj0();
  ASSERT_NE(testObj0, nullptr);
  ASSERT_NE(testObj0->getTypespec(), nullptr);
  const hldb::ClassTypespec *const ct = testObj0->getTypespec<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr);
  EXPECT_EQ(ct->getClassDefn(), getTestClsDefn());
}

TEST_F(ClassAssignmentTest, NetTestObj1TypespecResolvesToTestClsClassDefn) {
  const hldb::Net *const testObj1 = getNetTestObj1();
  ASSERT_NE(testObj1, nullptr);
  ASSERT_NE(testObj1->getTypespec(), nullptr);
  const hldb::ClassTypespec *const ct = testObj1->getTypespec<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr);
  EXPECT_EQ(ct->getClassDefn(), getTestClsDefn());
}

// --- initial process structure -------------------------------------------------

TEST_F(ClassAssignmentTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(ClassAssignmentTest, InitialBeginHasSixStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 6u);
}

// --- test_obj0 = new; (stmt[0]) --------------------------------------------------

TEST_F(ClassAssignmentTest, FirstStmtIsTestObj0New) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 0u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "stmt[0] should be an Assignment (test_obj0 = new)";
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "test_obj0");
  EXPECT_EQ(lhs->getActual<hldb::Net>(), getNetTestObj0());
  const hldb::MethodFuncCall *const newCall = assign->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(newCall, nullptr) << "'new' should resolve to a MethodFuncCall";
  EXPECT_EQ(newCall->getName(), "new");
  EXPECT_EQ(newCall->getArguments(), nullptr);
}

// The crux of this file: "test_obj1 = test_obj0;" is a HANDLE-COPY
// assignment, not construction. Its rhs must be a plain RefObj resolving
// to the SOURCE net, and must NOT be a "new" MethodFuncCall -- that
// absence is what structurally distinguishes aliasing a handle from
// constructing a new object (8.12).
TEST_F(ClassAssignmentTest, SecondStmtIsTestObj1AssignedTestObj0Handle) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(1));
  ASSERT_NE(assign, nullptr) << "stmt[1] should be an Assignment (test_obj1 = test_obj0)";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "test_obj1");
  EXPECT_EQ(lhs->getActual<hldb::Net>(), getNetTestObj1());

  EXPECT_EQ(assign->getRhs<hldb::MethodFuncCall>(), nullptr)
      << "a handle-copy assignment must NOT resolve to a 'new' MethodFuncCall";
  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "'test_obj0' (handle being copied) should resolve as a plain RefObj";
  EXPECT_EQ(rhs->getName(), "test_obj0");
  EXPECT_EQ(rhs->getActual<hldb::Net>(), getNetTestObj0());
}

// --- test_obj0.a = 12; (stmt[2]) --------------------------------------------------

TEST_F(ClassAssignmentTest, ThirdStmtAssignsTestObj0ATwelve) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 2u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(2));
  ASSERT_NE(assign, nullptr) << "stmt[2] should be an Assignment (test_obj0.a = 12)";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::HierPath *const lhs = assign->getLhs<hldb::HierPath>();
  ASSERT_NE(lhs, nullptr) << "'test_obj0.a' (write target) should be a HierPath";
  ASSERT_NE(lhs->getPathElems(), nullptr);
  ASSERT_EQ(lhs->getPathElems()->size(), 2u);
  const hldb::RefObj *const netRef = any_cast<hldb::RefObj>(lhs->getPathElems()->at(0));
  ASSERT_NE(netRef, nullptr);
  EXPECT_EQ(netRef->getName(), "test_obj0");
  EXPECT_EQ(netRef->getActual<hldb::Net>(), getNetTestObj0());
  const hldb::RefObj *const aRef = any_cast<hldb::RefObj>(lhs->getPathElems()->at(1));
  ASSERT_NE(aRef, nullptr);
  EXPECT_EQ(aRef->getName(), "a");
  EXPECT_EQ(aRef->getActual<hldb::Variable>(), getPropertyA());

  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "12");
}

// --- $display(test_obj0.a); (stmt[3]) ---------------------------------------------

TEST_F(ClassAssignmentTest, FourthStmtDisplaysTestObj0A) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 3u);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(3));
  ASSERT_NE(disp, nullptr) << "stmt[3] should be a $display SysTaskCall";
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);

  const hldb::HierPath *const path = any_cast<hldb::HierPath>(disp->getArguments()->at(0));
  ASSERT_NE(path, nullptr) << "'test_obj0.a' should be a HierPath";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);
  const hldb::RefObj *const netRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(netRef, nullptr);
  EXPECT_EQ(netRef->getName(), "test_obj0");
  EXPECT_EQ(netRef->getActual<hldb::Net>(), getNetTestObj0());
  const hldb::RefObj *const aRef = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
  ASSERT_NE(aRef, nullptr);
  EXPECT_EQ(aRef->getName(), "a");
  EXPECT_EQ(aRef->getActual<hldb::Variable>(), getPropertyA());
}

// --- test_obj0.test_method(9); (stmt[4]) ------------------------------------------

// This call appears directly as a bare HierPath statement (no enclosing
// Assignment or SysTaskCall), whose second path elem is a MethodTaskCall
// -- the task-call counterpart of MethodFuncCall, used because
// "test_method" is a Task rather than a Function/constructor.
TEST_F(ClassAssignmentTest, FifthStmtCallsTestObj0TestMethodWithNine) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 4u);
  const hldb::HierPath *const path = any_cast<hldb::HierPath>(begin->getStmts()->at(4));
  ASSERT_NE(path, nullptr) << "stmt[4] should be a bare HierPath (test_obj0.test_method(9))";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);

  const hldb::RefObj *const netRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(netRef, nullptr);
  EXPECT_EQ(netRef->getName(), "test_obj0");
  EXPECT_EQ(netRef->getActual<hldb::Net>(), getNetTestObj0());

  const hldb::MethodTaskCall *const call = any_cast<hldb::MethodTaskCall>(path->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "'test_obj0.test_method(9)' second path elem should be a MethodTaskCall";
  EXPECT_EQ(call->getName(), "test_method");
  EXPECT_EQ(call->getTaskFunc(), getTestMethodTask());
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getDecompile(), "9");
}

// --- $display(test_obj1.a); (stmt[5]) ---------------------------------------------

// The crux of this file, restated for the read side: reading "a" through
// test_obj1 -- the handle that was assigned FROM test_obj0, never
// separately constructed -- must still resolve "a" to the SAME declared
// property Variable as every "test_obj0.a" access above.
TEST_F(ClassAssignmentTest, SixthStmtDisplaysTestObj1A) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 5u);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(5));
  ASSERT_NE(disp, nullptr) << "stmt[5] should be a $display SysTaskCall";
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);

  const hldb::HierPath *const path = any_cast<hldb::HierPath>(disp->getArguments()->at(0));
  ASSERT_NE(path, nullptr) << "'test_obj1.a' should be a HierPath";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);
  const hldb::RefObj *const netRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(netRef, nullptr);
  EXPECT_EQ(netRef->getName(), "test_obj1");
  EXPECT_EQ(netRef->getActual<hldb::Net>(), getNetTestObj1());
  const hldb::RefObj *const aRef = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
  ASSERT_NE(aRef, nullptr);
  EXPECT_EQ(aRef->getName(), "a");
  EXPECT_EQ(aRef->getActual<hldb::Variable>(), getPropertyA())
      << "'test_obj1.a' must resolve back to the SAME declared property Variable as 'test_obj0.a'";
}

// --- compiler diagnostics ---------------------------------------------------------

TEST_F(ClassAssignmentTest, CompilerReportsNoErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
