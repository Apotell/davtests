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

// Tests for dut.sv (tags: ForeachForeach)
//   package uvm;
//     class uvm_phase;
//       bit m_predecessors[uvm_phase];
//     endclass
//     function void uvm_phase::get_adjacent_predecessor_nodes();
//       bit predecessors[uvm_phase];
//       foreach (predecessors[p]) begin
//         foreach (p.m_predecessors[next_p]) begin
//           predecessors[next_p] = 1;
//         end
//       end
//     endfunction
//   endpackage
//
// This exercises two nested "foreach" loops (IEEE 1800-2023 Sec 12.7.3):
// the outer loop iterates a local associative array "predecessors" indexed
// by class type "uvm_phase" (7.8.1 associative arrays may be indexed by a
// class handle), and the inner loop -- for every iteration of the outer
// loop -- iterates the class-member associative array
// "p.m_predecessors" reached through the outer loop's own iterator
// variable "p". This checks that the inner ForeachStmt is correctly nested
// inside the outer ForeachStmt's body, and that the inner loop's array
// expression can reference the outer loop's iterator.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/foreach_stmt.h>
#include <hldb/function.h>
#include <hldb/package.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ForeachForeachTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ForeachForeach.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getUvm() {
    return hldb::findByName<hldb::Package>("uvm", m_design->getAllPackages());
  }

  static const hldb::ClassDefn *getUvmPhase() {
    const hldb::Package *const uvm = getUvm();
    if (uvm == nullptr || uvm->getClassDefns() == nullptr) return nullptr;
    return hldb::findByName<hldb::ClassDefn>("uvm_phase", uvm->getClassDefns());
  }

  static const hldb::Function *getGetAdjacentPredecessorNodes() {
    const hldb::ClassDefn *const uvmPhase = getUvmPhase();
    if (uvmPhase == nullptr || uvmPhase->getMethods() == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("get_adjacent_predecessor_nodes", uvmPhase->getMethods());
  }

  static const hldb::ForeachStmt *getOuterForeach() {
    const hldb::Function *const fn = getGetAdjacentPredecessorNodes();
    if (fn == nullptr) return nullptr;
    const hldb::Begin *const body = fn->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() < 2) return nullptr;
    return any_cast<hldb::ForeachStmt>(body->getStmts()->at(1));
  }

  static const hldb::ForeachStmt *getInnerForeach() {
    const hldb::ForeachStmt *const outer = getOuterForeach();
    if (outer == nullptr) return nullptr;
    const hldb::Begin *const outerBody = outer->getStmt<hldb::Begin>();
    if (outerBody == nullptr || outerBody->getStmts() == nullptr || outerBody->getStmts()->empty()) return nullptr;
    return any_cast<hldb::ForeachStmt>(outerBody->getStmts()->at(0));
  }
};

// ===========================================================================
// package uvm / class uvm_phase / method get_adjacent_predecessor_nodes
// ===========================================================================

TEST_F(ForeachForeachTest, PackageUvmExists) { EXPECT_NE(getUvm(), nullptr); }

TEST_F(ForeachForeachTest, UvmPhaseHasMemberAssociativeArray) {
  const hldb::ClassDefn *const cls = getUvmPhase();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  const hldb::Variable *const member = hldb::findByName<hldb::Variable>("m_predecessors", cls->getVariables());
  ASSERT_NE(member, nullptr) << "class 'uvm_phase' should declare member 'm_predecessors'";
  EXPECT_EQ(member->getName(), std::string_view{"m_predecessors"});
}

TEST_F(ForeachForeachTest, MethodExists) {
  const hldb::Function *const fn = getGetAdjacentPredecessorNodes();
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn->getName(), std::string_view{"get_adjacent_predecessor_nodes"});
}

// ===========================================================================
// method body: [0] local 'predecessors' variable decl, [1] outer foreach
// ===========================================================================

TEST_F(ForeachForeachTest, MethodBodyHasVariableDeclThenOuterForeach) {
  const hldb::Function *const fn = getGetAdjacentPredecessorNodes();
  ASSERT_NE(fn, nullptr);
  const hldb::Begin *const body = fn->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u);

  const hldb::Variable *const decl = any_cast<hldb::Variable>(body->getStmts()->at(0));
  ASSERT_NE(decl, nullptr) << "first statement should be the local 'predecessors' variable declaration";
  EXPECT_EQ(decl->getName(), std::string_view{"predecessors"});

  const hldb::ForeachStmt *const outer = getOuterForeach();
  ASSERT_NE(outer, nullptr) << "second statement should be the outer ForeachStmt";
  EXPECT_EQ(outer->getAnyType(), hldb::AnyType::ForeachStmt);
}

// ===========================================================================
// outer foreach: iterates local associative array 'predecessors' with a
// single iterator 'p'
// ===========================================================================

TEST_F(ForeachForeachTest, OuterForeachIteratesLocalArray) {
  const hldb::ForeachStmt *const outer = getOuterForeach();
  ASSERT_NE(outer, nullptr);
  const hldb::RefObj *const arr = outer->getVariable();
  ASSERT_NE(arr, nullptr);
  EXPECT_EQ(arr->getName(), std::string_view{"predecessors"});

  ASSERT_NE(outer->getLoopVars(), nullptr);
  ASSERT_EQ(outer->getLoopVars()->size(), 1u);
  const hldb::Variable *const p = any_cast<hldb::Variable>(outer->getLoopVars()->at(0));
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getName(), std::string_view{"p"});
  EXPECT_TRUE(p->getIsIterator());
}

// ===========================================================================
// the outer foreach's body is a Begin whose only statement is the inner
// ForeachStmt -- confirms proper nesting (12.7.3)
// ===========================================================================

TEST_F(ForeachForeachTest, OuterForeachBodyIsInnerForeach) {
  const hldb::ForeachStmt *const outer = getOuterForeach();
  ASSERT_NE(outer, nullptr);
  const hldb::Begin *const outerBody = outer->getStmt<hldb::Begin>();
  ASSERT_NE(outerBody, nullptr);
  ASSERT_NE(outerBody->getStmts(), nullptr);
  ASSERT_EQ(outerBody->getStmts()->size(), 1u);

  const hldb::ForeachStmt *const inner = getInnerForeach();
  ASSERT_NE(inner, nullptr) << "outer foreach's single body statement should be the inner ForeachStmt";
  EXPECT_EQ(inner->getAnyType(), hldb::AnyType::ForeachStmt);
}

// ===========================================================================
// inner foreach: iterates 'p.m_predecessors', a class-member array reached
// through the outer loop's own iterator variable 'p', with iterator
// 'next_p'
// ===========================================================================

TEST_F(ForeachForeachTest, InnerForeachIteratesThroughOuterIterator) {
  const hldb::ForeachStmt *const inner = getInnerForeach();
  ASSERT_NE(inner, nullptr);
  const hldb::RefObj *const arr = inner->getVariable();
  ASSERT_NE(arr, nullptr);
  EXPECT_EQ(arr->getName(), std::string_view{"p.m_predecessors"});
  ASSERT_NE(arr->getPathElems(), nullptr);
  ASSERT_EQ(arr->getPathElems()->size(), 2u);
  const hldb::RefObj *const outerVarRef = any_cast<hldb::RefObj>(arr->getPathElems()->at(0));
  ASSERT_NE(outerVarRef, nullptr);
  EXPECT_EQ(outerVarRef->getName(), std::string_view{"p"});
  ASSERT_NE(outerVarRef->getActual(), nullptr)
      << "'p' should resolve to the outer foreach's own iterator variable";
  const hldb::Variable *const outerVarActual = outerVarRef->getActual<hldb::Variable>();
  ASSERT_NE(outerVarActual, nullptr);
  EXPECT_EQ(outerVarActual->getName(), std::string_view{"p"});
  EXPECT_TRUE(outerVarActual->getIsIterator());

  const hldb::RefObj *const memberRef = any_cast<hldb::RefObj>(arr->getPathElems()->at(1));
  ASSERT_NE(memberRef, nullptr);
  EXPECT_EQ(memberRef->getName(), std::string_view{"m_predecessors"});

  ASSERT_NE(inner->getLoopVars(), nullptr);
  ASSERT_EQ(inner->getLoopVars()->size(), 1u);
  const hldb::Variable *const nextP = any_cast<hldb::Variable>(inner->getLoopVars()->at(0));
  ASSERT_NE(nextP, nullptr);
  EXPECT_EQ(nextP->getName(), std::string_view{"next_p"});
  EXPECT_TRUE(nextP->getIsIterator());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
