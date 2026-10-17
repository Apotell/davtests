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

// Validates the HLDB model built for tests/PackFuncParent/dut.sv:
//
//   package my_pkg;
//      function automatic logic [7:0] sbox4_8bit(logic [7:0] state_in);
//         return state_in;
//      endfunction : sbox4_8bit
//      function automatic logic [15:0] sbox4_16bit(logic [15:0] state_in);
//         logic [15:0] state_out;
//         state_out[0 +: 8] = sbox4_8bit(state_in[0 +: 8]);
//         return state_out;
//      endfunction : sbox4_16bit
//   endpackage // my_pkg
//
//   module top(output logic [15:0] o);
//      assign o = my_pkg::sbox4_16bit(16'hABCD);
//   endmodule // top
//
// The point of the fixture is a package function that calls another function
// of the same package without a scope prefix, from a module that calls the
// outer function through the package scope resolution operator (IEEE
// 1800-2023 26.3). Both functions name their formal 'state_in'. The
// regression this file exists to catch is HLC resolving the inner call or
// its argument in the wrong scope: the call must bind to my_pkg's
// sbox4_8bit, and 'state_in' in its argument to sbox4_16bit's own formal,
// not to sbox4_8bit's formal of the same name.
//
// What is checked, and why:
//   Package my_pkg (26.2)
//     - exactly 2 subroutines, sbox4_8bit and sbox4_16bit, each an automatic
//       (13.4.2) Function with its own end label
//     - sbox4_8bit returns logic [7:0] and has exactly 1 formal, state_in,
//       an input of logic [7:0]; its body is a single ReturnStmt returning
//       RefObj 'state_in' bound to that formal
//     - sbox4_16bit returns logic [15:0] and has exactly 1 formal, state_in,
//       an input of logic [15:0], and exactly 1 local, state_out, of logic
//       [15:0]; its executable statements are, in order, a blocking
//       Assignment and a ReturnStmt returning RefObj 'state_out' bound to the
//       local
//   state_out[0 +: 8] = sbox4_8bit(state_in[0 +: 8]); (11.5.1)
//     - the LHS is an indexed part-select (vpiPosIndexed) with base Constant
//       "0" and width Constant "8" on RefObj 'state_out', bound to the local
//     - the RHS is a call of sbox4_8bit bound to my_pkg's Function (26.3:
//       the name is declared in the enclosing package), with exactly 1
//       argument: the same indexed part-select shape on RefObj 'state_in',
//       bound to sbox4_16bit's formal
//   Module top (23.2)
//     - exactly 1 port, o, an output. 'output logic [15:0] o' writes its
//       data type with the explicit data_type syntax, so the port is a
//       variable (23.2.2.3) and is the port's low connection
//     - exactly 1 ContAssign: LHS bound to the variable o; RHS a
//       package-scoped RefObj path whose prefix is bound to my_pkg and whose
//       last element is a call of sbox4_16bit bound to my_pkg's Function,
//       with exactly 1 argument, the Constant "16'hABCD"
//   Elaboration (23.3.1)
//     - top appears in no instantiation, so on an elaborated design it is
//       the only top-level instance, named "top"
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the RHS of a continuous assignment is not a
// constant-expression context, so the standard does not require HLC to fold
// the call; only the instance tree is checked under getElaborated().
//
// What is NOT checked, and why:
//   - The value o holds only exists while simulation runs, since the call is
//     not required to be folded. Permanently out of scope.
//   - Which scope owns state_out (the function itself or a Begin wrapping its
//     body) is a tool convention, so both are searched; bare declarations in
//     a statement list are skipped.
//   - HLC represents a package-scoped name as a RefObj path (the package,
//     then the call). That shape is a model convention; the test follows it
//     and asserts both the package and the call binding.
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention; my_pkg is looked up by
//     name.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.
//   - The '// my_pkg' and '// top' comments are not end labels (5.4).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/indexed_part_select.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/return_stmt.h>
#include <hldb/tf_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>
#include <vector>

namespace hlc {

class PackFuncParentTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackFuncParent.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() { return hldb::findByName<hldb::Package>("my_pkg", m_design->getAllPackages()); }

  static const hldb::Function *getFunction(std::string_view name) {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>(name, pkg->getTaskFuncs());
  }

  static const hldb::IODecl *getFormal(std::string_view fnName) {
    const hldb::Function *const fn = getFunction(fnName);
    if (fn == nullptr) return nullptr;
    return hldb::findByName<hldb::IODecl>("state_in", fn->getIODecls());
  }

  // sbox4_16bit's local state_out, owned by the function or by a Begin
  // wrapping its body (a tool convention, so both are searched).
  static const hldb::Variable *getStateOut() {
    const hldb::Function *const fn = getFunction("sbox4_16bit");
    if (fn == nullptr) return nullptr;
    if (const hldb::Variable *const v = hldb::findByName<hldb::Variable>("state_out", fn->getVariables())) return v;
    const hldb::Begin *const body = fn->getStmt<hldb::Begin>();
    if (body == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("state_out", body->getVariables());
  }

  // sbox4_16bit's statements with bare Variable declarations removed.
  static std::vector<const hldb::Any *> getExecutableStmts() {
    std::vector<const hldb::Any *> stmts;
    const hldb::Function *const fn = getFunction("sbox4_16bit");
    if (fn == nullptr) return stmts;
    const hldb::Begin *const body = fn->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr) return stmts;
    for (const hldb::Any *const stmt : *body->getStmts()) {
      if (any_cast<hldb::Variable>(stmt) == nullptr) stmts.emplace_back(stmt);
    }
    return stmts;
  }

  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  // Verifies 'type' is a LogicTypespec with the single packed range
  // [left:0].
  static void ExpectLogicVector(const hldb::RefTypespec *type, std::string_view left, std::string_view what) {
    ASSERT_NE(type, nullptr) << what << " has no typespec";
    const hldb::LogicTypespec *const lt = type->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << what << " is declared 'logic [" << left << ":0]'";
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u) << what;
    const hldb::Constant *const l = lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
    const hldb::Constant *const r = lt->getRanges()->at(0)->getRightExpr<hldb::Constant>();
    ASSERT_NE(l, nullptr);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(l->getDecompile(), left) << what;
    EXPECT_EQ(r->getDecompile(), "0") << what;
  }

  // Verifies 'expr' is '<name>[0 +: 8]' with the prefix bound to 'target'.
  static void ExpectLowByte(const hldb::Any *expr, std::string_view name, const hldb::Any *target) {
    const hldb::IndexedPartSelect *const sel = any_cast<hldb::IndexedPartSelect>(expr);
    ASSERT_NE(sel, nullptr) << "11.5.1: '" << name << "[0 +: 8]' is an indexed part-select";
    EXPECT_EQ(sel->getIndexedPartSelectType(), vpiPosIndexed) << "'+:' selects upward from the base";
    const hldb::Constant *const base = sel->getBaseExpr<hldb::Constant>();
    ASSERT_NE(base, nullptr);
    EXPECT_EQ(base->getDecompile(), "0");
    const hldb::Constant *const width = sel->getWidthExpr<hldb::Constant>();
    ASSERT_NE(width, nullptr);
    EXPECT_EQ(width->getDecompile(), "8");
    const hldb::RefObj *const prefix = sel->getPrefix<hldb::RefObj>();
    ASSERT_NE(prefix, nullptr);
    EXPECT_EQ(prefix->getName(), name);
    ASSERT_NE(target, nullptr) << "the declaration of '" << name << "' was not found";
    EXPECT_EQ(prefix->getActual(), target) << "'" << name << "' must bind to sbox4_16bit's own declaration";
  }
};

// ---------------------------------------------------------------------------
// package my_pkg; ... endpackage
// ---------------------------------------------------------------------------

TEST_F(PackFuncParentTest, PackageDeclaresTwoAutomaticFunctionsWithEndLabels) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'my_pkg' not found";
  EXPECT_EQ(pkg->getEndLabel(), "") << "'// my_pkg' after endpackage is a comment, not a label";
  ASSERT_NE(pkg->getTaskFuncs(), nullptr);
  EXPECT_EQ(pkg->getTaskFuncs()->size(), 2u) << "'sbox4_8bit' and 'sbox4_16bit'";
  for (std::string_view name : {"sbox4_8bit", "sbox4_16bit"}) {
    const hldb::Function *const fn = getFunction(name);
    ASSERT_NE(fn, nullptr) << "function '" << name << "' not found";
    EXPECT_TRUE(fn->getAutomatic()) << "13.4.2: '" << name << "' is declared 'function automatic'";
    EXPECT_EQ(fn->getEndLabel(), name) << "'endfunction : " << name << "' carries an end label";
  }
}

TEST_F(PackFuncParentTest, Sbox8bitReturnsItsFormal) {
  const hldb::Function *const fn = getFunction("sbox4_8bit");
  ASSERT_NE(fn, nullptr);
  ExpectLogicVector(fn->getReturn(), "7", "sbox4_8bit's return type");
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 1u);
  const hldb::IODecl *const formal = getFormal("sbox4_8bit");
  ASSERT_NE(formal, nullptr);
  EXPECT_EQ(formal->getDirection(), vpiInput) << "13.4: a formal with no direction is an input";
  ExpectLogicVector(formal->getTypespec(), "7", "sbox4_8bit's 'state_in'");
  const hldb::ReturnStmt *const ret = fn->getStmt<hldb::ReturnStmt>();
  ASSERT_NE(ret, nullptr) << "the body is the single return statement";
  const hldb::RefObj *const value = ret->getCondition<hldb::RefObj>();
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getName(), "state_in");
  EXPECT_EQ(value->getActual(), formal) << "'state_in' is sbox4_8bit's own formal";
}

TEST_F(PackFuncParentTest, Sbox16bitHasFormalAndLocalStateOut) {
  const hldb::Function *const fn = getFunction("sbox4_16bit");
  ASSERT_NE(fn, nullptr);
  ExpectLogicVector(fn->getReturn(), "15", "sbox4_16bit's return type");
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 1u);
  const hldb::IODecl *const formal = getFormal("sbox4_16bit");
  ASSERT_NE(formal, nullptr);
  EXPECT_EQ(formal->getDirection(), vpiInput);
  ExpectLogicVector(formal->getTypespec(), "15", "sbox4_16bit's 'state_in'");
  const hldb::Variable *const out = getStateOut();
  ASSERT_NE(out, nullptr) << "local variable 'state_out' not found";
  ExpectLogicVector(out->getTypespec(), "15", "'state_out'");
}

TEST_F(PackFuncParentTest, Sbox16bitBodyIsAssignmentThenReturn) {
  const std::vector<const hldb::Any *> stmts = getExecutableStmts();
  ASSERT_EQ(stmts.size(), 2u) << "the assignment, then the return";
  EXPECT_NE(any_cast<hldb::Assignment>(stmts[0]), nullptr);
  const hldb::ReturnStmt *const ret = any_cast<hldb::ReturnStmt>(stmts[1]);
  ASSERT_NE(ret, nullptr) << "13.4.1: the last statement is 'return state_out;'";
  const hldb::RefObj *const value = ret->getCondition<hldb::RefObj>();
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getName(), "state_out");
  EXPECT_EQ(value->getActual(), getStateOut());
}

// ---------------------------------------------------------------------------
// state_out[0 +: 8] = sbox4_8bit(state_in[0 +: 8]);
// ---------------------------------------------------------------------------

TEST_F(PackFuncParentTest, AssignmentWritesLowByteOfStateOut) {
  const std::vector<const hldb::Any *> stmts = getExecutableStmts();
  ASSERT_FALSE(stmts.empty());
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(stmts[0]);
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' is a blocking assignment";
  ExpectLowByte(assign->getLhs(), "state_out", getStateOut());
}

TEST_F(PackFuncParentTest, InnerCallBindsToSbox8bitWithOwnFormalSlice) {
  const std::vector<const hldb::Any *> stmts = getExecutableStmts();
  ASSERT_FALSE(stmts.empty());
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(stmts[0]);
  ASSERT_NE(assign, nullptr);
  const hldb::TFCall *const call = assign->getRhs<hldb::TFCall>();
  ASSERT_NE(call, nullptr) << "the RHS is the call 'sbox4_8bit(...)'";
  EXPECT_EQ(call->getName(), "sbox4_8bit");
  ASSERT_NE(getFunction("sbox4_8bit"), nullptr);
  EXPECT_EQ(call->getTaskFunc(), getFunction("sbox4_8bit")) << "26.3: sbox4_8bit is declared in the same package";
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  ExpectLowByte(call->getArguments()->at(0), "state_in", getFormal("sbox4_16bit"));
}

// ---------------------------------------------------------------------------
// module top(output logic [15:0] o); assign o = my_pkg::sbox4_16bit(16'hABCD);
// ---------------------------------------------------------------------------

TEST_F(PackFuncParentTest, TopHasOneOutputVariablePortO) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  EXPECT_EQ(top->getEndLabel(), "") << "'// top' after endmodule is a comment, not a label";
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 1u);
  const hldb::Port *const port = top->getPorts()->at(0);
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "o");
  EXPECT_EQ(port->getDirection(), vpiOutput);
  const hldb::Variable *const o = hldb::findByName<hldb::Variable>("o", top->getVariables());
  ASSERT_NE(o, nullptr) << "23.2.2.3: an output port with an explicit data type is a variable";
  ExpectLogicVector(o->getTypespec(), "15", "'o'");
  ASSERT_NE(port->getLowConn<hldb::RefObj>(), nullptr);
  EXPECT_EQ(port->getLowConn<hldb::RefObj>()->getActual(), o);
}

TEST_F(PackFuncParentTest, ContAssignCallsScopedSbox16bit) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual(), hldb::findByName<hldb::Variable>("o", top->getVariables()));
  const hldb::RefObj *const path = ca->getRhs<hldb::RefObj>();
  ASSERT_NE(path, nullptr) << "'my_pkg::sbox4_16bit(...)' should be a package-scoped RefObj path";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u) << "the package, then the call";
  const hldb::RefObj *const scope = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(scope, nullptr);
  EXPECT_EQ(scope->getName(), "my_pkg");
  EXPECT_EQ(scope->getActual<hldb::Package>(), getPkg()) << "26.3: the scope prefix names my_pkg";
  const hldb::TFCall *const call = any_cast<hldb::TFCall>(path->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "sbox4_16bit");
  EXPECT_EQ(call->getTaskFunc(), getFunction("sbox4_16bit")) << "26.3: the call binds to my_pkg's sbox4_16bit";
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getDecompile(), "16'hABCD");
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(PackFuncParentTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: 'top' appears in no instantiation";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getName(), "top");
  }
}

TEST_F(PackFuncParentTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
