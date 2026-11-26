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

// Tests for 6.9.2--vector_vectored_inv.sv (tags: 6.9.2)
//   16: module top();
//   17:   logic vectored [15:0] a = 0;
//   18:
//   19:   assign a[1] = 1;
//   20: endmodule
//   :should_fail_because: bit selects are not permitted on vectored vector nets
//
// What to check and why (IEEE 1800-2023 6.7.1 "Net declarations" and 6.8
// "Variable declarations", checked before any test code was written):
//   Grammar: "net_type [drive_strength | charge_strength] [vectored |
//   scalared]" -- the "vectored"/"scalared" modifiers are only valid in
//   a NET declaration, immediately after a net_type keyword. "logic" is
//   a variable-type keyword (6.8's integer_vector_type), never a
//   net_type (6.7), so "logic vectored [15:0] a" is not a legal
//   declaration at all -- it is a syntax error, not merely a semantic
//   one. This means the file's own :should_fail_because: tag ("bit
//   selects not permitted on vectored vector nets", a 6.9.2 semantic
//   restriction that only applies to a successfully-declared vectored
//   net) does not describe the failure this file triggers: the parse
//   fails on the illegal "logic vectored" combination itself, so there is
//   no vectored net whose bit select could be checked. The test checks
//   the true failure (the syntax error), not the tag's claimed one.
//
// What is checked:
//   - a syntax error at "vectored" (line 17, column 7) saying the keyword is only
//     allowed in a net declaration (PaNetOnlyKeyword).
//
// What is NOT checked and why:
//   - what the parser's error recovery leaves behind: the number of syntax
//     errors, whether a "top" module and its continuous assignment survive,
//     and the diagnostics that follow from it (with the declaration of "a"
//     lost, "assign a[1] = 1;" reports "a" as undefined). IEEE 1800-2023 only
//     requires the declaration to be rejected; the shape of the recovery is
//     an implementation detail and changes with the grammar (it used to
//     discard the module and leave unnamed module stubs; it now keeps the
//     module).
//   - the 6.9.2 bit-select restriction itself: it cannot be reached here (see
//     above).

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class VectorVectoredInvTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.9.2--vector_vectored_inv.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(VectorVectoredInvTest, VectoredOnAVariableIsASyntaxError) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  EXPECT_NE(findEvent(EventType::PaNetOnlyKeyword, "vectored", 17, 7), nullptr)
      << "\"vectored\" is only valid after a net_type (IEEE 1800-2023 6.7.1); \"logic vectored\" "
         "must be rejected";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
