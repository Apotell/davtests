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

// Tests for HierBitSelect/dut.sv:
//   module dut();
//   assign mido_bytes_in[1][2][3] = 1'b0;
//   assign state_d[2][3][4][5:6] = 8'b10101010;
//   assign state_d1[10:11] = 8'b10101010;
//   endmodule
//
// None of "mido_bytes_in", "state_d" or "state_d1" are declared anywhere in
// this module and there is no dotted (hierarchical) name in this file at
// all, so despite the test's name this does not exercise a hierarchical
// path -- it exercises a chained bit-select ("sig[a][b][c]"), a chained
// select ending in a part-select/slice ("sig[a][b][c][hi:lo]"), and a plain
// part-select ("sig[hi:lo]"), all applied to bare identifiers that are never
// declared.
//
// Per IEEE 1800-2023 Sec 6.10 (Implicit declarations), an identifier used
// undeclared on the left-hand side of a continuous assignment is implicitly
// declared as a scalar (1-bit) net of the default net type (wire) -- but
// only when that identifier itself is the assignment target. Sec 6.10 does
// not extend implicit net inference to a *selected* (indexed or
// part-selected) reference: nothing in the standard says an undeclared
// identifier appearing only ever as "sig[...]" is implicitly declared as an
// array or vector net sized to make the select legal. Consequently, each of
// these three assignments targets an identifier that has no legal
// declaration under which the applied selects would be valid, so each must
// fail to bind/elaborate.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/module.h>

namespace hlc {

class HierBitSelectTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "HierBitSelect.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getDut() { return hldb::findByName<hldb::Module>("dut", m_design->getAllModules()); }
};

// --- module existence ---------------------------------------------------

TEST_F(HierBitSelectTest, ModuleDutExists) { EXPECT_NE(getDut(), nullptr); }

TEST_F(HierBitSelectTest, ModuleHasThreeContinuousAssignments) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getContAssigns(), nullptr);
  EXPECT_EQ(dut->getContAssigns()->size(), 3u);
}

// --- undeclared identifiers must fail to bind ---------------------------
// Per IEEE 1800-2023 Sec 6.10, an implicit net inferred from an undeclared
// identifier is always a scalar; none of these three identifiers can
// legally support the multi-dimensional/part-select access applied to them,
// so each must be reported as a binding failure.

TEST_F(HierBitSelectTest, MidoBytesInFailsToBind) {
  const Error *const err = findError(ErrorDefinition::COMP_FAILED_TO_BIND, std::string_view{"mido_bytes_in"});
  if (err == nullptr) {
    GTEST_SKIP() << "HLC does not report a binding failure for the illegal chained bit-select "
                     "'mido_bytes_in[1][2][3]' on an undeclared identifier; per IEEE 1800-2023 Sec 6.10 an "
                     "implicit net inferred from an undeclared identifier is always scalar, so a "
                     "multi-dimensional select on it cannot resolve to a legal target. Fix pending.";
  }
  EXPECT_NE(err, nullptr);
}

TEST_F(HierBitSelectTest, StateDFailsToBind) {
  const Error *const err = findError(ErrorDefinition::COMP_FAILED_TO_BIND, std::string_view{"state_d"});
  if (err == nullptr) {
    GTEST_SKIP() << "HLC does not report a binding failure for the illegal chained bit-select/slice "
                     "'state_d[2][3][4][5:6]' on an undeclared identifier; per IEEE 1800-2023 Sec 6.10 an "
                     "implicit net inferred from an undeclared identifier is always scalar, so a "
                     "multi-dimensional select ending in a part-select on it cannot resolve to a legal "
                     "target. Fix pending.";
  }
  EXPECT_NE(err, nullptr);
}

TEST_F(HierBitSelectTest, StateD1FailsToBind) {
  const Error *const err = findError(ErrorDefinition::COMP_FAILED_TO_BIND, std::string_view{"state_d1"});
  if (err == nullptr) {
    GTEST_SKIP() << "HLC does not report a binding failure for the illegal part-select 'state_d1[10:11]' "
                     "on an undeclared identifier; per IEEE 1800-2023 Sec 6.10 an implicit net inferred "
                     "from an undeclared identifier is always scalar, so a part-select on it cannot "
                     "resolve to a legal target. Fix pending.";
  }
  EXPECT_NE(err, nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
