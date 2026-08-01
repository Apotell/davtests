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

// Tests for 6.5--variable_assignment.sv (tags: 6.5)
//   module top();
//     int v;
//     assign v = 12;
//   endmodule
//
// What to check and why (IEEE 1800-2023 10.3.2, p.248-249, checked before
// any test code was written):
//   "The continuous assignment statement shall place a continuous
//   assignment on a net OR VARIABLE data type ... Variables can only be
//   driven by one continuous assignment ..."
//   "int v; assign v = 12;" is exactly this: a legal "variable continuous
//   assignment." "v" is a VARIABLE -- "int" never appears in IEEE
//   1800-2023's net_type keyword list (6.7: wire, tri, triand, trior,
//   trireg, tri0, tri1, uwire, wand, wor, supply0, supply1) -- regardless
//   of the fact that it's driven the same way a net would be. The file's
//   own :description: ("Variable assignment tests") confirms this is the
//   intended point of the file.
//
//   A prior version of this test used hldb::Net/getNets() for "v"
//   throughout. That was not re-derived from this reasoning -- it
//   silently assumed hldb's own object-type label was correct. Every
//   check below instead targets hldb::Variable/getVariables(), per the
//   spec text above, and is written to FAIL if hldb still misclassifies
//   "v" as a Net. That failure (if it occurs) is the point: it documents
//   a real compiler bug, not something to "fix" by reverting to Net.
//
// What is checked:
//   - module top has zero Nets (no net-type keyword appears anywhere in
//     this file) and exactly one Variable, "v"
//   - "v" is IntTypespec (the "int" keyword's type; unaffected by the
//     net/variable question)
//   - "v" has no declaration-time initializer (IEEE 1800-2023 10.3.2: a
//     variable driven by a continuous assignment must not ALSO have an
//     initializer -- this file correctly avoids that additional error)
//   - exactly 1 ContAssign whose LHS RefObj "v" resolves via
//     getActual<hldb::Variable>() -- NOT getActual<hldb::Net>() -- to
//     that same Variable object; this is the crux of the whole file
//   - ContAssign RHS is Constant "12"
//   - top has no processes (nothing procedural touches "v" here, which is
//     what keeps this file legal -- contrast with the illegal sibling
//     6.5--variable_mixed_assignments.sv, which adds a procedural
//     assignment to the same continuously-assigned variable)
//   - compiler emits zero errors (this file is fully legal per spec)
//
// What is NOT checked and why:
//   - none: every corner above is fully structural and checkable without
//     simulation, and this file has no runtime-only assertion to skip.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class VariableAssignmentTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.5--variable_assignment.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- existence: module, and "v" as a Variable (not a Net) -----------------

TEST_F(VariableAssignmentTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(VariableAssignmentTest, ModuleHasNoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'int v' declares no net-type keyword (IEEE 1800-2023 6.7) anywhere in this file -- "
         "there should be no Net objects at all";
}

TEST_F(VariableAssignmentTest, ModuleHasOneVariableV) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr)
      << "'int v' should be represented as a Variable (IEEE 1800-2023 10.3.2: a variable can be "
         "driven by a continuous assignment); if this is null, hldb likely misclassified 'v' as "
         "a Net instead";
  ASSERT_EQ(top->getVariables()->size(), 1u);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("v", top->getVariables());
  ASSERT_NE(v, nullptr) << "Variable 'v' not found";
}

// --- content: v's type, and that it has no conflicting initializer --------

TEST_F(VariableAssignmentTest, VIsIntTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("v", top->getVariables());
  ASSERT_NE(v, nullptr);
  EXPECT_NE(v->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(VariableAssignmentTest, VHasNoDeclarationTimeInitializer) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("v", top->getVariables());
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getValue<hldb::Constant>(), nullptr)
      << "IEEE 1800-2023 10.3.2: a variable driven by a continuous assignment must not also "
         "have a declaration-time initializer -- this file correctly has none";
}

// --- shape: the continuous assignment itself, and what it targets ---------

TEST_F(VariableAssignmentTest, ContAssignExists) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
}

TEST_F(VariableAssignmentTest, ContAssignLhsResolvesToVariableVNotNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "v");
  EXPECT_EQ(lhs->getActual<hldb::Net>(), nullptr)
      << "'v' must NOT resolve to a Net -- 'int' is a variable-type keyword (IEEE 1800-2023 6.7 "
         "does not list it), regardless of where it's declared";
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr)
      << "'v' should resolve to the Variable object per IEEE 1800-2023 10.3.2";
}

TEST_F(VariableAssignmentTest, ContAssignRhsIsConstantTwelve) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Constant *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "12");
}

// --- absence: no procedural driver (that would make this file illegal) ----

TEST_F(VariableAssignmentTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty())
      << "no procedural assignment to 'v' -- this is what keeps this file legal, contrast with "
         "6.5--variable_mixed_assignments.sv";
}

TEST_F(VariableAssignmentTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
