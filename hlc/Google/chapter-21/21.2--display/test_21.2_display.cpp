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

// Tests for 21.2--display.sv (:name: display_task, :tags: 21.2,
// :type: simulation parsing)
//
//   module top();
//   initial begin
//     int val = 1234;
//     $display(val);
//   end
//   endmodule
//
// What the source says, and why each check follows from it (IEEE
// 1800-2023 unless noted; every expected value below was derived by
// reading the .sv above, not from running the tool):
//
//   Sec 21.2 "Display system tasks" / Sec 21.2.1 "The display and write
//   tasks": "$display" is the base form of the display task family. The
//   name alone selects the behaviour -- the b/o/h suffixed forms
//   ("$displayb", "$displayo", "$displayh") change only the default
//   radix applied to an argument carrying no format specification, and
//   "$write" differs only in not appending a newline. None of that
//   distinction lives in an operand: there is no format string and no
//   radix argument anywhere in this file. So getName() is the only place
//   the AST records which member of the family was written, and the test
//   below pins it to exactly "$display". A test that merely checked "one
//   SysTaskCall with one argument" would pass just as happily if the call
//   had been recorded as "$write" or "$displayh", which is precisely the
//   regression this file exists to catch.
//
//   Sec 21.2.1 also makes this a task, not a function -- it returns no
//   value and stands alone as a statement -- hence SysTaskCall rather
//   than SysFuncCall. And Sec 21.2 makes "$display" a task the LANGUAGE
//   defines, not one this file supplies: no user-defined system task is
//   registered anywhere in the source, so the call must not be flagged
//   user-defined.
//
//   Sec 23.2.1 "Module definitions": "module top()" has an empty
//   parenthesized port list, so the module declares no ports at all, and
//   the plain "endmodule" (no ": top" repeated after it) carries no end
//   label. Together with the fact that every declaration in the file is
//   block-scoped, module "top" must hold no ports, no nets and no
//   variables -- asserted as absent collections rather than empty ones,
//   since the source creates no such declarations for a collection to
//   hold.
//
//   Sec 9.2.1 "Initial construct" + Sec 9.3.1 "Sequential blocks": the
//   initial construct here wraps a begin-end block, so the Initial's stmt
//   is a Begin (unlike chapter-9/9.2.1--initial.sv, whose
//   single-statement initial body is not wrapped). The block carries no
//   ": label" on either "begin" or the closing "end", so per Sec 9.3.4 it
//   is an unnamed block: both getName() and getEndLabel() are empty.
//
//   Sec 9.3.1 / Sec 6.21: "int val = 1234;" is a block_item_declaration,
//   not a statement. It declares a variable local to the begin-end block,
//   so it lands in the Begin's own getVariables() and NOT in its
//   getStmts(). That is why getStmts() is asserted to hold exactly 1
//   entry -- the single $display call -- while getVariables() holds
//   exactly 1. Asserting the statement count is 1 and not 2 is what
//   proves the declaration was modelled as a block item rather than
//   folded into the statement list as an assignment.
//
//   Sec 6.11.1 "Integer data types": "int" is the 2-state SIGNED 32-bit
//   type, so the declared typespec must be an IntTypespec whose
//   getSigned() is true. Being an atom type rather than a vector type, it
//   also carries no packed range -- "int val" declares no explicit
//   dimensions, so getRanges() must be absent. "int" is also not a
//   net-type keyword (Sec 6.7), and the declaration is inside the block
//   anyway, so module "top" must have zero nets AND zero module-level
//   variables -- every declaration in this file is block-scoped.
//
//   Sec 5.7.1 "Integer literal constants": "1234" is an unsized, unbased
//   decimal literal. Its decompiled text is "1234"; this tool classifies
//   a bare decimal literal as vpiUIntConst (established by
//   chapter-5/5.7.1--integers-unsized.sv and
//   chapter-9/9.2.1--initial.sv).
//
//   Sec 6.21 "Scope and lifetime": the "val" in the $display argument is
//   a simple name reference that must bind back to the block-local
//   declaration -- so the argument is a RefObj whose getName() is "val"
//   and whose getActual<Variable>() is the very same Variable object the
//   Begin's getVariables() returns, not some other "val". The argument
//   must also stay a RefObj rather than a Constant: the source passes the
//   variable, and folding "val" into 1234 here would discard the
//   reference the source actually wrote.
//
// What is NOT checked, and why:
//   - Where the shared IntTypespec node is anchored (design-level
//     getTypespecs() vs. the Begin's own getTypespecs()). This file has
//     no module-level declaration at all, so which scope ends up owning
//     the single "int" typespec is a tool-internal sharing convention,
//     not something the source text decides. The typespec is instead
//     verified through "val"'s own RefTypespec -> IntTypespec chain,
//     which the source does determine.
//   - The actual printed output ("1234" in decimal, followed by a
//     newline). Both the default decimal radix and the trailing newline
//     are run-time formatting properties of $display, observable only by
//     executing the initial block. HLC is a static compiler/elaborator
//     and is not a simulator, so there is no console, output stream or
//     stored rendering anywhere in the model for a test to inspect --
//     the SysTaskCall node carries only its name string and its argument
//     list. This is out of scope permanently rather than a gap waiting
//     to be filled, so no placeholder test is carried for it;
//     OnlyStatementIsASysTaskCallNamedDisplay is the static evidence
//     that this call, and not "$write" or a radix-suffixed variant, is
//     the one the source wrote.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class DisplayTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "21.2--display.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Initial *getInitialProcess() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    return any_cast<hldb::Initial>(top->getProcesses()->at(0));
  }

  static const hldb::Begin *getInitialBody() {
    const hldb::Initial *const init = getInitialProcess();
    return (init == nullptr) ? nullptr : init->getStmt<hldb::Begin>();
  }

  static const hldb::Variable *getVal() {
    const hldb::Begin *const blk = getInitialBody();
    if (blk == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("val", blk->getVariables());
  }

  static const hldb::SysTaskCall *getDisplayCall() {
    const hldb::Begin *const blk = getInitialBody();
    if ((blk == nullptr) || (blk->getStmts() == nullptr) || blk->getStmts()->empty()) return nullptr;
    return any_cast<hldb::SysTaskCall>(blk->getStmts()->at(0));
  }
};

// --- module "top" ------------------------------------------------------------

TEST_F(DisplayTest, DesignHasExactlyOneModuleNamedTop) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  EXPECT_EQ(m_design->getAllModules()->size(), 1u) << "the source declares exactly one module";
  ASSERT_NE(getTop(), nullptr);
  EXPECT_EQ(getTop()->getName(), "top");
}

TEST_F(DisplayTest, TopHasEmptyPortList) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getPorts(), nullptr) << "'module top()' declares no ports, so there is no port collection at all";
}

TEST_F(DisplayTest, TopHasNoEndLabel) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getEndLabel(), "") << "23.2.1: the source closes with a plain 'endmodule', no ': top' repeated";
}

// Every declaration in this file sits inside the begin-end block, so the
// module scope itself must be empty of both nets and variables.
TEST_F(DisplayTest, TopDeclaresNoNetsAndNoModuleLevelVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getNets(), nullptr)
      << "'int' is not a net-type keyword (IEEE 1800-2023 Sec 6.7) and the module body declares nothing";
  EXPECT_EQ(top->getVariables(), nullptr)
      << "'int val' is a block_item_declaration inside 'initial begin', not a module-level variable";
}

// --- initial / begin structure -----------------------------------------------

TEST_F(DisplayTest, TopHasExactlyOneProcessAndItIsInitial) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 1u) << "the source has one 'initial' and no 'always'";
  EXPECT_NE(getInitialProcess(), nullptr) << "the one process must be an Initial";
}

TEST_F(DisplayTest, InitialStmtIsABeginBlock) {
  const hldb::Initial *const init = getInitialProcess();
  ASSERT_NE(init, nullptr);
  EXPECT_NE(init->getStmt<hldb::Begin>(), nullptr)
      << "9.3.1: the initial body is wrapped in 'begin'/'end', so its stmt is a Begin";
}

TEST_F(DisplayTest, BeginBlockIsUnnamed) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  EXPECT_TRUE(blk->getName().empty())
      << "9.3.4: the source writes a bare 'begin' with no ': label', so the block carries no name, but got: "
      << blk->getName();
  EXPECT_EQ(blk->getEndLabel(), "")
      << "9.3.4: the closing 'end' repeats no ': label' either, so the block's end label is empty too";
}

// The declaration is a block item, not a statement: exactly 1 statement.
TEST_F(DisplayTest, BeginBlockHasExactlyOneStatement) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 1u)
      << "'$display(val);' is the only statement; 'int val = 1234;' is a block_item_declaration, not a second one";
}

// --- the block-local variable "val" ------------------------------------------

TEST_F(DisplayTest, BeginBlockDeclaresExactlyOneVariableNamedVal) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getVariables(), nullptr);
  EXPECT_EQ(blk->getVariables()->size(), 1u);
  const hldb::Variable *const val = getVal();
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getName(), "val");
}

TEST_F(DisplayTest, ValTypespecIsSignedRangelessIntTypespec) {
  const hldb::Variable *const val = getVal();
  ASSERT_NE(val, nullptr);
  ASSERT_NE(val->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = val->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr) << "6.11.1: 'int' must elaborate to an IntTypespec";
  EXPECT_TRUE(it->getSigned()) << "6.11.1: 'int' is the 2-state SIGNED 32-bit type";
  EXPECT_EQ(it->getRanges(), nullptr)
      << "6.11.1: 'int' is an atom type, and 'int val' declares no explicit packed dimensions";
}

TEST_F(DisplayTest, ValInitialValueIsConstant1234) {
  const hldb::Variable *const val = getVal();
  ASSERT_NE(val, nullptr);
  const hldb::Constant *const initValue = val->getValue<hldb::Constant>();
  ASSERT_NE(initValue, nullptr) << "'int val = 1234' carries a declaration-time initializer";
  EXPECT_EQ(initValue->getDecompile(), "1234");
  EXPECT_EQ(initValue->getConstType(), vpiUIntConst) << "5.7.1: bare decimal literal -> constType unsigned int (9)";
}

// --- the $display call -------------------------------------------------------

// 21.2.1: the task NAME is the only thing separating "$display" from
// "$write" (no trailing newline) and from "$displayb/o/h" (other default
// radix). Pin it exactly.
TEST_F(DisplayTest, OnlyStatementIsASysTaskCallNamedDisplay) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr) << "21.2.1: $display is a task, so the only statement must be a SysTaskCall";
  EXPECT_EQ(call->getName(), "$display")
      << "the call must keep the plain '$display' name, not a radix-suffixed form and not '$write'";
  EXPECT_FALSE(call->getUserDefn())
      << "21.2: '$display' is a standard system task defined by the language, and this file registers no "
         "user-defined system task, so the call must not be marked user-defined";
}

// "$display(val)" passes a single expression and no format string.
TEST_F(DisplayTest, DisplayCallTakesExactlyOneArgument) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr) << "$display should carry an argument list";
  EXPECT_EQ(call->getArguments()->size(), 1u) << "the source writes one argument and no format string";
}

// 6.21: the reference must bind to the one block-local declaration, and it
// must stay a reference rather than being folded to the literal 1234.
TEST_F(DisplayTest, DisplayArgumentIsRefObjValBoundToTheBlockLocalDeclaration) {
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::Variable *const val = getVal();
  ASSERT_NE(val, nullptr);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr) << "$display's argument is the name 'val', so it is a RefObj, not a folded Constant";
  EXPECT_EQ(arg->getName(), "val");
  EXPECT_EQ(arg->getActual<hldb::Variable>(), val)
      << "$display's 'val' must resolve to the block-local 'int val = 1234', not a different object";
}

// --- compiler diagnostics ----------------------------------------------------

// The one construct here with real binding risk is "val" inside the
// $display argument list resolving back to the block-local declaration.
TEST_F(DisplayTest, ValReferenceIsNotAFailedBind) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr)
      << "'val' in the $display call must bind to the block-local 'int val = 1234' declaration";
}

TEST_F(DisplayTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0) << "21.2.1: $display is a standard system task, not an unknown one";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
