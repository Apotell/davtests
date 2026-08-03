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

// Tests for 6.12--real_bit_select_idx.sv (tags: 6.12)
//   :should_fail_because: it is illegal to do bit select on real data type
//   module top();
//     real a = 0.5;
//     wire [3:0] b;
//     wire c;
//     assign c = b[a];
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.12 "Real, shortreal, and
// realtime data types", p.110, checked before any test code was
// written):
//   "Real numbers and real variables are also prohibited in the
//   following cases: ... Real index expressions of bit-selects or
//   part-selects of vectors (see 11.5.1)." "b[a]" uses the real variable
//   "a" as the bit-select index into vector "b" -- exactly this
//   prohibited construct, matching the file's own :should_fail_because:
//   tag (the .sv file's own tag text is generic, but the underlying
//   spec-listed prohibition is this specific one, distinct from
//   6.12--real_bit_select.sv's "bit-select OF a real variable"). This is
//   a flat "prohibited" rule in the LRM, so a compliant tool should
//   reject it as a semantic error.
//
//   Also (IEEE 1800-2023 6.8): "real" is a non_integer_type keyword,
//   never a net_type -- "real a" declared at module scope must be a
//   Variable, not a Net. A prior version of this test used
//   hldb::Net/getNets() for "a" (the same net/variable misclassification
//   bug found and fixed in 6.5, 6.9.1, 6.12--real, 6.12--shortreal, and
//   6.12--real_bit_select), and had a Compiler_NoErrorsReported test
//   asserting nbError == 0, documented as "HLC does not reject 'b[a]' ...
//   at compile time" -- treating a confirmed spec violation as expected,
//   passing behavior. This version targets hldb::Variable for "a", and
//   asserts an error IS reported (real bug, currently failing).
//
// What is checked:
//   - module top has zero Nets named "a" (none should exist) and
//     exactly 1 Variable "a" (real, initial value vpiRealConst "0.5");
//     'b' (wire[3:0]) and 'c' (wire) remain Nets, both with no initial
//     value
//   - 1 ContAssign: LHS RefObj "c" resolves to Net "c", RHS = BitSelect
//     "b[a]" whose prefix RefObj "b" resolves to Net "b", and whose
//     index RefObj "a" resolves via getActual<hldb::Variable>() (not
//     Net) to the real Variable
//   - top has no processes
//   - THE POINT OF THIS FILE: the compiler should report at least one
//     error for the illegal real-typed bit-select index "b[a]", per
//     IEEE 1800-2023 6.12 quoted above -- a real, non-skipped,
//     currently-failing assertion
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

class RealBitSelectIdxTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.12--real_bit_select_idx.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

TEST_F(RealBitSelectIdxTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

// ---------------------------------------------------------------------------
// Declarations -- real 'a' is a Variable; wire[3:0] 'b' and wire 'c' are Nets
// ---------------------------------------------------------------------------
TEST_F(RealBitSelectIdxTest, ModuleHasTwoNetsAndOneVariableA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 2u) << "only 'b' and 'c' should be Nets; 'a' should be a Variable";
  EXPECT_EQ(hldb::findByName<hldb::Net>("a", top->getNets()), nullptr) << "'a' must not resolve to a Net";
  ASSERT_NE(top->getVariables(), nullptr)
      << "'real a' should be a Variable; if this is null, hldb likely misclassified it as a Net";
  ASSERT_EQ(top->getVariables()->size(), 1u);
  ASSERT_NE(hldb::findByName<hldb::Variable>("a", top->getVariables()), nullptr) << "Variable 'a' not found";
}

TEST_F(RealBitSelectIdxTest, ATypespecIsReal) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr) << "'a' has no typespec";
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr) << "'a' typespec should resolve to RealTypespec";
}

TEST_F(RealBitSelectIdxTest, AInitialValueIsHalf) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiRealConst);
  EXPECT_EQ(init->getDecompile(), "0.5");
}

TEST_F(RealBitSelectIdxTest, BNetIsWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr) << "net 'b' not found";
  EXPECT_EQ(b->getNetType(), vpiWire);
}

TEST_F(RealBitSelectIdxTest, CNetIsWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const c = hldb::findByName<hldb::Net>("c", top->getNets());
  ASSERT_NE(c, nullptr) << "net 'c' not found";
  EXPECT_EQ(c->getNetType(), vpiWire);
}

TEST_F(RealBitSelectIdxTest, BNetHasNoInitialValue) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getValue<hldb::Any>(), nullptr) << "wire 'b' is declared without an initializer";
}

TEST_F(RealBitSelectIdxTest, CNetHasNoInitialValue) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const c = hldb::findByName<hldb::Net>("c", top->getNets());
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue<hldb::Any>(), nullptr) << "wire 'c' is declared without an initializer";
}

// ---------------------------------------------------------------------------
// Continuous assignment -- assign c = b[a]
// ---------------------------------------------------------------------------
TEST_F(RealBitSelectIdxTest, ContAssignExists) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  EXPECT_EQ(top->getContAssigns()->size(), 1u);
}

TEST_F(RealBitSelectIdxTest, ContAssignLhsResolvesToNetC) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const hldb::RefObj *const lhs = top->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  const hldb::Net *const net = lhs->getActual<hldb::Net>();
  ASSERT_NE(net, nullptr) << "ContAssign LHS RefObj 'c' should resolve to the formally declared net 'c'";
  EXPECT_EQ(net->getName(), "c");
}

TEST_F(RealBitSelectIdxTest, ContAssignRhsIsBitSelect) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const hldb::BitSelect *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS is not a BitSelect";
  EXPECT_EQ(rhs->getName(), "b[a]");
}

// ---------------------------------------------------------------------------
// BitSelect internals -- prefix is net 'b', index is the real Variable 'a'
// ---------------------------------------------------------------------------
TEST_F(RealBitSelectIdxTest, BitSelectPrefixResolvesToNetB) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const hldb::BitSelect *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr);
  const hldb::RefObj *const prefix = rhs->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr) << "BitSelect prefix is not a RefObj";
  const hldb::Net *const net = prefix->getActual<hldb::Net>();
  ASSERT_NE(net, nullptr) << "BitSelect prefix does not resolve to a Net";
  EXPECT_EQ(net->getName(), "b");
}

TEST_F(RealBitSelectIdxTest, BitSelectIndexResolvesToRealVariableNotNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const hldb::BitSelect *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr);

  // Index is a real variable -- illegal in SV but hldb still records it as a RefObj
  const hldb::RefObj *const idx = rhs->getIndex<hldb::RefObj>();
  ASSERT_NE(idx, nullptr) << "BitSelect index is not a RefObj";
  EXPECT_EQ(idx->getName(), "a");
  EXPECT_EQ(idx->getActual<hldb::Net>(), nullptr) << "'a' must not resolve to a Net";

  const hldb::Variable *const var = idx->getActual<hldb::Variable>();
  ASSERT_NE(var, nullptr) << "BitSelect index RefObj does not resolve to the Variable 'a'";
  const hldb::RefTypespec *const rts = var->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr)
      << "index variable 'a' must be real-typed -- it is the illegal real index";
}

TEST_F(RealBitSelectIdxTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

// ---------------------------------------------------------------------------
// The actual point of the file: a real index into a vector bit-select is illegal
// ---------------------------------------------------------------------------
TEST_F(RealBitSelectIdxTest, CompilerShouldRejectRealTypedBitSelectIndexButDoesNot) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbFatal + stats.nbSyntax + stats.nbError, 0)
      << "IEEE 1800-2023 6.12: 'real index expressions of bit-selects or part-selects of "
         "vectors' are prohibited -- 'b[a]' does exactly this, matching this file's own "
         ":should_fail_because: tag -- HLC currently accepts it with zero diagnostics";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
