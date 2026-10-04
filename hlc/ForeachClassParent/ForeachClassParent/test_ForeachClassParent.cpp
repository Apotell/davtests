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

// Tests for dut.sv (tags: ForeachClassParent)
//   package uvm;
//     class root;
//       protected uvm_phase m_type_overrides[$];
//     endclass
//     class uvm_phase extends root;
//     endclass
//     function void uvm_phase::get_adjacent_predecessor_nodes();
//       foreach (m_type_overrides[index]) begin
//       end
//     endfunction
//   endpackage
//
// This exercises a "foreach" loop (IEEE 1800-2023 Sec 12.7.3) inside a
// method of a derived class ("uvm_phase"), where the array being iterated
// ("m_type_overrides") is not declared in "uvm_phase" itself but inherited
// from its base class "root" (Sec 8.13/8.19: a protected member of a base
// class is accessible, unqualified, from a derived class's own methods).
// "root" itself contains a queue of "uvm_phase" handles, so this file also
// exercises a class referring to itself through another class it is the
// base of.
//
// Note: "root" is used as the element type of "m_type_overrides" and as
// the base class of "uvm_phase" before "uvm_phase" itself is declared,
// which is fine (8.24 / 26.3: a class_declaration may reference another
// class type declared later in the same compilation unit as long as it is
// visible by the time it's actually used -- "root" only needs "uvm_phase"
// as a forward-referenced element type, not the reverse). The method body
// itself ("get_adjacent_predecessor_nodes") is defined out-of-block
// (8.24) via "uvm_phase::get_adjacent_predecessor_nodes()".

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/extends.h>
#include <hldb/foreach_stmt.h>
#include <hldb/function.h>
#include <hldb/package.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ForeachClassParentTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ForeachClassParent.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getUvm() {
    return hldb::findByName<hldb::Package>("uvm", m_design->getAllPackages());
  }

  static const hldb::ClassDefn *getRoot() {
    const hldb::Package *const uvm = getUvm();
    if (uvm == nullptr || uvm->getClassDefns() == nullptr) return nullptr;
    return hldb::findByName<hldb::ClassDefn>("root", uvm->getClassDefns());
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

  static const hldb::ForeachStmt *getForeach() {
    const hldb::Function *const fn = getGetAdjacentPredecessorNodes();
    if (fn == nullptr) return nullptr;
    return fn->getStmt<hldb::ForeachStmt>();
  }
};

// ===========================================================================
// package uvm / class root / class uvm_phase extends root
// ===========================================================================

TEST_F(ForeachClassParentTest, PackageUvmExists) { EXPECT_NE(getUvm(), nullptr); }

TEST_F(ForeachClassParentTest, RootHasMemberQueueOfUvmPhase) {
  const hldb::ClassDefn *const root = getRoot();
  ASSERT_NE(root, nullptr);
  ASSERT_NE(root->getVariables(), nullptr);
  const hldb::Variable *const member = hldb::findByName<hldb::Variable>("m_type_overrides", root->getVariables());
  ASSERT_NE(member, nullptr) << "class 'root' should declare member 'm_type_overrides'";
  EXPECT_EQ(member->getName(), std::string_view{"m_type_overrides"});
}

TEST_F(ForeachClassParentTest, UvmPhaseExtendsRoot) {
  const hldb::ClassDefn *const uvmPhase = getUvmPhase();
  ASSERT_NE(uvmPhase, nullptr);
  const hldb::Extends *const ext = uvmPhase->getExtends();
  ASSERT_NE(ext, nullptr) << "8.13: 'class uvm_phase extends root' should produce an Extends relation";
  ASSERT_NE(ext->getClassTypespecs(), nullptr);
  ASSERT_EQ(ext->getClassTypespecs()->size(), 1u);
  EXPECT_NE(ext->getClassTypespecs()->at(0), nullptr);
}

TEST_F(ForeachClassParentTest, MethodExists) {
  const hldb::Function *const fn = getGetAdjacentPredecessorNodes();
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn->getName(), std::string_view{"get_adjacent_predecessor_nodes"});
}

// ===========================================================================
// the method's single statement is directly the ForeachStmt (no enclosing
// Begin, since it's the function's only statement -- same pattern as
// DoWhileTest's PrintBodyIsDirectlyDoWhile)
// ===========================================================================

TEST_F(ForeachClassParentTest, MethodBodyIsDirectlyForeach) {
  const hldb::ForeachStmt *const fe = getForeach();
  ASSERT_NE(fe, nullptr) << "the method's single statement should resolve to ForeachStmt";
  EXPECT_EQ(fe->getAnyType(), hldb::AnyType::ForeachStmt);
}

// ===========================================================================
// 8.13/8.19: the foreach array 'm_type_overrides' is not a member of
// 'uvm_phase' itself but is inherited (protected) from its base 'root'
// ===========================================================================

TEST_F(ForeachClassParentTest, ForeachArrayResolvesToInheritedBaseMember) {
  const hldb::ForeachStmt *const fe = getForeach();
  ASSERT_NE(fe, nullptr);
  const hldb::RefObj *const arr = fe->getVariable();
  ASSERT_NE(arr, nullptr) << "12.7.3: foreach shall name the array being iterated";
  EXPECT_EQ(arr->getName(), std::string_view{"m_type_overrides"});

  ASSERT_NE(arr->getActual(), nullptr)
      << "8.13/8.19: 'm_type_overrides', though declared only in base class 'root', "
         "must still resolve when referenced unqualified from derived class 'uvm_phase'";
  const hldb::Variable *const actual = arr->getActual<hldb::Variable>();
  ASSERT_NE(actual, nullptr) << "'m_type_overrides' should resolve to the Variable declared in 'root'";
  EXPECT_EQ(actual->getName(), std::string_view{"m_type_overrides"});

  const hldb::ClassDefn *const root = getRoot();
  ASSERT_NE(root, nullptr);
  ASSERT_NE(root->getVariables(), nullptr);
  const hldb::Variable *const rootMember = hldb::findByName<hldb::Variable>("m_type_overrides", root->getVariables());
  ASSERT_NE(rootMember, nullptr);
  EXPECT_EQ(actual, rootMember) << "the resolved variable should be the same object declared in base class 'root'";
}

// ===========================================================================
// 12.7.3: exactly one loop variable ('index'), implicitly declared and
// marked as an iterator
// ===========================================================================

TEST_F(ForeachClassParentTest, ForeachHasOneIteratorLoopVar) {
  const hldb::ForeachStmt *const fe = getForeach();
  ASSERT_NE(fe, nullptr);
  ASSERT_NE(fe->getLoopVars(), nullptr);
  ASSERT_EQ(fe->getLoopVars()->size(), 1u);
  const hldb::Variable *const index = any_cast<hldb::Variable>(fe->getLoopVars()->at(0));
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->getName(), std::string_view{"index"});
  EXPECT_TRUE(index->getIsIterator()) << "12.7.3: foreach loop variables are implicitly declared iterators";
}

// ===========================================================================
// foreach body is present (empty 'begin end' per the source)
// ===========================================================================

TEST_F(ForeachClassParentTest, ForeachBodyIsEmptyBegin) {
  const hldb::ForeachStmt *const fe = getForeach();
  ASSERT_NE(fe, nullptr);
  const hldb::Begin *const body = fe->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "foreach body should be a Begin (explicit begin-end in source)";
  EXPECT_EQ(body->getStmts(), nullptr) << "'begin end' has no statements";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
