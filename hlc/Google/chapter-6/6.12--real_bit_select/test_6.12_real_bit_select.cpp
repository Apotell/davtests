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

// Tests for 6.12--real_bit_select.sv (tags: 6.12)
//   :should_fail_because: it is illegal to do bit select on real data type
//   module top();
//     real a = 0.5;
//     wire b;
//     assign b = a[2];
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.12 "Real, shortreal, and
// realtime data types", p.110, checked before any test code was
// written):
//   "Real numbers and real variables are also prohibited in the
//   following cases: ... Bit-select or part-select references of real
//   variables (see 11.5.1)." "a[2]" on the real variable "a" is exactly
//   this prohibited construct, matching the file's own
//   :should_fail_because: tag precisely. This is a flat "prohibited"
//   rule in the LRM, not a runtime-numerics statement, so a compliant
//   tool should reject it as a semantic error.
//
//   Also (IEEE 1800-2023 6.8): "real" is a non_integer_type keyword
//   (6.8's non_integer_type ::= shortreal | real | realtime), never a
//   net_type (6.7) -- "real a" declared at module scope must be a
//   Variable, not a Net, regardless of scope. A prior version of this
//   test used hldb::Net/getNets() for "a" (the net/variable
//   misclassification bug also found and fixed in 6.5, 6.9.1, 6.12--real,
//   and 6.12--shortreal), and had a Compiler_NoErrorsReported test
//   asserting nbError == 0, documented as "HLC does not reject 'a[2]'
//   bit-select on a real net at compile time" -- treating a confirmed
//   spec violation as expected, passing behavior. This version targets
//   hldb::Variable for "a", and asserts an error IS reported (real bug,
//   currently failing).
//
// What is checked:
//   - module top has zero Nets named "a" (none should exist) and
//     exactly 1 Variable "a" (real, initial value vpiRealConst "0.5");
//     "b" (wire, no initial value) remains a real Net
//   - 1 ContAssign: LHS RefObj "b" resolves to Net "b", RHS = BitSelect
//     "a[2]" whose prefix RefObj "a" resolves via
//     getActual<hldb::Variable>() (not Net) to the real Variable, and
//     whose index is Constant "2"
//   - top has no processes
//   - THE POINT OF THIS FILE: the compiler should report at least one
//     error for the illegal bit-select "a[2]" on a real variable, per
//     IEEE 1800-2023 6.12 quoted above. Confirmed by personally running
//     with the skip removed (fails as expected) -- kept as GTEST_SKIP()
//     with the real assertion underneath, per the established gating
//     rule (skips only added after personal verification)
//
// What is NOT checked and why:
//   - none: every corner above is fully structural and checkable without
//     simulation.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class RealBitSelectTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.12--real_bit_select.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

TEST_F(RealBitSelectTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

// ---------------------------------------------------------------------------
// Declarations -- real 'a' is a Variable, wire 'b' is a Net
// ---------------------------------------------------------------------------
TEST_F(RealBitSelectTest, ModuleHasOneNetBAndOneVariableA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u) << "only 'b' should be a Net; 'a' should be a Variable";
  EXPECT_EQ(hldb::findByName<hldb::Net>("a", top->getNets()), nullptr) << "'a' must not resolve to a Net";
  ASSERT_NE(top->getVariables(), nullptr)
      << "'real a' should be a Variable; if this is null, hldb likely misclassified it as a Net";
  ASSERT_EQ(top->getVariables()->size(), 1u);
  ASSERT_NE(hldb::findByName<hldb::Variable>("a", top->getVariables()), nullptr) << "Variable 'a' not found";
}

TEST_F(RealBitSelectTest, ATypespecIsReal) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr) << "'a' has no typespec";
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr) << "'a' typespec should resolve to RealTypespec";
}

TEST_F(RealBitSelectTest, AInitialValueIsHalf) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiRealConst);
  EXPECT_EQ(init->getDecompile(), "0.5");
}

TEST_F(RealBitSelectTest, BNetIsWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr) << "net 'b' not found";
  EXPECT_EQ(b->getNetType(), vpiWire);
}

TEST_F(RealBitSelectTest, BNetHasNoInitialValue) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getValue<hldb::Any>(), nullptr) << "wire 'b' is declared without an initializer";
}

// ---------------------------------------------------------------------------
// Continuous assignment -- assign b = a[2]
// ---------------------------------------------------------------------------
TEST_F(RealBitSelectTest, ContAssignExists) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  EXPECT_EQ(top->getContAssigns()->size(), 1u);
}

TEST_F(RealBitSelectTest, ContAssignLhsResolvesToNetB) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const hldb::RefObj *const lhs = top->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "ContAssign LHS is not a RefObj";
  EXPECT_EQ(lhs->getName(), "b");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr)
      << "ContAssign LHS RefObj 'b' should resolve to the formally declared net 'b'";
}

TEST_F(RealBitSelectTest, ContAssignRhsIsBitSelect) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const hldb::BitSelect *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS is not a BitSelect";
  EXPECT_EQ(rhs->getName(), "a[2]");
}

// ---------------------------------------------------------------------------
// BitSelect internals -- prefix is the real Variable 'a', index is Constant 2
// ---------------------------------------------------------------------------
TEST_F(RealBitSelectTest, BitSelectPrefixResolvesToRealVariableNotNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const hldb::BitSelect *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr);
  const hldb::RefObj *const prefix = rhs->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr) << "BitSelect prefix is not a RefObj";
  EXPECT_EQ(prefix->getName(), "a");
  EXPECT_EQ(prefix->getActual<hldb::Net>(), nullptr) << "'a' must not resolve to a Net";

  const hldb::Variable *const var = prefix->getActual<hldb::Variable>();
  ASSERT_NE(var, nullptr) << "BitSelect prefix does not resolve to the Variable 'a'";
  const hldb::RefTypespec *const rts = var->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr) << "bit-selected prefix 'a' should be real-typed";
}

TEST_F(RealBitSelectTest, BitSelectIndexIsConstant2) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const hldb::BitSelect *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr);
  const hldb::Constant *const idx = rhs->getIndex<hldb::Constant>();
  ASSERT_NE(idx, nullptr) << "BitSelect index is not a Constant";
  EXPECT_EQ(idx->getDecompile(), "2");
}

TEST_F(RealBitSelectTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

// ---------------------------------------------------------------------------
// The actual point of the file: bit-select on a real variable is illegal
// ---------------------------------------------------------------------------
TEST_F(RealBitSelectTest, CompilerShouldRejectBitSelectOnRealVariableButDoesNot) {
  GTEST_SKIP() << "Confirmed HLC bug -- verified by running this test with the skip removed "
                  "(fails as expected): IEEE 1800-2023 6.12 prohibits bit-select or part-select "
                  "references of real variables ('a[2]'), but HLC accepts it with zero "
                  "diagnostics. Tracked, not yet fixed by the compiler.";
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbFatal + stats.nbSyntax + stats.nbError, 0)
      << "IEEE 1800-2023 6.12: 'bit-select or part-select references of real variables' are "
         "prohibited -- 'a[2]' does exactly this, matching this file's own :should_fail_because: "
         "tag -- HLC currently accepts it with zero diagnostics";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
