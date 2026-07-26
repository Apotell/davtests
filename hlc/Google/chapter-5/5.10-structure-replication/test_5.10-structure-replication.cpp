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

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct_typespec.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>

namespace hlc {

class StructureReplication : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.10-structure-replication.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(StructureReplication, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net XYZ — anonymous struct with replication init '{3{1}}
// ---------------------------------------------------------------------------
TEST_F(StructureReplication, NetXYZExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr) << "module has no nets";

  const hldb::Net *xyz = nullptr;
  for (const hldb::Net *const n : *top->getNets()) {
    if (n->getName() == "XYZ") {
      xyz = n;
      break;
    }
  }
  ASSERT_NE(xyz, nullptr) << "net 'XYZ' not found in module";
}

TEST_F(StructureReplication, XYZTypespecHasThreeMembers) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);

  const hldb::Net *xyz = nullptr;
  for (const hldb::Net *const n : *top->getNets()) {
    if (n->getName() == "XYZ") {
      xyz = n;
      break;
    }
  }
  ASSERT_NE(xyz, nullptr);
  ASSERT_NE(xyz->getTypespec(), nullptr) << "XYZ has no typespec";

  const hldb::StructTypespec *const st = any_cast<hldb::StructTypespec>(xyz->getTypespec()->getActual());
  ASSERT_NE(st, nullptr) << "XYZ typespec is not a StructTypespec";
  ASSERT_NE(st->getMembers(), nullptr) << "XYZ StructTypespec has no members";
  EXPECT_EQ(st->getMembers()->size(), 3u) << "expected 3 members: X, Y, Z";
}

TEST_F(StructureReplication, XYZMemberNames) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);

  const hldb::Net *xyz = nullptr;
  for (const hldb::Net *const n : *top->getNets()) {
    if (n->getName() == "XYZ") {
      xyz = n;
      break;
    }
  }
  ASSERT_NE(xyz, nullptr);

  const hldb::StructTypespec *const st = any_cast<hldb::StructTypespec>(xyz->getTypespec()->getActual());
  ASSERT_NE(st, nullptr);
  ASSERT_EQ(st->getMembers()->size(), 3u);

  EXPECT_EQ((*st->getMembers())[0]->getName(), "X");
  EXPECT_EQ((*st->getMembers())[1]->getName(), "Y");
  EXPECT_EQ((*st->getMembers())[2]->getName(), "Z");
}

TEST_F(StructureReplication, XYZInitializerIsAssignPattern) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);

  const hldb::Net *xyz = nullptr;
  for (const hldb::Net *const n : *top->getNets()) {
    if (n->getName() == "XYZ") {
      xyz = n;
      break;
    }
  }
  ASSERT_NE(xyz, nullptr);
  ASSERT_NE(xyz->getValue(), nullptr) << "XYZ has no initializer";

  const hldb::Operation *const init = xyz->getValue<hldb::Operation>();
  ASSERT_NE(init, nullptr) << "XYZ initializer is not an Operation";
  EXPECT_EQ(init->getOpType(), vpiAssignmentPatternOp) << "XYZ initializer is not vpiAssignmentPatternOp";
}

TEST_F(StructureReplication, XYZInitializerHasReplicationCountAndValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);

  const hldb::Net *xyz = nullptr;
  for (const hldb::Net *const n : *top->getNets()) {
    if (n->getName() == "XYZ") {
      xyz = n;
      break;
    }
  }
  ASSERT_NE(xyz, nullptr);

  const hldb::Operation *const init = xyz->getValue<hldb::Operation>();
  ASSERT_NE(init, nullptr);
  ASSERT_NE(init->getOperands(), nullptr);
  // '{3{1}} → [Constant "3" (count), Constant "1" (value)]
  EXPECT_EQ(init->getOperands()->size(), 2u) << "expected 2 operands for '{3{1}} replication pattern";
}

// ---------------------------------------------------------------------------
// Typedef ab_t — struct with members a (int) and b (int array)
// ---------------------------------------------------------------------------
TEST_F(StructureReplication, TypedefAbTExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);

  const hldb::TypedefTypespec *abT = nullptr;
  for (const hldb::Typespec *const ts : *top->getTypespecs()) {
    if (const hldb::TypedefTypespec *const tdt = any_cast<hldb::TypedefTypespec>(ts)) {
      if (tdt->getName() == "ab_t") {
        abT = tdt;
        break;
      }
    }
  }
  ASSERT_NE(abT, nullptr) << "TypedefTypespec 'ab_t' not found in module";
}

TEST_F(StructureReplication, AbTStructHasTwoMembers) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);

  const hldb::TypedefTypespec *abT = nullptr;
  for (const hldb::Typespec *const ts : *top->getTypespecs()) {
    if (const hldb::TypedefTypespec *const tdt = any_cast<hldb::TypedefTypespec>(ts)) {
      if (tdt->getName() == "ab_t") {
        abT = tdt;
        break;
      }
    }
  }
  ASSERT_NE(abT, nullptr);

  const hldb::StructTypespec *const st = any_cast<hldb::StructTypespec>(abT->getTypedefAlias()->getActual());
  ASSERT_NE(st, nullptr) << "ab_t alias does not resolve to StructTypespec";
  ASSERT_NE(st->getMembers(), nullptr);
  EXPECT_EQ(st->getMembers()->size(), 2u) << "expected 2 members: a, b";
}

// ---------------------------------------------------------------------------
// Initial block — v1 variable and nested replication assignment
// ---------------------------------------------------------------------------
TEST_F(StructureReplication, InitialBlockExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr) << "module has no processes";

  const hldb::Initial *init = nullptr;
  for (const hldb::Process *const p : *top->getProcesses()) {
    if (const hldb::Initial *const i = any_cast<hldb::Initial>(p)) {
      init = i;
      break;
    }
  }
  ASSERT_NE(init, nullptr) << "no Initial block found in module";
}

TEST_F(StructureReplication, V1VariableExistsInInitialBlock) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);

  const hldb::Initial *init = nullptr;
  for (const hldb::Process *const p : *top->getProcesses()) {
    if (const hldb::Initial *const i = any_cast<hldb::Initial>(p)) {
      init = i;
      break;
    }
  }
  ASSERT_NE(init, nullptr);

  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr) << "Initial statement is not a Begin block";
  ASSERT_NE(blk->getVariables(), nullptr) << "begin block has no variables";

  const hldb::Variable *v1 = nullptr;
  for (const hldb::Variable *const v : *blk->getVariables()) {
    if (v->getName() == "v1") {
      v1 = v;
      break;
    }
  }
  ASSERT_NE(v1, nullptr) << "variable 'v1' not found in initial block";
}

TEST_F(StructureReplication, V1TypespecIsArray) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);

  const hldb::Initial *init = nullptr;
  for (const hldb::Process *const p : *top->getProcesses()) {
    if (const hldb::Initial *const i = any_cast<hldb::Initial>(p)) {
      init = i;
      break;
    }
  }
  ASSERT_NE(init, nullptr);

  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getVariables(), nullptr);

  const hldb::Variable *v1 = nullptr;
  for (const hldb::Variable *const v : *blk->getVariables()) {
    if (v->getName() == "v1") {
      v1 = v;
      break;
    }
  }
  ASSERT_NE(v1, nullptr);
  ASSERT_NE(v1->getTypespec(), nullptr) << "v1 has no typespec";

  const hldb::ArrayTypespec *const at = any_cast<hldb::ArrayTypespec>(v1->getTypespec()->getActual());
  ASSERT_NE(at, nullptr) << "v1 typespec is not an ArrayTypespec";
}

TEST_F(StructureReplication, V1AssignmentExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);

  const hldb::Initial *init = nullptr;
  for (const hldb::Process *const p : *top->getProcesses()) {
    if (const hldb::Initial *const i = any_cast<hldb::Initial>(p)) {
      init = i;
      break;
    }
  }
  ASSERT_NE(init, nullptr);

  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr) << "begin block has no statements";

  const hldb::Assignment *assign = nullptr;
  for (const hldb::Any *const s : *blk->getStmts()) {
    if (const hldb::Assignment *const a = any_cast<hldb::Assignment>(s)) {
      assign = a;
      break;
    }
  }
  ASSERT_NE(assign, nullptr) << "no Assignment found in initial block";
}

TEST_F(StructureReplication, AssignmentRhsIsOuterReplicationPattern) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);

  const hldb::Initial *init = nullptr;
  for (const hldb::Process *const p : *top->getProcesses()) {
    if (const hldb::Initial *const i = any_cast<hldb::Initial>(p)) {
      init = i;
      break;
    }
  }
  ASSERT_NE(init, nullptr);

  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);

  const hldb::Assignment *assign = nullptr;
  for (const hldb::Any *const s : *blk->getStmts()) {
    if (const hldb::Assignment *const a = any_cast<hldb::Assignment>(s)) {
      assign = a;
      break;
    }
  }
  ASSERT_NE(assign, nullptr);

  // v1 = '{2{...}} — outer replication: AssignPatternOp with [Const "2", nested pattern]
  const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "RHS is not an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiAssignmentPatternOp) << "RHS op is not vpiAssignmentPatternOp";
  ASSERT_NE(rhs->getOperands(), nullptr);
  EXPECT_EQ(rhs->getOperands()->size(), 2u) << "outer '{2{...}} should have 2 operands: [count=2, nested pattern]";
}

TEST_F(StructureReplication, NestedReplicationLevelsAreAssignPatterns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);

  const hldb::Initial *init = nullptr;
  for (const hldb::Process *const p : *top->getProcesses()) {
    if (const hldb::Initial *const i = any_cast<hldb::Initial>(p)) {
      init = i;
      break;
    }
  }
  ASSERT_NE(init, nullptr);

  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);

  const hldb::Assignment *assign = nullptr;
  for (const hldb::Any *const s : *blk->getStmts()) {
    if (const hldb::Assignment *const a = any_cast<hldb::Assignment>(s)) {
      assign = a;
      break;
    }
  }
  ASSERT_NE(assign, nullptr);

  // Level 0: '{2{...}}        → [Const "2",  level1]
  const hldb::Operation *const l0 = assign->getRhs<hldb::Operation>();
  ASSERT_NE(l0, nullptr);
  ASSERT_EQ(l0->getOperands()->size(), 2u);

  // Level 1: '{3{...}}        → [Const "3",  level2]
  const hldb::Operation *const l1 = any_cast<hldb::Operation>((*l0->getOperands())[1]);
  ASSERT_NE(l1, nullptr) << "level-1 operand is not an Operation (expected '{3{...}})";
  EXPECT_EQ(l1->getOpType(), vpiAssignmentPatternOp);
  ASSERT_EQ(l1->getOperands()->size(), 2u);

  // Level 2: '{a, '{2{b,c}}}  → [RefObj "a", level3]
  const hldb::Operation *const l2 = any_cast<hldb::Operation>((*l1->getOperands())[1]);
  ASSERT_NE(l2, nullptr) << "level-2 operand is not an Operation (expected '{a,...})";
  EXPECT_EQ(l2->getOpType(), vpiAssignmentPatternOp);
  ASSERT_EQ(l2->getOperands()->size(), 2u);

  // Level 2, first operand must be a RefObj to "a"
  const hldb::RefObj *const refA = any_cast<hldb::RefObj>((*l2->getOperands())[0]);
  ASSERT_NE(refA, nullptr) << "level-2 first operand is not a RefObj (expected 'a')";
  EXPECT_EQ(refA->getName(), "a");

  // Level 3: '{2{b,c}}        → [Const "2", RefObj "b", RefObj "c"]
  const hldb::Operation *const l3 = any_cast<hldb::Operation>((*l2->getOperands())[1]);
  ASSERT_NE(l3, nullptr) << "level-3 operand is not an Operation (expected '{2{b,c}})";
  EXPECT_EQ(l3->getOpType(), vpiAssignmentPatternOp);
  ASSERT_EQ(l3->getOperands()->size(), 3u) << "innermost '{2{b,c}} should have 3 operands: [count=2, 'b', 'c']";

  const hldb::RefObj *const refB = any_cast<hldb::RefObj>((*l3->getOperands())[1]);
  ASSERT_NE(refB, nullptr) << "innermost second operand is not a RefObj (expected 'b')";
  EXPECT_EQ(refB->getName(), "b");

  const hldb::RefObj *const refC = any_cast<hldb::RefObj>((*l3->getOperands())[2]);
  ASSERT_NE(refC, nullptr) << "innermost third operand is not a RefObj (expected 'c')";
  EXPECT_EQ(refC->getName(), "c");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
