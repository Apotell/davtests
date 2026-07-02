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

// Spec-based validation of special character escape sequences in string
// literals per IEEE 1800-2017 sec. 5.9.1.
//
// sec. 5.9.1 rules under test:
//   Each escape sequence in a string literal represents exactly one character.
//   Valid escape sequences and their ASCII values:
//     \n  -- newline         (0x0A)
//     \t  -- horizontal tab  (0x09)
//     \\  -- backslash       (0x5C)
//     \"  -- double quote    (0x22)
//     \v  -- vertical tab    (0x0B)
//     \f  -- form feed       (0x0C)
//     \a  -- bell            (0x07)
//     \ooo -- octal value    (e.g. \123 = 0x53 = 'S')
//     \xhh -- hex value      (e.g. \x12 = 0x12)
//
// SV source (module top, initial begin):
//   $display("newline \n");       // call 0 -- \n  = 1 char -> string: 9 chars
//   $display("tab \t");           // call 1 -- \t  = 1 char -> string: 5 chars
//   $display("backslash \\");     // call 2 -- \\  = 1 char -> string: 11 chars
//   $display("quote \"");         // call 3 -- \"  = 1 char -> string: 7 chars
//   $display("vertical tab \v");  // call 4 -- \v  = 1 char -> string: 14 chars
//   $display("form feed \f");     // call 5 -- \f  = 1 char -> string: 11 chars
//   $display("bell \a");          // call 6 -- \a  = 1 char -> string: 6 chars
//   $display("octal \123");       // call 7 -- \123 = 'S'  -> string: 7 chars
//   $display("hex \x12");         // call 8 -- \x12 = 0x12 -> string: 5 chars
//
// Spec-correct UHDM sizes (1 char = 8 bits):
//   call 0: 9  x 8 = 72  bits    call 4: 14 x 8 = 112 bits
//   call 1: 5  x 8 = 40  bits    call 5: 11 x 8 = 88  bits
//   call 2: 11 x 8 = 88  bits    call 6: 6  x 8 = 48  bits
//   call 3: 7  x 8 = 56  bits    call 7: 7  x 8 = 56  bits
//                                 call 8: 5  x 8 = 40  bits
//
// KNOWN SURELOG BUG (all 9 calls):
//   Surelog stores escape sequences verbatim instead of expanding them to their
//   ASCII values. Simple escapes (\n, \t, etc.) consume 2 bytes instead of 1
//   (adding 8 bits). Multi-char escapes (\ooo, \xhh) consume 4 bytes instead
//   of 1 (adding 24 bits). All size tests below will FAIL until fixed.
//   Additionally, Surelog emits WRN:PP0118 for \123, confirming it does not
//   recognize the octal escape format defined in sec. 5.9.1.

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

class StringSpecialChars : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.9.1-string-special-chars.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@top", d->getAllModules());
}

static const hldb::Begin *getBegin(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getProcesses() || m->getProcesses()->empty()) return nullptr;
  const auto *initial =
      any_cast<const hldb::Initial *>((*m->getProcesses())[0]);
  if (!initial) return nullptr;
  return initial->getStmt<hldb::Begin>();
}

static const hldb::SysFuncCall *getDisplayCall(const hldb::Design *d,
                                                std::size_t index) {
  const hldb::Begin *begin = getBegin(d);
  if (!begin || !begin->getStmts()) return nullptr;
  if (index >= begin->getStmts()->size()) return nullptr;
  return any_cast<const hldb::SysFuncCall *>((*begin->getStmts())[index]);
}

static const hldb::Constant *getStringArg(const hldb::SysFuncCall *call) {
  if (!call || !call->getArguments() || call->getArguments()->empty())
    return nullptr;
  return any_cast<const hldb::Constant *>((*call->getArguments())[0]);
}

// ---------------------------------------------------------------------------
// Module structure
// ---------------------------------------------------------------------------
TEST_F(StringSpecialChars, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found";
}

TEST_F(StringSpecialChars, NoNetsInModule) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getNets() || m->getNets()->empty())
      << "module has no net declarations -- only $display calls";
}

TEST_F(StringSpecialChars, InitialBlockHasBegin) {
  ASSERT_NE(getBegin(m_design), nullptr);
}

TEST_F(StringSpecialChars, BeginHasNineStatements) {
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 9u)
      << "expected 9 $display calls (one per escape sequence)";
}

// ---------------------------------------------------------------------------
// All 9 statements must be $display system calls.
// ---------------------------------------------------------------------------
TEST_F(StringSpecialChars, AllStatementsAreDisplayCalls) {
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  for (std::size_t i = 0; i < begin->getStmts()->size(); ++i) {
    const auto *call =
        any_cast<const hldb::SysFuncCall *>((*begin->getStmts())[i]);
    ASSERT_NE(call, nullptr) << "stmt[" << i << "] is not a SysFuncCall";
    EXPECT_EQ(call->getName(), "$display")
        << "stmt[" << i << "] should be $display";
  }
}

// ---------------------------------------------------------------------------
// sec. 5.9.1: string literals have constType = vpiStringConst (6) and a
// StringTypespec. These structural checks pass regardless of escape handling.
// ---------------------------------------------------------------------------
TEST_F(StringSpecialChars, AllArgumentsAreStringConstType) {
  for (std::size_t i = 0; i < 9; ++i) {
    const auto *call = getDisplayCall(m_design, i);
    ASSERT_NE(call, nullptr) << "call[" << i << "] is null";
    const auto *c = getStringArg(call);
    ASSERT_NE(c, nullptr) << "call[" << i << "] argument is null";
    EXPECT_EQ(c->getConstType(), 6)
        << "call[" << i << "]: string literal must be vpiStringConst (6)";
  }
}

TEST_F(StringSpecialChars, AllArgumentsHaveStringTypespec) {
  for (std::size_t i = 0; i < 9; ++i) {
    const auto *call = getDisplayCall(m_design, i);
    ASSERT_NE(call, nullptr) << "call[" << i << "] is null";
    const auto *c = getStringArg(call);
    ASSERT_NE(c, nullptr) << "call[" << i << "] argument is null";
    ASSERT_NE(c->getTypespec(), nullptr)
        << "call[" << i << "] argument has no typespec";
    EXPECT_NE(c->getTypespec()->getActual<hldb::StringTypespec>(), nullptr)
        << "call[" << i << "]: string literal must have StringTypespec";
  }
}

// ---------------------------------------------------------------------------
// sec. 5.9.1 escape sequence sizes.
// Each escape = exactly 1 character = 8 bits. The size of each string
// constant in UHDM must equal (number of literal chars + 1) x 8.
//
// SURELOG BUG: Surelog stores escape sequences verbatim rather than expanding
// them. Simple escapes (\n etc.) add 8 extra bits; \ooo and \xhh add 24 extra
// bits. All tests below FAIL until Surelog implements sec. 5.9.1 escape expansion.
// ---------------------------------------------------------------------------

// "newline \n" -- 8 literal chars + 1 newline char (0x0A) = 9 chars = 72 bits
TEST_F(StringSpecialChars, Call0_Newline_SizePerSpec) {
  const auto *c = getStringArg(getDisplayCall(m_design, 0));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 72)
      << "sec. 5.9.1: \\n = 1 char (0x0A) -> \"newline \\n\" = 9 chars = 72 bits; "
         "Surelog bug: stores \\n as 2 chars, gives 80";
}

// "tab \t" -- 4 literal chars + 1 tab char (0x09) = 5 chars = 40 bits
TEST_F(StringSpecialChars, Call1_Tab_SizePerSpec) {
  const auto *c = getStringArg(getDisplayCall(m_design, 1));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 40)
      << "sec. 5.9.1: \\t = 1 char (0x09) -> \"tab \\t\" = 5 chars = 40 bits; "
         "Surelog bug: stores \\t as 2 chars, gives 48";
}

// "backslash \\" -- 10 literal chars + 1 backslash (0x5C) = 11 chars = 88 bits
TEST_F(StringSpecialChars, Call2_Backslash_SizePerSpec) {
  const auto *c = getStringArg(getDisplayCall(m_design, 2));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 88)
      << "sec. 5.9.1: \\\\ = 1 char (0x5C) -> \"backslash \\\\\" = 11 chars = 88 bits; "
         "Surelog bug: stores \\\\ as 2 chars, gives 96";
}

// "quote \"" -- 6 literal chars + 1 double quote (0x22) = 7 chars = 56 bits
TEST_F(StringSpecialChars, Call3_Quote_SizePerSpec) {
  const auto *c = getStringArg(getDisplayCall(m_design, 3));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 56)
      << "sec. 5.9.1: \\\" = 1 char (0x22) -> \"quote \\\"\" = 7 chars = 56 bits; "
         "Surelog bug: stores \\\" as 2 chars, gives 64";
}

// "vertical tab \v" -- 13 literal chars + 1 vertical tab (0x0B) = 14 chars = 112 bits
TEST_F(StringSpecialChars, Call4_VerticalTab_SizePerSpec) {
  const auto *c = getStringArg(getDisplayCall(m_design, 4));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 112)
      << "sec. 5.9.1: \\v = 1 char (0x0B) -> \"vertical tab \\v\" = 14 chars = 112 bits; "
         "Surelog bug: stores \\v as 2 chars, gives 120";
}

// "form feed \f" -- 10 literal chars + 1 form feed (0x0C) = 11 chars = 88 bits
TEST_F(StringSpecialChars, Call5_FormFeed_SizePerSpec) {
  const auto *c = getStringArg(getDisplayCall(m_design, 5));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 88)
      << "sec. 5.9.1: \\f = 1 char (0x0C) -> \"form feed \\f\" = 11 chars = 88 bits; "
         "Surelog bug: stores \\f as 2 chars, gives 96";
}

// "bell \a" -- 5 literal chars + 1 bell char (0x07) = 6 chars = 48 bits
TEST_F(StringSpecialChars, Call6_Bell_SizePerSpec) {
  const auto *c = getStringArg(getDisplayCall(m_design, 6));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 48)
      << "sec. 5.9.1: \\a = 1 char (0x07) -> \"bell \\a\" = 6 chars = 48 bits; "
         "Surelog bug: stores \\a as 2 chars, gives 56";
}

// "octal \123" -- 6 literal chars + 1 octal char (0x53 = 'S') = 7 chars = 56 bits
// sec. 5.9.1: \ooo is 1-3 octal digits representing the ASCII value.
// Surelog emits WRN:PP0118 for \123, does not recognize the octal format,
// and stores the 4-char sequence \, 1, 2, 3 verbatim -> 10 chars -> 80 bits.
TEST_F(StringSpecialChars, Call7_OctalEscape_SizePerSpec) {
  const auto *c = getStringArg(getDisplayCall(m_design, 7));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 56)
      << "sec. 5.9.1: \\123 = octal 'S' (0x53) -> \"octal \\123\" = 7 chars = 56 bits; "
         "Surelog bug: WRN:PP0118 -- octal escape not recognized, stores 4 chars, gives 80";
}

// "hex \x12" -- 4 literal chars + 1 hex char (0x12) = 5 chars = 40 bits
// sec. 5.9.1: \xhh is a hex value representing the ASCII value.
// Surelog stores the 4-char sequence \, x, 1, 2 verbatim -> 8 chars -> 64 bits.
TEST_F(StringSpecialChars, Call8_HexEscape_SizePerSpec) {
  const auto *c = getStringArg(getDisplayCall(m_design, 8));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 40)
      << "sec. 5.9.1: \\x12 = hex 0x12 -> \"hex \\x12\" = 5 chars = 40 bits; "
         "Surelog bug: hex escape not expanded, stores 4 chars, gives 64";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
