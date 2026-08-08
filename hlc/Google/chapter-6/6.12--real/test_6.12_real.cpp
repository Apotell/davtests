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

// Tests for 6.12--real.sv (tags: 6.12)
//   module top();
//     real a = 0.5;
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.8 "Variable declarations",
// p.105, checked before any test code was written):
//   "data_type ::= ... | non_integer_type ..." with "non_integer_type
//   ::= shortreal | real | realtime". "real" is explicitly one of the
//   keywords that produces a variable_decl_assignment (a VARIABLE), and
//   it never appears in IEEE 1800-2023 6.7's net_type list. "real a =
//   0.5;" declared directly in a module body must therefore be a
//   Variable, not a Net, regardless of module-level scope.
//
//   A prior version of this test used hldb::Net/getNets() for "a", and
//   had a GTEST_SKIP'd test asserting getNetType() == vpiRealVar. That
//   assertion was itself based on a category error: vpiRealVar (=47, per
//   vpi_user.h "real variable") is an object-TYPE constant from the
//   classic VPI model (like vpiNet=36, vpiReg, vpiIntegerVar) -- it
//   identifies what KIND of node something is, it is never a value
//   stored in a Net's vpiNetType field (which only ever holds net_type
//   keyword values like vpiWire, vpiTri, etc). hldb's Variable class
//   reports getVpiType() == vpiVariable uniformly and distinguishes
//   real vs int vs logic purely via its RefTypespec -- there is no
//   "netType" concept on a Variable, so this test is removed rather than
//   replaced now that "a" is correctly modeled as a Variable.
//
// What is checked:
//   - module top exists, has exactly 1 Variable (not Net) named "a"
//   - "a" has a RealTypespec (via RefTypespec)
//   - "a" has an initial value: Constant vpiRealConst, decompile "0.5"
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
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class RealTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.12--real.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

TEST_F(RealTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(RealTest, ModuleHasNoNetsAndOneVariableA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'real a' declares no net-type keyword (IEEE 1800-2023 6.7) anywhere in this file";
  ASSERT_NE(top->getVariables(), nullptr)
      << "'real a' should be a Variable (IEEE 1800-2023 6.8: 'real' is a non_integer_type "
         "keyword); if this is null, hldb likely misclassified it as a Net";
  ASSERT_EQ(top->getVariables()->size(), 1u);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr) << "Variable 'a' not found";
}

// ---------------------------------------------------------------------------
// Typespec -- Variable 'a' must resolve to RealTypespec
// ---------------------------------------------------------------------------
TEST_F(RealTest, ATypespecIsReal) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);

  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr) << "'a' has no typespec";
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr) << "'a' typespec does not resolve to RealTypespec";
}

// ---------------------------------------------------------------------------
// Initial value -- vpiConstType=vpiRealConst, decompile "0.5"
// ---------------------------------------------------------------------------
TEST_F(RealTest, AInitialValueConstType) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);

  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "'a' has no initial value Constant";
  EXPECT_EQ(init->getConstType(), vpiRealConst) << "expected vpiRealConst (2) for a real-typed initial value";
}

TEST_F(RealTest, AInitialValueIsHalf) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);

  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getDecompile(), "0.5");
}

TEST_F(RealTest, NoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty())
      << "module should have no continuous assignments";
}

TEST_F(RealTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

TEST_F(RealTest, CompilerReportsZeroErrors) {
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
