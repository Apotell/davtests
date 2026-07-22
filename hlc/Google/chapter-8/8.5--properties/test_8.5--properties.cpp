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

// Tests for 8.5--properties.sv (tags: 8.5)
//   module class_tb ();
//     class test_cls;
//       int a;
//     endclass
//
//     test_cls test_obj;
//
//     initial begin
//       test_obj = new;
//
//       test_obj.a = 12;
//
//       $display(":assert:(%d == 12)", test_obj.a);
//     end
//   endmodule
//
// IEEE 1800-2017 8.5 "Properties" / 8.4 "Object creation": once a handle
// has been assigned an object ("test_obj = new"), its data properties are
// read and written through the handle using the "." member-access operator
// ("test_obj.a = 12", "test_obj.a"). Both the write and the later read must
// resolve "a" back to the SAME property declared in test_cls.
//
// Checked:
//   - design has module work@class_tb with exactly 1 net: "test_obj" (the
//     class handle, unparameterized so modeled as a Net, matching
//     chapter-8/8.4--instantiation.sv)
//   - the module has exactly 1 nested ClassDefn: "work@test_cls"
//   - ClassDefn "test_cls": classType vpiUserDefinedClass, has exactly 1
//     property ("a", signed IntTypespec) -- see the KNOWN COMPILER BUG note
//     below for its lifetime (automatic-by-default)
//   - net "test_obj": its typespec resolves (RefTypespec -> ClassTypespec)
//     to the SAME ClassDefn as "work@test_cls"
//   - the initial process' Begin block has exactly 3 statements:
//     "test_obj = new", "test_obj.a = 12", and a $display
//   - "test_obj = new": blocking Assignment, lhs RefObj "test_obj" resolved
//     to the Net, rhs MethodFuncCall "new" taking no arguments
//   - "test_obj.a = 12": blocking Assignment whose LHS is a HierPath (not a
//     plain RefObj, since it addresses a property through a handle) with 2
//     path elems (RefObj "test_obj" resolved to the Net; RefObj "a"
//     resolved to the class's Variable "a"), and whose rhs is Constant "12"
//   - "$display(...)" has 2 arguments: a Constant string
//     ":assert:(%d == 12)", and a HierPath "test_obj.a" whose second path
//     elem resolves to the SAME Variable "a" object as the write above --
//     confirming the write and the later read agree on which property they
//     address
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
// KNOWN COMPILER BUG #2 (property visibility defaulting, new finding,
// confirmed via ctest): IEEE 1800-2017 8.14 says a property with no
// explicit "local"/"protected" qualifier defaults to public visibility.
// Confirmed: Variable::getVisibility() for 'a' returns 0, which is not a
// valid vpiVisibility value at all (vpiPublicVis=1, vpiProtectedVis=2,
// vpiLocalVis=3 -- see sv_vpi_user.h) -- it is simply the field's
// never-touched default (int32_t m_visibility = 0;), meaning the compiler
// never explicitly assigns public visibility to an unqualified property.
// Same category of bug as #1: a source-level default that elaboration
// never fills in. PropertyAIsPublicByDefault below asserts the
// IEEE-mandated behavior and will FAIL until this is fixed.

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
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ClassPropertiesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "8.5--properties.hlc"}); }
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

  static const hldb::Begin *getInitialBegin() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (init == nullptr) return nullptr;
    return init->getStmt<hldb::Begin>();
  }

  static const hldb::Assignment *getAssignmentStmt(size_t index) {
    const hldb::Begin *const begin = getInitialBegin();
    if (begin == nullptr || begin->getStmts() == nullptr || begin->getStmts()->size() <= index) return nullptr;
    return any_cast<hldb::Assignment>(begin->getStmts()->at(index));
  }

  static const hldb::SysTaskCall *getDisplayStmt() {
    const hldb::Begin *const begin = getInitialBegin();
    if (begin == nullptr || begin->getStmts() == nullptr || begin->getStmts()->size() < 3) return nullptr;
    return any_cast<hldb::SysTaskCall>(begin->getStmts()->at(2));
  }
};

// --- module / design shape ---------------------------------------------------

TEST_F(ClassPropertiesTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ClassPropertiesTest, ModuleHasOneNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(ClassPropertiesTest, ModuleHasOneClassDefn) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClassDefns(), nullptr);
  EXPECT_EQ(top->getClassDefns()->size(), 1u);
}

// --- class "test_cls" ---------------------------------------------------------

TEST_F(ClassPropertiesTest, ClassTestClsExists) { EXPECT_NE(getTestClsDefn(), nullptr); }

TEST_F(ClassPropertiesTest, ClassIsUserDefinedClass) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getClassType(), vpiUserDefinedClass);
}

TEST_F(ClassPropertiesTest, ClassIsAutomaticByDefault) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->getAutomatic()) << "8.3: 'class test_cls' has no lifetime qualifier so it defaults to automatic; "
                                    "getAutomatic() must return true (see KNOWN COMPILER BUG note above)";
}

TEST_F(ClassPropertiesTest, ClassHasOnePropertyA) {
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

TEST_F(ClassPropertiesTest, PropertyAIsPublicByDefault) {
  const hldb::Variable *const a = getPropertyA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getVisibility(), vpiPublicVis) << "8.14: 'int a;' with no visibility qualifier defaults to public";
}

// --- net "test_obj": the class handle ------------------------------------------

TEST_F(ClassPropertiesTest, NetTestObjExists) { EXPECT_NE(getNetTestObj(), nullptr); }

TEST_F(ClassPropertiesTest, NetTestObjTypespecResolvesToTestClsClassDefn) {
  const hldb::Net *const testObj = getNetTestObj();
  ASSERT_NE(testObj, nullptr);
  ASSERT_NE(testObj->getTypespec(), nullptr);
  const hldb::ClassTypespec *const ct = testObj->getTypespec<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr) << "handle 'test_obj' must resolve to a ClassTypespec";
  EXPECT_EQ(ct->getClassDefn(), getTestClsDefn())
      << "the handle's ClassTypespec must point back to the SAME ClassDefn as 'test_cls'";
}

// --- initial process structure -------------------------------------------------

TEST_F(ClassPropertiesTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(ClassPropertiesTest, InitialBeginHasThreeStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 3u);
}

// --- test_obj = new --------------------------------------------------------------

TEST_F(ClassPropertiesTest, FirstStmtIsBlockingAssignment) {
  const hldb::Assignment *const assign = getAssignmentStmt(0);
  ASSERT_NE(assign, nullptr) << "'test_obj = new' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());
}

TEST_F(ClassPropertiesTest, FirstStmtLhsIsTestObjHandle) {
  const hldb::Assignment *const assign = getAssignmentStmt(0);
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "test_obj");
  EXPECT_EQ(lhs->getActual<hldb::Net>(), getNetTestObj());
}

TEST_F(ClassPropertiesTest, FirstStmtRhsIsNewMethodFuncCall) {
  const hldb::Assignment *const assign = getAssignmentStmt(0);
  ASSERT_NE(assign, nullptr);
  const hldb::MethodFuncCall *const newCall = assign->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(newCall, nullptr) << "'new' should resolve to a MethodFuncCall";
  EXPECT_EQ(newCall->getName(), "new");
  EXPECT_EQ(newCall->getArguments(), nullptr);
}

// --- test_obj.a = 12 --------------------------------------------------------------

TEST_F(ClassPropertiesTest, SecondStmtIsBlockingAssignment) {
  const hldb::Assignment *const assign = getAssignmentStmt(1);
  ASSERT_NE(assign, nullptr) << "'test_obj.a = 12' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());
}

TEST_F(ClassPropertiesTest, SecondStmtLhsIsTestObjDotA) {
  const hldb::Assignment *const assign = getAssignmentStmt(1);
  ASSERT_NE(assign, nullptr);
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
  EXPECT_EQ(aRef->getActual<hldb::Variable>(), getPropertyA())
      << "the write target must resolve 'a' to the SAME Variable declared on test_cls";
}

TEST_F(ClassPropertiesTest, SecondStmtRhsIsTwelve) {
  const hldb::Assignment *const assign = getAssignmentStmt(1);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "12");
}

// --- $display(":assert:(%d == 12)", test_obj.a) -------------------------------

TEST_F(ClassPropertiesTest, DisplayExistsWithTwoArguments) {
  const hldb::SysTaskCall *const disp = getDisplayStmt();
  ASSERT_NE(disp, nullptr) << "stmt[2] should be a $display SysFuncCall";
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  EXPECT_EQ(disp->getArguments()->size(), 2u);
}

TEST_F(ClassPropertiesTest, DisplayFirstArgIsAssertStringLiteral) {
  const hldb::SysTaskCall *const disp = getDisplayStmt();
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_GT(disp->getArguments()->size(), 0u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert:(%d == 12)");
}

TEST_F(ClassPropertiesTest, DisplaySecondArgIsTestObjDotAMatchingTheEarlierWrite) {
  const hldb::SysTaskCall *const disp = getDisplayStmt();
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_GT(disp->getArguments()->size(), 1u);
  const hldb::HierPath *const path = any_cast<hldb::HierPath>(disp->getArguments()->at(1));
  ASSERT_NE(path, nullptr) << "'test_obj.a' (read) should be a HierPath";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);

  const hldb::RefObj *const testObjRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(testObjRef, nullptr);
  EXPECT_EQ(testObjRef->getName(), "test_obj");
  EXPECT_EQ(testObjRef->getActual<hldb::Net>(), getNetTestObj());

  const hldb::RefObj *const aRef = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
  ASSERT_NE(aRef, nullptr);
  EXPECT_EQ(aRef->getName(), "a");
  EXPECT_EQ(aRef->getActual<hldb::Variable>(), getPropertyA())
      << "the read must resolve 'a' to the SAME Variable as the earlier write, confirming the parser "
         "treats both accesses as addressing the same property";
}

// --- compiler diagnostics ---------------------------------------------------------

TEST_F(ClassPropertiesTest, CompilerReportsNoErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
