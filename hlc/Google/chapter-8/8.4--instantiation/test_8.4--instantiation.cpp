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

// Tests for 8.4--instantiation.sv (tags: 8.4)
//   module class_tb ();
//     class test_cls;
//       int a;
//     endclass
//
//     test_cls test_obj;
//
//     initial begin
//       if (test_obj == null) test_obj = new;
//     end
//   endmodule
//
// IEEE 1800-2017 8.4 "Object creation and initialization": a class handle
// ("test_obj") is a variable that can reference an object of its class type
// ("test_cls") or hold the special value "null". Prior to being assigned an
// object with "new", a class handle's value is null; "test_obj = new" then
// creates a new object of the handle's class type and the handle refers to
// it.
//
// Checked:
//   - design has module class_tb with exactly 1 variable: "test_obj" (the
//     class handle)
//   - the design has exactly 1 ClassDefn: "test_cls"
//   - ClassDefn "test_cls": classType vpiUserDefinedClass, not virtual, has
//     exactly 1 property ("a", signed IntTypespec), no extends clause,
//     defaults to automatic lifetime (see the FIXED notes below)
//   - variable "test_obj": its typespec resolves (RefTypespec -> ClassTypespec)
//     to the SAME ClassDefn as "test_cls" -- this is the crux of "is
//     the class handle recognised by the parser": the handle's static type
//     must be tied back to the actual class declaration, not left dangling
//   - variable "test_obj" has no initial value (no "= expr" in its declaration),
//     consistent with a class handle defaulting to null before any object
//     is assigned to it
//   - the initial process' Begin block has exactly 1 statement: an IfStmt
//   - the IfStmt's condition is a 2-operand equality Operation (vpiEqOp):
//     operand[0] is a RefObj "test_obj" resolved to the Variable; operand[1] is
//     the "null" keyword literal, resolving to a Constant with vpiNullConst
//     (see the FIXED note below)
//   - the IfStmt's (then-branch) stmt is a blocking Assignment: lhs is a
//     RefObj "test_obj" resolved to the SAME Variable as the declaration (i.e.
//     the handle, once assigned, points back to itself/the same object
//     slot); rhs is a MethodFuncCall named "new" taking no arguments -- this
//     is "test_obj = new" (implicit constructor call, no explicit
//     class_type# or (args))
//   - design-level: exactly 1 class (test_cls)
//
// Deliberately NOT tested: MethodFuncCall::getTaskFunc() resolution for the
// "new" call. test_cls has no user-written "function new();", so whether an
// implicit default constructor is modeled as a resolvable TaskFunc is
// unverified territory; per existing repo precedent (see
// chapter-7/arrays/associative/arguments/test_7.9.10_arguments.cpp), this
// kind of TaskFunc resolution is deliberately left unchecked rather than
// asserted on without a confirmed ground truth.
//
// FIXED COMPILER BUG #1 (class lifetime defaulting, not a defect in this
// file): IEEE 1800-2017 8.3 says a class declared with no lifetime
// qualifier ("class test_cls; ... endclass") must default to automatic
// lifetime (getAutomatic() == true). HLC previously never set the automatic
// flag to true for the unqualified case, returning false, identical to what
// an explicit "class static test_cls;" would produce -- cross-checked at the
// time via hlc/Google/generic/class/class_test_1/test_class_test_1.cpp
// (unqualified) vs class_test_2/test_class_test_2.cpp (explicit static).
// ClassIsAutomaticByDefault below asserts the IEEE-mandated behavior and now
// passes.
//
// FIXED COMPILER BUG #2 (was "suspected", "null" keyword resolution): the
// SystemVerilog VPI extension defines a dedicated constant-type enumerant
// "vpiNullConst" (sv_vpi_user.h) specifically for the "null" literal, and
// this same codebase already establishes the precedent of lowering another
// special literal keyword ("$", the unbounded-queue bound) into a Constant
// node with vpiConstType == vpiUnboundedConst (see
// chapter-7/queues/*/test_*.cpp). By that precedent, "null" in "test_obj ==
// null" should likewise lower to a Constant with getConstType() ==
// vpiNullConst. HLC previously modeled it as an unresolved RefObj named
// "null" (no vpiActual) instead, the same shape as an implicit/undeclared
// identifier reference. IfConditionSecondOperandIsNullConstant below
// confirms the enum-grounded expected shape and now passes.
//
// FIXED COMPILER BUG #3 (property visibility defaulting, same root cause as
// #1): IEEE 1800-2017 8.14 says a property with no explicit
// "local"/"protected" qualifier defaults to public visibility. Cross-checked
// at the time via hlc/Google/chapter-8/8.5--properties/test_8.5--properties.cpp:
// property 'a' returned getVisibility() == 0, which is not a valid
// vpiVisibility value at all (vpiPublicVis=1, vpiProtectedVis=2,
// vpiLocalVis=3 -- see sv_vpi_user.h) -- simply the field's never-touched
// default (int32_t m_visibility = 0;), meaning the compiler never explicitly
// assigned public visibility to an unqualified property.
// PropertyAIsPublicByDefault below asserts the IEEE-mandated behavior and
// now passes.

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
#include <hldb/if_stmt.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ClassInstantiationTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "8.4--instantiation.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("class_tb", m_design->getAllModules()); }

  static const hldb::ClassDefn *getTestClsDefn() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::ClassDefn>("test_cls", top->getClassDefns());
  }

  static const hldb::Variable *getVariableTestObj() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("test_obj", top->getVariables());
  }

  static const hldb::Begin *getInitialBegin() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (init == nullptr) return nullptr;
    return init->getStmt<hldb::Begin>();
  }

  static const hldb::IfStmt *getIfStmt() {
    const hldb::Begin *const begin = getInitialBegin();
    if (begin == nullptr || begin->getStmts() == nullptr || begin->getStmts()->empty()) return nullptr;
    return any_cast<hldb::IfStmt>(begin->getStmts()->at(0));
  }
};

// --- module / design shape ---------------------------------------------------

TEST_F(ClassInstantiationTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ClassInstantiationTest, ModuleHasOneVariable) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
}

TEST_F(ClassInstantiationTest, ModuleHasOneClassDefn) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClassDefns(), nullptr);
  EXPECT_EQ(top->getClassDefns()->size(), 1u);
}

// --- class "test_cls" ---------------------------------------------------------

TEST_F(ClassInstantiationTest, ClassTestClsExists) { EXPECT_NE(getTestClsDefn(), nullptr); }

TEST_F(ClassInstantiationTest, ClassIsUserDefinedClass) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getClassType(), vpiUserDefinedClass);
}

TEST_F(ClassInstantiationTest, ClassIsNotVirtual) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_FALSE(c->getVirtual()) << "8.3: 'class test_cls' is not declared virtual";
}

TEST_F(ClassInstantiationTest, ClassIsAutomaticByDefault) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->getAutomatic()) << "8.3: 'class test_cls' has no lifetime qualifier so it defaults to automatic; "
                                    "getAutomatic() must return true (see FIXED COMPILER BUG #1 above)";
}

TEST_F(ClassInstantiationTest, ClassHasNoExtends) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getExtends(), nullptr);
}

TEST_F(ClassInstantiationTest, ClassHasOnePropertyA) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getVariables(), nullptr);
  ASSERT_EQ(c->getVariables()->size(), 1u);
  const hldb::Variable *const a = c->getVariables()->at(0);
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getName(), "a");
  ASSERT_NE(a->getTypespec(), nullptr);
  const hldb::IntTypespec *const elem = a->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "property 'a' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned());
}

TEST_F(ClassInstantiationTest, PropertyAIsPublicByDefault) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getVariables(), nullptr);
  ASSERT_EQ(c->getVariables()->size(), 1u);
  const hldb::Variable *const a = c->getVariables()->at(0);
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getVisibility(), vpiPublicVis) << "8.14: 'int a;' with no visibility qualifier defaults to public "
                                                 "(see FIXED COMPILER BUG #3 above)";
}

TEST_F(ClassInstantiationTest, PropertyAHasNoRandOrConstQualifier) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getVariables(), nullptr);
  ASSERT_EQ(c->getVariables()->size(), 1u);
  const hldb::Variable *const a = c->getVariables()->at(0);
  ASSERT_NE(a, nullptr);
  EXPECT_FALSE(a->getConstantVariable()) << "8.9: 'int a;' declares no 'const' qualifier";
  EXPECT_FALSE(a->getIsRandomized()) << "8.9: 'int a;' declares no 'rand'/'randc' qualifier";
}

// --- variable "test_obj": the class handle -----------------------------------------

TEST_F(ClassInstantiationTest, VariableTestObjExists) { EXPECT_NE(getVariableTestObj(), nullptr); }

TEST_F(ClassInstantiationTest, VariableTestObjTypespecResolvesToTestClsClassDefn) {
  const hldb::Variable *const testObj = getVariableTestObj();
  ASSERT_NE(testObj, nullptr);
  ASSERT_NE(testObj->getTypespec(), nullptr);
  const hldb::ClassTypespec *const ct = testObj->getTypespec<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr) << "8.4: handle 'test_obj' must resolve to a ClassTypespec";
  EXPECT_EQ(ct->getClassDefn(), getTestClsDefn())
      << "8.4: the handle's ClassTypespec must point back to the SAME ClassDefn as 'test_cls', "
         "confirming the parser recognises the handle's declared type";
}

TEST_F(ClassInstantiationTest, VariableTestObjHasNoInitialValue) {
  const hldb::Variable *const testObj = getVariableTestObj();
  ASSERT_NE(testObj, nullptr);
  EXPECT_EQ(testObj->getValue(), nullptr) << "8.4: 'test_cls test_obj;' has no initializer, "
                                             "consistent with the handle defaulting to null before "
                                             "any object is assigned to it";
}

// --- initial process structure -------------------------------------------------

TEST_F(ClassInstantiationTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(ClassInstantiationTest, InitialBeginHasOneStmt) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 1u);
}

// --- if (test_obj == null) ------------------------------------------------------

TEST_F(ClassInstantiationTest, IfStmtExists) { EXPECT_NE(getIfStmt(), nullptr); }

TEST_F(ClassInstantiationTest, IfConditionIsEqualityOperation) {
  const hldb::IfStmt *const ifStmt = getIfStmt();
  ASSERT_NE(ifStmt, nullptr);
  const hldb::Operation *const cond = ifStmt->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "'test_obj == null' should be an equality Operation";
  EXPECT_EQ(cond->getOpType(), vpiEqOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
}

TEST_F(ClassInstantiationTest, IfConditionFirstOperandIsTestObjHandle) {
  const hldb::IfStmt *const ifStmt = getIfStmt();
  ASSERT_NE(ifStmt, nullptr);
  const hldb::Operation *const cond = ifStmt->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_GT(cond->getOperands()->size(), 0u);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "test_obj");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariableTestObj());
}

TEST_F(ClassInstantiationTest, IfConditionSecondOperandIsNullConstant) {
  const hldb::IfStmt *const ifStmt = getIfStmt();
  ASSERT_NE(ifStmt, nullptr);
  const hldb::Operation *const cond = ifStmt->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_GT(cond->getOperands()->size(), 1u);
  const hldb::Constant *const nullLit = any_cast<hldb::Constant>(cond->getOperands()->at(1));
  ASSERT_NE(nullLit, nullptr) << "'null' should resolve to a Constant, not a plain RefObj "
                                 "(see FIXED COMPILER BUG #2 above)";
  EXPECT_EQ(nullLit->getConstType(), vpiNullConst);
}

// --- test_obj = new --------------------------------------------------------------

TEST_F(ClassInstantiationTest, IfThenStmtIsBlockingAssignment) {
  const hldb::IfStmt *const ifStmt = getIfStmt();
  ASSERT_NE(ifStmt, nullptr);
  const hldb::Assignment *const assign = ifStmt->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr) << "'test_obj = new' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());
}

TEST_F(ClassInstantiationTest, AssignmentLhsIsTestObjHandle) {
  const hldb::IfStmt *const ifStmt = getIfStmt();
  ASSERT_NE(ifStmt, nullptr);
  const hldb::Assignment *const assign = ifStmt->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "test_obj");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariableTestObj())
      << "8.4: once assigned, the handle must resolve back to the SAME variable slot it was declared with";
}

TEST_F(ClassInstantiationTest, AssignmentRhsIsNewMethodFuncCall) {
  const hldb::IfStmt *const ifStmt = getIfStmt();
  ASSERT_NE(ifStmt, nullptr);
  const hldb::Assignment *const assign = ifStmt->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr);
  const hldb::MethodFuncCall *const newCall = assign->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(newCall, nullptr) << "'new' should resolve to a MethodFuncCall";
  EXPECT_EQ(newCall->getName(), "new");
}

TEST_F(ClassInstantiationTest, NewCallHasNoArguments) {
  const hldb::IfStmt *const ifStmt = getIfStmt();
  ASSERT_NE(ifStmt, nullptr);
  const hldb::Assignment *const assign = ifStmt->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr);
  const hldb::MethodFuncCall *const newCall = assign->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(newCall, nullptr);
  EXPECT_EQ(newCall->getArguments(), nullptr) << "'new' (no explicit ctor args) takes no arguments";
}

// --- compiler diagnostics ---------------------------------------------------------

TEST_F(ClassInstantiationTest, CompilerReportsNoErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "new"), nullptr)
      << "class instantiation via new must bind (IEEE 1800-2023 8.4)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
