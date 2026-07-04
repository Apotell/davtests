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

// Spec-based validation of basic string literal usage per IEEE 1800-2017 §5.9.
//
// Key §5.9 rules under test:
//   1. "A string is a sequence of characters enclosed by double quotes."
//   2. "The number of bits required to hold a string is 8 times the number of
//      characters in the string."
//   3. String literals used in expressions are treated as unsigned integer
//      constants of type vpiStringConst (6).
//
// SV source (module top):
//   initial begin
//     $display("one line");
//   end
//
// UHDM representation:
//   One $display SysFuncCall with one Constant argument:
//     constType = vpiStringConst (6)
//     size      = 64   — "one line" = 8 characters × 8 bits = 64 bits (correct)
//     value     = "one line"
//     typespec  = StringTypespec
//
// Note on size correctness:
//   Unlike §5.9.1 escape-sequence strings (which Surelog stores verbatim and
//   gets the wrong size), plain strings with no escape sequences are sized
//   correctly. "one line" has 8 characters so size=64 is the spec-correct
//   value. All tests below PASS.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>

namespace hlc {

class StringBasics : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.9-string-basics.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@top", d->getAllModules());
}

static const hldb::Begin *getBegin(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getProcesses() || m->getProcesses()->empty()) return nullptr;
  const auto *initial = any_cast<const hldb::Initial *>((*m->getProcesses())[0]);
  if (!initial) return nullptr;
  return initial->getStmt<hldb::Begin>();
}

static const hldb::SysFuncCall *getDisplayCall(const hldb::Design *d) {
  const hldb::Begin *begin = getBegin(d);
  if (!begin || !begin->getStmts() || begin->getStmts()->empty()) return nullptr;
  return any_cast<const hldb::SysFuncCall *>((*begin->getStmts())[0]);
}

static const hldb::Constant *getStringArg(const hldb::Design *d) {
  const hldb::SysFuncCall *call = getDisplayCall(d);
  if (!call || !call->getArguments() || call->getArguments()->empty()) return nullptr;
  return any_cast<const hldb::Constant *>((*call->getArguments())[0]);
}

// ---------------------------------------------------------------------------
// Module structure
// ---------------------------------------------------------------------------
TEST_F(StringBasics, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found"; }

TEST_F(StringBasics, ModuleHasNoNets) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  // §5.9: the module only contains an initial block — no variable declarations.
  EXPECT_TRUE(!m->getNets() || m->getNets()->empty()) << "module top has no net/variable declarations";
}

// ---------------------------------------------------------------------------
// Initial block structure
// ---------------------------------------------------------------------------
TEST_F(StringBasics, InitialBlockHasBegin) { ASSERT_NE(getBegin(m_design), nullptr) << "initial begin not found"; }

TEST_F(StringBasics, BeginHasOneStatement) {
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 1u) << "expected exactly 1 statement: $display(\"one line\")";
}

// ---------------------------------------------------------------------------
// $display call
// ---------------------------------------------------------------------------
TEST_F(StringBasics, StatementIsDisplayCall) {
  const hldb::SysFuncCall *const call = getDisplayCall(m_design);
  ASSERT_NE(call, nullptr) << "stmt[0] is not a SysFuncCall";
  EXPECT_EQ(call->getName(), "$display") << "system call must be $display";
}

TEST_F(StringBasics, DisplayCallHasOneArgument) {
  const hldb::SysFuncCall *const call = getDisplayCall(m_design);
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr) << "$display has no argument list";
  EXPECT_EQ(call->getArguments()->size(), 1u) << "$display(\"one line\") has exactly 1 argument";
}

// ---------------------------------------------------------------------------
// §5.9: the string literal "one line" as a $display argument.
// vpiStringConst (6) is the correct const type for all string literals.
// ---------------------------------------------------------------------------
TEST_F(StringBasics, Argument_IsStringConstType) {
  const hldb::Constant *const c = getStringArg(m_design);
  ASSERT_NE(c, nullptr) << "argument is not a Constant";
  EXPECT_EQ(c->getConstType(), 6) << "§5.9: string literal must be vpiStringConst (6)";
}

// ---------------------------------------------------------------------------
// §5.9: size = 8 × number of characters.
// "one line" = 8 characters → 8 × 8 = 64 bits.
// No escape sequences — Surelog correctly computes the size (64).
// ---------------------------------------------------------------------------
TEST_F(StringBasics, Argument_SizeIs64PerSpec) {
  const hldb::Constant *const c = getStringArg(m_design);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 64) << "§5.9: \"one line\" = 8 characters × 8 bits = 64 bits";
}

// ---------------------------------------------------------------------------
// §5.9: the string value stored in UHDM must match the source characters.
// vpiValue returns the raw string content without surrounding double quotes.
// ---------------------------------------------------------------------------
TEST_F(StringBasics, Argument_ValueIsOneLine) {
  const hldb::Constant *const c = getStringArg(m_design);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue(), "one line") << "§5.9: string literal value must be \"one line\"";
}

// ---------------------------------------------------------------------------
// §5.9: string literals carry a StringTypespec — not LogicTypespec or any
// other integral typespec.
// ---------------------------------------------------------------------------
TEST_F(StringBasics, Argument_HasStringTypespec) {
  const hldb::Constant *const c = getStringArg(m_design);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getTypespec(), nullptr) << "string constant has no typespec";
  EXPECT_NE(c->getTypespec()->getActual<hldb::StringTypespec>(), nullptr)
      << "§5.9: string literal must have a StringTypespec in UHDM";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
