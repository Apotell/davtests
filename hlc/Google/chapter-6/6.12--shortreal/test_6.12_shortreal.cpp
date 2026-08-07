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

// Tests for 6.12--shortreal.sv (tags: 6.12)
//   module top();
//     shortreal a = 0.5;
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.8 "Variable declarations",
// p.105, checked before any test code was written):
//   "non_integer_type ::= shortreal | real | realtime". "shortreal" is
//   explicitly one of the keywords that produces a
//   variable_decl_assignment (a VARIABLE), and it never appears in IEEE
//   1800-2023 6.7's net_type list. "shortreal a = 0.5;" declared
//   directly in a module body must therefore be a Variable, not a Net,
//   regardless of module-level scope -- the same bug category as
//   6.12--real.sv (real is also a non_integer_type, fixed alongside
//   this file).
//
// What is checked:
//   - module top exists, has exactly 1 Variable (not Net) named "a"
//   - "a" has a ShortRealTypespec (via RefTypespec), NOT a RealTypespec
//     (a distinct, narrower type per 6.12)
//   - "a" has an initial value: Constant vpiRealConst, decompile "0.5"
//     (shortreal stores its constant using the same vpiRealConst kind
//     as real)
//   - the 32-bit vs 64-bit precision difference between shortreal and
//     real is not observable statically: the initial value Constant
//     reports vpiSize=64 regardless of the shortreal typespec (storage
//     width is a simulation-time property, not present in the object
//     model)
//   - top has no continuous assignments, no processes
//   - compiler reports zero errors (this file is fully legal per 6.8)
//
// What is NOT checked and why:
//   - none: every corner above is fully structural and checkable without
//     simulation.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_typespec.h>
#include <hldb/short_real_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ShortrealTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.12--shortreal.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

TEST_F(ShortrealTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ShortrealTest, ModuleHasNoNetsAndOneVariableA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'shortreal a' declares no net-type keyword (IEEE 1800-2023 6.7) anywhere in this file";
  ASSERT_NE(top->getVariables(), nullptr)
      << "'shortreal a' should be a Variable (IEEE 1800-2023 6.8: 'shortreal' is a "
         "non_integer_type keyword); if this is null, hldb likely misclassified it as a Net";
  ASSERT_EQ(top->getVariables()->size(), 1u);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr) << "Variable 'a' not found";
}

// ---------------------------------------------------------------------------
// Typespec -- Variable 'a' must resolve to ShortRealTypespec, NOT RealTypespec
// ---------------------------------------------------------------------------
TEST_F(ShortrealTest, ATypespecIsShortReal) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);

  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr) << "'a' has no typespec";
  EXPECT_NE(rts->getActual<hldb::ShortRealTypespec>(), nullptr)
      << "'a' typespec should resolve to ShortRealTypespec (not RealTypespec)";
}

TEST_F(ShortrealTest, ATypespecIsNotPlainReal) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_EQ(rts->getActual<hldb::RealTypespec>(), nullptr) << "shortreal should NOT resolve to RealTypespec";
}

// ---------------------------------------------------------------------------
// Initial value -- recorded as a real constant "0.5"
// ---------------------------------------------------------------------------
TEST_F(ShortrealTest, AInitialValueConstType) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "'a' has no initial value Constant";
  EXPECT_EQ(init->getConstType(), vpiRealConst);
}

TEST_F(ShortrealTest, AInitialValueIsHalf) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getDecompile(), "0.5");
}

TEST_F(ShortrealTest, NoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

TEST_F(ShortrealTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

// ---------------------------------------------------------------------------
// Precision -- shortreal is a distinct 32-bit type vs real's 64-bit, but the
// object model has no field for storage width; this can only be observed by
// simulating an assignment that overflows shortreal precision.
// ---------------------------------------------------------------------------
TEST_F(ShortrealTest, StorageWidthNotObservableStatically) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getSize(), 64)
      << "shortreal's 32-bit storage width is not reflected in the Constant's vpiSize (always 64, "
         "same as real)";
}

TEST_F(ShortrealTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
