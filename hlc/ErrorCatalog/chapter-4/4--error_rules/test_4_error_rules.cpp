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

// Tests for the IEEE 1800-2023 Clause 4 error scenario catalogued in the SV
// error catalog (filtered set), row 4.
//
// Scope: as with the Chapter 3 catalog tests (see test_3_error_rules.cpp),
// this file asserts ONLY that the diagnostic the catalog row requires is
// emitted. It makes no assertion about the shape of the compiled model.
//
// Fixture: 4--error_rules.sv (row 4).
//
// The command file deliberately omits "-timescale=1ns/1ns", matching every
// other ErrorCatalog chapter, so a missing/default time unit never masks an
// unrelated diagnostic.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter4ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "4--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 4: primitive output terminal must be a 1-bit net (4.9.6) ----------

TEST_F(Chapter4ErrorRulesTest, Row4_PrimitiveOutputTerminalMustBeOneBitNet) {
  // catalog row 4 | 4.9.6 | COMP
  // "Primitive (including UDP) output and inout terminals shall be connected
  // directly to 1-bit nets or 1-bit structural net expressions ... A multibit
  // net or a non-structural expression on a primitive output terminal is
  // illegal." r4_m connects the gate-primitive output of 'not g1' to the
  // 2-bit net r4_y.
  //
  // FIXED 2026-08-26: wired in ModelBuilder::reportIllegalPrimitiveTerminal()
  // (ModelBuilder.cpp), a post-pass over every Gate/Udp/SwitchTran once
  // ObjectBinder has resolved each output/inout PrimTerm's RefObj to its
  // net/variable, comparing the resolved LogicTypespec/BitTypespec's packed
  // width against 1. Scoped to a plain net/variable reference only -- a
  // structural net expression (concatenation, part-select) is not
  // width-checked, to avoid a false positive rather than guess its width.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_PRIMITIVE_TERMINAL, "r4_y"), nullptr)
      << "a primitive output terminal must be a 1-bit net (IEEE 1800-2023 4.9.6)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
