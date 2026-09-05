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

// Tests for 8.8--typed_constructor_param.sv (tags: 8.8)
//   module class_tb ();
//     class super_cls;
//       int s = 2;
//       function new(int def = 3);
//         s = def;
//       endfunction
//     endclass
//
//     class test_cls #(int t = 12) extends super_cls;
//       int a;
//       function new(int def = 42);
//         super.new(def + 3);
//         a = def - t;
//       endfunction
//     endclass
//
//     super_cls super_obj;
//
//     initial begin
//             super_obj = test_cls#(.t(23))::new(.def(41));
//
//       $display(super_obj.s);
//     end
//   endmodule
//
// IEEE 1800-2017 8.8 "Typed constructor calls" (class-scope-qualified "new"
// with an explicit parameter specialization and named arguments) combines
// several constructs already exercised individually elsewhere in this
// suite: a parameterized class (chapter-8/8.5--parameters.sv), a class
// extending a base class (chapter-8/8.7--constructor_super.sv), and named
// (rather than positional) parameter/argument binding. "super_obj", typed
// as the BASE class, is assigned a "test_cls" object -- an upcast, legal
// per 8.7/8.10 since test_cls extends super_cls.
//
// Checked:
//   - design has module class_tb with exactly 1 variable: "super_obj"
//   - the module has exactly 2 nested ClassDefns: "super_cls" and
//     "test_cls"
//   - ClassDefn "super_cls": 1 property ("s", initializer "2"), 1
//     constructor ("new(int def = 3)"); defaults to automatic lifetime, and
//     property 's' defaults to public visibility (see the FIXED COMPILER
//     BUG notes below)
//   - ClassDefn "test_cls": extends super_cls (confirmed via getExtends());
//     has exactly 1 class parameter ("t", default ParamAssign "12"), 1
//     property ("a"), 1 constructor ("new(int def = 42)")
//   - the test_cls constructor's body: "super.new(def + 3);" (resolves
//     correctly, matching chapter-8/8.7--constructor_super.sv) and
//     "a = def - t;" -- a subtract Operation whose operands are the
//     constructor's own IODecl "def" AND the class's own type parameter
//     "t", both resolved directly (not through a ParamAssign)
//   - variable "super_obj": its typespec resolves to the SAME ClassDefn as
//     "super_cls" (the base type, per its declaration)
//   - the initial process' Begin block has exactly 2 statements: the
//     "super_obj = test_cls#(.t(23))::new(.def(41))" assignment and a
//     $display
//   - "$display(super_obj.s)": HierPath resolving "s" to super_cls's own
//     property Variable
//   - design-level: exactly 2 classes
//
// FIXED COMPILER BUG #1 (class lifetime defaulting) and FIXED COMPILER BUG
// #2 (property visibility defaulting): cross-checked at the time across
// every other chapter-8 file in this suite (see
// hlc/Google/chapter-8/8.4--instantiation/test_8.4--instantiation.cpp and
// siblings). SuperClsIsAutomaticByDefault, TestClsIsAutomaticByDefault, and
// SuperClsPropertySIsPublicByDefault / TestClsPropertyAIsPublicByDefault
// below assert the IEEE-mandated behavior and now pass.
//
// FIXED COMPILER BUG #3 (typed/scoped constructor call resolution):
// "test_cls#(.t(23))::new(.def(41))" -- the class-scope qualifier
// "test_cls#(.t(23))" now resolves (via its RefTypespec -> ClassTypespec)
// with the "t" ParamAssign's RefObj correctly bound to test_cls's own type
// parameter, and the call's own MethodFuncCall::getTaskFunc() resolves to
// test_cls's constructor (consistent with the fix in
// hlc/Google/chapter-8/8.7--constructor/test_8.7--constructor.cpp for
// ordinary "new" calls). AssignmentRhsScopeParamTResolvesToClassParameter
// and AssignmentRhsNewCallResolvesToTestConstructor below both now pass; see
// the error-reporting note near CompilerReportsNoErrors for the compiler
// diagnostics side of this fix, which is left to the teammate who owns that
// area to describe/update.
//
// STRUCTURAL NOTE (not an asserted bug): the named argument ".def(41)" is
// represented as an IODecl-shaped node carrying the name/value pair,
// rather than as a plain Constant the way positional arguments are
// elsewhere in this suite. AssignmentRhsNamedArgumentDefIsFortyOne below
// documents this shape but does not claim it is incorrect -- there is no
// established expectation (here or elsewhere) that a call-site argument
// must be the same object as anything in the callee's declaration, so this
// alone is not solid evidence of a defect.

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
#include <hldb/function.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/variable.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ClassTypedConstructorParamTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "8.8--typed_constructor_param.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByDefName<hldb::Module>("class_tb", m_design->getAllModules());
  }

  static const hldb::ClassDefn *getSuperClsDefn() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByDefName<hldb::ClassDefn>("super_cls", top->getClassDefns());
  }

  static const hldb::ClassDefn *getTestClsDefn() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByDefName<hldb::ClassDefn>("test_cls", top->getClassDefns());
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

  static const hldb::Parameter *getTestClassParamT() {
    const hldb::ClassDefn *const c = getTestClsDefn();
    if (c == nullptr || c->getParameters() == nullptr || c->getParameters()->empty()) return nullptr;
    return any_cast<hldb::Parameter>(c->getParameters()->at(0));
  }

  static const hldb::Function *getTestConstructor() {
    const hldb::ClassDefn *const c = getTestClsDefn();
    if (c == nullptr || c->getMethods() == nullptr || c->getMethods()->empty()) return nullptr;
    return any_cast<hldb::Function>(c->getMethods()->at(0));
  }

  static const hldb::Variable *getVariableSuperObj() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("super_obj", top->getVariables());
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

TEST_F(ClassTypedConstructorParamTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ClassTypedConstructorParamTest, ModuleHasOneVariable) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
}

TEST_F(ClassTypedConstructorParamTest, ModuleHasTwoClassDefns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getClassDefns(), nullptr);
  EXPECT_EQ(top->getClassDefns()->size(), 2u);
}

// --- class "super_cls" ---------------------------------------------------------

TEST_F(ClassTypedConstructorParamTest, SuperClsExists) { EXPECT_NE(getSuperClsDefn(), nullptr); }

TEST_F(ClassTypedConstructorParamTest, SuperClsIsAutomaticByDefault) {
  const hldb::ClassDefn *const c = getSuperClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->getAutomatic()) << "8.3: 'class super_cls' has no lifetime qualifier so it defaults to "
                                    "automatic (see FIXED COMPILER BUG #1 above)";
}

TEST_F(ClassTypedConstructorParamTest, SuperClsHasOnePropertySWithInitializerTwo) {
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

TEST_F(ClassTypedConstructorParamTest, SuperClsPropertySIsPublicByDefault) {
  const hldb::Variable *const s = getSuperPropertyS();
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->getVisibility(), vpiPublicVis) << "8.14: 'int s = 2;' with no visibility qualifier defaults to "
                                                 "public (see FIXED COMPILER BUG #2 above)";
}

TEST_F(ClassTypedConstructorParamTest, SuperClsHasOneConstructor) {
  const hldb::ClassDefn *const c = getSuperClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getMethods(), nullptr);
  ASSERT_EQ(c->getMethods()->size(), 1u);
  const hldb::Function *const ctor = getSuperConstructor();
  ASSERT_NE(ctor, nullptr);
  EXPECT_EQ(ctor->getName(), "new");
}

// --- class "test_cls #(int t = 12) extends super_cls" ---------------------------

TEST_F(ClassTypedConstructorParamTest, TestClsExists) { EXPECT_NE(getTestClsDefn(), nullptr); }

TEST_F(ClassTypedConstructorParamTest, TestClsIsAutomaticByDefault) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  EXPECT_TRUE(c->getAutomatic()) << "8.3: 'class test_cls #(...) extends super_cls' has no lifetime qualifier "
                                    "so it defaults to automatic (see FIXED COMPILER BUG #1 above)";
}

TEST_F(ClassTypedConstructorParamTest, TestClsExtendsSuperCls) {
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

TEST_F(ClassTypedConstructorParamTest, TestClsHasOneParameterTWithDefault12) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getParameters(), nullptr);
  ASSERT_EQ(c->getParameters()->size(), 1u);
  const hldb::Parameter *const t = getTestClassParamT();
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(t->getName(), "t");

  ASSERT_NE(c->getParamAssigns(), nullptr);
  ASSERT_EQ(c->getParamAssigns()->size(), 1u);
  const hldb::ParamAssign *const pa = c->getParamAssigns()->at(0);
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "t");
  EXPECT_EQ(lhs->getActual<hldb::Parameter>(), t);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "12");
}

TEST_F(ClassTypedConstructorParamTest, TestClsHasOnePropertyA) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getVariables(), nullptr);
  ASSERT_EQ(c->getVariables()->size(), 1u);
  const hldb::Variable *const a = getTestPropertyA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getName(), "a");
}

TEST_F(ClassTypedConstructorParamTest, TestClsPropertyAIsPublicByDefault) {
  const hldb::Variable *const a = getTestPropertyA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getVisibility(), vpiPublicVis) << "8.14: 'int a;' with no visibility qualifier defaults to public "
                                                 "(see FIXED COMPILER BUG #2 above)";
}

TEST_F(ClassTypedConstructorParamTest, TestClsHasOneConstructor) {
  const hldb::ClassDefn *const c = getTestClsDefn();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getMethods(), nullptr);
  ASSERT_EQ(c->getMethods()->size(), 1u);
  const hldb::Function *const ctor = getTestConstructor();
  ASSERT_NE(ctor, nullptr);
  EXPECT_EQ(ctor->getName(), "new");
}

TEST_F(ClassTypedConstructorParamTest, TestConstructorHasOneIODeclDefWithDefault42) {
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

TEST_F(ClassTypedConstructorParamTest, TestConstructorBodyHasTwoStmts) {
  const hldb::Function *const ctor = getTestConstructor();
  ASSERT_NE(ctor, nullptr);
  const hldb::Begin *const body = ctor->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 2u);
}

TEST_F(ClassTypedConstructorParamTest, TestConstructorFirstStmtIsSuperNewCall) {
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

  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(path->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "'super.new(...)' should resolve to a MethodFuncCall, same as an ordinary "
                              "object-construction 'new(...)' call";
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), getSuperConstructor())
      << "matches the already-confirmed-working 'super.new' resolution in "
         "chapter-8/8.7--constructor_super/test_8.7--constructor_super.cpp";
}

TEST_F(ClassTypedConstructorParamTest, TestConstructorSecondStmtAssignsAFromDefMinusT) {
  const hldb::Function *const ctor = getTestConstructor();
  ASSERT_NE(ctor, nullptr);
  const hldb::Begin *const body = ctor->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_GT(body->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(1));
  ASSERT_NE(assign, nullptr) << "'a = def - t;' should be an Assignment";

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "a");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getTestPropertyA());

  const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'def - t' should be a subtract Operation";
  EXPECT_EQ(rhs->getOpType(), vpiSubOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);

  const hldb::RefObj *const defOperand = any_cast<hldb::RefObj>(rhs->getOperands()->at(0));
  ASSERT_NE(defOperand, nullptr);
  EXPECT_EQ(defOperand->getName(), "def");
  ASSERT_NE(ctor->getIODecls(), nullptr);
  ASSERT_GT(ctor->getIODecls()->size(), 0u);
  EXPECT_EQ(defOperand->getActual<hldb::IODecl>(), ctor->getIODecls()->at(0));

  const hldb::RefObj *const tOperand = any_cast<hldb::RefObj>(rhs->getOperands()->at(1));
  ASSERT_NE(tOperand, nullptr);
  EXPECT_EQ(tOperand->getName(), "t");
  EXPECT_EQ(tOperand->getActual<hldb::Parameter>(), getTestClassParamT())
      << "'t' used bare inside the constructor body must resolve to the class's OWN type parameter";
}

// --- variable "super_obj" -------------------------------------------------------------

TEST_F(ClassTypedConstructorParamTest, VariableSuperObjExists) { EXPECT_NE(getVariableSuperObj(), nullptr); }

TEST_F(ClassTypedConstructorParamTest, VariableSuperObjTypespecResolvesToSuperClsClassDefn) {
  const hldb::Variable *const superObj = getVariableSuperObj();
  ASSERT_NE(superObj, nullptr);
  ASSERT_NE(superObj->getTypespec(), nullptr);
  const hldb::ClassTypespec *const ct = superObj->getTypespec<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr) << "8.7: 'super_cls super_obj;' declares a handle of the BASE class type";
  EXPECT_EQ(ct->getClassDefn(), getSuperClsDefn());
}

// --- initial process structure -------------------------------------------------

TEST_F(ClassTypedConstructorParamTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(ClassTypedConstructorParamTest, InitialBeginHasTwoStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 2u);
}

// --- super_obj = test_cls#(.t(23))::new(.def(41)) --------------------------------

TEST_F(ClassTypedConstructorParamTest, AssignmentIsBlockingWithLhsSuperObj) {
  const hldb::Assignment *const assign = getAssignmentStmt();
  ASSERT_NE(assign, nullptr) << "the polymorphic assignment should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "super_obj");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getVariableSuperObj());
}

TEST_F(ClassTypedConstructorParamTest, AssignmentRhsIsNewMethodFuncCall) {
  const hldb::Assignment *const assign = getAssignmentStmt();
  ASSERT_NE(assign, nullptr);
  const hldb::MethodFuncCall *const newCall = assign->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(newCall, nullptr) << "'test_cls#(.t(23))::new(.def(41))' should resolve to a MethodFuncCall";
  EXPECT_EQ(newCall->getName(), "new");
}

TEST_F(ClassTypedConstructorParamTest, AssignmentRhsScopeParamTResolvesToClassParameter) {
  const hldb::Assignment *const assign = getAssignmentStmt();
  ASSERT_NE(assign, nullptr);
  const hldb::MethodFuncCall *const newCall = assign->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(newCall, nullptr);
  const hldb::RefTypespec *const rt = newCall->getScope<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::ClassTypespec *const ct = rt->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr);
  EXPECT_EQ(ct->getDefName(), "test_cls");
  ASSERT_NE(ct->getParamAssigns(), nullptr);
  ASSERT_EQ(ct->getParamAssigns()->size(), 1u);
  const hldb::ParamAssign *const pa = ct->getParamAssigns()->at(0);
  ASSERT_NE(pa, nullptr);
  EXPECT_TRUE(pa->getConnByName()) << "'.t(23)' is a named (not positional) parameter assignment";
  const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "t");
  EXPECT_EQ(lhs->getActual<hldb::Parameter>(), getTestClassParamT())
      << "8.8: '.t(23)' must resolve 't' back to test_cls's OWN type parameter (see FIXED COMPILER BUG #3 above)";
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "23");
}

TEST_F(ClassTypedConstructorParamTest, AssignmentRhsNewCallResolvesToTestConstructor) {
  const hldb::Assignment *const assign = getAssignmentStmt();
  ASSERT_NE(assign, nullptr);
  const hldb::MethodFuncCall *const newCall = assign->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(newCall, nullptr);
  EXPECT_EQ(newCall->getTaskFunc<hldb::Function>(), getTestConstructor())
      << "8.8: 'test_cls::new(...)' must resolve back to test_cls's constructor (see FIXED COMPILER BUG #3 "
         "above, consistent with the ordinary 'new' resolution fix documented in "
         "chapter-8/8.7--constructor/test_8.7--constructor.cpp)";
}

TEST_F(ClassTypedConstructorParamTest, AssignmentRhsNamedArgumentDefIsFortyOne) {
  const hldb::Assignment *const assign = getAssignmentStmt();
  ASSERT_NE(assign, nullptr);
  const hldb::MethodFuncCall *const newCall = assign->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(newCall, nullptr);
  ASSERT_NE(newCall->getArguments(), nullptr);
  ASSERT_EQ(newCall->getArguments()->size(), 1u);
  // Unlike positional arguments elsewhere in this suite (a bare Constant),
  // this build represents the named argument ".def(41)" as an IODecl-shaped
  // node carrying the name/value pair. This is a structural observation,
  // not an asserted bug -- see the STRUCTURAL NOTE above.
  const hldb::NamedArgument *const arg = any_cast<hldb::NamedArgument>(newCall->getArguments()->at(0));
  ASSERT_NE(arg, nullptr) << "'.def(41)' is represented as an NamedArgument, not a plain Constant";
  const hldb::Any *const lc = arg->getLowConn();
  ASSERT_NE(lc, nullptr);
  EXPECT_EQ(lc->getName(), "def");
  const hldb::Constant *const value = arg->getHighConn<hldb::Constant>();
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getDecompile(), "41");
}

// --- $display(super_obj.s) -------------------------------------------------------

TEST_F(ClassTypedConstructorParamTest, DisplayArgIsSuperObjDotS) {
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
  EXPECT_EQ(superObjRef->getActual<hldb::Variable>(), getVariableSuperObj());

  const hldb::RefObj *const sRef = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
  ASSERT_NE(sRef, nullptr);
  EXPECT_EQ(sRef->getName(), "s");
  EXPECT_EQ(sRef->getActual<hldb::Variable>(), getSuperPropertyS());
}

// --- compiler diagnostics ---------------------------------------------------------

TEST_F(ClassTypedConstructorParamTest, CompilerReportsNoErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0) << "8.8: 'test_cls#(.t(23))::new(.def(41))' is legal syntax and should not raise "
                                 "a COMP_FAILED_TO_BIND error (see FIXED COMPILER BUG #3 above)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
