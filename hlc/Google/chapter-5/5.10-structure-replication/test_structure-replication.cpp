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

// Validates structure replication assignment patterns in UHDM:
//   struct{int X,Y,Z;} XYZ = '{3{1}};               → assign pattern [count=3, val=1]
//   typedef struct{int a,b[4];} ab_t;
//   ab_t v1[1:0][2:0]; v1 = '{2{'{3{'{a,'{2{b,c}}}}}}};
// Grammar: assignment_pattern → constant_expression { expression } (multi-replication)

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/array_typespec.h>
#include <uhdm/assignment.h>
#include <uhdm/begin.h>
#include <uhdm/design.h>
#include <uhdm/initial.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/operation.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/struct_typespec.h>
#include <uhdm/typedef_typespec.h>
#include <uhdm/typespec_member.h>
#include <uhdm/variable.h>

namespace SURELOG {

class StructureReplication : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.10-structure-replication.hlc"});

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

TEST_F(StructureReplication, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net XYZ — anonymous struct with replication init '{3{1}}
// ---------------------------------------------------------------------------
TEST_F(StructureReplication, NetXYZExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr) << "module has no nets";

  const uhdm::Net *xyz = nullptr;
  for (const uhdm::Net *const n : *top->getNets()) {
    if (n->getName() == "XYZ") { xyz = n; break; }
  }
  ASSERT_NE(xyz, nullptr) << "net 'XYZ' not found in module";
}

TEST_F(StructureReplication, XYZTypespecHasThreeMembers) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);

  const uhdm::Net *xyz = nullptr;
  for (const uhdm::Net *const n : *top->getNets()) {
    if (n->getName() == "XYZ") { xyz = n; break; }
  }
  ASSERT_NE(xyz, nullptr);
  ASSERT_NE(xyz->getTypespec(), nullptr) << "XYZ has no typespec";

  const uhdm::StructTypespec *const st =
      any_cast<uhdm::StructTypespec>(xyz->getTypespec()->getActual());
  ASSERT_NE(st, nullptr) << "XYZ typespec is not a StructTypespec";
  ASSERT_NE(st->getMembers(), nullptr) << "XYZ StructTypespec has no members";
  EXPECT_EQ(st->getMembers()->size(), 3u) << "expected 3 members: X, Y, Z";
}

TEST_F(StructureReplication, XYZMemberNames) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);

  const uhdm::Net *xyz = nullptr;
  for (const uhdm::Net *const n : *top->getNets()) {
    if (n->getName() == "XYZ") { xyz = n; break; }
  }
  ASSERT_NE(xyz, nullptr);

  const uhdm::StructTypespec *const st =
      any_cast<uhdm::StructTypespec>(xyz->getTypespec()->getActual());
  ASSERT_NE(st, nullptr);
  ASSERT_EQ(st->getMembers()->size(), 3u);

  EXPECT_EQ((*st->getMembers())[0]->getName(), "X");
  EXPECT_EQ((*st->getMembers())[1]->getName(), "Y");
  EXPECT_EQ((*st->getMembers())[2]->getName(), "Z");
}

TEST_F(StructureReplication, XYZInitializerIsAssignPattern) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);

  const uhdm::Net *xyz = nullptr;
  for (const uhdm::Net *const n : *top->getNets()) {
    if (n->getName() == "XYZ") { xyz = n; break; }
  }
  ASSERT_NE(xyz, nullptr);
  ASSERT_NE(xyz->getValue(), nullptr) << "XYZ has no initializer";

  const uhdm::Operation *const init = xyz->getValue<uhdm::Operation>();
  ASSERT_NE(init, nullptr) << "XYZ initializer is not an Operation";
  EXPECT_EQ(init->getOpType(), vpiAssignmentPatternOp)
      << "XYZ initializer is not vpiAssignmentPatternOp";
}

TEST_F(StructureReplication, XYZInitializerHasReplicationCountAndValue) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);

  const uhdm::Net *xyz = nullptr;
  for (const uhdm::Net *const n : *top->getNets()) {
    if (n->getName() == "XYZ") { xyz = n; break; }
  }
  ASSERT_NE(xyz, nullptr);

  const uhdm::Operation *const init = xyz->getValue<uhdm::Operation>();
  ASSERT_NE(init, nullptr);
  ASSERT_NE(init->getOperands(), nullptr);
  // '{3{1}} → [Constant "3" (count), Constant "1" (value)]
  EXPECT_EQ(init->getOperands()->size(), 2u)
      << "expected 2 operands for '{3{1}} replication pattern";
}

// ---------------------------------------------------------------------------
// Typedef ab_t — struct with members a (int) and b (int array)
// ---------------------------------------------------------------------------
TEST_F(StructureReplication, TypedefAbTExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);

  const uhdm::TypedefTypespec *abT = nullptr;
  for (const uhdm::Typespec *const ts : *top->getTypespecs()) {
    if (const uhdm::TypedefTypespec *const tdt = any_cast<uhdm::TypedefTypespec>(ts)) {
      if (tdt->getName() == "ab_t") { abT = tdt; break; }
    }
  }
  ASSERT_NE(abT, nullptr) << "TypedefTypespec 'ab_t' not found in module";
}

TEST_F(StructureReplication, AbTStructHasTwoMembers) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);

  const uhdm::TypedefTypespec *abT = nullptr;
  for (const uhdm::Typespec *const ts : *top->getTypespecs()) {
    if (const uhdm::TypedefTypespec *const tdt = any_cast<uhdm::TypedefTypespec>(ts)) {
      if (tdt->getName() == "ab_t") { abT = tdt; break; }
    }
  }
  ASSERT_NE(abT, nullptr);

  const uhdm::StructTypespec *const st =
      any_cast<uhdm::StructTypespec>(abT->getTypedefAlias()->getActual());
  ASSERT_NE(st, nullptr) << "ab_t alias does not resolve to StructTypespec";
  ASSERT_NE(st->getMembers(), nullptr);
  EXPECT_EQ(st->getMembers()->size(), 2u) << "expected 2 members: a, b";
}

// ---------------------------------------------------------------------------
// Initial block — v1 variable and nested replication assignment
// ---------------------------------------------------------------------------
TEST_F(StructureReplication, InitialBlockExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr) << "module has no processes";

  const uhdm::Initial *init = nullptr;
  for (const uhdm::Process *const p : *top->getProcesses()) {
    if (const uhdm::Initial *const i = any_cast<uhdm::Initial>(p)) {
      init = i; break;
    }
  }
  ASSERT_NE(init, nullptr) << "no Initial block found in module";
}

TEST_F(StructureReplication, V1VariableExistsInInitialBlock) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);

  const uhdm::Initial *init = nullptr;
  for (const uhdm::Process *const p : *top->getProcesses()) {
    if (const uhdm::Initial *const i = any_cast<uhdm::Initial>(p)) {
      init = i; break;
    }
  }
  ASSERT_NE(init, nullptr);

  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr) << "Initial statement is not a Begin block";
  ASSERT_NE(blk->getVariables(), nullptr) << "begin block has no variables";

  const uhdm::Variable *v1 = nullptr;
  for (const uhdm::Variable *const v : *blk->getVariables()) {
    if (v->getName() == "v1") { v1 = v; break; }
  }
  ASSERT_NE(v1, nullptr) << "variable 'v1' not found in initial block";
}

TEST_F(StructureReplication, V1TypespecIsArray) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);

  const uhdm::Initial *init = nullptr;
  for (const uhdm::Process *const p : *top->getProcesses()) {
    if (const uhdm::Initial *const i = any_cast<uhdm::Initial>(p)) {
      init = i; break;
    }
  }
  ASSERT_NE(init, nullptr);

  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getVariables(), nullptr);

  const uhdm::Variable *v1 = nullptr;
  for (const uhdm::Variable *const v : *blk->getVariables()) {
    if (v->getName() == "v1") { v1 = v; break; }
  }
  ASSERT_NE(v1, nullptr);
  ASSERT_NE(v1->getTypespec(), nullptr) << "v1 has no typespec";

  const uhdm::ArrayTypespec *const at =
      any_cast<uhdm::ArrayTypespec>(v1->getTypespec()->getActual());
  ASSERT_NE(at, nullptr) << "v1 typespec is not an ArrayTypespec";
}

TEST_F(StructureReplication, V1AssignmentExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);

  const uhdm::Initial *init = nullptr;
  for (const uhdm::Process *const p : *top->getProcesses()) {
    if (const uhdm::Initial *const i = any_cast<uhdm::Initial>(p)) {
      init = i; break;
    }
  }
  ASSERT_NE(init, nullptr);

  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr) << "begin block has no statements";

  const uhdm::Assignment *assign = nullptr;
  for (const uhdm::Any *const s : *blk->getStmts()) {
    if (const uhdm::Assignment *const a = any_cast<uhdm::Assignment>(s)) {
      assign = a; break;
    }
  }
  ASSERT_NE(assign, nullptr) << "no Assignment found in initial block";
}

TEST_F(StructureReplication, AssignmentRhsIsOuterReplicationPattern) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);

  const uhdm::Initial *init = nullptr;
  for (const uhdm::Process *const p : *top->getProcesses()) {
    if (const uhdm::Initial *const i = any_cast<uhdm::Initial>(p)) {
      init = i; break;
    }
  }
  ASSERT_NE(init, nullptr);

  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);

  const uhdm::Assignment *assign = nullptr;
  for (const uhdm::Any *const s : *blk->getStmts()) {
    if (const uhdm::Assignment *const a = any_cast<uhdm::Assignment>(s)) {
      assign = a; break;
    }
  }
  ASSERT_NE(assign, nullptr);

  // v1 = '{2{...}} — outer replication: AssignPatternOp with [Const "2", nested pattern]
  const uhdm::Operation *const rhs = assign->getRhs<uhdm::Operation>();
  ASSERT_NE(rhs, nullptr) << "RHS is not an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiAssignmentPatternOp)
      << "RHS op is not vpiAssignmentPatternOp";
  ASSERT_NE(rhs->getOperands(), nullptr);
  EXPECT_EQ(rhs->getOperands()->size(), 2u)
      << "outer '{2{...}} should have 2 operands: [count=2, nested pattern]";
}

TEST_F(StructureReplication, NestedReplicationLevelsAreAssignPatterns) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);

  const uhdm::Initial *init = nullptr;
  for (const uhdm::Process *const p : *top->getProcesses()) {
    if (const uhdm::Initial *const i = any_cast<uhdm::Initial>(p)) {
      init = i; break;
    }
  }
  ASSERT_NE(init, nullptr);

  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);

  const uhdm::Assignment *assign = nullptr;
  for (const uhdm::Any *const s : *blk->getStmts()) {
    if (const uhdm::Assignment *const a = any_cast<uhdm::Assignment>(s)) {
      assign = a; break;
    }
  }
  ASSERT_NE(assign, nullptr);

  // Level 0: '{2{...}}        → [Const "2",  level1]
  const uhdm::Operation *const l0 = assign->getRhs<uhdm::Operation>();
  ASSERT_NE(l0, nullptr);
  ASSERT_EQ(l0->getOperands()->size(), 2u);

  // Level 1: '{3{...}}        → [Const "3",  level2]
  const uhdm::Operation *const l1 =
      any_cast<uhdm::Operation>((*l0->getOperands())[1]);
  ASSERT_NE(l1, nullptr) << "level-1 operand is not an Operation (expected '{3{...}})";
  EXPECT_EQ(l1->getOpType(), vpiAssignmentPatternOp);
  ASSERT_EQ(l1->getOperands()->size(), 2u);

  // Level 2: '{a, '{2{b,c}}}  → [RefObj "a", level3]
  const uhdm::Operation *const l2 =
      any_cast<uhdm::Operation>((*l1->getOperands())[1]);
  ASSERT_NE(l2, nullptr) << "level-2 operand is not an Operation (expected '{a,...})";
  EXPECT_EQ(l2->getOpType(), vpiAssignmentPatternOp);
  ASSERT_EQ(l2->getOperands()->size(), 2u);

  // Level 2, first operand must be a RefObj to "a"
  const uhdm::RefObj *const refA = any_cast<uhdm::RefObj>((*l2->getOperands())[0]);
  ASSERT_NE(refA, nullptr) << "level-2 first operand is not a RefObj (expected 'a')";
  EXPECT_EQ(refA->getName(), "a");

  // Level 3: '{2{b,c}}        → [Const "2", RefObj "b", RefObj "c"]
  const uhdm::Operation *const l3 =
      any_cast<uhdm::Operation>((*l2->getOperands())[1]);
  ASSERT_NE(l3, nullptr) << "level-3 operand is not an Operation (expected '{2{b,c}})";
  EXPECT_EQ(l3->getOpType(), vpiAssignmentPatternOp);
  ASSERT_EQ(l3->getOperands()->size(), 3u)
      << "innermost '{2{b,c}} should have 3 operands: [count=2, 'b', 'c']";

  const uhdm::RefObj *const refB = any_cast<uhdm::RefObj>((*l3->getOperands())[1]);
  ASSERT_NE(refB, nullptr) << "innermost second operand is not a RefObj (expected 'b')";
  EXPECT_EQ(refB->getName(), "b");

  const uhdm::RefObj *const refC = any_cast<uhdm::RefObj>((*l3->getOperands())[2]);
  ASSERT_NE(refC, nullptr) << "innermost third operand is not a RefObj (expected 'c')";
  EXPECT_EQ(refC->getName(), "c");
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
