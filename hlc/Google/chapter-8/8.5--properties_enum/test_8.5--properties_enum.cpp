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

// Tests for 8.5--properties_enum.sv (tags: 8.5)
//   module class_tb ();
//     class test_cls;
//       typedef enum {A = 10, B = 20, C = 30, D = 1} e_type;
//     endclass
//
//     test_cls test_obj;
//
//     initial begin
//       test_obj = new;
//       $display(test_obj.C);
//     end
//   endmodule
//
// IEEE 1800-2017 6.19 "Enumerations" (enum declared as a class property) and
// 8.4 "Object creation": an anonymous enumerated type, given a name via
// "typedef ... e_type", may be declared inside a class body; each of its
// enumeration constants (A, B, C, D) becomes a member accessible through a
// handle of that class ("test_obj.C"), the same way a data property would
// be.
//
// Checked:
//   - design has module work@class_tb with exactly 1 net: "test_obj" (the
//     class handle -- unparameterized, so modeled as a Net here, matching
//     chapter-8/8.4--instantiation.sv rather than the Variable seen for the
//     parameterized handle in chapter-8/8.5--parameters.sv)
//   - the module has exactly 1 nested ClassDefn: "work@test_cls"
//   - ClassDefn "test_cls": classType vpiUserDefinedClass -- see the KNOWN
//     COMPILER BUG note below for its lifetime (automatic-by-default)
//   - ClassDefn "test_cls" has exactly 2 typespecs: an anonymous
//     EnumTypespec (4 EnumConsts: A=10, B=20, C=30, D=1, in source order)
//     and a TypedefTypespec "e_type" whose alias resolves to that SAME
//     EnumTypespec
//   - scope containment: neither the EnumTypespec nor the TypedefTypespec
//     also appears in the enclosing module's own typespec list -- they
//     stay correctly scoped to the class, not leaked to module scope
//   - net "test_obj": its typespec resolves (RefTypespec -> ClassTypespec)
//     to the SAME ClassDefn as "work@test_cls"
//   - the initial process' Begin block has exactly 2 statements: an
//     Assignment ("test_obj = new") and a $display SysFuncCall
//   - "test_obj = new": blocking Assignment, lhs RefObj "test_obj" resolved
//     to the Net, rhs MethodFuncCall "new" taking no arguments
//   - "$display(test_obj.C)" has exactly 1 argument (no format string, only
//     the enum-constant reference): a HierPath "test_obj.C" with 2 path
//     elems (RefObj "test_obj" resolved to the Net; RefObj "C" resolved to
//     the SAME EnumConst "C" found on the class's EnumTypespec)
//   - design-level: exactly 1 class (work@test_cls)
//
// KNOWN COMPILER BUG (class lifetime defaulting, not a defect in this
// file): IEEE 1800-2017 8.3 says a class declared with no lifetime
// qualifier must default to automatic lifetime (getAutomatic() == true).
// This HLC build never sets the automatic flag to true for the unqualified
// case. Already confirmed independently via
// hlc/Google/generic/class/class_test_1/test_class_test_1.cpp,
// hlc/Google/chapter-8/8.4--instantiation/test_8.4--instantiation.cpp and
// hlc/Google/chapter-8/8.5--parameters/test_8.5--parameters.cpp (all fail
// the analogous check). ClassIsAutomaticByDefault below asserts the
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
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/typedef_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ClassPropertiesEnumTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "8.5--properties_enum.hlc"}); }
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

  static const hldb::EnumTypespec *getEnumTypespec() {
    const hldb::ClassDefn *const c = getTestClsDefn();
    if (c == nullptr || c->getTypespecs() == nullptr) return nullptr;
    for (const hldb::Typespec *const ts : *c->getTypespecs()) {
      const hldb::EnumTypespec *const et = any_cast<hldb::EnumTypespec>(ts);
      if (et != nullptr) return et;
    }
    return nullptr;
  }

  static const hldb::TypedefTypespec *getTypedefTypespec() {
    const hldb::ClassDefn *const c = getTestClsDefn();
    if (c == nullptr || c->getTypespecs() == nullptr) return nullptr;
    for (const hldb::Typespec *const ts : *c->getTypespecs()) {
      const hldb::TypedefTypespec *const tt = any_cast<hldb::TypedefTypespec>(ts);
      if (tt != nullptr) return tt;
    }
    return nullptr;
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

  static const hldb::SysFuncCall *getDisplayStmt() {
    const hldb::Begin *const begin = getInitialBegin();
    if (begin == nullptr || begin->getStmts() == nullptr || begin->getStmts()->size() < 2) return nullptr;
    return any_cast<hldb::SysFuncCall>(begin->getStmts()->at(1));
  }

  // Verifies the EnumTypespec's enumConsts[index] is named "name" and has a
  // Constant value that decompiles to "value".
  static void ExpectEnumConst(size_t index, std::string_view name, std::string_view value) {
    const hldb::EnumTypespec *const et = getEnumTypespec();
    ASSERT_NE(et, nullptr);
    ASSERT_NE(et->getEnumConsts(), nullptr);
    ASSERT_GT(et->getEnumConsts()->size(), index);
    const hldb::EnumConst *const ec = et->getEnumConsts()->at(index);
    ASSERT_NE(ec, nullptr);
    EXPECT_EQ(ec->getName(), name);
    const hldb::Constant *const val = ec->getValue<hldb::Constant>();
    ASSERT_NE(val, nullptr) << "enum constant '" << name << "' should have a Constant value";
    EXPECT_EQ(val->getDecompile(), value);
  }
};

// --- module / design shape ---------------------------------------------------

TEST_F(ClassPropertiesEnumTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ClassPropertiesEnumTest, ModuleHasOneNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(ClassPropertiesEnumTest, ModuleHasOneClassDefn) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClassDefns(), nullptr);
  EXPECT_EQ(top->getClassDefns()->size(), 1u);
}

// --- class "test_cls" ---------------------------------------------------------

TEST_F(ClassPropertiesEnumTest, ClassTestClsExists) { EXPECT_NE(getTestClsDefn(), nullptr); }

TEST_F(ClassPropertiesEnumTest, ClassIsUserDefinedClass) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getClassType(), vpiUserDefinedClass);
}

TEST_F(ClassPropertiesEnumTest, ClassIsAutomaticByDefault) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->getAutomatic()) << "8.3: 'class test_cls' has no lifetime qualifier so it defaults to automatic; "
                                    "getAutomatic() must return true (see KNOWN COMPILER BUG note above)";
}

TEST_F(ClassPropertiesEnumTest, ClassHasTwoTypespecs) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getTypespecs(), nullptr);
  EXPECT_EQ(c->getTypespecs()->size(), 2u);
}

// --- the anonymous enum and its "e_type" typedef ------------------------------

TEST_F(ClassPropertiesEnumTest, EnumTypespecExists) { EXPECT_NE(getEnumTypespec(), nullptr); }

TEST_F(ClassPropertiesEnumTest, EnumTypespecHasFourEnumConsts) {
  const hldb::EnumTypespec *const et = getEnumTypespec();
  ASSERT_NE(et, nullptr);
  ASSERT_NE(et->getEnumConsts(), nullptr);
  EXPECT_EQ(et->getEnumConsts()->size(), 4u);
}

TEST_F(ClassPropertiesEnumTest, EnumConstAIsTen) { ExpectEnumConst(0, "A", "10"); }
TEST_F(ClassPropertiesEnumTest, EnumConstBIsTwenty) { ExpectEnumConst(1, "B", "20"); }
TEST_F(ClassPropertiesEnumTest, EnumConstCIsThirty) { ExpectEnumConst(2, "C", "30"); }
TEST_F(ClassPropertiesEnumTest, EnumConstDIsOne) { ExpectEnumConst(3, "D", "1"); }

TEST_F(ClassPropertiesEnumTest, TypedefETypeExists) { EXPECT_NE(getTypedefTypespec(), nullptr); }

TEST_F(ClassPropertiesEnumTest, TypedefETypeNameIsEType) {
  const hldb::TypedefTypespec *const tt = getTypedefTypespec();
  ASSERT_NE(tt, nullptr);
  EXPECT_EQ(tt->getName(), "e_type");
}

TEST_F(ClassPropertiesEnumTest, TypedefETypeAliasesTheEnumTypespec) {
  const hldb::TypedefTypespec *const tt = getTypedefTypespec();
  ASSERT_NE(tt, nullptr);
  ASSERT_NE(tt->getTypedefAlias(), nullptr);
  EXPECT_EQ(tt->getTypedefAlias<hldb::RefTypespec>()->getActual<hldb::EnumTypespec>(), getEnumTypespec())
      << "'typedef enum {...} e_type' must alias the SAME EnumTypespec declared alongside it";
}

// "e_type" and its enum constants are class-scoped members: declared inside
// test_cls's body, they must stay nested under the ClassDefn's own
// typespecs and NOT also appear directly in the enclosing module's scope
// (module scope should only ever see them through a handle, e.g.
// "test_obj.C" -- never as a bare module-level typespec). This guards
// against the enum/typedef being scoped too broadly (leaked to module or
// design level) rather than correctly contained within the class.
TEST_F(ClassPropertiesEnumTest, ModuleScopeDoesNotContainClassEnumOrTypedef) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::EnumTypespec *const et = getEnumTypespec();
  const hldb::TypedefTypespec *const tt = getTypedefTypespec();
  ASSERT_NE(et, nullptr);
  ASSERT_NE(tt, nullptr);
  if (top->getTypespecs() == nullptr) return;
  for (const hldb::Typespec *const ts : *top->getTypespecs()) {
    EXPECT_NE(ts, static_cast<const hldb::Typespec *>(et))
        << "the class-scoped EnumTypespec must not also appear in the module's own typespec list";
    EXPECT_NE(ts, static_cast<const hldb::Typespec *>(tt))
        << "the class-scoped TypedefTypespec 'e_type' must not also appear in the module's own typespec list";
  }
}

// --- net "test_obj": the class handle ------------------------------------------

TEST_F(ClassPropertiesEnumTest, NetTestObjExists) { EXPECT_NE(getNetTestObj(), nullptr); }

TEST_F(ClassPropertiesEnumTest, NetTestObjTypespecResolvesToTestClsClassDefn) {
  const hldb::Net *const testObj = getNetTestObj();
  ASSERT_NE(testObj, nullptr);
  ASSERT_NE(testObj->getTypespec(), nullptr);
  const hldb::ClassTypespec *const ct = testObj->getTypespec<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr) << "handle 'test_obj' must resolve to a ClassTypespec";
  EXPECT_EQ(ct->getClassDefn(), getTestClsDefn())
      << "the handle's ClassTypespec must point back to the SAME ClassDefn as 'test_cls'";
}

// --- initial process structure -------------------------------------------------

TEST_F(ClassPropertiesEnumTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(ClassPropertiesEnumTest, InitialBeginHasTwoStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 2u);
}

// --- test_obj = new --------------------------------------------------------------

TEST_F(ClassPropertiesEnumTest, AssignmentIsBlocking) {
  const hldb::Assignment *const assign = getAssignmentStmt();
  ASSERT_NE(assign, nullptr) << "'test_obj = new' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());
}

TEST_F(ClassPropertiesEnumTest, AssignmentLhsIsTestObjHandle) {
  const hldb::Assignment *const assign = getAssignmentStmt();
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "test_obj");
  EXPECT_EQ(lhs->getActual<hldb::Net>(), getNetTestObj());
}

TEST_F(ClassPropertiesEnumTest, AssignmentRhsIsNewMethodFuncCall) {
  const hldb::Assignment *const assign = getAssignmentStmt();
  ASSERT_NE(assign, nullptr);
  const hldb::MethodFuncCall *const newCall = assign->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(newCall, nullptr) << "'new' should resolve to a MethodFuncCall";
  EXPECT_EQ(newCall->getName(), "new");
  EXPECT_EQ(newCall->getArguments(), nullptr);
}

// --- $display(test_obj.C) -------------------------------------------------------

TEST_F(ClassPropertiesEnumTest, DisplayExistsWithOneArgument) {
  const hldb::SysFuncCall *const disp = getDisplayStmt();
  ASSERT_NE(disp, nullptr) << "stmt[1] should be a $display SysFuncCall";
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  EXPECT_EQ(disp->getArguments()->size(), 1u) << "'$display(test_obj.C)' takes no format string, just the reference";
}

TEST_F(ClassPropertiesEnumTest, DisplayArgIsTestObjDotC) {
  const hldb::SysFuncCall *const disp = getDisplayStmt();
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_GT(disp->getArguments()->size(), 0u);
  const hldb::HierPath *const path = any_cast<hldb::HierPath>(disp->getArguments()->at(0));
  ASSERT_NE(path, nullptr) << "'test_obj.C' should be a HierPath";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);

  const hldb::RefObj *const testObjRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(testObjRef, nullptr);
  EXPECT_EQ(testObjRef->getName(), "test_obj");
  EXPECT_EQ(testObjRef->getActual<hldb::Net>(), getNetTestObj());

  const hldb::RefObj *const cRef = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
  ASSERT_NE(cRef, nullptr);
  EXPECT_EQ(cRef->getName(), "C");
  const hldb::EnumTypespec *const et = getEnumTypespec();
  ASSERT_NE(et, nullptr);
  ASSERT_NE(et->getEnumConsts(), nullptr);
  ASSERT_GT(et->getEnumConsts()->size(), 2u);
  EXPECT_EQ(cRef->getActual<hldb::EnumConst>(), et->getEnumConsts()->at(2))
      << "'test_obj.C' must resolve back to the SAME EnumConst 'C' declared on the class's enum";
}

// --- compiler diagnostics ---------------------------------------------------------

TEST_F(ClassPropertiesEnumTest, CompilerReportsNoErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
