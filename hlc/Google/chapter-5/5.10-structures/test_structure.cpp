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

// Validates three forms of struct assignment pattern in UHDM:
//   ms = '{0, 1}                  → positional: 2 Constants
//   ms = '{default:1, int:1}      → keyed: TaggedPattern(RefObj "default"), TaggedPattern(RefTypespec int)
//   ms = '{int:0, int:1}          → keyed: 2 TaggedPatterns with RefTypespec int tags
// Grammar: assignment_pattern → positional | structure_pattern_key:expression

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/assignment.h>
#include <uhdm/begin.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/initial.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/operation.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/struct_typespec.h>
#include <uhdm/tagged_pattern.h>
#include <uhdm/typedef_typespec.h>
#include <uhdm/typespec_member.h>

namespace SURELOG {

class Structure : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.10-structures.hlc"});

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

// Helper: returns the Begin block from the module's only Initial process.
static const uhdm::Begin *getInitialBegin(const uhdm::Module *top) {
  if (!top->getProcesses()) return nullptr;
  for (const uhdm::Process *const p : *top->getProcesses()) {
    if (const uhdm::Initial *const i = any_cast<uhdm::Initial>(p))
      return i->getStmt<uhdm::Begin>();
  }
  return nullptr;
}

// Helper: returns the Nth Assignment from the begin block (0-based).
static const uhdm::Assignment *getNthAssignment(const uhdm::Begin *blk, size_t n) {
  if (!blk || !blk->getStmts()) return nullptr;
  size_t idx = 0;
  for (const uhdm::Any *const s : *blk->getStmts()) {
    if (const uhdm::Assignment *const a = any_cast<uhdm::Assignment>(s)) {
      if (idx++ == n) return a;
    }
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
// Module and typedef
// ---------------------------------------------------------------------------
TEST_F(Structure, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

TEST_F(Structure, TypedefMsTExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);

  const uhdm::TypedefTypespec *msT = nullptr;
  for (const uhdm::Typespec *const ts : *top->getTypespecs()) {
    if (const uhdm::TypedefTypespec *const tdt = any_cast<uhdm::TypedefTypespec>(ts)) {
      if (tdt->getName() == "ms_t") { msT = tdt; break; }
    }
  }
  ASSERT_NE(msT, nullptr) << "TypedefTypespec 'ms_t' not found";
}

TEST_F(Structure, StructHasTwoMembers) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);

  const uhdm::TypedefTypespec *msT = nullptr;
  for (const uhdm::Typespec *const ts : *top->getTypespecs()) {
    if (const uhdm::TypedefTypespec *const tdt = any_cast<uhdm::TypedefTypespec>(ts)) {
      if (tdt->getName() == "ms_t") { msT = tdt; break; }
    }
  }
  ASSERT_NE(msT, nullptr);

  const uhdm::StructTypespec *const st =
      any_cast<uhdm::StructTypespec>(msT->getTypedefAlias()->getActual());
  ASSERT_NE(st, nullptr) << "ms_t does not alias a StructTypespec";
  ASSERT_NE(st->getMembers(), nullptr);
  EXPECT_EQ(st->getMembers()->size(), 2u) << "expected members: a, b";
  EXPECT_EQ((*st->getMembers())[0]->getName(), "a");
  EXPECT_EQ((*st->getMembers())[1]->getName(), "b");
}

// ---------------------------------------------------------------------------
// Net ms
// ---------------------------------------------------------------------------
TEST_F(Structure, NetMsExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);

  const uhdm::Net *ms = nullptr;
  for (const uhdm::Net *const n : *top->getNets()) {
    if (n->getName() == "ms") { ms = n; break; }
  }
  ASSERT_NE(ms, nullptr) << "net 'ms' not found in module";
}

// ---------------------------------------------------------------------------
// Initial block — three assignments
// ---------------------------------------------------------------------------
TEST_F(Structure, InitialBlockHasThreeAssignments) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const uhdm::Begin *const blk = getInitialBegin(top);
  ASSERT_NE(blk, nullptr) << "no Initial/Begin block found";
  ASSERT_NE(blk->getStmts(), nullptr);

  size_t count = 0;
  for (const uhdm::Any *const s : *blk->getStmts())
    if (any_cast<uhdm::Assignment>(s)) ++count;
  EXPECT_EQ(count, 3u) << "expected 3 assignments in initial block";
}

// ---------------------------------------------------------------------------
// Assignment 1: ms = '{0, 1} — positional pattern, 2 Constants
// ---------------------------------------------------------------------------
TEST_F(Structure, FirstAssignmentIsPositionalPattern) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const uhdm::Begin *const blk = getInitialBegin(top);
  const uhdm::Assignment *const a = getNthAssignment(blk, 0);
  ASSERT_NE(a, nullptr) << "first assignment not found";

  const uhdm::Operation *const rhs = a->getRhs<uhdm::Operation>();
  ASSERT_NE(rhs, nullptr) << "first assignment RHS is not an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiAssignmentPatternOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  EXPECT_EQ(rhs->getOperands()->size(), 2u) << "expected 2 positional operands";
}

TEST_F(Structure, FirstAssignmentOperandsAreConstants) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const uhdm::Begin *const blk = getInitialBegin(top);
  const uhdm::Assignment *const a = getNthAssignment(blk, 0);
  ASSERT_NE(a, nullptr);

  const uhdm::Operation *const rhs = a->getRhs<uhdm::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);

  // '{0, 1} — no TaggedPattern, just raw Constants
  EXPECT_NE(any_cast<uhdm::Constant>((*rhs->getOperands())[0]), nullptr)
      << "first operand should be a Constant (value 0)";
  EXPECT_NE(any_cast<uhdm::Constant>((*rhs->getOperands())[1]), nullptr)
      << "second operand should be a Constant (value 1)";
}

// ---------------------------------------------------------------------------
// Assignment 2: ms = '{default:1, int:1} — keyed with default and type tags
// ---------------------------------------------------------------------------
TEST_F(Structure, SecondAssignmentIsKeyedPattern) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const uhdm::Begin *const blk = getInitialBegin(top);
  const uhdm::Assignment *const a = getNthAssignment(blk, 1);
  ASSERT_NE(a, nullptr) << "second assignment not found";

  const uhdm::Operation *const rhs = a->getRhs<uhdm::Operation>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getOpType(), vpiAssignmentPatternOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  EXPECT_EQ(rhs->getOperands()->size(), 2u);
}

TEST_F(Structure, SecondAssignmentOperandsAreTaggedPatterns) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const uhdm::Begin *const blk = getInitialBegin(top);
  const uhdm::Assignment *const a = getNthAssignment(blk, 1);
  ASSERT_NE(a, nullptr);

  const uhdm::Operation *const rhs = a->getRhs<uhdm::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);

  for (size_t i = 0; i < 2u; ++i) {
    EXPECT_NE(any_cast<uhdm::TaggedPattern>((*rhs->getOperands())[i]), nullptr)
        << "operand [" << i << "] should be a TaggedPattern";
  }
}

TEST_F(Structure, SecondAssignmentFirstTagIsDefault) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const uhdm::Begin *const blk = getInitialBegin(top);
  const uhdm::Assignment *const a = getNthAssignment(blk, 1);
  ASSERT_NE(a, nullptr);

  const uhdm::Operation *const rhs = a->getRhs<uhdm::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);

  // default:1 → TaggedPattern with tag = RefObj "default"
  const uhdm::TaggedPattern *const tp0 =
      any_cast<uhdm::TaggedPattern>((*rhs->getOperands())[0]);
  ASSERT_NE(tp0, nullptr);
  const uhdm::RefObj *const tag = tp0->getTag<uhdm::RefObj>();
  ASSERT_NE(tag, nullptr) << "first tag is not a RefObj (expected 'default')";
  EXPECT_EQ(tag->getName(), "default");
}

TEST_F(Structure, SecondAssignmentSecondTagIsTypeRef) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const uhdm::Begin *const blk = getInitialBegin(top);
  const uhdm::Assignment *const a = getNthAssignment(blk, 1);
  ASSERT_NE(a, nullptr);

  const uhdm::Operation *const rhs = a->getRhs<uhdm::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);

  // int:1 → TaggedPattern with tag = RefTypespec (IntTypespec)
  const uhdm::TaggedPattern *const tp1 =
      any_cast<uhdm::TaggedPattern>((*rhs->getOperands())[1]);
  ASSERT_NE(tp1, nullptr);
  EXPECT_NE(tp1->getTag<uhdm::RefTypespec>(), nullptr)
      << "second tag should be a RefTypespec (for 'int:')";
}

// ---------------------------------------------------------------------------
// Assignment 3: ms = '{int:0, int:1} — both operands keyed by type
// ---------------------------------------------------------------------------
TEST_F(Structure, ThirdAssignmentBothTagsAreTypeRefs) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const uhdm::Begin *const blk = getInitialBegin(top);
  const uhdm::Assignment *const a = getNthAssignment(blk, 2);
  ASSERT_NE(a, nullptr) << "third assignment not found";

  const uhdm::Operation *const rhs = a->getRhs<uhdm::Operation>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getOpType(), vpiAssignmentPatternOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);

  for (size_t i = 0; i < 2u; ++i) {
    const uhdm::TaggedPattern *const tp =
        any_cast<uhdm::TaggedPattern>((*rhs->getOperands())[i]);
    ASSERT_NE(tp, nullptr) << "operand [" << i << "] is not a TaggedPattern";
    EXPECT_NE(tp->getTag<uhdm::RefTypespec>(), nullptr)
        << "operand [" << i << "] tag should be a RefTypespec (for 'int:')";
  }
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
