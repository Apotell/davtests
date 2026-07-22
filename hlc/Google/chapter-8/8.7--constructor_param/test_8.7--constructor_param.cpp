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

// Tests for 8.7--constructor_param.sv (tags: 8.7)
//   module class_tb ();
//     class test_cls;
//       int a;
//       function new(int def = 42);
//         a = def;
//       endfunction
//     endclass
//
//     initial begin
//       test_cls test_obj = new(37);
//
//       $display(":assert:(%d == 37)", test_obj.a);
//     end
//   endmodule
//
// IEEE 1800-2017 8.7 "Constructors": a user-written "new" constructor may
// take arguments like any other function, including a default value for a
// parameter ("int def = 42"). Calling "new(37)" supplies an explicit
// argument that overrides that default -- the source's own
// ":assert:(%d == 37)" records 37 (not the default 42) as the expected
// value once the constructor runs. This file otherwise mirrors
// chapter-8/8.7--constructor/test_8.7--constructor.cpp structurally: a
// block-scoped local "test_obj" with an inline "= new(...)" initializer,
// same known bugs expected to reproduce here (see below).
//
// Checked:
//   - design has module work@class_tb with NO module-level nets or
//     variables -- "test_obj" is block-scoped inside the initial process
//   - the module has exactly 1 nested ClassDefn: "work@test_cls"
//   - ClassDefn "test_cls": classType vpiUserDefinedClass, has exactly 1
//     property ("a") and exactly 1 method ("new", a Function) -- see the
//     KNOWN COMPILER BUG notes below for the class's lifetime and the
//     property's visibility
//   - constructor "new": resolves to a Function, correctly marked public
//     by default, return typespec resolves to the SAME ClassDefn as
//     "test_cls" itself
//   - constructor has exactly 1 IODecl ("def", direction input, signed
//     IntTypespec) whose OWN default-value expression (IODecl::getExpr())
//     is a Constant decompiling to "42" -- the parameter's default, as
//     distinct from any value supplied at a call site
//   - the constructor's body is a single blocking Assignment (not wrapped
//     in a Begin): lhs RefObj "a" resolved to the SAME Variable as the
//     class's property; rhs RefObj "def" resolved to the SAME IODecl as
//     the constructor's own parameter
//   - the initial process' Begin block has exactly 1 local Variable
//     ("test_obj") and exactly 1 statement (the $display)
//   - local Variable "test_obj": typespec resolves to the SAME ClassDefn
//     as "test_cls"; its getValue() is a MethodFuncCall named "new" taking
//     exactly 1 Constant argument "37" (the explicit override, not the
//     IODecl's default of 42) -- see the KNOWN COMPILER BUG note below for
//     whether this call resolves back to the constructor
//   - "$display(...)" has 2 arguments: a Constant string
//     ":assert:(%d == 37)", and a HierPath "test_obj.a" with 2 path elems
//     (RefObj "test_obj" resolved to the local Variable; RefObj "a"
//     resolved to the class's property Variable)
//   - design-level: exactly 1 class (work@test_cls)
//
// KNOWN COMPILER BUG #1 (class lifetime defaulting, not a defect in this
// file): IEEE 1800-2017 8.3 says a class declared with no lifetime
// qualifier must default to automatic lifetime (getAutomatic() == true).
// Already confirmed independently via multiple other chapter-8 files (see
// hlc/Google/chapter-8/8.4--instantiation/test_8.4--instantiation.cpp and
// siblings, including
// hlc/Google/chapter-8/8.7--constructor/test_8.7--constructor.cpp).
// ClassIsAutomaticByDefault below asserts the IEEE-mandated behavior and
// will FAIL until this is fixed.
//
// KNOWN COMPILER BUG #2 (property visibility defaulting, not a defect in
// this file): IEEE 1800-2017 8.14 says a property with no explicit
// "local"/"protected" qualifier defaults to public visibility. Already
// confirmed independently via
// hlc/Google/chapter-8/8.5--properties/test_8.5--properties.cpp and
// hlc/Google/chapter-8/8.7--constructor/test_8.7--constructor.cpp.
// PropertyAIsPublicByDefault below asserts the IEEE-mandated behavior and
// will FAIL until this is fixed.
//
// KNOWN COMPILER BUG #3 (constructor call resolution, not a defect in this
// file): confirmed via ctest in
// hlc/Google/chapter-8/8.7--constructor/test_8.7--constructor.cpp that a
// "new" call site's getTaskFunc() never resolves back to the actual
// user-written constructor Function, even though the constructor itself is
// correctly declared. LocalTestObjNewCallResolvesToConstructor below
// re-checks the same gap with an argument-passing constructor and is
// expected to FAIL the same way.

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
#include <hldb/function.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ClassConstructorParamTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "8.7--constructor_param.hlc"}); }
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

  static const hldb::Function *getConstructorFunction() {
    const hldb::ClassDefn *const c = getTestClsDefn();
    if (c == nullptr || c->getMethods() == nullptr || c->getMethods()->empty()) return nullptr;
    return any_cast<hldb::Function>(c->getMethods()->at(0));
  }

  static const hldb::IODecl *getDefIODecl() {
    const hldb::Function *const ctor = getConstructorFunction();
    if (ctor == nullptr || ctor->getIODecls() == nullptr || ctor->getIODecls()->empty()) return nullptr;
    return ctor->getIODecls()->at(0);
  }

  static const hldb::Begin *getInitialBegin() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (init == nullptr) return nullptr;
    return init->getStmt<hldb::Begin>();
  }

  static const hldb::Variable *getLocalTestObj() {
    const hldb::Begin *const begin = getInitialBegin();
    if (begin == nullptr || begin->getVariables() == nullptr || begin->getVariables()->empty()) return nullptr;
    return begin->getVariables()->at(0);
  }
};

// --- module / design shape ---------------------------------------------------

TEST_F(ClassConstructorParamTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ClassConstructorParamTest, ModuleHasNoTopLevelNetsOrVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const bool hasNets = top->getNets() != nullptr && !top->getNets()->empty();
  const bool hasVariables = top->getVariables() != nullptr && !top->getVariables()->empty();
  EXPECT_FALSE(hasNets) << "'test_obj' is block-scoped inside the initial process, not a module-level net";
  EXPECT_FALSE(hasVariables) << "'test_obj' is block-scoped inside the initial process, not a module-level variable";
}

TEST_F(ClassConstructorParamTest, ModuleHasOneClassDefn) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClassDefns(), nullptr);
  EXPECT_EQ(top->getClassDefns()->size(), 1u);
}

// --- class "test_cls" ---------------------------------------------------------

TEST_F(ClassConstructorParamTest, ClassTestClsExists) { EXPECT_NE(getTestClsDefn(), nullptr); }

TEST_F(ClassConstructorParamTest, ClassIsUserDefinedClass) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getClassType(), vpiUserDefinedClass);
}

TEST_F(ClassConstructorParamTest, ClassIsAutomaticByDefault) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->getAutomatic()) << "8.3: 'class test_cls' has no lifetime qualifier so it defaults to automatic; "
                                    "getAutomatic() must return true (see KNOWN COMPILER BUG #1 above)";
}

TEST_F(ClassConstructorParamTest, ClassHasOnePropertyA) {
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

TEST_F(ClassConstructorParamTest, PropertyAIsPublicByDefault) {
  const hldb::Variable *const a = getPropertyA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getVisibility(), vpiPublicVis) << "8.14: 'int a;' with no visibility qualifier defaults to public "
                                                 "(see KNOWN COMPILER BUG #2 above)";
}

TEST_F(ClassConstructorParamTest, ClassHasOneConstructorFunction) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getMethods(), nullptr);
  ASSERT_EQ(c->getMethods()->size(), 1u);
  const hldb::Function *const ctor = getConstructorFunction();
  ASSERT_NE(ctor, nullptr) << "'function new(int def = 42)' should resolve to a Function";
  EXPECT_EQ(ctor->getName(), "new");
}

TEST_F(ClassConstructorParamTest, ConstructorIsPublicByDefault) {
  const hldb::Function *const ctor = getConstructorFunction();
  ASSERT_NE(ctor, nullptr);
  EXPECT_EQ(ctor->getVisibility(), vpiPublicVis)
      << "8.14: 'function new(...)' with no visibility qualifier defaults to public";
}

TEST_F(ClassConstructorParamTest, ConstructorReturnsTestClsType) {
  const hldb::Function *const ctor = getConstructorFunction();
  ASSERT_NE(ctor, nullptr);
  ASSERT_NE(ctor->getReturn(), nullptr);
  const hldb::ClassTypespec *const ct = ctor->getReturn<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr) << "8.7: a constructor implicitly returns the class's own type";
  EXPECT_EQ(ct->getClassDefn(), getTestClsDefn());
}

TEST_F(ClassConstructorParamTest, ConstructorHasOneIODeclDefWithDefaultValue42) {
  const hldb::Function *const ctor = getConstructorFunction();
  ASSERT_NE(ctor, nullptr);
  ASSERT_NE(ctor->getIODecls(), nullptr);
  ASSERT_EQ(ctor->getIODecls()->size(), 1u);
  const hldb::IODecl *const def = getDefIODecl();
  ASSERT_NE(def, nullptr);
  EXPECT_EQ(def->getName(), "def");
  EXPECT_EQ(def->getDirection(), vpiInput);
  ASSERT_NE(def->getTypespec(), nullptr);
  const hldb::IntTypespec *const elem = def->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "IODecl 'def' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned());

  const hldb::Constant *const defaultValue = def->getExpr<hldb::Constant>();
  ASSERT_NE(defaultValue, nullptr) << "'int def = 42' should attach '42' as the IODecl's own default-value expr";
  EXPECT_EQ(defaultValue->getDecompile(), "42");
}

TEST_F(ClassConstructorParamTest, ConstructorBodyAssignsPropertyAFromDef) {
  const hldb::Function *const ctor = getConstructorFunction();
  ASSERT_NE(ctor, nullptr);
  const hldb::Assignment *const assign = ctor->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr) << "'a = def;' (the constructor's only statement) should be an Assignment, "
                                "not wrapped in a Begin";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "a");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getPropertyA());

  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "'def' on the rhs should be a RefObj";
  EXPECT_EQ(rhs->getName(), "def");
  EXPECT_EQ(rhs->getActual<hldb::IODecl>(), getDefIODecl())
      << "'def' inside the constructor body must resolve to the SAME IODecl as the constructor's own parameter";
}

// --- initial process / local "test_obj" ----------------------------------------

TEST_F(ClassConstructorParamTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(ClassConstructorParamTest, InitialBeginHasOneLocalVariableTestObj) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getVariables(), nullptr);
  ASSERT_EQ(begin->getVariables()->size(), 1u);
  const hldb::Variable *const testObj = getLocalTestObj();
  ASSERT_NE(testObj, nullptr);
  EXPECT_EQ(testObj->getName(), "test_obj");
}

TEST_F(ClassConstructorParamTest, InitialBeginHasOneStmt) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 1u) << "the local variable declaration itself is not counted as a "
                                              "statement -- only the $display is";
}

TEST_F(ClassConstructorParamTest, LocalTestObjTypespecResolvesToTestClsClassDefn) {
  const hldb::Variable *const testObj = getLocalTestObj();
  ASSERT_NE(testObj, nullptr);
  ASSERT_NE(testObj->getTypespec(), nullptr);
  const hldb::ClassTypespec *const ct = testObj->getTypespec<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr) << "local 'test_obj' must resolve to a ClassTypespec";
  EXPECT_EQ(ct->getClassDefn(), getTestClsDefn());
}

TEST_F(ClassConstructorParamTest, LocalTestObjValueIsNewCallWithArgThirtySeven) {
  const hldb::Variable *const testObj = getLocalTestObj();
  ASSERT_NE(testObj, nullptr);
  const hldb::MethodFuncCall *const newCall = testObj->getValue<hldb::MethodFuncCall>();
  ASSERT_NE(newCall, nullptr) << "'= new(37)' should be attached as the Variable's own getValue()";
  EXPECT_EQ(newCall->getName(), "new");
  ASSERT_NE(newCall->getArguments(), nullptr);
  ASSERT_EQ(newCall->getArguments()->size(), 1u);
  const hldb::Constant *const arg = any_cast<hldb::Constant>(newCall->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getDecompile(), "37") << "the call site's explicit argument (37) must be distinct from and "
                                          "not confused with the IODecl's own default (42)";
}

TEST_F(ClassConstructorParamTest, LocalTestObjNewCallResolvesToConstructor) {
  const hldb::Variable *const testObj = getLocalTestObj();
  ASSERT_NE(testObj, nullptr);
  const hldb::MethodFuncCall *const newCall = testObj->getValue<hldb::MethodFuncCall>();
  ASSERT_NE(newCall, nullptr);
  EXPECT_EQ(newCall->getTaskFunc<hldb::Function>(), getConstructorFunction())
      << "8.7: 'new(37)' must resolve back to the SAME user-written 'function new(int def = 42)' declared on "
         "test_cls (see KNOWN COMPILER BUG #3 above)";
}

// --- $display(":assert:(%d == 37)", test_obj.a) --------------------------------

TEST_F(ClassConstructorParamTest, DisplayExistsWithTwoArguments) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  ASSERT_GT(begin->getStmts()->size(), 0u);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(0));
  ASSERT_NE(disp, nullptr) << "stmt[0] should be a $display SysFuncCall";
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  EXPECT_EQ(disp->getArguments()->size(), 2u);
}

TEST_F(ClassConstructorParamTest, DisplayFirstArgIsAssertStringLiteral) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 0u);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(0));
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_GT(disp->getArguments()->size(), 0u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert:(%d == 37)");
}

TEST_F(ClassConstructorParamTest, DisplaySecondArgIsTestObjDotA) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 0u);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(0));
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_GT(disp->getArguments()->size(), 1u);
  const hldb::HierPath *const path = any_cast<hldb::HierPath>(disp->getArguments()->at(1));
  ASSERT_NE(path, nullptr) << "'test_obj.a' should be a HierPath";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);

  const hldb::RefObj *const testObjRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(testObjRef, nullptr);
  EXPECT_EQ(testObjRef->getName(), "test_obj");
  EXPECT_EQ(testObjRef->getActual<hldb::Variable>(), getLocalTestObj());

  const hldb::RefObj *const aRef = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
  ASSERT_NE(aRef, nullptr);
  EXPECT_EQ(aRef->getName(), "a");
  EXPECT_EQ(aRef->getActual<hldb::Variable>(), getPropertyA());
}

// --- compiler diagnostics ---------------------------------------------------------

TEST_F(ClassConstructorParamTest, CompilerReportsNoErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
