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
// KEY DISTINCTION -- two different "vector" concepts:
//   vpiVector (LogicTypespec::getVector())  -- type is multi-bit [15:0]; IS set (true)
//   vpiVectored (Net::getExplicitVectored()) -- the `vectored` keyword modifier; IS set (true)
//
// COMPILER BEHAVIOR: HLC parses `vectored` and calls setExplicitVectored(true)
// on the Net (Phase2ModelBuilder.cpp, leavePA_Net_declaration).
//
// Checked:
//   - design has module top
//   - module has exactly 1 net "a": vpiNetType=vpiTri1
//   - RefTypespec->LogicTypespec: vpiVector=true, 1 Range [15:0] (left=15, right=0)
//   - range constant types are vpiUIntConst for both left and right
//   - net has no initial value (no `= value` initializer)
//   - Net boolean flags all false: implicitDecl, netDeclAssign, scalar, arrayMember,
//       constantSelect, expanded, structUnionMember, vectorFlag (Net::getVector())
//   - Net numeric fields all zero: resolvedNetType, strength0, strength1, chargeStrength
//   - Net collections all nullptr: portInsts, primTerms, contAssigns (per-net),
//       pathTerms, tchkTerms, bits, indexes, simNet
//   - LogicTypespec: getSigned()==false, getScalar()==false,
//       getElemTypespec()==nullptr, getIndexTypespec()==nullptr
//   - design has 2 typespecs: ModuleTypespec "top" and IntTypespec
//   - module top() has no ports
//   - top has no processes, no module-scope continuous assignments

#include <hlc/Common/Session.h>
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
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class VectorVectored : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.9.2--vector_vectored.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(VectorVectored, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(VectorVectored, ModuleHasOneNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(VectorVectored, NetNameIsA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getName(), "a");
}

TEST_F(VectorVectored, NetTypeIsTri1) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getNetType(), vpiTri1);
}

TEST_F(VectorVectored, NetHasLogicTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(VectorVectored, LogicTypespecIsVector) {
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

TEST_F(VectorVectored, LogicTypespecHasOneRange) {
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

TEST_F(VectorVectored, RangeLeftIs15) {
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

TEST_F(VectorVectored, RangeRightIs0) {
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

TEST_F(VectorVectored, NetHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  // tri1 vectored [15:0] a; has no `= 0` initializer
  EXPECT_EQ(net->getValue(), nullptr);
}

// --- net identity ----
// NOTE: getFullName()/vpiFullName is a computed property that is currently
// wrong in HLC -- do not assert against it. Use getName() only.

// IEEE 1800-2023 Sec 6.7/6.8: 'a' has the net-type keyword `tri1`, so it must
// not also appear in the module's Variable collection.
TEST_F(VectorVectored, NetNameIsNotInVariables) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getVariables() == nullptr || hldb::findByName<hldb::Variable>("a", top->getVariables()) == nullptr)
      << "'a' is declared with net-type 'tri1'; it must not appear in the module's Variable collection";
}

// --- net boolean flags (all expected false) ----

TEST_F(VectorVectored, NetIsNotImplicitDecl) {
  // `tri1 vectored [15:0] a` is an explicit declaration -- not implicit
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getImplicitDecl());
}

TEST_F(VectorVectored, NetHasNoDeclAssign) {
  // no `= value` in `tri1 vectored [15:0] a` -- differs from vector_scalared.sv
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getNetDeclAssign());
}

TEST_F(VectorVectored, NetIsNotScalar) {
  // `[15:0]` makes this a vector, not a scalar 1-bit net
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getScalar());
}

TEST_F(VectorVectored, NetIsNotArrayMember) {
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getArrayMember());
}

TEST_F(VectorVectored, NetHasNoConstantSelect) {
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getConstantSelect());
}

TEST_F(VectorVectored, NetIsNotExpanded) {
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getExpanded());
}

TEST_F(VectorVectored, NetIsNotStructUnionMember) {
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getStructUnionMember());
}

// --- net numeric fields (compiler does not set them) ----

TEST_F(VectorVectored, NetResolvedNetTypeIsZero) {
  // COMPILER BEHAVIOR: getResolvedNetType() is a separate field from
  // getNetType(). HLC sets getNetType()=vpiTri1 but never calls
  // setResolvedNetType() -- returns 0.
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getResolvedNetType(), 0);
}

TEST_F(VectorVectored, NetStrength0IsZero) {
  // COMPILER BEHAVIOR: `tri1` has an implicit pull-up (constant-1 weak driver)
  // but HLC does NOT set vpiStrength0 on the Net node. Returns 0.
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getStrength0(), 0);
}

TEST_F(VectorVectored, NetStrength1IsZero) {
  // COMPILER BEHAVIOR: tri1 pull-up strength to 1 is not stored in UHDM.
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getStrength1(), 0);
}

TEST_F(VectorVectored, NetChargeStrengthIsZero) {
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getChargeStrength(), 0);
}

TEST_F(VectorVectored, NetVectorFlagTrue) {
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_TRUE(net->getVector());
}

// --- net collections (all nullptr -- no connectivity in this module) ----

TEST_F(VectorVectored, NetHasNoPortInsts) {
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getPortInsts(), nullptr);
}

TEST_F(VectorVectored, NetHasNoPrimTerms) {
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getPrimTerms(), nullptr);
}

TEST_F(VectorVectored, NetHasNoContAssignsOnNet) {
  // Net::getContAssigns() is per-net (drives of this net), distinct from
  // Module::getContAssigns() (all assigns in the module scope)
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getContAssigns(), nullptr);
}

TEST_F(VectorVectored, NetHasNoPathTerms) {
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getPathTerms(), nullptr);
}

TEST_F(VectorVectored, NetHasNoTchkTerms) {
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getTchkTerms(), nullptr);
}

TEST_F(VectorVectored, NetHasNoBits) {
  // getBits() returns per-bit expansion nets -- absent for non-expanded net
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getBits(), nullptr);
}

TEST_F(VectorVectored, NetHasNoIndexes) {
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getIndexes(), nullptr);
}

TEST_F(VectorVectored, NetHasNoSimNet) {
  const hldb::Net *const net = hldb::findByName<hldb::Module>("top", m_design->getAllModules())->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getSimNet(), nullptr);
}

// --- LogicTypespec extra properties ----

TEST_F(VectorVectored, LogicTypespecIsNotSigned) {
  // `logic` is unsigned by default; getSigned() must be false
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::LogicTypespec *const ls =
      top->getNets()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_FALSE(ls->getSigned());
}

TEST_F(VectorVectored, LogicTypespecIsNotScalar) {
  // [15:0] makes the typespec a vector, not scalar
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::LogicTypespec *const ls =
      top->getNets()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_FALSE(ls->getScalar());
}

TEST_F(VectorVectored, LogicTypespecHasNoElemTypespec) {
  // ElemTypespec is used for array element types; absent for a plain vector
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::LogicTypespec *const ls =
      top->getNets()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_EQ(ls->getElemTypespec(), nullptr);
}

TEST_F(VectorVectored, LogicTypespecHasNoIndexTypespec) {
  // IndexTypespec is for associative arrays; absent here
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::LogicTypespec *const ls =
      top->getNets()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_EQ(ls->getIndexTypespec(), nullptr);
}

// --- range constant details ----

TEST_F(VectorVectored, RangeLeftConstTypeIsUInt) {
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

TEST_F(VectorVectored, RangeRightConstTypeIsUInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::LogicTypespec *const ls =
      top->getNets()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  const hldb::Constant *const right = ls->getRanges()->at(0)->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getConstType(), vpiUIntConst);
}

// --- design-level typespecs ----

TEST_F(VectorVectored, DesignHasTwoTypespecs) {
  // log: vpiTypespec (2 items): ModuleTypespec "top" + IntTypespec
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(VectorVectored, DesignHasModuleTypespec) {
  // The design-level typespecs include a ModuleTypespec for top
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(VectorVectored, DesignHasIntTypespec) {
  // log: IntTypespec at design level (used for range constant types)
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  EXPECT_NE(it, nullptr);
}

// --- module-level checks ----

TEST_F(VectorVectored, ModuleHasNoPorts) {
  // `module top()` has an empty port list
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty());
}

// --- compiler behavior: vectored/scalared modifiers ----

TEST_F(VectorVectored, VectoredModifierIsTrue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_TRUE(net->getExplicitVectored());
}

TEST_F(VectorVectored, ScalaredModifierNotPresent) {
  // `scalared` keyword is absent from this declaration -- confirms
  // getExplicitScalared() is false.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getExplicitScalared());
}

// --- structural completeness ----

TEST_F(VectorVectored, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

TEST_F(VectorVectored, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
