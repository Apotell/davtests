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

// Tests for 6.9.2--vector_vectored.sv (tags: 6.9.2)
//   module top();
//     tri1 vectored [15:0] a;
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.9.2 "Vector net accessibility",
// p.109, checked before any test code was written):
//   "net_type [drive_strength | charge_strength] [vectored | scalared]"
//   -- "tri1" is a real net-type keyword (IEEE 1800-2023 6.7), so
//   classifying "a" as a Net is correct (unlike 6.9.1's "logic" case).
//   This file has no :should_fail_because: tag -- it is legal per spec.
//
//   KEY DISTINCTION -- two different "vector" concepts:
//     vpiVector (LogicTypespec::getVector())  -- type is multi-bit
//       [15:0]; correctly set true
//     vpiExplicitVectored (Net::getExplicitVectored()) -- whether the
//       literal "vectored" keyword was used in the declaration
//
//   The Annex A grammar diagram (p.1019) maps the "vectored declaration"
//   branch directly to "bool: vpiExplicitVectored", and vpi_user.h
//   defines vpiExplicitVectored = 24 ("explicitly vectored (Boolean)").
//   Since this declaration explicitly uses "vectored", that property
//   should read true. A prior version of this test asserted
//   getExplicitVectored() == false as neutral "COMPILER BEHAVIOR" --
//   this version instead asserts it should be true (real bug, currently
//   failing, since HLC drops the modifier), matching the sibling bug in
//   6.9.2--vector_scalared.sv (vpiExplicitScalared also silently
//   dropped).
//
// What is checked:
//   - design has module top, exactly 1 net "a": vpiNetType=vpiTri1,
//     getFullName()="top.a"
//   - RefTypespec -> LogicTypespec: vpiVector=true, 1 Range [15:0]
//     (left=15, right=0), both range constants vpiUIntConst
//   - net has no initial value (no "= value" initializer)
//   - Net boolean flags all false: implicitDecl, netDeclAssign, scalar,
//     arrayMember, constantSelect, expanded, structUnionMember,
//     vectorFlag (Net::getVector(), a separate flag from
//     LogicTypespec::getVector())
//   - getExplicitScalared() == false -- correct, no "scalared" keyword
//     anywhere in this declaration
//   - Net numeric fields all zero: resolvedNetType, strength0,
//     strength1, chargeStrength -- no explicit drive_strength/
//     charge_strength token appears in the source, so nothing to store
//   - Net collections all nullptr: portInsts, primTerms, contAssigns
//     (per-net), pathTerms, tchkTerms, bits, indexes, simNet
//   - LogicTypespec: getSigned()==false, getScalar()==false,
//     getElemTypespec()==nullptr, getIndexTypespec()==nullptr
//   - design has 2 typespecs: ModuleTypespec "top" and IntTypespec
//   - module top() has no ports; no processes, no continuous assignments
//   - compiler reports zero errors (this file is fully legal)
//   - THE POINT OF THIS FILE: the "vectored" keyword is explicitly
//     present in source, so vpiExplicitVectored should be true per the
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
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class VectorVectoredTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.9.2--vector_vectored.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(VectorVectoredTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(VectorVectoredTest, ModuleHasOneNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(VectorVectoredTest, NetNameIsA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getName(), "a");
}

TEST_F(VectorVectoredTest, NetTypeIsTri1) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getNetType(), vpiTri1);
}

TEST_F(VectorVectoredTest, NetHasLogicTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(VectorVectoredTest, LogicTypespecIsVector) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
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

TEST_F(VectorVectoredTest, LogicTypespecHasOneRange) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
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

TEST_F(VectorVectoredTest, RangeLeftIs15) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
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

TEST_F(VectorVectoredTest, RangeRightIs0) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
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

TEST_F(VectorVectoredTest, NetHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  // tri1 vectored [15:0] a; has no `= 0` initializer
  EXPECT_EQ(net->getValue(), nullptr);
}

// --- net identity -----------------------------------------------------------

TEST_F(VectorVectoredTest, NetFullNameIsWorkAtTopDotA) {
  // log line: vpiFullName: top.a
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getFullName(), "top.a");
}

// --- net boolean flags (all expected false) ----------------------------------

TEST_F(VectorVectoredTest, NetIsNotImplicitDecl) {
  // `tri1 vectored [15:0] a` is an explicit declaration -- not implicit
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getImplicitDecl());
}

TEST_F(VectorVectoredTest, NetHasNoDeclAssign) {
  // no `= value` in `tri1 vectored [15:0] a` -- differs from vector_scalared.sv
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getNetDeclAssign());
}

TEST_F(VectorVectoredTest, NetIsNotScalar) {
  // `[15:0]` makes this a vector, not a scalar 1-bit net
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getScalar());
}

TEST_F(VectorVectoredTest, NetIsNotArrayMember) {
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getArrayMember());
}

TEST_F(VectorVectoredTest, NetHasNoConstantSelect) {
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getConstantSelect());
}

TEST_F(VectorVectoredTest, NetIsNotExpanded) {
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getExpanded());
}

TEST_F(VectorVectoredTest, NetIsNotStructUnionMember) {
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getStructUnionMember());
}

// --- net numeric fields (compiler does not set them) -------------------------

TEST_F(VectorVectoredTest, NetResolvedNetTypeIsZero) {
  // COMPILER BEHAVIOR: getResolvedNetType() is a separate field from
  // getNetType(). HLC sets getNetType()=vpiTri1 but never calls
  // setResolvedNetType() -- returns 0.
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getResolvedNetType(), 0);
}

TEST_F(VectorVectoredTest, NetStrength0IsZero) {
  // COMPILER BEHAVIOR: `tri1` has an implicit pull-up (constant-1 weak driver)
  // but HLC does NOT set vpiStrength0 on the Net node. Returns 0.
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getStrength0(), 0);
}

TEST_F(VectorVectoredTest, NetStrength1IsZero) {
  // COMPILER BEHAVIOR: tri1 pull-up strength to 1 is not stored in UHDM.
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getStrength1(), 0);
}

TEST_F(VectorVectoredTest, NetChargeStrengthIsZero) {
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getChargeStrength(), 0);
}

TEST_F(VectorVectoredTest, NetVectorFlagFalse) {
  // COMPILER BEHAVIOR: Net::getVector() is a separate field from
  // LogicTypespec::getVector(). HLC only sets vpiVector on the
  // LogicTypespec (confirmed by log), NOT directly on the Net node.
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getVector());
}

// --- net collections (all nullptr -- no connectivity in this module) ----------

TEST_F(VectorVectoredTest, NetHasNoPortInsts) {
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getPortInsts(), nullptr);
}

TEST_F(VectorVectoredTest, NetHasNoPrimTerms) {
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getPrimTerms(), nullptr);
}

TEST_F(VectorVectoredTest, NetHasNoContAssignsOnNet) {
  // Net::getContAssigns() is per-net (drives of this net), distinct from
  // Module::getContAssigns() (all assigns in the module scope)
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getContAssigns(), nullptr);
}

TEST_F(VectorVectoredTest, NetHasNoPathTerms) {
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getPathTerms(), nullptr);
}

TEST_F(VectorVectoredTest, NetHasNoTchkTerms) {
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getTchkTerms(), nullptr);
}

TEST_F(VectorVectoredTest, NetHasNoBits) {
  // getBits() returns per-bit expansion nets -- absent for non-expanded net
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getBits(), nullptr);
}

TEST_F(VectorVectoredTest, NetHasNoIndexes) {
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getIndexes(), nullptr);
}

TEST_F(VectorVectoredTest, NetHasNoSimNet) {
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getSimNet(), nullptr);
}

// --- LogicTypespec extra properties ------------------------------------------

TEST_F(VectorVectoredTest, LogicTypespecIsNotSigned) {
  // `logic` is unsigned by default; getSigned() must be false
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::LogicTypespec *const ls =
      top->getNets()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_FALSE(ls->getSigned());
}

TEST_F(VectorVectoredTest, LogicTypespecIsNotScalar) {
  // [15:0] makes the typespec a vector, not scalar
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::LogicTypespec *const ls =
      top->getNets()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_FALSE(ls->getScalar());
}

TEST_F(VectorVectoredTest, LogicTypespecHasNoElemTypespec) {
  // ElemTypespec is used for array element types; absent for a plain vector
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::LogicTypespec *const ls =
      top->getNets()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_EQ(ls->getElemTypespec(), nullptr);
}

TEST_F(VectorVectoredTest, LogicTypespecHasNoIndexTypespec) {
  // IndexTypespec is for associative arrays; absent here
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::LogicTypespec *const ls =
      top->getNets()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_EQ(ls->getIndexTypespec(), nullptr);
}

// --- range constant details --------------------------------------------------

TEST_F(VectorVectoredTest, RangeLeftConstTypeIsUInt) {
  // log line: vpiConstType: unsigned int (9)
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::LogicTypespec *const ls =
      top->getNets()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  const hldb::Constant *const left = ls->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getConstType(), vpiUIntConst);
}

TEST_F(VectorVectoredTest, RangeRightConstTypeIsUInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::LogicTypespec *const ls =
      top->getNets()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  const hldb::Constant *const right = ls->getRanges()->at(0)->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getConstType(), vpiUIntConst);
}

// --- design-level typespecs --------------------------------------------------

TEST_F(VectorVectoredTest, DesignHasTwoTypespecs) {
  // log: vpiTypespec (2 items): ModuleTypespec "top" + IntTypespec
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(VectorVectoredTest, DesignHasModuleTypespec) {
  // The design-level typespecs include a ModuleTypespec for top
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(VectorVectoredTest, DesignHasIntTypespec) {
  // log: IntTypespec at design level (used for range constant types)
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  EXPECT_NE(it, nullptr);
}

// --- module-level checks -----------------------------------------------------

TEST_F(VectorVectoredTest, ModuleHasNoPorts) {
  // `module top()` has an empty port list
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty());
}

// --- compiler behavior: vectored/scalared modifiers -------------------------

TEST_F(VectorVectoredTest, ScalaredModifierNotPresent) {
  // `scalared` keyword is absent from this declaration -- correctly
  // confirms getExplicitScalared() is false.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getExplicitScalared());
}

// --- structural completeness ------------------------------------------------

TEST_F(VectorVectoredTest, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

TEST_F(VectorVectoredTest, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

TEST_F(VectorVectoredTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

// ---------------------------------------------------------------------------
// The actual point of this file: the "vectored" keyword is explicitly used
// ---------------------------------------------------------------------------
TEST_F(VectorVectoredTest, NetShouldBeExplicitlyVectoredButIsNot) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_TRUE(net->getExplicitVectored())
      << "IEEE 1800-2023 Annex A grammar diagram (p.1019) maps the 'vectored declaration' branch "
         "directly to vpiExplicitVectored -- 'a' is declared with the literal 'vectored' keyword, "
         "so this should be true. HLC currently drops the modifier and always returns false. Note: "
         "vpiVector (LogicTypespec::getVector()) is a separate concept (multi-bit width) and IS "
         "set correctly -- this bug is specifically about the 'vectored' keyword modifier";
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
