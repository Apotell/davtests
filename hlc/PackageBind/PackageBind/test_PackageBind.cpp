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

// Validates the HLDB model built for tests/PackageBind/dut.sv:
//
//   package prim_util_pkg;
//      function automatic integer _clog2(integer value);
//         integer result;
//         for (result = 0; value > 1; result++) begin
//            value = value >> 1;
//         end
//         return result;
//      endfunction
//   endpackage
//
//   module top(output integer o);
//      assign o = prim_util_pkg::_clog2(15);
//   endmodule // top
//
// The point of the fixture is that a call written with the package scope
// resolution operator ('prim_util_pkg::_clog2', IEEE 1800-2023 26.3) binds
// to the function declared inside that package, with no import in scope.
//
// What is checked, and why:
//   Package prim_util_pkg (26.2)
//     - exists in Design::getAllPackages(), declares exactly 1 TaskFunc and
//       no parameters or package-level variables
//     - no 'endpackage : label' in the source, so getEndLabel() is empty
//   Function _clog2 (13.4)
//     - is a Function named "_clog2" ('_' is a legal identifier start, 5.6)
//     - 'automatic' is recorded (13.4.2)
//     - return type 'integer' -> IntegerTypespec, signed (6.11, Table 6-8)
//     - exactly 1 formal 'value': direction defaults to input (13.4), type
//       integer
//     - exactly 1 local variable 'result' of type integer, no initializer
//     - the body's executable statements are, in order, a ForStmt and a
//       ReturnStmt (the 'integer result;' declaration is not counted,
//       because whether a bare declaration appears in the statement list
//       is a tool convention the source does not determine)
//   for (result = 0; value > 1; result++) (12.7.1)
//     - 'result' is declared outside the loop, so the ForStmt has no local
//       variables of its own
//     - init: one Assignment, LHS RefObj 'result' bound to the local
//       Variable, RHS Constant "0"
//     - condition: Operation vpiGtOp over RefObj 'value' (bound to the
//       IODecl) and Constant "1"
//     - step: one Operation vpiPostIncOp over RefObj 'result'
//     - body: begin-end with exactly 1 blocking Assignment
//       'value = value >> 1', RHS Operation vpiRShiftOp (logical shift,
//       11.4.10) over RefObj 'value' and Constant "1"
//   return result; -> ReturnStmt whose expression is RefObj 'result' bound
//     to the local Variable
//   Module top (23.2.2.3)
//     - exactly 1 port 'o', direction output
//     - 'output integer o' uses an explicit data type, so the port kind
//       defaults to a variable: 'o' is a Variable of type integer, never a
//       Net; the port's low connection is bound to that Variable
//     - no processes
//   assign o = prim_util_pkg::_clog2(15); (10.3, 26.3)
//     - exactly 1 ContAssign; LHS RefObj 'o' bound to the Variable 'o'
//       (a variable may be driven by a single continuous assignment, 6.5)
//     - RHS is a package-scoped RefObj path with exactly 2 elements: a
//       RefObj 'prim_util_pkg' bound to the Package, then a FuncCall bound,
//       by object identity, to the package's Function _clog2, with exactly
//       1 argument, Constant "15"
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics, and no
//       COMP_FAILED_TO_BIND for '_clog2'
//
// What is NOT checked, and why:
//   - The value the call returns (3 = floor(log2(15)) by stepping the loop
//     by hand) is the value 'o' takes while simulation runs. The RHS of a
//     continuous assignment is not a constant-expression context (11.2.1),
//     so nothing in the standard requires HLC to fold it. This is
//     permanently out of scope; the static half -- that the call binds to
//     the right function with the right argument -- is covered by
//     ContAssignRhsCallsPackageFunctionWithFifteen.
//   - The exact spelling HLC gives the call's name ("_clog2" versus the
//     scoped "prim_util_pkg::_clog2") is a tool convention; either is
//     accepted, and the binding itself is checked by object identity.
//   - HLC represents a package-scoped name as a RefObj path (the package,
//     then the named item) rather than a bare FuncCall. That shape is a
//     model convention; the test follows it and asserts both bindings.
//   - This fixture has nothing that must reduce or elaborate, so no check
//     is gated on Design::getElaborated().

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
#include <hldb/for_stmt.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/integer_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/return_stmt.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>
#include <vector>

namespace hlc {

class PackageBindTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackageBind.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("prim_util_pkg", m_design->getAllPackages());
  }

  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Function *getClog2() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("_clog2", pkg->getTaskFuncs());
  }

  static const hldb::Begin *getClog2Body() {
    const hldb::Function *const fn = getClog2();
    if (fn == nullptr) return nullptr;
    return fn->getStmt<hldb::Begin>();
  }

  static const hldb::IODecl *getValueDecl() {
    const hldb::Function *const fn = getClog2();
    if (fn == nullptr) return nullptr;
    return hldb::findByName<hldb::IODecl>("value", fn->getIODecls());
  }

  // 'integer result;' may be owned by the function scope itself or by the
  // Begin that wraps its body -- which scope owns it is a tool convention,
  // so both are searched. LocalResultIsTheOnlyLocalVariable checks there is
  // exactly one such declaration across both.
  static const hldb::Variable *getResult() {
    const hldb::Function *const fn = getClog2();
    if (fn == nullptr) return nullptr;
    if (const hldb::Variable *const v = hldb::findByName<hldb::Variable>("result", fn->getVariables())) return v;
    const hldb::Begin *const body = getClog2Body();
    if (body == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("result", body->getVariables());
  }

  // The function body's statements with bare Variable declarations removed.
  static std::vector<const hldb::Any *> getExecutableStmts() {
    std::vector<const hldb::Any *> stmts;
    const hldb::Begin *const body = getClog2Body();
    if (body == nullptr || body->getStmts() == nullptr) return stmts;
    for (const hldb::Any *const stmt : *body->getStmts()) {
      if (any_cast<hldb::Variable>(stmt) == nullptr) stmts.emplace_back(stmt);
    }
    return stmts;
  }

  static const hldb::ForStmt *getFor() {
    const std::vector<const hldb::Any *> stmts = getExecutableStmts();
    if (stmts.empty()) return nullptr;
    return any_cast<hldb::ForStmt>(stmts.front());
  }

  static const hldb::Variable *getO() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("o", top->getVariables());
  }

  static const hldb::ContAssign *getContAssign() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getContAssigns() == nullptr || top->getContAssigns()->empty()) return nullptr;
    return top->getContAssigns()->at(0);
  }
};

// ---------------------------------------------------------------------------
// package prim_util_pkg
// ---------------------------------------------------------------------------

TEST_F(PackageBindTest, PackageExistsWithOneFunctionAndNoOtherItems) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'prim_util_pkg' not found in Design::getAllPackages()";
  EXPECT_EQ(pkg->getName(), "prim_util_pkg");
  ASSERT_NE(pkg->getTaskFuncs(), nullptr);
  EXPECT_EQ(pkg->getTaskFuncs()->size(), 1u) << "the package declares exactly one function";
  EXPECT_TRUE(pkg->getParameters() == nullptr || pkg->getParameters()->empty());
  EXPECT_TRUE(pkg->getVariables() == nullptr || pkg->getVariables()->empty())
      << "'integer result' is local to _clog2, not a package-level variable";
}

TEST_F(PackageBindTest, PackageHasNoEndLabel) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  EXPECT_EQ(pkg->getEndLabel(), "") << "'endpackage' carries no ': label' in the source";
}

// ---------------------------------------------------------------------------
// function automatic integer _clog2(integer value);
// ---------------------------------------------------------------------------

TEST_F(PackageBindTest, Clog2IsAutomaticFunction) {
  const hldb::Function *const fn = getClog2();
  ASSERT_NE(fn, nullptr) << "Function '_clog2' not found in the package's TaskFuncs";
  EXPECT_EQ(fn->getName(), "_clog2");
  EXPECT_TRUE(fn->getAutomatic()) << "13.4.2: 'function automatic' must record automatic lifetime";
}

TEST_F(PackageBindTest, Clog2ReturnsSignedInteger) {
  const hldb::Function *const fn = getClog2();
  ASSERT_NE(fn, nullptr);
  const hldb::RefTypespec *const rts = fn->getReturn();
  ASSERT_NE(rts, nullptr);
  const hldb::IntegerTypespec *const ts = rts->getActual<hldb::IntegerTypespec>();
  ASSERT_NE(ts, nullptr) << "return type 'integer' should resolve to IntegerTypespec";
  EXPECT_TRUE(hldb::getSigned(ts)) << "6.11 Table 6-8: 'integer' is a signed type";
}

TEST_F(PackageBindTest, Clog2HasOneInputIntegerFormalValue) {
  const hldb::Function *const fn = getClog2();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 1u);
  const hldb::IODecl *const value = fn->getIODecls()->at(0);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getName(), "value");
  EXPECT_EQ(value->getDirection(), vpiInput) << "13.4: a formal with no direction defaults to input";
  ASSERT_NE(value->getTypespec(), nullptr);
  EXPECT_NE(value->getTypespec()->getActual<hldb::IntegerTypespec>(), nullptr)
      << "formal 'value' is declared 'integer'";
}

TEST_F(PackageBindTest, Clog2BodyIsBegin) {
  EXPECT_NE(getClog2Body(), nullptr) << "a multi-statement function body should be wrapped in a Begin";
}

TEST_F(PackageBindTest, LocalResultIsTheOnlyLocalVariable) {
  const hldb::Function *const fn = getClog2();
  ASSERT_NE(fn, nullptr);
  size_t count = (fn->getVariables() == nullptr) ? 0u : fn->getVariables()->size();
  const hldb::Begin *const body = getClog2Body();
  if (body != nullptr && body->getVariables() != nullptr) count += body->getVariables()->size();
  EXPECT_EQ(count, 1u) << "_clog2 declares exactly one local variable: 'integer result'";

  const hldb::Variable *const result = getResult();
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->getName(), "result");
  ASSERT_NE(result->getTypespec(), nullptr);
  EXPECT_NE(result->getTypespec()->getActual<hldb::IntegerTypespec>(), nullptr) << "'result' is declared 'integer'";
  EXPECT_EQ(result->getValue(), nullptr) << "'integer result;' has no initializer";
}

TEST_F(PackageBindTest, Clog2ExecutableStatementsAreForThenReturn) {
  const std::vector<const hldb::Any *> stmts = getExecutableStmts();
  ASSERT_EQ(stmts.size(), 2u) << "the body executes exactly two statements: the for loop and the return";
  EXPECT_NE(any_cast<hldb::ForStmt>(stmts[0]), nullptr) << "first executable statement should be the for loop";
  EXPECT_NE(any_cast<hldb::ReturnStmt>(stmts[1]), nullptr) << "second executable statement should be the return";
}

// ---------------------------------------------------------------------------
// for (result = 0; value > 1; result++)
// ---------------------------------------------------------------------------

TEST_F(PackageBindTest, ForHasNoLoopLocalVariables) {
  const hldb::ForStmt *const forStmt = getFor();
  ASSERT_NE(forStmt, nullptr);
  EXPECT_TRUE(forStmt->getVariables() == nullptr || forStmt->getVariables()->empty())
      << "'result' is declared outside the loop, so the ForStmt owns no variables";
}

TEST_F(PackageBindTest, ForInitAssignsZeroToResult) {
  const hldb::ForStmt *const forStmt = getFor();
  ASSERT_NE(forStmt, nullptr);
  ASSERT_NE(forStmt->getForInitStmts(), nullptr);
  ASSERT_EQ(forStmt->getForInitStmts()->size(), 1u);
  const hldb::Assignment *const init = any_cast<hldb::Assignment>(forStmt->getForInitStmts()->at(0));
  ASSERT_NE(init, nullptr) << "'result = 0' should be an Assignment";
  const hldb::RefObj *const lhs = init->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "'result' is not declared in the for-init, so the LHS is a reference";
  EXPECT_EQ(lhs->getName(), "result");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getResult());
  const hldb::Constant *const rhs = init->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "0");
}

TEST_F(PackageBindTest, ForConditionIsValueGreaterThanOne) {
  const hldb::ForStmt *const forStmt = getFor();
  ASSERT_NE(forStmt, nullptr);
  const hldb::Operation *const cond = forStmt->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiGtOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "value");
  EXPECT_EQ(lhs->getActual<hldb::IODecl>(), getValueDecl()) << "'value' should bind to the formal argument";
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(cond->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "1");
}

TEST_F(PackageBindTest, ForStepIsResultPostIncrement) {
  const hldb::ForStmt *const forStmt = getFor();
  ASSERT_NE(forStmt, nullptr);
  ASSERT_NE(forStmt->getForIncStmts(), nullptr);
  ASSERT_EQ(forStmt->getForIncStmts()->size(), 1u);
  const hldb::Operation *const step = any_cast<hldb::Operation>(forStmt->getForIncStmts()->at(0));
  ASSERT_NE(step, nullptr) << "'result++' should be an Operation";
  EXPECT_EQ(step->getOpType(), vpiPostIncOp);
  ASSERT_NE(step->getOperands(), nullptr);
  ASSERT_EQ(step->getOperands()->size(), 1u);
  const hldb::RefObj *const operand = any_cast<hldb::RefObj>(step->getOperands()->at(0));
  ASSERT_NE(operand, nullptr);
  EXPECT_EQ(operand->getName(), "result");
  EXPECT_EQ(operand->getActual<hldb::Variable>(), getResult());
}

TEST_F(PackageBindTest, ForBodyShiftsValueRightByOne) {
  const hldb::ForStmt *const forStmt = getFor();
  ASSERT_NE(forStmt, nullptr);
  const hldb::Begin *const loopBody = forStmt->getStmt<hldb::Begin>();
  ASSERT_NE(loopBody, nullptr) << "the loop body is an explicit begin-end";
  ASSERT_NE(loopBody->getStmts(), nullptr);
  ASSERT_EQ(loopBody->getStmts()->size(), 1u);

  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(loopBody->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "'value = value >> 1' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "value");
  EXPECT_EQ(lhs->getActual<hldb::IODecl>(), getValueDecl());

  const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getOpType(), vpiRShiftOp) << "11.4.10: '>>' is the logical right shift";
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);
  const hldb::RefObj *const shifted = any_cast<hldb::RefObj>(rhs->getOperands()->at(0));
  ASSERT_NE(shifted, nullptr);
  EXPECT_EQ(shifted->getName(), "value");
  EXPECT_EQ(shifted->getActual<hldb::IODecl>(), getValueDecl());
  const hldb::Constant *const amount = any_cast<hldb::Constant>(rhs->getOperands()->at(1));
  ASSERT_NE(amount, nullptr);
  EXPECT_EQ(amount->getDecompile(), "1");
}

// ---------------------------------------------------------------------------
// return result;
// ---------------------------------------------------------------------------

TEST_F(PackageBindTest, ReturnYieldsResult) {
  const std::vector<const hldb::Any *> stmts = getExecutableStmts();
  ASSERT_EQ(stmts.size(), 2u);
  const hldb::ReturnStmt *const ret = any_cast<hldb::ReturnStmt>(stmts[1]);
  ASSERT_NE(ret, nullptr);
  const hldb::RefObj *const expr = ret->getCondition<hldb::RefObj>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getName(), "result");
  EXPECT_EQ(expr->getActual<hldb::Variable>(), getResult());
}

// ---------------------------------------------------------------------------
// module top(output integer o);
// ---------------------------------------------------------------------------

TEST_F(PackageBindTest, TopHasOneOutputPortO) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 1u);
  const hldb::Port *const port = top->getPorts()->at(0);
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "o");
  EXPECT_EQ(port->getDirection(), vpiOutput);
  const hldb::RefObj *const lowConn = port->getLowConn<hldb::RefObj>();
  ASSERT_NE(lowConn, nullptr);
  EXPECT_EQ(lowConn->getActual<hldb::Variable>(), getO()) << "port 'o' should connect to the Variable 'o'";
}

TEST_F(PackageBindTest, OutputIntegerOIsVariableNotNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
  const hldb::Variable *const o = getO();
  ASSERT_NE(o, nullptr) << "23.2.2.3: 'output integer o' has an explicit data type, so it is a variable port";
  ASSERT_NE(o->getTypespec(), nullptr);
  EXPECT_NE(o->getTypespec()->getActual<hldb::IntegerTypespec>(), nullptr) << "'o' is declared 'integer'";
  EXPECT_EQ(hldb::findByName<hldb::Net>("o", top->getNets()), nullptr) << "'o' must not also appear as a Net";
}

TEST_F(PackageBindTest, TopHasNoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

// ---------------------------------------------------------------------------
// assign o = prim_util_pkg::_clog2(15);
// ---------------------------------------------------------------------------

TEST_F(PackageBindTest, TopHasOneContAssignDrivingO) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = getContAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "o");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getO());
}

TEST_F(PackageBindTest, ContAssignRhsCallsPackageFunctionWithFifteen) {
  const hldb::ContAssign *const ca = getContAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const path = ca->getRhs<hldb::RefObj>();
  ASSERT_NE(path, nullptr) << "'prim_util_pkg::_clog2(15)' should be a package-scoped RefObj path";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u) << "the package, then the call";
  const hldb::RefObj *const scope = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(scope, nullptr);
  EXPECT_EQ(scope->getName(), "prim_util_pkg");
  ASSERT_NE(getPkg(), nullptr);
  EXPECT_EQ(scope->getActual<hldb::Package>(), getPkg()) << "26.3: the scope prefix names package prim_util_pkg";
  const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(path->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "the last path element should be the FuncCall '_clog2(15)'";
  EXPECT_TRUE(call->getName() == "_clog2") << "unexpected call name '" << call->getName() << "'";
  ASSERT_NE(getClog2(), nullptr);
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), getClog2())
      << "26.3: the scoped call must bind to _clog2 declared in prim_util_pkg";
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const arg = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getDecompile(), "15");
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_F(PackageBindTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "_clog2"), nullptr)
      << "the package-scoped call must bind (IEEE 1800-2023 26.3)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
