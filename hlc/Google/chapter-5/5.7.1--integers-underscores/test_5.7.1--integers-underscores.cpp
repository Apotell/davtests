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

// Validates that underscore separators in integer literals and extra whitespace
// between the size, base, and value tokens are stripped by Surelog before
// storage in UHDM.
//
// SV source (module top):
//   logic [31:0] a;   logic [15:0] b;   logic [31:0] c;
//   initial begin
//     a = 27_195_000;               // unsized decimal with underscore separators
//     b = 16'b0011_0101_0001_1111;  // 16-bit binary with underscore separators
//     c = 32 'h 12ab_f001;          // 32-bit hex with spaces and underscore
//   end
//
// UHDM strips all underscores and collapses all whitespace:
//   a → constType: unsigned int (9), size: 64, getValue(): "27195000"
//       getDecompile(): "27195000"
//   b → constType: binary (3), size: 16, getValue(): "0011010100011111"
//       getDecompile(): "16'b0011010100011111"
//   c → constType: hexadecimal (5), size: 32, getValue(): "12abf001"
//       getDecompile(): "32'h12abf001"
//
// Note: unsized decimal constants are stored with size 64 (Surelog's default
// integer width), NOT -1 like unsized hex/binary literals.

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

class IntegersUnderscores : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.7.1--integers-underscores.hlc"});

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
  const auto *initial =
      any_cast<const hldb::Initial *>((*m->getProcesses())[0]);
  if (!initial) return nullptr;
  return initial->getStmt<hldb::Begin>();
}

static const hldb::Assignment *getAssignment(const hldb::Design *d,
                                              std::size_t index) {
  const hldb::Begin *begin = getBegin(d);
  if (!begin || !begin->getStmts()) return nullptr;
  if (index >= begin->getStmts()->size()) return nullptr;
  return any_cast<const hldb::Assignment *>((*begin->getStmts())[index]);
}

// ---------------------------------------------------------------------------
// Module structure
// ---------------------------------------------------------------------------
TEST_F(IntegersUnderscores, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found";
}

TEST_F(IntegersUnderscores, ThreeNetsExist) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 3u)
      << "expected 3 nets: a [31:0], b [15:0], c [31:0]";
}

// ---------------------------------------------------------------------------
// Initial block
// ---------------------------------------------------------------------------
TEST_F(IntegersUnderscores, InitialBlockHasBegin) {
  ASSERT_NE(getBegin(m_design), nullptr);
}

TEST_F(IntegersUnderscores, BeginHasThreeStatements) {
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 3u);
}

TEST_F(IntegersUnderscores, AllAssignmentsAreBlocking) {
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  for (std::size_t i = 0; i < begin->getStmts()->size(); ++i) {
    const auto *assign =
        any_cast<const hldb::Assignment *>((*begin->getStmts())[i]);
    ASSERT_NE(assign, nullptr) << "stmt[" << i << "] is not an Assignment";
    EXPECT_TRUE(assign->getBlocking())
        << "assignment[" << i << "] should be blocking (=)";
  }
}

// ---------------------------------------------------------------------------
// a = 27_195_000  — unsized decimal with underscore thousands separators
// Underscores are stripped; stored as unsigned int (9), size 64.
// ---------------------------------------------------------------------------
TEST_F(IntegersUnderscores, AssignmentA_ConstType) {
  const hldb::Assignment *const assign = getAssignment(m_design, 0);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  // Unsized decimal → unsigned int (9)
  EXPECT_EQ(c->getConstType(), 9)
      << "27_195_000: constType should be unsigned int (9)";
}

TEST_F(IntegersUnderscores, AssignmentA_SizeIs64) {
  // Unsized decimal is given Surelog's default 64-bit integer width, not -1
  const hldb::Assignment *const assign = getAssignment(m_design, 0);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 64)
      << "unsized decimal should have size 64 (Surelog default integer width)";
}

TEST_F(IntegersUnderscores, AssignmentA_UnderscoresStripped_getValue) {
  const hldb::Assignment *const assign = getAssignment(m_design, 0);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue(), "27195000")
      << "underscore separators should be stripped from getValue()";
}

TEST_F(IntegersUnderscores, AssignmentA_UnderscoresStripped_getDecompile) {
  const hldb::Assignment *const assign = getAssignment(m_design, 0);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "27195000")
      << "underscore separators should be stripped from getDecompile()";
}

// ---------------------------------------------------------------------------
// b = 16'b0011_0101_0001_1111  — 16-bit binary with underscore separators
// Underscores stripped; getDecompile() emits the compact form without them.
// ---------------------------------------------------------------------------
TEST_F(IntegersUnderscores, AssignmentB_ConstType) {
  const hldb::Assignment *const assign = getAssignment(m_design, 1);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), 3) << "16'b...: constType should be binary (3)";
}

TEST_F(IntegersUnderscores, AssignmentB_SizeIs16) {
  const hldb::Assignment *const assign = getAssignment(m_design, 1);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 16);
}

TEST_F(IntegersUnderscores, AssignmentB_UnderscoresStripped_getValue) {
  const hldb::Assignment *const assign = getAssignment(m_design, 1);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue(), "0011010100011111")
      << "underscore separators should be stripped from getValue()";
}

TEST_F(IntegersUnderscores, AssignmentB_UnderscoresStripped_getDecompile) {
  const hldb::Assignment *const assign = getAssignment(m_design, 1);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "16'b0011010100011111")
      << "underscore separators should be stripped from getDecompile()";
}

// ---------------------------------------------------------------------------
// c = 32 'h 12ab_f001  — 32-bit hex with internal spaces and underscore
// Both whitespace and underscores are collapsed/stripped in UHDM output.
// ---------------------------------------------------------------------------
TEST_F(IntegersUnderscores, AssignmentC_ConstType) {
  const hldb::Assignment *const assign = getAssignment(m_design, 2);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), 5)
      << "32'h...: constType should be hexadecimal (5)";
}

TEST_F(IntegersUnderscores, AssignmentC_SizeIs32) {
  const hldb::Assignment *const assign = getAssignment(m_design, 2);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 32);
}

TEST_F(IntegersUnderscores, AssignmentC_SpacesAndUnderscoresStripped_getValue) {
  const hldb::Assignment *const assign = getAssignment(m_design, 2);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue(), "12abf001")
      << "spaces and underscores should be stripped from getValue()";
}

TEST_F(IntegersUnderscores,
       AssignmentC_SpacesAndUnderscoresStripped_getDecompile) {
  const hldb::Assignment *const assign = getAssignment(m_design, 2);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "32'h12abf001")
      << "spaces and underscores should be stripped from getDecompile()";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
