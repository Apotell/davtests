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

// Tests for FuncAttrib/dut.sv:
//   function [7:0] do_add;
//   input [7:0] inp_a;
//   input [7:0] inp_b;
//
//   do_add = inp_a + inp_b;
//
//   endfunction
//
//   module foo(clk, rst, inp_a, inp_b, out);
//   input  wire clk;
//   input  wire rst;
//   input  wire [7:0] inp_a;
//   input  wire [7:0] inp_b;
//   output reg  [7:0] out;
//
//   always @(posedge clk)
//     if (rst) out <= 0;
//     else
//       out <=
//       do_add (* combinational_adder *) (inp_a, inp_b);
//
//   endmodule
//
// What to check and why (IEEE 1800-2023, checked before any test code was
// written -- no .log file consulted for this file's expected shape, only
// the standard text, the grammar, and the real hldb API headers):
//
//   Annex A.8.2 grammar: 'tf_call ::= ps_or_hierarchical_tf_identifier
//   {attribute_instance} [ '(' list_of_arguments ')' ]' -- an
//   attribute_instance is explicitly permitted BETWEEN a subroutine call's
//   name and its argument list. 'do_add (* combinational_adder *)
//   (inp_a, inp_b)' is exactly this shape: per Sec 5.12, the attribute
//   attaches to the do_add tf_call (FuncCall) node itself, not to the
//   do_add function *declaration*, and not hoisted onto the enclosing
//   module or statement -- same "attach to the specific node, not
//   hoisted" rule the existing 5.12-attributes-operator test established
//   for attributes on operator operands.
//
//   Sec 13.4.1 (old-style function declaration): 'function [7:0] do_add;'
//   with 'input [7:0] inp_a; input [7:0] inp_b;' declared as separate
//   statements is the same non-ANSI style exercised by FuncArgs -- two
//   IODecls, both direction vpiInput, each an implicit unsigned [7:0]
//   vector; 'do_add' is declared at compilation-unit scope (outside any
//   module), so it must be found via Design::getTaskFuncs(), not via any
//   module.
//
//   Sec 6.7: 'input wire clk/rst/inp_a/inp_b' and 'output reg [7:0] out'
//   carry explicit type/net keywords: the wire ports are Nets, and 'reg'
//   is a variable data type (Sec 6.8), so 'out' must be a Variable.
//
//   Sec 9.2.2.1 / 9.4.2: 'always @(posedge clk) if (rst) ... else ...' is
//   a single-event-controlled always process whose body is an IfElse
//   (no begin-end on either branch); the else-branch's non-blocking
//   Assignment RHS is the attributed do_add call.
//
// What is NOT checked and why:
//   - the runtime-evaluated numeric result of do_add(inp_a, inp_b) is a
//     simulation-time concept.
//   - do_add's own body ('do_add = inp_a + inp_b;') is not walked in
//     detail; this test is about the attribute on the *call*, not the
//     callee's implementation.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/attribute.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/if_else.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FuncAttribTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FuncAttrib.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getFoo() { return hldb::findByName<hldb::Module>("foo", m_design->getAllModules()); }

  static const hldb::Function *getDoAdd() {
    if (m_design->getTaskFuncs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("do_add", m_design->getTaskFuncs());
  }

  // Walks foo's single always process down to the else-branch's
  // non-blocking Assignment RHS: the attributed do_add() call.
  static const hldb::FuncCall *getDoAddCall() {
    const hldb::Module *const top = getFoo();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Always *const alw = any_cast<hldb::Always>(top->getProcesses()->at(0));
    if (alw == nullptr) return nullptr;
    const hldb::EventControl *const ec = alw->getStmt<hldb::EventControl>();
    if (ec == nullptr) return nullptr;
    const hldb::IfElse *const ifElse = ec->getStmt<hldb::IfElse>();
    if (ifElse == nullptr || ifElse->getElseStmt() == nullptr) return nullptr;
    const hldb::Assignment *const elseAssign = any_cast<hldb::Assignment>(ifElse->getElseStmt());
    if (elseAssign == nullptr) return nullptr;
    return elseAssign->getRhs<hldb::FuncCall>();
  }
};

TEST_F(FuncAttribTest, ModuleAndCompilationUnitFunctionExist) {
  EXPECT_NE(getFoo(), nullptr);
  EXPECT_NE(getDoAdd(), nullptr) << "'do_add' is declared outside any module (compilation-unit scope)";
}

// ---------------------------------------------------------------------------
// function [7:0] do_add; input [7:0] inp_a; input [7:0] inp_b; ...
// ---------------------------------------------------------------------------
TEST_F(FuncAttribTest, DoAddHasTwoInputArguments) {
  const hldb::Function *const fn = getDoAdd();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 2u);
  bool hasA = false, hasB = false;
  for (const hldb::IODecl *const io : *fn->getIODecls()) {
    ASSERT_NE(io, nullptr);
    EXPECT_EQ(io->getDirection(), vpiInput);
    if (io->getName() == std::string_view("inp_a")) hasA = true;
    if (io->getName() == std::string_view("inp_b")) hasB = true;
  }
  EXPECT_TRUE(hasA) << "formal 'inp_a' missing";
  EXPECT_TRUE(hasB) << "formal 'inp_b' missing";
}

// ---------------------------------------------------------------------------
// module foo ports
// ---------------------------------------------------------------------------
TEST_F(FuncAttribTest, ClkRstAreNetsAndOutIsAVariable) {
  const hldb::Module *const top = getFoo();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("clk", top->getNets()), nullptr) << "'input wire clk' should be a Net";
  EXPECT_NE(hldb::findByName<hldb::Net>("rst", top->getNets()), nullptr) << "'input wire rst' should be a Net";
  EXPECT_NE(hldb::findByName<hldb::Net>("inp_a", top->getNets()), nullptr)
      << "'input wire [7:0] inp_a' should be a Net";
  EXPECT_NE(hldb::findByName<hldb::Net>("inp_b", top->getNets()), nullptr)
      << "'input wire [7:0] inp_b' should be a Net";

  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("out", top->getVariables()), nullptr)
      << "Sec 6.8: 'output reg [7:0] out' should be a Variable, not a Net";
}

// ---------------------------------------------------------------------------
// always @(posedge clk) if (rst) out <= 0; else out <= do_add(*...*)(...);
// ---------------------------------------------------------------------------
TEST_F(FuncAttribTest, AlwaysBodyIsIfElseOnRstWithNonBlockingAssignments) {
  const hldb::Module *const top = getFoo();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Always *const alw = any_cast<hldb::Always>(top->getProcesses()->at(0));
  ASSERT_NE(alw, nullptr);
  const hldb::EventControl *const ec = alw->getStmt<hldb::EventControl>();
  ASSERT_NE(alw->getStmt(), nullptr);
  ASSERT_NE(ec, nullptr) << "'@(posedge clk)' should produce an EventControl";
  const hldb::IfElse *const ifElse = ec->getStmt<hldb::IfElse>();
  ASSERT_NE(ec->getStmt(), nullptr);
  ASSERT_NE(ifElse, nullptr) << "'if (rst) ... else ...' should be an IfElse";

  const hldb::Assignment *const thenAssign = any_cast<hldb::Assignment>(ifElse->getStmt());
  ASSERT_NE(ifElse->getStmt(), nullptr);
  ASSERT_NE(thenAssign, nullptr) << "'out <= 0;' should be an Assignment";
  EXPECT_FALSE(thenAssign->getBlocking()) << "'<=' is a non-blocking assignment";

  ASSERT_NE(ifElse->getElseStmt(), nullptr);
  const hldb::Assignment *const elseAssign = any_cast<hldb::Assignment>(ifElse->getElseStmt());
  ASSERT_NE(elseAssign, nullptr) << "the else-branch should be a single Assignment";
  EXPECT_FALSE(elseAssign->getBlocking()) << "'<=' is a non-blocking assignment";
}

// ---------------------------------------------------------------------------
// do_add (* combinational_adder *) (inp_a, inp_b)
// ---------------------------------------------------------------------------
TEST_F(FuncAttribTest, DoAddCallResolvesWithTwoArguments) {
  const hldb::FuncCall *const call = getDoAddCall();
  ASSERT_NE(call, nullptr) << "else-branch RHS should be a FuncCall to 'do_add'";
  EXPECT_EQ(call->getName(), std::string_view("do_add"));
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), getDoAdd());
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);
  const hldb::RefObj *const argA = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  const hldb::RefObj *const argB = any_cast<hldb::RefObj>(call->getArguments()->at(1));
  ASSERT_NE(argA, nullptr);
  ASSERT_NE(argB, nullptr);
  EXPECT_EQ(argA->getName(), std::string_view("inp_a"));
  EXPECT_EQ(argB->getName(), std::string_view("inp_b"));
}

TEST_F(FuncAttribTest, DoAddCallHasCombinationalAdderAttribute) {
  const hldb::FuncCall *const call = getDoAddCall();
  ASSERT_NE(call, nullptr);

  // Annex A.8.2: the attribute_instance between the tf identifier and the
  // argument list attaches to the tf_call node itself.
  ASSERT_NE(call->getAttributes(), nullptr)
      << "'do_add (* combinational_adder *) (inp_a, inp_b)' should carry the attribute on the FuncCall node";
  ASSERT_EQ(call->getAttributes()->size(), 1u);
  const hldb::Attribute *const attr = (*call->getAttributes())[0];
  ASSERT_NE(attr, nullptr);
  EXPECT_EQ(attr->getName(), std::string_view("combinational_adder"));
  // '(* combinational_adder *)' has no '= value', so it defaults to 1
  // (IEEE 1800-2023 5.12: "the value shall default to 1").
  const hldb::Constant *const value = attr->getValue<hldb::Constant>();
  ASSERT_NE(attr->getValue(), nullptr) << "a defaulted attribute should still carry a value of 1";
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getDecompile(), std::string_view("1"));
}

TEST_F(FuncAttribTest, CompilerReportsZeroErrors) {
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
