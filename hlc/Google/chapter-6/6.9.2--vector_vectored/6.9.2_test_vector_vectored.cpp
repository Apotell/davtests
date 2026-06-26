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
// KEY DISTINCTION — two different "vector" concepts:
//   vpiVector (LogicTypespec::getVector())  — type is multi-bit [15:0];  IS set (true)
//   vpiVectored (Net::getExplicitVectored()) — the `vectored` keyword modifier; NOT set
//
// COMPILER BEHAVIOR: Surelog parses `vectored` without errors but does NOT call
// setExplicitVectored(true) on the Net. The UHDM dump has no vpiVectored property.
//
// Checked:
//   - design has module work@top
//   - module has exactly 1 net "a": vpiNetType=vpiTri1, getFullName()="work@top.a"
//   - RefTypespec→LogicTypespec: vpiVector=true, 1 Range [15:0] (left=15, right=0)
//   - range constant types are vpiUIntConst for both left and right
//   - net has no initial value (no `= value` initializer)
//   - Net boolean flags all false: implicitDecl, netDeclAssign, scalar, arrayMember,
//       constantSelect, expanded, structUnionMember, vectorFlag (Net::getVector())
//   - getExplicitVectored() == false — COMPILER BEHAVIOR: vectored keyword silently dropped
//   - getExplicitScalared() == false — no scalared keyword in this declaration
//   - Net numeric fields all zero: resolvedNetType, strength0, strength1, chargeStrength
//   - Net collections all nullptr: portInsts, primTerms, contAssigns (per-net),
//       pathTerms, tchkTerms, bits, indexes, simNet
//   - LogicTypespec: getSigned()==false, getScalar()==false,
//       getElemTypespec()==nullptr, getIndexTypespec()==nullptr
//   - design has 2 typespecs: ModuleTypespec "work@top" and IntTypespec
//   - module top() has no ports
//   - work@top has no processes, no module-scope continuous assignments
//
// Not checked:
//   - SV-spec enforcement: bit-selects on vectored nets should be illegal at simulation

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/int_typespec.h>
#include <uhdm/logic_typespec.h>
#include <uhdm/module.h>
#include <uhdm/module_typespec.h>
#include <uhdm/net.h>
#include <uhdm/range.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class VectorVectored : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.9.2--vector_vectored.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

TEST_F(VectorVectored, ModuleExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(VectorVectored, ModuleHasOneNet) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(VectorVectored, NetNameIsA) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getName(), "a");
}

TEST_F(VectorVectored, NetTypeIsTri1) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getNetType(), vpiTri1);
}

TEST_F(VectorVectored, NetHasLogicTypespec) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const uhdm::RefTypespec *const rt = net->getTypespec<uhdm::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<uhdm::LogicTypespec>(), nullptr);
}

TEST_F(VectorVectored, LogicTypespecIsVector) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const uhdm::RefTypespec *const rt = net->getTypespec<uhdm::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const uhdm::LogicTypespec *const ls =
      rt->getActual<uhdm::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_TRUE(ls->getVector());
}

TEST_F(VectorVectored, LogicTypespecHasOneRange) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const uhdm::RefTypespec *const rt = net->getTypespec<uhdm::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const uhdm::LogicTypespec *const ls =
      rt->getActual<uhdm::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  ASSERT_NE(ls->getRanges(), nullptr);
  EXPECT_EQ(ls->getRanges()->size(), 1u);
}

TEST_F(VectorVectored, RangeLeftIs15) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const uhdm::RefTypespec *const rt = net->getTypespec<uhdm::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const uhdm::LogicTypespec *const ls =
      rt->getActual<uhdm::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  ASSERT_NE(ls->getRanges(), nullptr);
  const uhdm::Range *const range = ls->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  const uhdm::Constant *const left =
      range->getLeftExpr<uhdm::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "15");
}

TEST_F(VectorVectored, RangeRightIs0) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const uhdm::RefTypespec *const rt = net->getTypespec<uhdm::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  const uhdm::LogicTypespec *const ls =
      rt->getActual<uhdm::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  ASSERT_NE(ls->getRanges(), nullptr);
  const uhdm::Range *const range = ls->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  const uhdm::Constant *const right =
      range->getRightExpr<uhdm::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(VectorVectored, NetHasNoInitialValue) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  // tri1 vectored [15:0] a; has no `= 0` initializer
  EXPECT_EQ(net->getValue(), nullptr);
}

// --- net identity -----------------------------------------------------------

TEST_F(VectorVectored, NetFullNameIsWorkAtTopDotA) {
  // log line: vpiFullName: work@top.a
  const uhdm::Net *const net =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules())
          ->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getFullName(), "work@top.a");
}

// --- net boolean flags (all expected false) ----------------------------------

TEST_F(VectorVectored, NetIsNotImplicitDecl) {
  // `tri1 vectored [15:0] a` is an explicit declaration — not implicit
  const uhdm::Net *const net =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules())
          ->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getImplicitDecl());
}

TEST_F(VectorVectored, NetHasNoDeclAssign) {
  // no `= value` in `tri1 vectored [15:0] a` — differs from vector_scalared.sv
  const uhdm::Net *const net =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules())
          ->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getNetDeclAssign());
}

TEST_F(VectorVectored, NetIsNotScalar) {
  // `[15:0]` makes this a vector, not a scalar 1-bit net
  const uhdm::Net *const net =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules())
          ->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getScalar());
}

TEST_F(VectorVectored, NetIsNotArrayMember) {
  const uhdm::Net *const net =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules())
          ->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getArrayMember());
}

TEST_F(VectorVectored, NetHasNoConstantSelect) {
  const uhdm::Net *const net =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules())
          ->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getConstantSelect());
}

TEST_F(VectorVectored, NetIsNotExpanded) {
  const uhdm::Net *const net =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules())
          ->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getExpanded());
}

TEST_F(VectorVectored, NetIsNotStructUnionMember) {
  const uhdm::Net *const net =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules())
          ->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getStructUnionMember());
}

// --- net numeric fields (compiler does not set them) -------------------------

TEST_F(VectorVectored, NetResolvedNetTypeIsZero) {
  // COMPILER BEHAVIOR: getResolvedNetType() is a separate field from
  // getNetType(). Surelog sets getNetType()=vpiTri1 but never calls
  // setResolvedNetType() — returns 0.
  const uhdm::Net *const net =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules())
          ->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getResolvedNetType(), 0);
}

TEST_F(VectorVectored, NetStrength0IsZero) {
  // COMPILER BEHAVIOR: `tri1` has an implicit pull-up (constant-1 weak driver)
  // but Surelog does NOT set vpiStrength0 on the Net node. Returns 0.
  const uhdm::Net *const net =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules())
          ->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getStrength0(), 0);
}

TEST_F(VectorVectored, NetStrength1IsZero) {
  // COMPILER BEHAVIOR: tri1 pull-up strength to 1 is not stored in UHDM.
  const uhdm::Net *const net =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules())
          ->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getStrength1(), 0);
}

TEST_F(VectorVectored, NetChargeStrengthIsZero) {
  const uhdm::Net *const net =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules())
          ->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getChargeStrength(), 0);
}

TEST_F(VectorVectored, NetVectorFlagFalse) {
  // COMPILER BEHAVIOR: Net::getVector() is a separate field from
  // LogicTypespec::getVector(). Surelog only sets vpiVector on the
  // LogicTypespec (confirmed by log), NOT directly on the Net node.
  const uhdm::Net *const net =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules())
          ->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getVector());
}

// --- net collections (all nullptr — no connectivity in this module) ----------

TEST_F(VectorVectored, NetHasNoPortInsts) {
  const uhdm::Net *const net =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules())
          ->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getPortInsts(), nullptr);
}

TEST_F(VectorVectored, NetHasNoPrimTerms) {
  const uhdm::Net *const net =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules())
          ->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getPrimTerms(), nullptr);
}

TEST_F(VectorVectored, NetHasNoContAssignsOnNet) {
  // Net::getContAssigns() is per-net (drives of this net), distinct from
  // Module::getContAssigns() (all assigns in the module scope)
  const uhdm::Net *const net =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules())
          ->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getContAssigns(), nullptr);
}

TEST_F(VectorVectored, NetHasNoPathTerms) {
  const uhdm::Net *const net =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules())
          ->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getPathTerms(), nullptr);
}

TEST_F(VectorVectored, NetHasNoTchkTerms) {
  const uhdm::Net *const net =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules())
          ->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getTchkTerms(), nullptr);
}

TEST_F(VectorVectored, NetHasNoBits) {
  // getBits() returns per-bit expansion nets — absent for non-expanded net
  const uhdm::Net *const net =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules())
          ->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getBits(), nullptr);
}

TEST_F(VectorVectored, NetHasNoIndexes) {
  const uhdm::Net *const net =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules())
          ->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getIndexes(), nullptr);
}

TEST_F(VectorVectored, NetHasNoSimNet) {
  const uhdm::Net *const net =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules())
          ->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getSimNet(), nullptr);
}

// --- LogicTypespec extra properties ------------------------------------------

TEST_F(VectorVectored, LogicTypespecIsNotSigned) {
  // `logic` is unsigned by default; getSigned() must be false
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::LogicTypespec *const ls =
      top->getNets()->at(0)->getTypespec<uhdm::RefTypespec>()
          ->getActual<uhdm::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_FALSE(ls->getSigned());
}

TEST_F(VectorVectored, LogicTypespecIsNotScalar) {
  // [15:0] makes the typespec a vector, not scalar
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::LogicTypespec *const ls =
      top->getNets()->at(0)->getTypespec<uhdm::RefTypespec>()
          ->getActual<uhdm::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_FALSE(ls->getScalar());
}

TEST_F(VectorVectored, LogicTypespecHasNoElemTypespec) {
  // ElemTypespec is used for array element types; absent for a plain vector
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::LogicTypespec *const ls =
      top->getNets()->at(0)->getTypespec<uhdm::RefTypespec>()
          ->getActual<uhdm::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_EQ(ls->getElemTypespec(), nullptr);
}

TEST_F(VectorVectored, LogicTypespecHasNoIndexTypespec) {
  // IndexTypespec is for associative arrays; absent here
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::LogicTypespec *const ls =
      top->getNets()->at(0)->getTypespec<uhdm::RefTypespec>()
          ->getActual<uhdm::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  EXPECT_EQ(ls->getIndexTypespec(), nullptr);
}

// --- range constant details --------------------------------------------------

TEST_F(VectorVectored, RangeLeftConstTypeIsUInt) {
  // log line: vpiConstType: unsigned int (9)
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::LogicTypespec *const ls =
      top->getNets()->at(0)->getTypespec<uhdm::RefTypespec>()
          ->getActual<uhdm::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  const uhdm::Constant *const left =
      ls->getRanges()->at(0)->getLeftExpr<uhdm::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getConstType(), vpiUIntConst);
}

TEST_F(VectorVectored, RangeRightConstTypeIsUInt) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::LogicTypespec *const ls =
      top->getNets()->at(0)->getTypespec<uhdm::RefTypespec>()
          ->getActual<uhdm::LogicTypespec>();
  ASSERT_NE(ls, nullptr);
  const uhdm::Constant *const right =
      ls->getRanges()->at(0)->getRightExpr<uhdm::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getConstType(), vpiUIntConst);
}

// --- design-level typespecs --------------------------------------------------

TEST_F(VectorVectored, DesignHasTwoTypespecs) {
  // log: vpiTypespec (2 items): ModuleTypespec "work@top" + IntTypespec
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(VectorVectored, DesignHasModuleTypespec) {
  // The design-level typespecs include a ModuleTypespec for work@top
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const uhdm::ModuleTypespec *const mt =
      any_cast<uhdm::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "work@top");
}

TEST_F(VectorVectored, DesignHasIntTypespec) {
  // log: IntTypespec at design level (used for range constant types)
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const uhdm::IntTypespec *const it =
      any_cast<uhdm::IntTypespec>(m_design->getTypespecs()->at(1));
  EXPECT_NE(it, nullptr);
}

// --- module-level checks -----------------------------------------------------

TEST_F(VectorVectored, ModuleHasNoPorts) {
  // `module top()` has an empty port list
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty());
}

// --- compiler behavior: vectored/scalared modifiers -------------------------

TEST_F(VectorVectored, VectoredModifierNotStoredByCompiler) {
  // COMPILER BEHAVIOR: Surelog parses `vectored` without error but does NOT
  // call setExplicitVectored(true) — the modifier is silently dropped in UHDM.
  // The UHDM log has no vpiVectored line for this net.
  // Note: vpiVector=true (LogicTypespec::getVector()) is a separate concept —
  // it marks the type as multi-bit [15:0] and IS set correctly.
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getExplicitVectored());
}

TEST_F(VectorVectored, ScalaredModifierNotPresent) {
  // `scalared` keyword is absent from this declaration — confirms
  // getExplicitScalared() is false (not set by Surelog).
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_FALSE(net->getExplicitScalared());
}

// --- structural completeness ------------------------------------------------

TEST_F(VectorVectored, NoProcesses) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

TEST_F(VectorVectored, NoContAssigns) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace SURELOG
