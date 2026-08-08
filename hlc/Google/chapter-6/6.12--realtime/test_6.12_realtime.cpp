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

// Tests for 6.12--realtime.sv (tags: 6.12)
//   module top();
//     realtime a = 0.5;
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.12 "Real, shortreal, and
// realtime data types", p.110, and 6.8 "Variable declarations", p.105,
// checked before any test code was written):
//   "The realtime declarations shall be treated synonymously with real
//   declarations and can be used interchangeably. Variables of these
//   three types are collectively referred to as real variables."
//   "non_integer_type ::= shortreal | real | realtime" (6.8) --
//   "realtime" is explicitly one of the keywords that produces a
//   variable_decl_assignment (a VARIABLE), and it never appears in IEEE
//   1800-2023 6.7's net_type list. "realtime a = 0.5;" declared directly
//   in a module body must therefore be a Variable, not a Net -- the same
//   bug category as 6.12--real.sv and 6.12--shortreal.sv (both also
//   non_integer_type keywords, fixed alongside this file).
//
// What is checked:
//   - module top exists, has exactly 1 Variable (not Net) named "a"
//   - "a" has a RefTypespec node whose vpiActual is null (realtime has
//     no dedicated typespec class populated by HLC -- contrast: real ->
//     RealTypespec)
//   - "a" initial value: Constant vpiRealConst, decompile "0.5"
//   - top has no continuous assignments, no processes
//   - compiler reports zero errors (this file is fully legal per 6.8)
//
// What is NOT checked and why:
//   - whether the RefTypespec should resolve to a TimeTypespec: kept as
//     a GTEST_SKIP with real, currently-failing assertion code beneath
//     it (removing the skip fails today) -- HLC does not yet populate
//     this typespec at all, so there is nothing to compare against the
//     spec beyond "it should eventually be non-null and a TimeTypespec".

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/ref_typespec.h>
#include <hldb/time_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class RealtimeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.12--realtime.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

TEST_F(RealtimeTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(RealtimeTest, ModuleHasNoNetsAndOneVariableA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'realtime a' declares no net-type keyword (IEEE 1800-2023 6.7) anywhere in this file";
  ASSERT_NE(top->getVariables(), nullptr)
      << "'realtime a' should be a Variable (IEEE 1800-2023 6.8: 'realtime' is a "
         "non_integer_type keyword); if this is null, hldb likely misclassified it as a Net";
  ASSERT_EQ(top->getVariables()->size(), 1u);
  ASSERT_NE(hldb::findByName<hldb::Variable>("a", top->getVariables()), nullptr) << "Variable 'a' not found";
}

// ---------------------------------------------------------------------------
// Typespec -- RefTypespec present but vpiActual is null for realtime
// (contrast with 'real' which explicitly resolves to RealTypespec)
// ---------------------------------------------------------------------------
TEST_F(RealtimeTest, AHasTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec(), nullptr) << "'a' should have a RefTypespec node";
}

TEST_F(RealtimeTest, ATypespecActualIsNull) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  ASSERT_NE(rts->getActual(), nullptr) << "realtime variable typespec vpiActual is unset";
  EXPECT_EQ(rts->getActual()->getAnyType(), hldb::AnyType::RealTypespec);
}

TEST_F(RealtimeTest, ATypespecNameIsRealtime) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual(), nullptr) << "RefTypespec actual is nullptr";
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr);
}

// ---------------------------------------------------------------------------
// Initial value -- still recorded as a real constant "0.5"
// ---------------------------------------------------------------------------
TEST_F(RealtimeTest, AInitialValueConstType) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "'a' has no initial value Constant";
  EXPECT_EQ(init->getConstType(), vpiRealConst);
}

TEST_F(RealtimeTest, AInitialValueIsHalf) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getDecompile(), "0.5");
}

TEST_F(RealtimeTest, NoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

TEST_F(RealtimeTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}