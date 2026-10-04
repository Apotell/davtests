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

// Tests for tests/ExtendClassMember/dut.sv:
//
//   package pkg;
//     class uvm_tlm_fifo;
//       bit print_enabled = 1;
//     endclass
//
//     class uvm_sequencer_analysis_fifo #(type RSP = uvm_sequence_item)
//         extends uvm_tlm_fifo #(RSP);
//     endclass
//
//     class uvm_sequencer_param_base #(type REQ = uvm_sequence_item,
//                                      type RSP = REQ)
//         extends uvm_sequencer_base;
//       uvm_sequencer_analysis_fifo #(RSP) sqr_rsp_analysis_fifo;
//     endclass
//
//     function uvm_sequencer_param_base::new ();
//       sqr_rsp_analysis_fifo.print_enabled = 0;
//     endfunction
//   endpackage
//
// This exercises IEEE 1800-2023 Sec 8.13 "Inheritance" through a chain of
// parameterized classes: "uvm_sequencer_analysis_fifo" is declared to
// extend "uvm_tlm_fifo #(RSP)", so a handle typed
// "uvm_sequencer_analysis_fifo#(RSP)" (as "sqr_rsp_analysis_fifo" is, a
// member of "uvm_sequencer_param_base") must be able to reach
// "print_enabled" -- a property declared ONLY in the base class
// "uvm_tlm_fifo", never in "uvm_sequencer_analysis_fifo" itself. The
// out-of-body constructor body "sqr_rsp_analysis_fifo.print_enabled = 0;"
// is exactly that access.
//
// Two things in this source are NOT well-formed and are called out rather
// than silently asserted around:
//   - "uvm_sequencer_param_base extends uvm_sequencer_base": Sec 8.13
//     requires the base class type to be declared and visible; there is
//     no "uvm_sequencer_base" class anywhere in this file (or any import
//     -- there is none), so this extends clause cannot resolve, and a
//     Sec 6.3-style failed-to-bind diagnostic naming "uvm_sequencer_base"
//     is expected.
//   - "function uvm_sequencer_param_base::new ();" is an out-of-block
//     method declaration (Sec 8.24), but "uvm_sequencer_param_base"
//     declares no matching "extern function new();" prototype inside its
//     class body. Per Sec 8.24 this out-of-block body has nothing to bind
//     to. hlc/ErrorCatalog/chapter-8/8--error_rules/test_8_error_rules.cpp
//     already documents that this exact category of diagnostic
//     (COMP_MISPLACED_EXTERN_DECLARATION for an out-of-block method with
//     no preceding extern prototype) is not wired up in this build, so
//     the object-model shape produced for a case like this one is not
//     independently established anywhere else in this suite either; this
//     file does not guess at it (see the GTEST_SKIP'd cases below).
//
// Checked:
//   - package "pkg" exists with exactly 3 ClassDefns, in source order:
//     "uvm_tlm_fifo", "uvm_sequencer_analysis_fifo",
//     "uvm_sequencer_param_base"
//   - "uvm_tlm_fifo": exactly 1 property "print_enabled", initializer
//     Constant "1", public visibility by default (Sec 8.14)
//   - "uvm_sequencer_analysis_fifo": exactly 1 type parameter "RSP";
//     extends "uvm_tlm_fifo", and that Extends resolves (via its
//     RefTypespec -> ClassTypespec) to "uvm_tlm_fifo"'s own ClassDefn --
//     THE crux of the extends-with-inherited-member relationship
//   - "uvm_sequencer_param_base": exactly 2 type parameters "REQ", "RSP";
//     declares "uvm_sequencer_base" as its base (Extends' RefTypespec
//     named "uvm_sequencer_base"), which does NOT resolve to any
//     ClassDefn, and the compiler reports a failed-to-bind diagnostic for
//     that name
//   - "uvm_sequencer_param_base" has exactly 1 property
//     "sqr_rsp_analysis_fifo", whose typespec resolves to a ClassTypespec
//     named "uvm_sequencer_analysis_fifo" that itself resolves
//     (getClassDefn()) to "uvm_sequencer_analysis_fifo"'s own ClassDefn,
//     with 1 ParamAssign for "RSP"
//   - the out-of-body "new"'s member-access resolution
//     (GTEST_SKIP'd -- see note above)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/extends.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/type_parameter.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ExtendClassMemberTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ExtendClassMember.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("pkg", m_design->getAllPackages());
  }

  static const hldb::ClassDefn *getTlmFifo() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByDefName<hldb::ClassDefn>("uvm_tlm_fifo", pkg->getClassDefns());
  }

  static const hldb::ClassDefn *getAnalysisFifo() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByDefName<hldb::ClassDefn>("uvm_sequencer_analysis_fifo", pkg->getClassDefns());
  }

  static const hldb::ClassDefn *getParamBase() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByDefName<hldb::ClassDefn>("uvm_sequencer_param_base", pkg->getClassDefns());
  }

  static const hldb::Variable *getPrintEnabled() {
    const hldb::ClassDefn *const c = getTlmFifo();
    if (c == nullptr || c->getVariables() == nullptr || c->getVariables()->empty()) return nullptr;
    return c->getVariables()->at(0);
  }

  static const hldb::Variable *getSqrRspAnalysisFifo() {
    const hldb::ClassDefn *const c = getParamBase();
    if (c == nullptr || c->getVariables() == nullptr || c->getVariables()->empty()) return nullptr;
    return c->getVariables()->at(0);
  }
};

// ---------------------------------------------------------------------------
// package / class shape
// ---------------------------------------------------------------------------

TEST_F(ExtendClassMemberTest, PackagePkgExists) { ASSERT_NE(getPkg(), nullptr); }

TEST_F(ExtendClassMemberTest, PackageHasThreeClassDefns) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getClassDefns(), nullptr);
  EXPECT_EQ(pkg->getClassDefns()->size(), 3u);
}

// ---------------------------------------------------------------------------
// class uvm_tlm_fifo
// ---------------------------------------------------------------------------

TEST_F(ExtendClassMemberTest, TlmFifoExists) { ASSERT_NE(getTlmFifo(), nullptr); }

TEST_F(ExtendClassMemberTest, TlmFifoHasOnePropertyPrintEnabled) {
  const hldb::ClassDefn *const c = getTlmFifo();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getVariables(), nullptr);
  ASSERT_EQ(c->getVariables()->size(), 1u);
  const hldb::Variable *const p = getPrintEnabled();
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getName(), "print_enabled");
}

TEST_F(ExtendClassMemberTest, PrintEnabledHasInitializerOne) {
  const hldb::Variable *const p = getPrintEnabled();
  ASSERT_NE(p, nullptr);
  const hldb::Constant *const init = p->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "'bit print_enabled = 1;' should attach '1' as its own initializer";
  EXPECT_EQ(init->getDecompile(), "1");
}

TEST_F(ExtendClassMemberTest, PrintEnabledIsPublicByDefault) {
  const hldb::Variable *const p = getPrintEnabled();
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getVisibility(), vpiPublicVis) << "Sec 8.14: 'bit print_enabled = 1;' with no visibility "
                                                  "qualifier defaults to public";
}

// ---------------------------------------------------------------------------
// class uvm_sequencer_analysis_fifo #(type RSP = uvm_sequence_item)
//     extends uvm_tlm_fifo #(RSP)
// ---------------------------------------------------------------------------

TEST_F(ExtendClassMemberTest, AnalysisFifoExists) { ASSERT_NE(getAnalysisFifo(), nullptr); }

TEST_F(ExtendClassMemberTest, AnalysisFifoHasOneTypeParameterRsp) {
  const hldb::ClassDefn *const c = getAnalysisFifo();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getParameters(), nullptr);
  ASSERT_EQ(c->getParameters()->size(), 1u);
  const hldb::TypeParameter *const rsp = any_cast<hldb::TypeParameter>(c->getParameters()->at(0));
  ASSERT_NE(rsp, nullptr) << "'type RSP = uvm_sequence_item' should be a TypeParameter";
  EXPECT_EQ(rsp->getName(), "RSP");
}

// THE crux of the extends-with-inherited-member relationship: this
// Extends must resolve back to uvm_tlm_fifo's own ClassDefn, the class
// that actually declares 'print_enabled'.
TEST_F(ExtendClassMemberTest, AnalysisFifoExtendsTlmFifo) {
  const hldb::ClassDefn *const c = getAnalysisFifo();
  ASSERT_NE(c, nullptr);
  const hldb::Extends *const ext = c->getExtends();
  ASSERT_NE(ext, nullptr) << "'extends uvm_tlm_fifo #(RSP)' should attach an Extends object";
  ASSERT_NE(ext->getClassTypespecs(), nullptr);
  ASSERT_EQ(ext->getClassTypespecs()->size(), 1u);
  const hldb::RefTypespec *const ref = ext->getClassTypespecs()->at(0);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "uvm_tlm_fifo #(RSP)");
  const hldb::ClassTypespec *const ct = ref->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr) << "'uvm_tlm_fifo' IS declared, so the base-class typespec should resolve";
  EXPECT_EQ(ct->getClassDefn(), getTlmFifo());
}

// ---------------------------------------------------------------------------
// class uvm_sequencer_param_base #(type REQ = ..., type RSP = REQ)
//     extends uvm_sequencer_base
// ---------------------------------------------------------------------------

TEST_F(ExtendClassMemberTest, ParamBaseExists) { ASSERT_NE(getParamBase(), nullptr); }

TEST_F(ExtendClassMemberTest, ParamBaseHasTwoTypeParameters) {
  const hldb::ClassDefn *const c = getParamBase();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getParameters(), nullptr);
  ASSERT_EQ(c->getParameters()->size(), 2u);
  const hldb::TypeParameter *const req = any_cast<hldb::TypeParameter>(c->getParameters()->at(0));
  ASSERT_NE(req, nullptr);
  EXPECT_EQ(req->getName(), "REQ");
  const hldb::TypeParameter *const rsp = any_cast<hldb::TypeParameter>(c->getParameters()->at(1));
  ASSERT_NE(rsp, nullptr);
  EXPECT_EQ(rsp->getName(), "RSP");
}

// 'uvm_sequencer_base' is never declared anywhere in this file: per
// Sec 8.13, the extends clause names a class that must be visible, so
// this cannot resolve to any ClassDefn.
TEST_F(ExtendClassMemberTest, ParamBaseExtendsUnresolvedSequencerBase) {
  const hldb::ClassDefn *const c = getParamBase();
  ASSERT_NE(c, nullptr);
  const hldb::Extends *const ext = c->getExtends();
  ASSERT_NE(ext, nullptr) << "'extends uvm_sequencer_base' should still attach an Extends object syntactically";
  ASSERT_NE(ext->getClassTypespecs(), nullptr);
  ASSERT_EQ(ext->getClassTypespecs()->size(), 1u);
  const hldb::RefTypespec *const ref = ext->getClassTypespecs()->at(0);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "uvm_sequencer_base");
  EXPECT_EQ(ref->getActual(), nullptr) << "'uvm_sequencer_base' is never declared, so it must not resolve";
}

TEST_F(ExtendClassMemberTest, CompilerReportsFailedToBindForSequencerBase) {
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "uvm_sequencer_base"), nullptr)
      << "Sec 8.13/6.3: 'uvm_sequencer_base' is used as a base class but never declared";
}

TEST_F(ExtendClassMemberTest, ParamBaseHasOnePropertySqrRspAnalysisFifo) {
  const hldb::ClassDefn *const c = getParamBase();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getVariables(), nullptr);
  ASSERT_EQ(c->getVariables()->size(), 1u);
  const hldb::Variable *const v = getSqrRspAnalysisFifo();
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getName(), "sqr_rsp_analysis_fifo");
}

TEST_F(ExtendClassMemberTest, SqrRspAnalysisFifoTypespecResolvesToAnalysisFifoClassDefn) {
  const hldb::Variable *const v = getSqrRspAnalysisFifo();
  ASSERT_NE(v, nullptr);
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::ClassTypespec *const ct = v->getTypespec<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr) << "'uvm_sequencer_analysis_fifo #(RSP) sqr_rsp_analysis_fifo;' should resolve to a "
                             "ClassTypespec";
  EXPECT_EQ(ct->getDefName(), "uvm_sequencer_analysis_fifo");
  EXPECT_EQ(ct->getClassDefn(), getAnalysisFifo());
}

TEST_F(ExtendClassMemberTest, SqrRspAnalysisFifoTypespecHasOneParamAssignForRsp) {
  const hldb::Variable *const v = getSqrRspAnalysisFifo();
  ASSERT_NE(v, nullptr);
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::ClassTypespec *const ct = v->getTypespec<hldb::RefTypespec>()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(ct, nullptr);
  ASSERT_NE(ct->getParamAssigns(), nullptr);
  ASSERT_EQ(ct->getParamAssigns()->size(), 1u);
  const hldb::ParamAssign *const pa = ct->getParamAssigns()->at(0);
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "RSP");
}

// ---------------------------------------------------------------------------
// function uvm_sequencer_param_base::new (); sqr_rsp_analysis_fifo.print_enabled = 0; endfunction
// ---------------------------------------------------------------------------

// This out-of-block "new" has no matching "extern function new();"
// prototype inside uvm_sequencer_param_base's class body (Sec 8.24), and
// hlc/ErrorCatalog/chapter-8/8--error_rules/test_8_error_rules.cpp already
// documents that HLC does not wire up a diagnostic for this exact
// category of malformed out-of-block declaration. With no other test in
// this suite establishing what object-model shape an unmatched out-of-block
// method produces, asserting one here would just lock in unverified
// output rather than something derived from the standard. Skipped rather
// than guessed at.
TEST_F(ExtendClassMemberTest, OutOfBlockNewMemberAccessResolution) {
  GTEST_SKIP() << "IEEE 1800-2023 Sec 8.24: 'function uvm_sequencer_param_base::new ()' has no matching "
                  "'extern function new();' prototype inside the class body, so this out-of-block declaration "
                  "is malformed; HLC does not diagnose this case (see "
                  "hlc/ErrorCatalog/chapter-8/8--error_rules/test_8_error_rules.cpp), and no other test in this "
                  "suite establishes what shape its resulting object model should take. Fix pending upstream "
                  "before the 'sqr_rsp_analysis_fifo.print_enabled' member-access chain through the extends "
                  "relationship can be verified here.";
}

// ---------------------------------------------------------------------------
// compiler diagnostics
// ---------------------------------------------------------------------------

TEST_F(ExtendClassMemberTest, CompilerReportsAtLeastOneError) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbError, 0) << "'extends uvm_sequencer_base' names an undeclared class (Sec 8.13/6.3) and "
                                   "must be diagnosed as an error";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
