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

// Tests for 11.3.6--assign_in_expr_inv.sv (tags: 11.3.6)
//   :should_fail_because: blocking assignments within expression must be
//                          enclosed in parentheses
//   module top();
//     int a;
//     int b;
//     int c;
//
//     initial begin
//       a = b = c = 5;
//     end
//
//     endmodule
//
// This file is the deliberate negative case for IEEE 1800-2017 11.3.6:
// "The use of assignment operators in expressions is permitted only in
// certain cases; in these cases, the assignment shall be enclosed in
// parentheses." Chained assignment written *without* the required parens
// ("a = b = c = 5;") is not legal SystemVerilog, unlike its parenthesized
// sibling "a = (b = (c = 5));" tested in 11.3.6--assign_in_expr.sv. So
// unlike most files in this batch, the interesting corner here is not what
// the AST looks like -- it is whether the parser actually rejects the
// construct instead of silently accepting it.
//
// The parser rejects it with a syntax error at the second '=' of the
// statement (line 23, column 7).
//
// Checked:
//   - a syntax error at 23:7, and 0 warnings -- i.e. this is caught at the
//     parser stage, which is exactly where IEEE 11.3.6's parenthesization
//     requirement is a grammar-level rule, not a later semantic check.
//
// Not checked:
//   - what the parser's error recovery leaves behind (the number of
//     errors, whether a "top" module and which parts of its body survive).
//     IEEE 1800-2023 only requires the construct to be rejected; the shape
//     of the recovery is an implementation detail and changes with the
//     grammar (it used to discard the module and leave partial stubs; it
//     now reports the one error and keeps the module).
//
// This test is intentionally NOT a GTEST_SKIP: it is a real, currently
// PASSING assertion that HLC correctly enforces IEEE 11.3.6's
// parenthesization rule (unlike some other constructs in this codebase
// where a `:should_fail_because:` tag does NOT match the compiler's actual
// (0-error) output -- this is the opposite, confirmed-correct case).

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class AssignInExprInvTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.3.6--assign_in_expr_inv.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- the point of the file: the parenthesization rule is enforced ---------

TEST_F(AssignInExprInvTest, CompilerCorrectlyRejectsUnparenthesizedChainedAssignment) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  EXPECT_NE(findEvent(EventType::PaSyntaxError, 23, 7), nullptr)
      << "IEEE 11.3.6 requires 'a = b = c = 5;' (no parens) to be rejected, matching this file's "
         ":should_fail_because: tag";
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_warnings, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
