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

// Spec-based validation of the backslash-newline line continuation in string
// literals per IEEE 1800-2017 §5.9.
//
// Key §5.9 rule under test:
//   "A string that does not fit on one line may be continued on the next line
//    using a backslash-newline sequence. The backslash and the newline are not
//    part of the string."
//
// SV source:
//   $display("broken \
//               line");
//
// The backslash immediately followed by a newline is a line continuation.
// Neither the backslash nor the newline is part of the resulting string.
// The characters on the next source line (spaces + "line") ARE part of the
// string. Schematically:
//
//   "broken \"   →  "broken "       (7 chars; the \ and \n are stripped)
//   "              line"  →  "              line"  (14 spaces + "line" = 18 chars)
//
// Spec-correct string = "broken               line" = 25 characters = 200 bits.
//
// KNOWN SURELOG BUG:
//   Surelog does not treat backslash-newline as a line continuation. Instead it
//   stores the backslash and the newline as literal characters in the string
//   value, producing a 27-character string (7 + 1 backslash + 1 newline +
//   18 continuation chars = 27 chars = 216 bits).
//
//   The two size and value tests below (Argument_SizePerSpec,
//   Argument_ValuePerSpec) FAIL until Surelog implements §5.9 line continuation.

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

#include <string>

namespace hlc {

class StringBrokenLine : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.9-string-broken-line.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("top", d->getAllModules());
}

static const hldb::Begin *getBegin(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getProcesses() || m->getProcesses()->empty()) return nullptr;
  const auto *initial = any_cast<const hldb::Initial *>((*m->getProcesses())[0]);
  if (!initial) return nullptr;
  return initial->getStmt<hldb::Begin>();
}

static const hldb::SysTaskCall *getDisplayCall(const hldb::Design *d) {
  const hldb::Begin *begin = getBegin(d);
  if (!begin || !begin->getStmts() || begin->getStmts()->empty()) return nullptr;
  return any_cast<const hldb::SysTaskCall *>((*begin->getStmts())[0]);
}

static const hldb::Constant *getStringArg(const hldb::Design *d) {
  const hldb::SysTaskCall *call = getDisplayCall(d);
  if (!call || !call->getArguments() || call->getArguments()->empty()) return nullptr;
  return any_cast<const hldb::Constant *>((*call->getArguments())[0]);
}

// ---------------------------------------------------------------------------
// Module structure
// ---------------------------------------------------------------------------
TEST_F(StringBrokenLine, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'top' not found"; }

TEST_F(StringBrokenLine, ModuleHasNoNets) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getNets() || m->getNets()->empty()) << "module top has no net declarations";
}

// ---------------------------------------------------------------------------
// Initial block structure
// ---------------------------------------------------------------------------
TEST_F(StringBrokenLine, InitialBlockHasBegin) { ASSERT_NE(getBegin(m_design), nullptr) << "initial begin not found"; }

TEST_F(StringBrokenLine, BeginHasOneStatement) {
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 1u) << "expected exactly 1 statement: the $display call";
}

// ---------------------------------------------------------------------------
// $display call
// ---------------------------------------------------------------------------
TEST_F(StringBrokenLine, StatementIsDisplayCall) {
  const hldb::SysTaskCall *const call = getDisplayCall(m_design);
  ASSERT_NE(call, nullptr) << "stmt[0] is not a SysTaskCall";
  EXPECT_EQ(call->getName(), "$display") << "system call must be $display";
}

TEST_F(StringBrokenLine, DisplayCallHasOneArgument) {
  const hldb::SysTaskCall *const call = getDisplayCall(m_design);
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr) << "$display has no argument list";
  EXPECT_EQ(call->getArguments()->size(), 1u) << "$display has exactly 1 argument (the broken-line string)";
}

// ---------------------------------------------------------------------------
// §5.9: the string argument structural properties — these pass regardless of
// whether Surelog handles line continuation correctly.
// ---------------------------------------------------------------------------
TEST_F(StringBrokenLine, Argument_IsStringConstType) {
  const hldb::Constant *const c = getStringArg(m_design);
  ASSERT_NE(c, nullptr) << "argument is not a Constant";
  EXPECT_EQ(c->getConstType(), 6) << "§5.9: string literal must be vpiStringConst (6)";
}

TEST_F(StringBrokenLine, Argument_HasStringTypespec) {
  const hldb::Constant *const c = getStringArg(m_design);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getTypespec(), nullptr) << "string constant has no typespec";
  EXPECT_NE(c->getTypespec()->getActual<hldb::StringTypespec>(), nullptr)
      << "§5.9: string literal must have a StringTypespec in UHDM";
}

// ---------------------------------------------------------------------------
// §5.9 line continuation: the backslash-newline pair is not part of the string.
// The resulting string must be:
//   "broken "      (7 chars — source chars before the backslash)
//   + 14 spaces    (leading whitespace on the continuation line, IS part of
//                   the string — the spec removes only the \ and \n themselves)
//   + "line"       (4 chars)
//   = 25 chars × 8 bits = 200 bits.
//
// SURELOG BUG: Surelog stores the backslash and the newline as literal
// characters rather than treating them as a line continuation. It produces a
// 27-character string (7 + 1 + 1 + 18 = 27 chars = 216 bits).
// ---------------------------------------------------------------------------
TEST_F(StringBrokenLine, Argument_SizePerSpec) {
  const hldb::Constant *const c = getStringArg(m_design);
  ASSERT_NE(c, nullptr);
  // §5.9: "broken " (7) + 14 spaces + "line" (4) = 25 chars = 200 bits.
  EXPECT_EQ(c->getSize(), 200) << "§5.9: backslash-newline is a line continuation — \\ and \\n must be "
                                  "stripped. Result: 25 chars = 200 bits. "
                                  "Surelog bug: stores \\ and \\n verbatim, gives 27 chars = 216 bits";
}

TEST_F(StringBrokenLine, Argument_ValuePerSpec) {
  const hldb::Constant *const c = getStringArg(m_design);
  ASSERT_NE(c, nullptr);
  // §5.9: "broken " + 14 spaces of continuation indent + "line".
  // The backslash and the newline are removed by the line-continuation rule.
  const std::string spec_correct = "broken " + std::string(14, ' ') + "line";
  EXPECT_EQ(c->getValue(), spec_correct) << "§5.9: backslash-newline line continuation must be stripped from "
                                            "the string value. Expected: \"broken               line\" (25 chars). "
                                            "Surelog bug: getValue() includes the literal \\ and newline character";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
