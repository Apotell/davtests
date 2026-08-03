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

// Tests for 6.9.2--vector_scalared.sv (tags: 6.9.2)
//   module top;
//     tri1 scalared [15:0] a = 0;
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.9.2 "Vector net accessibility",
// p.109, checked before any test code was written):
//   "net_type [drive_strength | charge_strength] [vectored | scalared]"
//   is the actual net declaration grammar, and the spec's own example is
//   nearly identical to this file: "tri1 scalared [63:0] bus64; // a bus
//   that will be expanded". "tri1" IS a net-type keyword (IEEE 1800-2023
//   6.7), so classifying "a" as a Net is correct here (unlike the 6.5 and
//   6.9.1 files, this is not a net/variable misclassification). This file
//   has no :should_fail_because: tag -- it is legal per spec.
//
//   The Annex A grammar diagram (p.1019) maps the "scalared declaration"
//   branch directly to "bool: vpiExplicitScalared", and vpi_user.h defines
//   vpiExplicitScalared = 23 ("explicitly scalared (Boolean)"). Since this
//   file's declaration explicitly uses the "scalared" keyword, that
//   property should read true. A prior version of this test asserted
//   getExplicitScalared() == false as neutral "COMPILER BEHAVIOR", which
//   silently accepted the object model dropping information that was
//   actually present in the source -- this version instead asserts it
//   should be true (real bug, currently failing, since HLC drops it).
//
// What is checked:
//   - module top exists, has exactly 1 Net named "a", vpiNetType=vpiTri1
//   - Net has a LogicTypespec (via RefTypespec), vpiVector=true, exactly
//     1 Range: left=15, right=0
//   - Net's initial value is Constant "0" (vpiUIntConst)
//   - top has no processes, no continuous assignments
//   - compiler reports zero errors (this file is fully legal, matching
//     the spec's own worked example almost verbatim)
//   - THE POINT OF THIS FILE: the "scalared" keyword is explicitly
//     present in source, so vpiExplicitScalared should be true per the
//     spec's own Annex A grammar-to-property mapping -- a real,
//     non-skipped, currently-failing assertion (HLC currently drops it)
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
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class VectorScalaredTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.9.2--vector_scalared.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

TEST_F(VectorScalaredTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(VectorScalaredTest, ModuleHasOneNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(VectorScalaredTest, NetNameIsA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getName(), "a");
}

TEST_F(VectorScalaredTest, NetTypeIsTri1) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getNetType(), vpiTri1);
}

TEST_F(VectorScalaredTest, NetHasLogicTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(VectorScalaredTest, LogicTypespecIsVector) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::LogicTypespec *const ls = rt->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_TRUE(ls->getVector());
}

TEST_F(VectorScalaredTest, LogicTypespecHasOneRange) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::LogicTypespec *const ls = rt->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  ASSERT_NE(ls->getRanges(), nullptr);
  EXPECT_EQ(ls->getRanges()->size(), 1u);
}

TEST_F(VectorScalaredTest, RangeLeftIs15) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::LogicTypespec *const ls = rt->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  ASSERT_NE(ls->getRanges(), nullptr);
  const hldb::Range *const range = ls->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const left = range->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "15");
}

TEST_F(VectorScalaredTest, RangeRightIs0) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const hldb::LogicTypespec *const ls = rt->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  ASSERT_NE(ls->getRanges(), nullptr);
  const hldb::Range *const range = ls->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const right = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(VectorScalaredTest, NetHasInitialValue) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_NE(net->getValue(), nullptr);
}

TEST_F(VectorScalaredTest, NetInitialValueIsZero) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::Constant *const val = net->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getDecompile(), "0");
}

TEST_F(VectorScalaredTest, NetInitialValueConstTypeIsUInt) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::Constant *const val = net->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getConstType(), vpiUIntConst);
}

TEST_F(VectorScalaredTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

TEST_F(VectorScalaredTest, NoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

TEST_F(VectorScalaredTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

// ---------------------------------------------------------------------------
// The actual point of this file: the "scalared" keyword is explicitly used
// ---------------------------------------------------------------------------
TEST_F(VectorScalaredTest, NetShouldBeExplicitlyScalaredButIsNot) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_TRUE(net->getExplicitScalared())
      << "IEEE 1800-2023 Annex A grammar diagram (p.1019) maps the 'scalared declaration' branch "
         "directly to vpiExplicitScalared -- 'a' is declared with the literal 'scalared' keyword, "
         "so this should be true. HLC currently drops the modifier and always returns false";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
