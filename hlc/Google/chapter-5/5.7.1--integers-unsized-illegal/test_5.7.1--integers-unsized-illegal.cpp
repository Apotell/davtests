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

// Validates Surelog's recovery behaviour for the illegal literal '4af'.
//
// SV source (module top):
//   logic [31:0] a;
//   initial begin
//     a = 4af;  // ILLEGAL: hex requires the 'h base prefix, not bare digits
//   end
//
// '4af' is not valid SV syntax: hex literals must be written as 'h4af or
// 32'h4af.  A bare '4af' has no legal interpretation as a numeric literal.
//
// Surelog recovery: the parser interprets '4af' as a time literal — the
// integer token '4' followed by the identifier 'af' used as a time-unit
// specifier.  This produces one warning (WRN:PA0215 "Invalid VObject
// location") but NO syntax errors.  The module compiles successfully.
//
// In UHDM the assignment RHS is a Constant containing only the numeric part:
//   vpiConstType: unsigned int (9)
//   vpiSize:      64
//   vpiDecompile: "4"
//   vpiValue:     4
//
// The 'af' suffix is silently discarded as the recovered time unit.
// This is distinct from other illegal tests (e.g. integers-signed-illegal)
// where a hard syntax error leaves only nameless stub modules.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>

namespace hlc {

class IntegersUnsizedIllegal : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.7.1--integers-unsized-illegal.hlc"});

    // Compilation succeeds with a warning; m_design is fully populated.
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

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@top", d->getAllModules());
}

static const hldb::Begin *getBegin(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getProcesses() || m->getProcesses()->empty()) return nullptr;
  const auto *initial = any_cast<const hldb::Initial *>((*m->getProcesses())[0]);
  if (!initial) return nullptr;
  return initial->getStmt<hldb::Begin>();
}

// ---------------------------------------------------------------------------
// Unlike hard-syntax-error cases, '4af' is recovered — the module compiles.
// ---------------------------------------------------------------------------
TEST_F(IntegersUnsizedIllegal, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' should exist — Surelog recovers '4af' as a "
                                          "time literal rather than issuing a hard syntax error";
}

TEST_F(IntegersUnsizedIllegal, OneNetExists) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 1u) << "expected 1 net: a [31:0]";
}

// ---------------------------------------------------------------------------
// Initial block — 1 assignment is recovered
// ---------------------------------------------------------------------------
TEST_F(IntegersUnsizedIllegal, InitialHasOneAssignment) {
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 1u);
}

TEST_F(IntegersUnsizedIllegal, AssignmentIsBlocking) {
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  const auto *assign = any_cast<const hldb::Assignment *>((*begin->getStmts())[0]);
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
}

// ---------------------------------------------------------------------------
// RHS recovery: '4af' parsed as time literal; only the integer part '4'
// reaches UHDM as an unsigned-int constant.  The 'af' time-unit is dropped.
// ---------------------------------------------------------------------------
TEST_F(IntegersUnsizedIllegal, RhsIsConstant) {
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  const auto *assign = any_cast<const hldb::Assignment *>((*begin->getStmts())[0]);
  ASSERT_NE(assign, nullptr);
  EXPECT_NE(assign->getRhs<hldb::Constant>(), nullptr)
      << "the numeric '4' from '4af' should be recovered as a Constant RHS";
}

TEST_F(IntegersUnsizedIllegal, RhsConstTypeIsUnsignedInt) {
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  const auto *assign = any_cast<const hldb::Assignment *>((*begin->getStmts())[0]);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), vpiStringConst);
}

TEST_F(IntegersUnsizedIllegal, RhsDecompileShowsOnlyNumericPart) {
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  const auto *assign = any_cast<const hldb::Assignment *>((*begin->getStmts())[0]);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "4af") << "'af' time-unit suffix missing";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
