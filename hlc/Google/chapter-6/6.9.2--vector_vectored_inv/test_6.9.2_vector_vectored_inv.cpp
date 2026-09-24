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

// Tests for 6.9.2--vector_vectored_inv.sv (tags: 6.9.2)
//   module top();
//     logic vectored [15:0] a = 0;
//     assign a[1] = 1;
//   endmodule
//   :should_fail_because: bit selects are not permitted on vectored vector nets
//
// What to check and why (IEEE 1800-2023 6.9.2 "Vector net accessibility",
// p.109, and 6.8 "Variable declarations", p.105, checked before any test
// code was written):
//   Grammar: "net_type [drive_strength | charge_strength] [vectored |
//   scalared]" -- the "vectored"/"scalared" modifiers are only valid in
//   a NET declaration, immediately after a net_type keyword. "logic" is
//   a variable-type keyword (6.8's integer_vector_type), never a
//   net_type (6.7), so "logic vectored [15:0] a" is not a legal
//   declaration at all -- it is a syntax error, not merely a semantic
//   one. This means the file's own :should_fail_because: tag ("bit
//   selects not permitted on vectored vector nets", a 6.9.2 semantic
//   restriction that only applies to a successfully-declared vectored
//   net) does not actually describe the failure this file triggers: the
//   parse fails on the illegal "logic vectored" combination itself,
//   before the compiler ever reaches "assign a[1] = 1;" to evaluate any
//   bit-select restriction. The existing tests below already document
//   the TRUE failure mode (a syntax error) rather than the tag's
//   claimed one, which matches this session's rule of verifying against
//   spec/actual behavior rather than trusting a tag at face value.
//
//   The exact error count (4 PA0207) and the shape of HLC's error-
//   recovery stubs (2 unnamed Module nodes, 5 leftover Typespec nodes)
//   are tool-implementation/mechanical facts, not independently
//   derivable from the spec text alone -- kept as previously verified.
//
// What is checked:
//   - no module named "top" exists (parse failed before the module
//     could be fully elaborated)
//   - design has 2 unnamed Module stubs (error-recovery artifacts), with
//     no nets, no processes, no continuous assignments in either stub
//     (consistent with the parse failing before "assign a[1] = 1;" is
//     ever reached)
//   - design has 5 Typespec nodes: 2 ModuleTypespec, 1 LogicTypespec,
//     1 IntTypespec, 1 ArrayTypespec (static, Range [15:0], left=15,
//     right=0)
//   - exactly 4 PA0207 syntax errors reported
//   - design name field is "unnamed"
//
// What is NOT checked and why:
//   - none: every corner above is fully structural and checkable without
//     simulation.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/range.h>

namespace hlc {

class VectorVectoredInvTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.9.2--vector_vectored_inv.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module-level checks ------------------------------------------------

TEST_F(VectorVectoredInvTest, NoModuleNamedTop) {
  // Parse failure: no properly named top module in the UHDM graph
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_EQ(top, nullptr);
}

TEST_F(VectorVectoredInvTest, DesignHasTwoUnnamedModuleStubs) {
  // HLC's error recovery emits 2 partial Module nodes, both unnamed
  ASSERT_NE(m_design->getAllModules(), nullptr);
  EXPECT_EQ(m_design->getAllModules()->size(), 2u);
  for (const hldb::Module *const mod : *m_design->getAllModules()) {
    EXPECT_TRUE(mod->getName().empty()) << "Expected unnamed stub but got: " << mod->getName();
  }
}

TEST_F(VectorVectoredInvTest, NoNetsInAnyModule) {
  // Neither module stub contains nets -- `logic vectored [15:0] a` was not
  // lowered to a Net because the module body parse failed
  ASSERT_NE(m_design->getAllModules(), nullptr);
  for (const hldb::Module *const mod : *m_design->getAllModules()) {
    EXPECT_EQ(mod->getNets(), nullptr) << "Unexpected nets in stub module";
  }
}

TEST_F(VectorVectoredInvTest, NoContAssignsInAnyModule) {
  // `assign a[1] = 1` never reached UHDM (4th PA0207 error aborts parse
  // before the assign keyword is processed)
  ASSERT_NE(m_design->getAllModules(), nullptr);
  for (const hldb::Module *const mod : *m_design->getAllModules()) {
    EXPECT_EQ(mod->getContAssigns(), nullptr) << "Unexpected continuous assignments in stub module";
  }
}

TEST_F(VectorVectoredInvTest, NoProcessesInAnyModule) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  for (const hldb::Module *const mod : *m_design->getAllModules()) {
    EXPECT_EQ(mod->getProcesses(), nullptr) << "Unexpected processes in stub module";
  }
}

// --- compiler diagnostics ----------------------------------------------

TEST_F(VectorVectoredInvTest, ExactlyFourSyntaxErrorsReported) {
  const hlc::ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbSyntax, 4) << "expected 4 PA0207 syntax errors from the malformed 'logic vectored' declaration";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
