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

// Tests for 8.8--typed_constructor.sv (tags: 8.8)
//   module class_tb ();
//     class super_cls;
//       int s = 2;
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
//     test_cls super_obj;
//
//     initial begin
//       super_obj = test_cls::new;
//
//       $display(super_obj.s);
//     end
//   endmodule
//
// IEEE 1800-2017 8.8 "Typed constructor calls": "test_cls::new" is a
// class-scope-qualified constructor call with NO parameter specialization
// and NO explicit arguments (the constructor's own default "def = 42"
// applies). This file isolates that base case from
// chapter-8/8.8--typed_constructor_param/test_8.8--typed_constructor_param.cpp,
// which combines the same scoped-call syntax with a named parameter
// override and a named argument -- and, unlike that file, raises no
// compiler error, see the KNOWN COMPILER BUG note below for the important
// distinction.
//
// Checked:
//   - design has module work@class_tb with exactly 1 net: "super_obj"
//   - the module has exactly 2 nested ClassDefns: "work@super_cls" and
//     "work@test_cls"
//   - ClassDefn "super_cls": 1 property ("s", initializer "2"), 1
//     constructor ("new(int def = 3)")
//   - ClassDefn "test_cls": extends super_cls (confirmed via getExtends());
//     1 property ("a"), 1 constructor ("new(int def = 42)")
//   - the test_cls constructor's body: "super.new(def + 3);" (resolves
//     correctly, matching
//     chapter-8/8.7--constructor_super/test_8.7--constructor_super.cpp)
//     and "a = def;"
//   - net "super_obj": declared as "test_cls super_obj;" -- its typespec
//     resolves to test_cls's OWN ClassDefn (not super_cls's; despite the
//     variable's name, its declared type is the derived class)
//   - the initial process' Begin block has exactly 2 statements: the
//     "super_obj = test_cls::new" assignment and a $display
//   - "super_obj = test_cls::new": blocking Assignment, lhs RefObj
//     "super_obj" resolved to the Net, rhs MethodFuncCall "new" taking no
//     arguments -- see the KNOWN COMPILER BUG note below for how its scope
//     and constructor-call resolve
//   - "$display(super_obj.s)": HierPath resolving "s" to super_cls's own
//     property Variable (inherited-property access through the derived
//     handle, matching chapter-8/8.7--constructor_super.sv)
//   - design-level: exactly 2 classes
//
// KNOWN COMPILER BUG #1 (class lifetime defaulting) and KNOWN COMPILER BUG
// #2 (property visibility defaulting): already confirmed independently
// across every other chapter-8 file in this suite.
// SuperClsIsAutomaticByDefault, TestClsIsAutomaticByDefault, and
// SuperClsPropertySIsPublicByDefault / TestClsPropertyAIsPublicByDefault
// below assert the IEEE-mandated behavior and will FAIL until these are
// fixed.
//
// KNOWN COMPILER BUG #3 (typed constructor call, refined finding): unlike
// hlc/Google/chapter-8/8.8--typed_constructor_param/test_8.8--typed_constructor_param.cpp
// (where the SAME scoped-call syntax, combined with a named parameter
// override "#(.t(23))", raises an ACTUAL compiler error), the bare
// "test_cls::new" here compiles with 0 errors -- so the error in that
// sibling file comes specifically from the parameter-override handling,
// not from class-scope qualification on its own. However, the underlying
// resolution gap is still present here, just silent rather than erroring:
// the scope "test_cls" is still modeled as an UnsupportedScope whose
// getActual() does not resolve to test_cls's ClassDefn, and the "new"
// call's getTaskFunc() does not resolve to test_cls's constructor (the
// same call-resolution gap already confirmed for ordinary "new" calls in
// hlc/Google/chapter-8/8.7--constructor/test_8.7--constructor.cpp).
// AssignmentRhsScopeResolvesToTestClsClassDefn and
// AssignmentRhsNewCallResolvesToTestConstructor below assert the
// IEEE-mandated/spec-correct shape and are expected to FAIL, while
// CompilerReportsNoErrors is expected to PASS (a genuine contrast with the
// "_param" sibling file).

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
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/unsupported_scope.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ClassTypedConstructorTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "8.8--typed_constructor.hlc"}); }
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

  static const hldb::Net *getNetSuperObj() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Net>("super_obj", top->getNets());
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
};

// --- module / design shape ---------------------------------------------------

TEST_F(ClassTypedConstructorTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ClassTypedConstructorTest, ModuleHasOneNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(ClassTypedConstructorTest, ModuleHasTwoClassDefns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClassDefns(), nullptr);
  EXPECT_EQ(top->getClassDefns()->size(), 2u);
}

// --- class "super_cls" ---------------------------------------------------------

TEST_F(ClassTypedConstructorTest, SuperClsExists) { EXPECT_NE(getSuperClsDefn(), nullptr); }

TEST_F(ClassTypedConstructorTest, SuperClsIsAutomaticByDefault) {
  const hldb::ClassDefn *const c = getSuperClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->getAutomatic()) << "8.3: 'class super_cls' has no lifetime qualifier so it defaults to "
                                    "automatic (see KNOWN COMPILER BUG #1 above)";
}

TEST_F(ClassTypedConstructorTest, SuperClsHasOnePropertySWithInitializerTwo) {
  const hldb::ClassDefn *const c = getSuperClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getVariables(), nullptr);
  ASSERT_EQ(c->getVariables()->size(), 1u);
  const hldb::Variable *const s = getSuperPropertyS();
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->getName(), "s");
  const hldb::Constant *const init = s->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getDecompile(), "2");
}

TEST_F(ClassTypedConstructorTest, SuperClsPropertySIsPublicByDefault) {
  const hldb::Variable *const s = getSuperPropertyS();
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->getVisibility(), vpiPublicVis) << "8.14: 'int s = 2;' with no visibility qualifier defaults to "
                                                 "public (see KNOWN COMPILER BUG #2 above)";
}

TEST_F(ClassTypedConstructorTest, SuperClsHasOneConstructor) {
  const hldb::ClassDefn *const c = getSuperClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getMethods(), nullptr);
  ASSERT_EQ(c->getMethods()->size(), 1u);
  const hldb::Function *const ctor = getSuperConstructor();
  ASSERT_NE(ctor, nullptr);
  EXPECT_EQ(ctor->getName(), "new");
}

// --- class "test_cls extends super_cls" -----------------------------------------

TEST_F(ClassTypedConstructorTest, TestClsExists) { EXPECT_NE(getTestClsDefn(), nullptr); }

TEST_F(ClassTypedConstructorTest, TestClsIsAutomaticByDefault) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->getAutomatic()) << "8.3: 'class test_cls extends super_cls' has no lifetime qualifier so it "
                                    "defaults to automatic (see KNOWN COMPILER BUG #1 above)";
}

TEST_F(ClassTypedConstructorTest, TestClsExtendsSuperCls) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  const hldb::Extends *const ext = c->getExtends();
  ASSERT_NE(ext, nullptr);
  ASSERT_NE(ext->getClassTypespecs(), nullptr);
  ASSERT_EQ(ext->getClassTypespecs()->size(), 1u);
  const hldb::ClassTypespec *const baseCt = ext->getClassTypespecs()->at(0)->getActual<hldb::ClassTypespec>();
  ASSERT_NE(baseCt, nullptr);
  EXPECT_EQ(baseCt->getClassDefn(), getSuperClsDefn());
}

TEST_F(ClassTypedConstructorTest, TestClsHasOnePropertyA) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getVariables(), nullptr);
  ASSERT_EQ(c->getVariables()->size(), 1u);
  const hldb::Variable *const a = getTestPropertyA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getName(), "a");
}

TEST_F(ClassTypedConstructorTest, TestClsPropertyAIsPublicByDefault) {
  const hldb::Variable *const a = getTestPropertyA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getVisibility(), vpiPublicVis) << "8.14: 'int a;' with no visibility qualifier defaults to public "
                                                 "(see KNOWN COMPILER BUG #2 above)";
}

TEST_F(ClassTypedConstructorTest, TestClsHasOneConstructor) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getMethods(), nullptr);
  ASSERT_EQ(c->getMethods()->size(), 1u);
  const hldb::Function *const ctor = getTestConstructor();
  ASSERT_NE(ctor, nullptr);
  EXPECT_EQ(ctor->getName(), "new");
}

TEST_F(ClassTypedConstructorTest, TestConstructorHasOneIODeclDefWithDefault42) {
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

TEST_F(ClassTypedConstructorTest, TestConstructorBodyHasTwoStmts) {
  const hldb::Function *const ctor = getTestConstructor();
  ASSERT_NE(ctor, nullptr);
  const hldb::Begin *const body = ctor->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 2u);
}

TEST_F(ClassTypedConstructorTest, TestConstructorFirstStmtIsSuperNewCall) {
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
  ASSERT_NE(superRef, nullptr);
  EXPECT_EQ(superRef->getActual<hldb::ClassDefn>(), getSuperClsDefn());

  const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(path->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "'super.new(...)' should resolve to a FuncCall";
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), getSuperConstructor())
      << "matches the already-confirmed-working 'super.new' resolution in "
         "chapter-8/8.7--constructor_super/test_8.7--constructor_super.cpp";
}

TEST_F(ClassTypedConstructorTest, TestConstructorSecondStmtAssignsAFromDef) {
  const hldb::Function *const ctor = getTestConstructor();
  ASSERT_NE(ctor, nullptr);
  const hldb::Begin *const body = ctor->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_GT(body->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(1));
  ASSERT_NE(assign, nullptr) << "'a = def;' should be an Assignment";

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

// --- net "super_obj" (declared as "test_cls super_obj;") -------------------------

TEST_F(ClassTypedConstructorTest, NetSuperObjExists) { EXPECT_NE(getNetSuperObj(), nullptr); }

TEST_F(ClassTypedConstructorTest, NetSuperObjTypespecResolvesToTestClsClassDefn) {
  const hldb::Net *const superObj = getNetSuperObj();
  ASSERT_NE(superObj, nullptr);
  ASSERT_NE(superObj->getTypespec(), nullptr);
  const hldb::ClassTypespec *const ct = superObj->getTypespec<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr) << "8.8: 'test_cls super_obj;' declares a handle of the DERIVED class type, despite "
                            "the variable's name";
  EXPECT_EQ(ct->getClassDefn(), getTestClsDefn());
}

// --- initial process structure -------------------------------------------------

TEST_F(ClassTypedConstructorTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(ClassTypedConstructorTest, InitialBeginHasTwoStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 2u);
}

// --- super_obj = test_cls::new ----------------------------------------------------

TEST_F(ClassTypedConstructorTest, AssignmentIsBlockingWithLhsSuperObj) {
  const hldb::Assignment *const assign = getAssignmentStmt();
  ASSERT_NE(assign, nullptr) << "'super_obj = test_cls::new' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "super_obj");
  EXPECT_EQ(lhs->getActual<hldb::Net>(), getNetSuperObj());
}

TEST_F(ClassTypedConstructorTest, AssignmentRhsIsNewMethodFuncCallWithNoArguments) {
  const hldb::Assignment *const assign = getAssignmentStmt();
  ASSERT_NE(assign, nullptr);
  const hldb::MethodFuncCall *const newCall = assign->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(newCall, nullptr) << "'test_cls::new' should resolve to a MethodFuncCall";
  EXPECT_EQ(newCall->getName(), "new");
  EXPECT_EQ(newCall->getArguments(), nullptr) << "bare 'new' (no parens) takes no arguments";
}

TEST_F(ClassTypedConstructorTest, AssignmentRhsScopeResolvesToTestClsClassDefn) {
  const hldb::Assignment *const assign = getAssignmentStmt();
  ASSERT_NE(assign, nullptr);
  const hldb::MethodFuncCall *const newCall = assign->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(newCall, nullptr);
  const hldb::UnsupportedScope *const scope = newCall->getScope<hldb::UnsupportedScope>();
  ASSERT_NE(scope, nullptr) << "'test_cls::' is currently modeled as an UnsupportedScope (see KNOWN COMPILER "
                               "BUG #3 above)";
  EXPECT_EQ(scope->getName(), "test_cls");
  EXPECT_EQ(scope->getActual<hldb::ClassDefn>(), getTestClsDefn())
      << "8.8: 'test_cls::' must resolve back to the SAME ClassDefn as 'test_cls' (see KNOWN COMPILER BUG #3 "
         "above)";
}

TEST_F(ClassTypedConstructorTest, AssignmentRhsNewCallResolvesToTestConstructor) {
  const hldb::Assignment *const assign = getAssignmentStmt();
  ASSERT_NE(assign, nullptr);
  const hldb::MethodFuncCall *const newCall = assign->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(newCall, nullptr);
  EXPECT_EQ(newCall->getTaskFunc<hldb::Function>(), getTestConstructor())
      << "8.8: 'test_cls::new' must resolve back to test_cls's constructor (see KNOWN COMPILER BUG #3 above, "
         "consistent with the ordinary 'new' resolution gap in "
         "chapter-8/8.7--constructor/test_8.7--constructor.cpp)";
}

// --- $display(super_obj.s) -------------------------------------------------------

TEST_F(ClassTypedConstructorTest, DisplayArgIsSuperObjDotS) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 1u);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(1));
  ASSERT_NE(disp, nullptr) << "stmt[1] should be a $display SysFuncCall";
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);
  const hldb::HierPath *const path = any_cast<hldb::HierPath>(disp->getArguments()->at(0));
  ASSERT_NE(path, nullptr) << "'super_obj.s' should be a HierPath";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);

  const hldb::RefObj *const superObjRef = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(superObjRef, nullptr);
  EXPECT_EQ(superObjRef->getActual<hldb::Net>(), getNetSuperObj());

  const hldb::RefObj *const sRef = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
  ASSERT_NE(sRef, nullptr);
  EXPECT_EQ(sRef->getName(), "s");
  EXPECT_EQ(sRef->getActual<hldb::Variable>(), getSuperPropertyS())
      << "8.17: inherited property access through the derived-class handle";
}

// --- compiler diagnostics ---------------------------------------------------------

TEST_F(ClassTypedConstructorTest, CompilerReportsNoErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0) << "unlike "
                                 "chapter-8/8.8--typed_constructor_param/test_8.8--typed_constructor_param.cpp, "
                                 "the bare 'test_cls::new' (no parameter override) is expected to compile "
                                 "cleanly";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
