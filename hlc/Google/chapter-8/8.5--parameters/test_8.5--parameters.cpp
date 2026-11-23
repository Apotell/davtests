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

// Tests for 8.5--parameters.sv (tags: 8.5 8.25)
//   module class_tb ();
//     class test_cls #(parameter a = 12);
//     endclass
//
//     test_cls #(34) test_obj;
//
//     initial begin
//       test_obj = new;
//       $display(":assert:(%d == 34)", test_obj.a);
//     end
//   endmodule
//
// IEEE 1800-2017 8.25 "Parameterized classes": a class may declare one or
// more parameters in its own parameter port list ("#(parameter a = 12)").
// A handle declared with an explicit parameter value list ("test_cls #(34)
// test_obj") specializes the class for that handle: within that
// specialization, "a" is bound to 34, not the class's own default of 12 --
// the source's own ":assert:(%d == 34)" comment records this as the
// expected, spec-correct value.
//
// Checked:
//   - design has module class_tb with exactly 1 variable: "test_obj"
//     (the class handle, modeled as a Variable, same as the unparameterized
//     handle in chapter-8/8.4--instantiation.sv)
//   - the module holds 2 ClassDefns: "test_cls" as written, and the
//     specialization of it for "#(34)" (static elaboration: a
//     specialization is a separate class, parented next to the class it
//     was made from)
//   - ClassDefn "test_cls": classType vpiUserDefinedClass, has exactly 1
//     parameter ("a", not a localparam) whose class-level default
//     (Parameter::getExpr()) is Constant "12", defaults to automatic lifetime (see
//     the FIXED COMPILER BUG #1 note below)
//   - variable "test_obj": its typespec resolves (RefTypespec ->
//     ClassTypespec) to the specialization of "test_cls" for "#(34)", whose
//     own "a" holds 34; the override's name is bound to that "a"
//     (23.10.2.2: a parameter of the class the typespec resolves to)
//   - the initial process' Begin block has exactly 2 statements: an
//     Assignment ("test_obj = new") and a $display SysFuncCall
//   - "test_obj = new": blocking Assignment, lhs RefObj "test_obj" resolved
//     to the Variable, rhs MethodFuncCall "new" taking no arguments
//   - "$display(...)" has 2 arguments: a Constant string
//     ":assert:(%d == 34)", and a RefObj "test_obj.a" with 2 path elems
//     (RefObj "test_obj" resolved to the Variable; RefObj "a" resolved to
//     the specialization's own Parameter "a", the one holding 34), with
//     the 34-override also on test_obj's own ClassTypespec ParamAssigns
//     (see the FIXED COMPILER BUG #2 note below)
//
// FIXED COMPILER BUG #1 (class lifetime defaulting, not a defect in this
// file): IEEE 1800-2017 8.3 says a class declared with no lifetime
// qualifier must default to automatic lifetime (getAutomatic() == true).
// HLC previously never set the automatic flag to true for the unqualified
// case -- cross-checked at the time via
// hlc/Google/generic/class/class_test_1/test_class_test_1.cpp and
// hlc/Google/chapter-8/8.4--instantiation/test_8.4--instantiation.cpp.
// ClassIsAutomaticByDefault below asserts the IEEE-mandated behavior and now
// passes.
//
// FIXED COMPILER BUG #2 (parameter-override tracking, not a defect in this
// file): IEEE 1800-2017 8.25 specialization means "test_cls #(34) test_obj"
// binds this handle's "a" to 34, distinct from the class's own default of
// 12 (confirmed by the source's own ":assert:(%d == 34)" expectation). HLC
// previously left the ClassTypespec reached through test_obj's typespec with
// getParamAssigns() == nullptr -- no ParamAssign overriding "a" to 34 was
// attached there (a typespec carries only the overrides written); the "#(34)"
// override parsed and elaborated without any compiler error, but was not
// tracked anywhere reachable from test_obj's HLDB typespec.
// VariableTestObjTypespecParamAssignsReflectOverride below asserts the
// IEEE-mandated specialization and now passes.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <algorithm>

namespace hlc {

class ClassParametersTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "8.5--parameters.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByDefName<hldb::Module>("class_tb", m_design->getAllModules());
  }

  static const hldb::ClassDefn *getTestClsDefn() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByDefName<hldb::ClassDefn>("test_cls", top->getClassDefns());
  }

  // The class test_obj's typespec resolves to: the specialization of test_cls for '#(34)'.
  static const hldb::ClassDefn *getTestObjClass() {
    const hldb::Variable *const testObj = getVariableTestObj();
    if ((testObj == nullptr) || (testObj->getTypespec() == nullptr)) return nullptr;
    const hldb::ClassTypespec *const ct = testObj->getTypespec()->getActual<hldb::ClassTypespec>();
    return (ct == nullptr) ? nullptr : ct->getClassDefn();
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

  static const hldb::Assignment *getAssignmentStmt() {
    const hldb::Begin *const begin = getInitialBegin();
    if (begin == nullptr || begin->getStmts() == nullptr || begin->getStmts()->empty()) return nullptr;
    return any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  }

  static const hldb::SysTaskCall *getDisplayStmt() {
    const hldb::Begin *const begin = getInitialBegin();
    if (begin == nullptr || begin->getStmts() == nullptr || begin->getStmts()->size() < 2) return nullptr;
    return any_cast<hldb::SysTaskCall>(begin->getStmts()->at(1));
  }
};

// --- module / design shape ---------------------------------------------------

TEST_F(ClassParametersTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ClassParametersTest, ModuleHasOneVariable) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
}

TEST_F(ClassParametersTest, ModuleHasTestClsAndItsSpecialization) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClassDefns(), nullptr);
  ASSERT_EQ(top->getClassDefns()->size(), 2u) << "test_cls, and its specialization for '#(34)'";
  ASSERT_NE(getTestClsDefn(), nullptr);
  EXPECT_FALSE(getTestClsDefn()->getIsSpecialization());
  ASSERT_NE(getTestObjClass(), nullptr);
  EXPECT_NE(getTestObjClass(), getTestClsDefn());
  EXPECT_NE(std::find(top->getClassDefns()->cbegin(), top->getClassDefns()->cend(), getTestObjClass()),
            top->getClassDefns()->cend())
      << "a specialization is parented next to the class it was made from";
}

// --- class "test_cls" ---------------------------------------------------------

TEST_F(ClassParametersTest, ClassTestClsExists) { EXPECT_NE(getTestClsDefn(), nullptr); }

TEST_F(ClassParametersTest, ClassIsUserDefinedClass) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getClassType(), vpiUserDefinedClass);
}

TEST_F(ClassParametersTest, ClassIsAutomaticByDefault) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->getAutomatic()) << "8.3: 'class test_cls #(parameter a = 12)' has no lifetime qualifier so it "
                                    "defaults to automatic; getAutomatic() must return true (same FIXED COMPILER "
                                    "BUG #1 documented in chapter-8/8.4--instantiation.sv and class_test_1.sv)";
}

TEST_F(ClassParametersTest, ClassHasOneParameterA) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getParameters(), nullptr);
  ASSERT_EQ(c->getParameters()->size(), 1u);
  const hldb::Parameter *const a = any_cast<hldb::Parameter>(c->getParameters()->at(0));
  ASSERT_NE(a, nullptr) << "8.25: 'parameter a = 12' should resolve to a Parameter";
  EXPECT_EQ(a->getName(), "a");
  EXPECT_FALSE(a->getLocalParam()) << "8.25: 'a' is declared with 'parameter', not 'localparam'";
}

TEST_F(ClassParametersTest, ClassParameterADefaultAssignIsTwelve) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getParameters(), nullptr);
  ASSERT_EQ(c->getParameters()->size(), 1u);
  const hldb::Parameter *const pa = any_cast<hldb::Parameter>(c->getParameters()->at(0));
  ASSERT_NE(pa, nullptr);
  EXPECT_EQ(pa->getName(), "a");
  const hldb::Constant *const rhs = pa->getExpr<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "8.25: the class-level default for 'a' should be Constant '12'";
  EXPECT_EQ(rhs->getDecompile(), "12");
}

// --- variable "test_obj": the parameterized-class handle ----------------------

TEST_F(ClassParametersTest, VariableTestObjExists) { EXPECT_NE(getVariableTestObj(), nullptr); }

TEST_F(ClassParametersTest, VariableTestObjTypespecResolvesToTestClsClassDefn) {
  const hldb::Variable *const testObj = getVariableTestObj();
  ASSERT_NE(testObj, nullptr);
  ASSERT_NE(testObj->getTypespec(), nullptr);
  const hldb::ClassTypespec *const ct = testObj->getTypespec<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr) << "8.4/8.25: handle 'test_obj' must resolve to a ClassTypespec";

  const hldb::ClassDefn *const cd = ct->getClassDefn();
  ASSERT_NE(cd, nullptr);
  EXPECT_TRUE(cd->getIsSpecialization()) << "8.25: '#(34)' gives 'a' a value other than its default";
  EXPECT_EQ(cd->getSpecializedFrom(), getTestClsDefn());
  EXPECT_NE(cd, getTestClsDefn()) << "8.25: a specialization is a class type of its own";
}

TEST_F(ClassParametersTest, SpecializationParameterAIsThirtyFour) {
  const hldb::ClassDefn *const cd = getTestObjClass();
  ASSERT_NE(cd, nullptr);
  ASSERT_NE(cd->getParameters(), nullptr);
  ASSERT_EQ(cd->getParameters()->size(), 1u);
  const hldb::Parameter *const a = hldb::findByName<hldb::Parameter>("a", cd->getParameters());
  ASSERT_NE(a, nullptr) << "the specialization declares its own 'a'";
  ASSERT_NE(getTestClsDefn(), nullptr);
  EXPECT_NE(a, hldb::findByName<hldb::Parameter>("a", getTestClsDefn()->getParameters()));
  const hldb::Constant *const value = a->getExpr<hldb::Constant>();
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getDecompile(), "34") << "8.25: in test_cls #(34), 'a' is 34";
}

TEST_F(ClassParametersTest, OverrideNameBindsToSpecializationParameterA) {
  const hldb::Variable *const testObj = getVariableTestObj();
  ASSERT_NE(testObj, nullptr);
  ASSERT_NE(testObj->getTypespec(), nullptr);
  const hldb::ClassTypespec *const ct = testObj->getTypespec()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr);
  ASSERT_NE(ct->getParamAssigns(), nullptr);
  ASSERT_EQ(ct->getParamAssigns()->size(), 1u);
  const hldb::RefObj *const lhs = ct->getParamAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "a") << "23.10.2.1: the first ordered value goes to 'a'";
  ASSERT_NE(getTestObjClass(), nullptr);
  EXPECT_EQ(lhs->getActual(), hldb::findByName<hldb::Parameter>("a", getTestObjClass()->getParameters()))
      << "23.10.2.2: a parameter of the class the typespec resolves to";
}

// FIXED COMPILER BUG #2: the per-handle override is now tracked -- see the
// file-level comment above.
TEST_F(ClassParametersTest, VariableTestObjTypespecParamAssignsReflectOverride) {
  const hldb::Variable *const testObj = getVariableTestObj();
  ASSERT_NE(testObj, nullptr);
  ASSERT_NE(testObj->getTypespec(), nullptr);
  const hldb::ClassTypespec *const ct = testObj->getTypespec<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr);
  const hldb::ParamAssignCollection *const paramAssigns = ct->getParamAssigns();
  ASSERT_NE(paramAssigns, nullptr) << "8.25: 'test_cls #(34)' should attach a ParamAssign overriding 'a' "
                                      "on test_obj's OWN ClassTypespec (distinct from the class's default)";
  ASSERT_EQ(paramAssigns->size(), 1u);
  const hldb::ParamAssign *const pa = paramAssigns->at(0);
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "34") << "8.25: test_obj's specialization must override 'a' to 34, not "
                                          "reuse the class's default of 12";
  EXPECT_FALSE(pa->getConnByName()) << "'#(34)' is a positional (ordered) parameter value assignment, not by-name";
}

// --- test_obj = new --------------------------------------------------------------

TEST_F(ClassParametersTest, AssignmentIsBlocking) {
  const hldb::Assignment *const assign = getAssignmentStmt();
  ASSERT_NE(assign, nullptr) << "'test_obj = new' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());
}

TEST_F(ClassParametersTest, AssignmentLhsIsTestObjHandle) {
  const hldb::Assignment *const assign = getAssignmentStmt();
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "test_obj");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariableTestObj());
}

TEST_F(ClassParametersTest, AssignmentRhsIsNewMethodFuncCall) {
  const hldb::Assignment *const assign = getAssignmentStmt();
  ASSERT_NE(assign, nullptr);
  const hldb::MethodFuncCall *const newCall = assign->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(newCall, nullptr) << "'new' should resolve to a MethodFuncCall";
  EXPECT_EQ(newCall->getName(), "new");
  EXPECT_EQ(newCall->getArguments(), nullptr);
}

// --- $display(":assert:(%d == 34)", test_obj.a) -------------------------------

TEST_F(ClassParametersTest, DisplayExistsWithTwoArguments) {
  const hldb::SysTaskCall *const disp = getDisplayStmt();
  ASSERT_NE(disp, nullptr) << "stmt[1] should be a $display SysFuncCall";
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  EXPECT_EQ(disp->getArguments()->size(), 2u);
}

TEST_F(ClassParametersTest, DisplayFirstArgIsAssertStringLiteral) {
  const hldb::SysTaskCall *const disp = getDisplayStmt();
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_GT(disp->getArguments()->size(), 0u);
  const hldb::NamedArgument *const arg0 = disp->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const fmt = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert:(%d == 34)");
}

TEST_F(ClassParametersTest, DisplaySecondArgIsTestObjDotA) {
  const hldb::SysTaskCall *const disp = getDisplayStmt();
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_GT(disp->getArguments()->size(), 1u);
  const hldb::NamedArgument *const arg1 = disp->getArguments()->at(1);
  ASSERT_NE(arg1, nullptr);
  const hldb::RefObj *const path = arg1->getHighConn<hldb::RefObj>();
  ASSERT_NE(path, nullptr) << "'test_obj.a' should be a RefObj";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);

  const hldb::RefObj *const testObjRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(testObjRef, nullptr);
  EXPECT_EQ(testObjRef->getName(), "test_obj");
  EXPECT_EQ(testObjRef->getActual<hldb::Variable>(), getVariableTestObj());

  const hldb::RefObj *const aRef = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
  ASSERT_NE(aRef, nullptr);
  EXPECT_EQ(aRef->getName(), "a");
  ASSERT_NE(getTestObjClass(), nullptr);
  EXPECT_EQ(aRef->getActual(), hldb::findByName<hldb::Parameter>("a", getTestObjClass()->getParameters()))
      << "8.25: test_obj is a test_cls #(34), so 'test_obj.a' is that specialization's 'a' (34)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
