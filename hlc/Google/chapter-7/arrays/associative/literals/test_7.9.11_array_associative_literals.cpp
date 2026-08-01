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

// Tests for literals.sv (tags: 7.9.11)
//   module top ();
//     string words[int] = '{default: "hello"};
//     initial begin
//       $display(":assert: ('%s' == 'hello')", words[1]);
//       words[1] = "world";
//       $display(":assert: (('%s' == 'hello') and ('%s' == 'world'))", words[0], words[1]);
//     end
//   endmodule
//
// Checked:
//   - design has module top
//   - module has exactly 1 variable: 'words' (assoc ArrayTypespec, idx=IntTypespec, elem=StringTypespec)
//   - variable vpiValue = assign-pattern Operation (vpiAssignmentPatternOp=87) with 1 TaggedPattern
//   - TaggedPattern tag = RefObj "default"; pattern = Constant value "hello"
//   - 1 Initial process; Begin with 3 stmts
//   - stmt[0]: $display(2 args) ? BitSelect words[1] as second arg
//   - stmt[1]: blocking Assignment words[1]="world" (BitSelect lhs, Constant rhs)
//   - stmt[2]: $display(3 args) ? words[0] and words[1] BitSelects verified
//   - top has no continuous assignments
//
// Also checked:
//   - HLC reports no compile errors for the '{default: "hello"} assignment
//     pattern (structural proxy for "this construct is legal"; the actual
//     default-value read for unset keys is runtime-only, out of scope here)
//   - format string in stmt[2] verified

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/variable.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/tagged_pattern.h>

namespace hlc {

class Literals : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "literals.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module ----

TEST_F(Literals, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

// --- variable "words" : string[int] with default value ----

TEST_F(Literals, ModuleHasOneVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
}

TEST_F(Literals, VariableNameIsWords) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->at(0)->getName(), "words");
}

TEST_F(Literals, VariableHasAssociativeArrayTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const variable = top->getVariables()->at(0);
  ASSERT_NE(variable, nullptr);
  const hldb::RefTypespec *const rt = variable->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::ArrayTypespec *const at = rt->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), 3);  // associative = 3
}

TEST_F(Literals, AssocArrayKeyTypeIsInt) {
  // `int` (2-state) is the index type for string words[int]
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getIndexTypespec(), nullptr);
  EXPECT_NE(at->getIndexTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(Literals, AssocArrayValueTypeIsString) {
  // element type is StringTypespec because `string words[int]`
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::StringTypespec>(), nullptr);
}

// --- default value: '{default: "hello"} ----

TEST_F(Literals, VariableHasAssignPatternValue) {
  // The initializer `= '{default: "hello"}` is stored as an Operation
  // with vpiAssignmentPatternOp (87) attached to the variable as vpiValue.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const variable = top->getVariables()->at(0);
  ASSERT_NE(variable, nullptr);
  const hldb::Operation *const op = variable->getValue<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiAssignmentPatternOp);
}

TEST_F(Literals, DefaultPatternHasOneOperand) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Operation *const op = top->getVariables()->at(0)->getValue<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 1u);
}

TEST_F(Literals, DefaultPatternTagIsDefault) {
  // Tag is a RefObj with name "default", representing the `default:` key
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Operation *const op = top->getVariables()->at(0)->getValue<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  const hldb::TaggedPattern *const tp = any_cast<hldb::TaggedPattern>(op->getOperands()->at(0));
  ASSERT_NE(tp, nullptr);
  const hldb::RefObj *const tag = tp->getTag<hldb::RefObj>();
  ASSERT_NE(tag, nullptr);
  EXPECT_EQ(tag->getName(), "default");
}

TEST_F(Literals, DefaultPatternValueIsHello) {
  // Pattern is a Constant with vpiValue="hello" (the literal string "hello")
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Operation *const op = top->getVariables()->at(0)->getValue<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  const hldb::TaggedPattern *const tp = any_cast<hldb::TaggedPattern>(op->getOperands()->at(0));
  ASSERT_NE(tp, nullptr);
  const hldb::Constant *const pattern = tp->getPattern<hldb::Constant>();
  ASSERT_NE(pattern, nullptr);
  EXPECT_EQ(pattern->getValue(), "hello");
}

// --- initial block ----

TEST_F(Literals, InitialBodyIsBeginWith3Stmts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const body = init->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 3u);
}

TEST_F(Literals, FirstStmtIsDisplayWithDefaultHello) {
  // $display(":assert: ('%s' == 'hello')", words[1])
  // words[1] reads the default value before any assignment
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const body = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(body->getStmts()->at(0));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  EXPECT_EQ(disp->getArguments()->size(), 2u);
  // second arg is BitSelect words[1]
  const hldb::BitSelect *const bs = any_cast<hldb::BitSelect>(disp->getArguments()->at(1));
  ASSERT_NE(bs, nullptr);
  EXPECT_EQ(bs->getName(), "words[1]");
}

TEST_F(Literals, SecondStmtAssignsWords1ToWorld) {
  // words[1] = "world"
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const body = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const hldb::BitSelect *const lhs = assign->getLhs<hldb::BitSelect>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "words[1]");
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getValue(), "world");
}

TEST_F(Literals, ThirdStmtIsDisplayWithBothValues) {
  // $display(":assert: ...", words[0], words[1]) ? 3 arguments
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const body = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(body->getStmts()->at(2));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  EXPECT_EQ(disp->getArguments()->size(), 3u);
  const hldb::BitSelect *const words0 = any_cast<hldb::BitSelect>(disp->getArguments()->at(1));
  ASSERT_NE(words0, nullptr);
  EXPECT_EQ(words0->getName(), "words[0]");
  const hldb::BitSelect *const words1 = any_cast<hldb::BitSelect>(disp->getArguments()->at(2));
  ASSERT_NE(words1, nullptr);
  EXPECT_EQ(words1->getName(), "words[1]");
}

TEST_F(Literals, ThirdStmtFormatStringIsHelloWorldAssert) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const body = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(body->getStmts()->at(2));
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getDecompile(), "\":assert: (('%s' == 'hello') and ('%s' == 'world'))\"");
}

// --- structural completeness ----

TEST_F(Literals, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty())
      << "'{default: \"hello\"} is stored as variable vpiValue, not a ContAssign";
}

TEST_F(Literals, CompilerHasNoErrors) {
  // '{default: "hello"} is a legal assignment pattern; HLC must accept it without diagnostics.
  const hlc::ErrorContainer::Stats stats = m_compiler->getErrorStats();
  EXPECT_EQ(stats.nbError, 0) << "default-value assignment pattern must not produce compile errors";
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
