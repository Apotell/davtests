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

// Tests for FuncArgs/dut.sv:
//   module dut(
//           input wire [3:0] inp,
//           output wire [3:0] out1, out2
//   );
//           function automatic [3:0] pow_a;
//                   input [3:0] base, exp;
//                   begin
//                           pow_a = 1;
//                           if (exp > 0)
//                                   pow_a = base * pow_a(base, exp - 1);
//                   end
//           endfunction
//
//           function automatic [3:0] pow_b;
//                   input [3:0] base, exp;
//                   begin
//                           pow_b = 1;
//                           if (exp > 0)
//                                   pow_b = base * pow_b(base, exp - 1);
//                   end
//           endfunction
//
//           assign out1 = pow_a(inp, 3);
//           assign out2 = pow_b(2, 2);
//   endmodule
//
// What to check and why (IEEE 1800-2023, checked before any test code was
// written -- no .log file consulted for this file's expected shape, only
// the standard text and the real hldb API headers):
//
//   Sec 13.4/A.2.7 (old-style, non-ANSI function argument declarations):
//   'input [3:0] base, exp;' inside the function body (rather than in the
//   parenthesized formal list) declares two formal IODecls, 'base' and
//   'exp', both direction vpiInput per the shared 'input' keyword, each
//   carrying a [3:0] range. This is the "function argument declarations"
//   construct this test exercises, distinct from ANSI-style formals
//   declared directly in the parameter list.
//
//   Sec 13.4.1: 'function automatic [3:0] pow_a; ... pow_a = 1; ...' with
//   no explicit type keyword implicitly declares an unsigned [3:0]
//   variable return (Sec 6.7/6.8: no net-type keyword -> variable, i.e.
//   LogicTypespec, not a Net).
//
//   Sec 13.4.1: assigning to the function's own name ('pow_a = 1;',
//   'pow_a = base * pow_a(base, exp - 1);') is the old-style form of
//   returning a function's value.
//
//   Sec 13.4: 'pow_a(base, exp - 1)' inside pow_a's own body is a
//   legal recursive call (13.4: "It is legal for a function to call
//   itself recursively" when automatic); the call's arguments are
//   positional, unlike named-argument connections.
//
//   Sec 6.7: 'input wire [3:0] inp' / 'output wire [3:0] out1, out2'
//   carry the explicit net-type keyword 'wire', so these three ports
//   must be modeled as Nets (vpiWire), not Variables.
//
// What is NOT checked and why:
//   - pow_b's body is structurally identical to pow_a's (same shape,
//     different name); only pow_a's body is walked in full detail to
//     avoid duplicating the same assertions twice, while pow_b's
//     signature (IODecls/direction/return) is still checked directly.
//   - the runtime-evaluated numeric values of pow_a(inp, 3) / pow_b(2, 2)
//     are simulation-time concepts.

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
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/if_stmt.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FuncArgsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FuncArgs.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getDut() { return hldb::findByName<hldb::Module>("dut", m_design->getAllModules()); }

  static const hldb::Function *getFn(std::string_view name) {
    const hldb::Module *const top = getDut();
    if (top == nullptr || top->getTaskFuncs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>(name, top->getTaskFuncs());
  }

  // Verifies a function has 2 old-style 'input [3:0]' IODecls named
  // 'base'/'exp' and an implicit unsigned [3:0] logic return.
  static void CheckPowSignature(const hldb::Function *fn) {
    ASSERT_NE(fn, nullptr);
    EXPECT_TRUE(fn->getAutomatic());

    ASSERT_NE(fn->getIODecls(), nullptr);
    ASSERT_EQ(fn->getIODecls()->size(), 2u);
    bool hasBase = false, hasExp = false;
    for (const hldb::IODecl *const io : *fn->getIODecls()) {
      ASSERT_NE(io, nullptr);
      EXPECT_EQ(io->getDirection(), vpiInput);
      if (io->getName() == std::string_view("base")) hasBase = true;
      if (io->getName() == std::string_view("exp")) hasExp = true;
    }
    EXPECT_TRUE(hasBase) << "formal 'base' missing";
    EXPECT_TRUE(hasExp) << "formal 'exp' missing";

    const hldb::RefTypespec *const rts = fn->getReturn();
    ASSERT_NE(rts, nullptr);
    const hldb::LogicTypespec *const logic = rts->getActual<hldb::LogicTypespec>();
    ASSERT_NE(logic, nullptr) << "'[3:0]' implicit return should be a LogicTypespec (Sec 6.7/6.8)";
    EXPECT_FALSE(logic->getSigned());
  }
};

TEST_F(FuncArgsTest, ModuleExists) { EXPECT_NE(getDut(), nullptr); }

TEST_F(FuncArgsTest, PortsAreNetsBecauseOfExplicitWireKeyword) {
  const hldb::Module *const top = getDut();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const inp = hldb::findByName<hldb::Net>("inp", top->getNets());
  const hldb::Net *const out1 = hldb::findByName<hldb::Net>("out1", top->getNets());
  const hldb::Net *const out2 = hldb::findByName<hldb::Net>("out2", top->getNets());
  ASSERT_NE(inp, nullptr) << "'input wire [3:0] inp' should be a Net";
  ASSERT_NE(out1, nullptr) << "'output wire [3:0] out1' should be a Net";
  ASSERT_NE(out2, nullptr) << "'output wire [3:0] out2' should be a Net";
  EXPECT_EQ(inp->getNetType(), vpiWire);
  EXPECT_EQ(out1->getNetType(), vpiWire);
  EXPECT_EQ(out2->getNetType(), vpiWire);
}

// ---------------------------------------------------------------------------
// pow_a / pow_b signatures
// ---------------------------------------------------------------------------
TEST_F(FuncArgsTest, PowASignature) { CheckPowSignature(getFn("pow_a")); }
TEST_F(FuncArgsTest, PowBSignature) { CheckPowSignature(getFn("pow_b")); }

// ---------------------------------------------------------------------------
// pow_a's body:
//   begin
//     pow_a = 1;
//     if (exp > 0)
//       pow_a = base * pow_a(base, exp - 1);
//   end
// ---------------------------------------------------------------------------
TEST_F(FuncArgsTest, PowABodyIsBeginWithAssignmentThenIf) {
  const hldb::Function *const fn = getFn("pow_a");
  ASSERT_NE(fn, nullptr);
  const hldb::Begin *const body = fn->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "'begin ... end' body should be a Begin scope";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u);

  const hldb::Assignment *const initAssign = any_cast<hldb::Assignment>(body->getStmts()->at(0));
  ASSERT_NE(initAssign, nullptr) << "'pow_a = 1;' should be an Assignment";
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(initAssign->getLhs());
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view("pow_a"));
  const hldb::Constant *const one = any_cast<hldb::Constant>(initAssign->getRhs());
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), std::string_view("1"));

  const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(body->getStmts()->at(1));
  ASSERT_NE(ifStmt, nullptr) << "'if (exp > 0) ...' should be an IfStmt";
  const hldb::Operation *const cond = ifStmt->getCondition<hldb::Operation>();
  ASSERT_NE(ifStmt->getCondition(), nullptr);
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiGtOp);
}

// ---------------------------------------------------------------------------
// pow_a = base * pow_a(base, exp - 1); -- recursive call, positional args
// ---------------------------------------------------------------------------
TEST_F(FuncArgsTest, PowARecursesWithPositionalArguments) {
  const hldb::Function *const fn = getFn("pow_a");
  ASSERT_NE(fn, nullptr);
  const hldb::Begin *const body = fn->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u);
  const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(body->getStmts()->at(1));
  ASSERT_NE(ifStmt, nullptr);
  ASSERT_NE(ifStmt->getStmt(), nullptr);
  const hldb::Assignment *const thenAssign = any_cast<hldb::Assignment>(ifStmt->getStmt());
  ASSERT_NE(thenAssign, nullptr) << "the if's then-branch should be a single Assignment";

  const hldb::Operation *const mul = any_cast<hldb::Operation>(thenAssign->getRhs());
  ASSERT_NE(mul, nullptr) << "'base * pow_a(base, exp - 1)' should be an Operation";
  EXPECT_EQ(mul->getOpType(), vpiMultOp);
  ASSERT_NE(mul->getOperands(), nullptr);
  ASSERT_EQ(mul->getOperands()->size(), 2u);

  const hldb::RefObj *const baseRef = any_cast<hldb::RefObj>(mul->getOperands()->at(0));
  ASSERT_NE(baseRef, nullptr);
  EXPECT_EQ(baseRef->getName(), std::string_view("base"));

  const hldb::FuncCall *const recCall = any_cast<hldb::FuncCall>(mul->getOperands()->at(1));
  ASSERT_NE(recCall, nullptr) << "second operand should be the recursive call 'pow_a(base, exp - 1)'";
  EXPECT_EQ(recCall->getName(), std::string_view("pow_a"));
  EXPECT_EQ(recCall->getTaskFunc<hldb::Function>(), fn) << "the recursive call should resolve back to pow_a itself";

  ASSERT_NE(recCall->getArguments(), nullptr);
  ASSERT_EQ(recCall->getArguments()->size(), 2u) << "'(base, exp - 1)' is 2 positional actual arguments";
  const hldb::RefObj *const arg0 = any_cast<hldb::RefObj>(recCall->getArguments()->at(0));
  ASSERT_NE(arg0, nullptr) << "first positional actual should be RefObj 'base'";
  EXPECT_EQ(arg0->getName(), std::string_view("base"));

  const hldb::Operation *const arg1 = any_cast<hldb::Operation>(recCall->getArguments()->at(1));
  ASSERT_NE(arg1, nullptr) << "second positional actual should be Operation 'exp - 1'";
  EXPECT_EQ(arg1->getOpType(), vpiSubOp);
}

// ---------------------------------------------------------------------------
// assign out1 = pow_a(inp, 3);  assign out2 = pow_b(2, 2);
// ---------------------------------------------------------------------------
TEST_F(FuncArgsTest, ContinuousAssignsCallPowFunctionsPositionally) {
  const hldb::Module *const top = getDut();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 2u);

  bool foundPowA = false, foundPowB = false;
  for (const hldb::ContAssign *const ca : *top->getContAssigns()) {
    ASSERT_NE(ca, nullptr);
    const hldb::FuncCall *const call = ca->getRhs<hldb::FuncCall>();
    ASSERT_NE(ca->getRhs(), nullptr);
    ASSERT_NE(call, nullptr) << "continuous assign RHS should be a FuncCall";
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 2u);
    if (call->getName() == std::string_view("pow_a")) {
      foundPowA = true;
      const hldb::RefObj *const inpArg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
      ASSERT_NE(inpArg, nullptr);
      EXPECT_EQ(inpArg->getName(), std::string_view("inp"));
      const hldb::Constant *const three = any_cast<hldb::Constant>(call->getArguments()->at(1));
      ASSERT_NE(three, nullptr);
      EXPECT_EQ(three->getDecompile(), std::string_view("3"));
    } else if (call->getName() == std::string_view("pow_b")) {
      foundPowB = true;
      const hldb::Constant *const two0 = any_cast<hldb::Constant>(call->getArguments()->at(0));
      const hldb::Constant *const two1 = any_cast<hldb::Constant>(call->getArguments()->at(1));
      ASSERT_NE(two0, nullptr);
      ASSERT_NE(two1, nullptr);
      EXPECT_EQ(two0->getDecompile(), std::string_view("2"));
      EXPECT_EQ(two1->getDecompile(), std::string_view("2"));
    }
  }
  EXPECT_TRUE(foundPowA) << "no continuous assign calls pow_a";
  EXPECT_TRUE(foundPowB) << "no continuous assign calls pow_b";
}

TEST_F(FuncArgsTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
