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

// Tests for tests/LocalScopeHierPath/dut.sv:
//
//   package pack;
//   typedef struct {
//      string spath;
//   } uvm_hdl_path_slice;
//
//   class uvm_hdl_path_concat;
//      uvm_hdl_path_slice slices[];
//   endclass
//
//   class uvm_mem_mam;
//   protected virtual function void check_reg();
//     uvm_hdl_path_concat paths[$];
//     foreach(paths[p]) begin
//       uvm_hdl_path_concat path=paths[p];
//       foreach (path.slices[j]) begin
//         string p_ = path.slices[j].spath;
//       end
//     end
//   endfunction
//   endclass
//   endpackage
//
// The construct under test is a dotted name rooted at a LOCAL variable of
// an enclosing block scope: 'path.slices' / 'path.slices[j].spath', where
// 'path' is declared in the outer foreach body, 'slices' is a property of
// class uvm_hdl_path_concat, and 'spath' is a member of the struct type of
// the array element.
//
// -- rules under test ---------------------------------------------------
//
// IEEE 1800-2023 Sec 7.2 / 6.18: 'typedef struct { string spath; }
//   uvm_hdl_path_slice;' is an unpacked struct (no 'packed' keyword) with
//   one member 'spath' of type string (Sec 6.16).
// IEEE 1800-2023 Sec 7.5 "Dynamic arrays": 'slices[]' is a dynamic array
//   whose element type is 'uvm_hdl_path_slice'.
// IEEE 1800-2023 Sec 7.10 "Queues": 'paths[$]' is a queue of class handles
//   of type 'uvm_hdl_path_concat'.
// IEEE 1800-2023 Sec 8.18: 'protected' gives the method protected
//   visibility (vpiProtectedVis). Sec 8.20: 'virtual' makes it a virtual
//   method. Sec 8.6: "The lifetime of methods declared as part of a class
//   type shall be automatic." Sec 13.4.1: 'function void' has a void
//   return type.
// IEEE 1800-2023 Sec 12.7.3 "The foreach-loop": 'foreach(paths[p])'
//   iterates over array 'paths' with loop variable 'p'; 'foreach
//   (path.slices[j])' iterates over the array named by the hierarchical
//   reference 'path.slices' with loop variable 'j'.
// IEEE 1800-2023 Sec 8.4 / 8.5: 'path.slices' accesses the class property
//   'slices' through the handle 'path' -- it must resolve to the 'slices'
//   property of class uvm_hdl_path_concat. Sec 7.2: '.spath' selects the
//   struct member 'spath' of the element 'path.slices[j]'.
// IEEE 1800-2023 Sec 6.8 / 10.5: 'uvm_hdl_path_concat path=paths[p];' and
//   'string p_ = path.slices[j].spath;' are local variable declarations with
//   initializers.
//
// Model note: as for any function body without an explicit begin/end,
// findLocal() accepts the locals of 'check_reg' on either the Function
// scope or its implicit body Begin; the standard only requires that they
// are declared within the function.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/design.h>
#include <hldb/foreach_stmt.h>
#include <hldb/function.h>
#include <hldb/package.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/void_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class LocalScopeHierPathTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LocalScopeHierPath.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPack() { return hldb::findByName<hldb::Package>("pack", m_design->getAllPackages()); }

  static const hldb::ClassDefn *getClass(std::string_view name) {
    const hldb::Package *const pkg = getPack();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::ClassDefn>(name, pkg->getClassDefns());
  }

  static const hldb::Function *getCheckReg() {
    const hldb::ClassDefn *const c = getClass("uvm_mem_mam");
    if (c == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("check_reg", c->getMethods());
  }

  static const hldb::Variable *findLocal(std::string_view name) {
    const hldb::Function *const f = getCheckReg();
    if (f == nullptr) return nullptr;
    if (const hldb::Variable *const v = hldb::findByName<hldb::Variable>(name, f->getVariables())) return v;
    if (const hldb::Begin *const b = f->getStmt<hldb::Begin>()) {
      return hldb::findByName<hldb::Variable>(name, b->getVariables());
    }
    return nullptr;
  }

  static const hldb::ForeachStmt *findForeachIn(const hldb::Any *stmt) {
    if (stmt == nullptr) return nullptr;
    if (const hldb::ForeachStmt *const fe = any_cast<hldb::ForeachStmt>(stmt)) return fe;
    if (const hldb::Begin *const b = any_cast<hldb::Begin>(stmt)) {
      if (b->getStmts() == nullptr) return nullptr;
      for (const hldb::Any *const s : *b->getStmts()) {
        if (const hldb::ForeachStmt *const fe = any_cast<hldb::ForeachStmt>(s)) return fe;
      }
    }
    return nullptr;
  }

  static const hldb::ForeachStmt *getOuterForeach() {
    const hldb::Function *const f = getCheckReg();
    return (f == nullptr) ? nullptr : findForeachIn(f->getStmt());
  }

  static const hldb::Begin *getOuterBody() {
    const hldb::ForeachStmt *const fe = getOuterForeach();
    return (fe == nullptr) ? nullptr : fe->getStmt<hldb::Begin>();
  }

  static const hldb::ForeachStmt *getInnerForeach() { return findForeachIn(getOuterBody()); }

  static const hldb::Begin *getInnerBody() {
    const hldb::ForeachStmt *const fe = getInnerForeach();
    return (fe == nullptr) ? nullptr : fe->getStmt<hldb::Begin>();
  }

  static const hldb::Variable *getSlicesProperty() {
    const hldb::ClassDefn *const c = getClass("uvm_hdl_path_concat");
    return (c == nullptr) ? nullptr : hldb::findByName<hldb::Variable>("slices", c->getVariables());
  }

  static const hldb::TypespecMember *getSpathMember() {
    const hldb::Package *const pkg = getPack();
    if (pkg == nullptr) return nullptr;
    const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>("uvm_hdl_path_slice", pkg->getTypedefs());
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::StructTypespec *const sts = td->getAlias()->getActual<hldb::StructTypespec>();
    if (sts == nullptr || sts->getStruct() == nullptr) return nullptr;
    return hldb::findByName<hldb::TypespecMember>("spath", sts->getStruct()->getMembers());
  }
};

// ---------------------------------------------------------------------------
// Package / struct typedef -- Sec 7.2, 6.18
// ---------------------------------------------------------------------------

TEST_F(LocalScopeHierPathTest, PackageAndClassesExist) {
  EXPECT_NE(getPack(), nullptr) << "package 'pack' not found";
  EXPECT_NE(getClass("uvm_hdl_path_concat"), nullptr);
  EXPECT_NE(getClass("uvm_mem_mam"), nullptr);
}

TEST_F(LocalScopeHierPathTest, SliceTypedefIsUnpackedStructWithStringSpath) {
  const hldb::Package *const pkg = getPack();
  ASSERT_NE(pkg, nullptr);
  const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>("uvm_hdl_path_slice", pkg->getTypedefs());
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  ASSERT_NE(td->getAlias()->getActual(), nullptr);
  ASSERT_EQ(td->getAlias()->getActual()->getAnyType(), hldb::AnyType::StructTypespec);
  const hldb::Struct *const s = td->getAlias()->getActual<hldb::StructTypespec>()->getStruct();
  ASSERT_NE(s, nullptr);
  EXPECT_FALSE(s->getPacked()) << "no 'packed' keyword -> unpacked struct (Sec 7.2)";
  ASSERT_NE(s->getMembers(), nullptr);
  ASSERT_EQ(s->getMembers()->size(), 1u);
  const hldb::TypespecMember *const m = getSpathMember();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getTypespec(), nullptr);
  ASSERT_NE(m->getTypespec()->getActual(), nullptr);
  EXPECT_EQ(m->getTypespec()->getActual()->getAnyType(), hldb::AnyType::StringTypespec);
}

// ---------------------------------------------------------------------------
// Class properties / method -- Sec 7.5, 8.6, 8.18, 8.20
// ---------------------------------------------------------------------------

TEST_F(LocalScopeHierPathTest, SlicesIsDynamicArrayOfSliceTypedef) {
  const hldb::Variable *const v = getSlicesProperty();
  ASSERT_NE(v, nullptr) << "property 'slices' not found in uvm_hdl_path_concat";
  EXPECT_EQ(v->getVisibility(), vpiPublicVis) << "unqualified class properties are public (Sec 8.18)";
  ASSERT_NE(v->getTypespec(), nullptr);
  ASSERT_NE(v->getTypespec()->getActual(), nullptr);
  ASSERT_EQ(v->getTypespec()->getActual()->getAnyType(), hldb::AnyType::ArrayTypespec);
  const hldb::ArrayTypespec *const ats = v->getTypespec()->getActual<hldb::ArrayTypespec>();
  EXPECT_EQ(ats->getArrayType(), vpiDynamicArray);
  EXPECT_FALSE(ats->getPacked());
  ASSERT_NE(ats->getElemTypespec(), nullptr);
  const hldb::TypedefTypespec *const elem = ats->getElemTypespec()->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(elem, nullptr);
  EXPECT_EQ(elem->getName(), std::string_view("uvm_hdl_path_slice"));
}

TEST_F(LocalScopeHierPathTest, CheckRegIsProtectedVirtualVoidMethod) {
  const hldb::Function *const f = getCheckReg();
  ASSERT_NE(f, nullptr) << "method 'check_reg' not found in uvm_mem_mam";
  EXPECT_TRUE(f->getMethod());
  EXPECT_TRUE(f->getVirtual()) << "'virtual function' (Sec 8.20)";
  ASSERT_NE(f->getReturn(), nullptr);
  ASSERT_NE(f->getReturn()->getActual(), nullptr);
  EXPECT_EQ(f->getReturn()->getActual()->getAnyType(), hldb::AnyType::VoidTypespec);
}

TEST_F(LocalScopeHierPathTest, CheckRegHasProtectedVisibility) {
  const hldb::Function *const f = getCheckReg();
  ASSERT_NE(f, nullptr);
  EXPECT_EQ(f->getVisibility(), vpiProtectedVis) << "'protected virtual function' (Sec 8.18)";
}

TEST_F(LocalScopeHierPathTest, CheckRegIsAutomatic) {
  const hldb::Function *const f = getCheckReg();
  ASSERT_NE(f, nullptr);
  EXPECT_TRUE(f->getAutomatic()) << "class methods have automatic lifetime (Sec 8.6, 13.4.2)";
}

// ---------------------------------------------------------------------------
// 'uvm_hdl_path_concat paths[$];' -- Sec 7.10
// ---------------------------------------------------------------------------

TEST_F(LocalScopeHierPathTest, PathsIsQueueOfConcatHandles) {
  const hldb::Variable *const v = findLocal("paths");
  ASSERT_NE(v, nullptr) << "local 'paths' not found in check_reg";
  ASSERT_NE(v->getTypespec(), nullptr);
  ASSERT_NE(v->getTypespec()->getActual(), nullptr);
  ASSERT_EQ(v->getTypespec()->getActual()->getAnyType(), hldb::AnyType::ArrayTypespec);
  const hldb::ArrayTypespec *const ats = v->getTypespec()->getActual<hldb::ArrayTypespec>();
  EXPECT_EQ(ats->getArrayType(), vpiQueueArray);
  ASSERT_NE(ats->getElemTypespec(), nullptr);
  const hldb::ClassTypespec *const elem = ats->getElemTypespec()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(elem, nullptr);
  EXPECT_EQ(elem->getClassDefn(), getClass("uvm_hdl_path_concat"));
}

// ---------------------------------------------------------------------------
// Outer foreach -- Sec 12.7.3
// ---------------------------------------------------------------------------

TEST_F(LocalScopeHierPathTest, OuterForeachIteratesPathsWithP) {
  const hldb::ForeachStmt *const fe = getOuterForeach();
  ASSERT_NE(fe, nullptr) << "'foreach(paths[p])' not found";
  ASSERT_NE(fe->getVariable(), nullptr);
  EXPECT_EQ(fe->getVariable()->getName(), std::string_view("paths"));
  ASSERT_NE(fe->getVariable()->getActual(), nullptr);
  EXPECT_EQ(fe->getVariable()->getActual(), findLocal("paths"));
  ASSERT_NE(fe->getLoopVars(), nullptr);
  ASSERT_EQ(fe->getLoopVars()->size(), 1u);
  EXPECT_EQ(fe->getLoopVars()->at(0)->getName(), std::string_view("p"));
}

TEST_F(LocalScopeHierPathTest, PathIsLocalOfOuterBodyInitializedFromPathsP) {
  const hldb::Begin *const body = getOuterBody();
  ASSERT_NE(body, nullptr);
  const hldb::Variable *const path = hldb::findByName<hldb::Variable>("path", body->getVariables());
  ASSERT_NE(path, nullptr) << "'path' should be declared in the outer foreach body";
  ASSERT_NE(path->getTypespec(), nullptr);
  const hldb::ClassTypespec *const cts = path->getTypespec()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(cts, nullptr);
  EXPECT_EQ(cts->getClassDefn(), getClass("uvm_hdl_path_concat"));
  const hldb::BitSelect *const init = path->getValue<hldb::BitSelect>();
  ASSERT_NE(init, nullptr) << "'= paths[p]' initializer should be a BitSelect";
  const hldb::RefObj *const prefix = init->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  ASSERT_NE(prefix->getActual(), nullptr);
  EXPECT_EQ(prefix->getActual(), findLocal("paths"));
}

// ---------------------------------------------------------------------------
// Inner foreach over 'path.slices' -- Sec 12.7.3, 8.4
// ---------------------------------------------------------------------------

TEST_F(LocalScopeHierPathTest, InnerForeachIteratesPathSlicesWithJ) {
  const hldb::ForeachStmt *const fe = getInnerForeach();
  ASSERT_NE(fe, nullptr) << "'foreach (path.slices[j])' not found";
  ASSERT_NE(fe->getLoopVars(), nullptr);
  ASSERT_EQ(fe->getLoopVars()->size(), 1u);
  EXPECT_EQ(fe->getLoopVars()->at(0)->getName(), std::string_view("j"));
  const hldb::RefObj *const arr = fe->getVariable();
  ASSERT_NE(arr, nullptr);
  ASSERT_NE(arr->getActual(), nullptr) << "'path.slices' must resolve";
  EXPECT_EQ(arr->getActual(), getSlicesProperty()) << "'path.slices' is the class property 'slices' (Sec 8.4)";
}

TEST_F(LocalScopeHierPathTest, PathSlicesRootBindsToLocalPath) {
  const hldb::ForeachStmt *const fe = getInnerForeach();
  ASSERT_NE(fe, nullptr);
  const hldb::RefObj *const arr = fe->getVariable();
  ASSERT_NE(arr, nullptr);
  ASSERT_NE(arr->getPathElems(), nullptr);
  ASSERT_EQ(arr->getPathElems()->size(), 2u);
  const hldb::RefObj *const root = any_cast<hldb::RefObj>(arr->getPathElems()->at(0));
  ASSERT_NE(root, nullptr);
  EXPECT_EQ(root->getName(), std::string_view("path"));
  const hldb::Begin *const body = getOuterBody();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(root->getActual(), nullptr);
  EXPECT_EQ(root->getActual(), hldb::findByName<hldb::Variable>("path", body->getVariables()))
      << "'path' must bind to the local of the enclosing block scope";
}

// ---------------------------------------------------------------------------
// 'string p_ = path.slices[j].spath;' -- Sec 7.2, 6.16
// ---------------------------------------------------------------------------

TEST_F(LocalScopeHierPathTest, PUnderscoreIsStringLocalOfInnerBody) {
  const hldb::Begin *const body = getInnerBody();
  ASSERT_NE(body, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("p_", body->getVariables());
  ASSERT_NE(v, nullptr) << "'p_' should be declared in the inner foreach body";
  ASSERT_NE(v->getTypespec(), nullptr);
  ASSERT_NE(v->getTypespec()->getActual(), nullptr);
  EXPECT_EQ(v->getTypespec()->getActual()->getAnyType(), hldb::AnyType::StringTypespec);
}

TEST_F(LocalScopeHierPathTest, PUnderscoreInitializerResolvesToSpathMember) {
  const hldb::Begin *const body = getInnerBody();
  ASSERT_NE(body, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("p_", body->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefObj *const init = v->getValue<hldb::RefObj>();
  ASSERT_NE(init, nullptr) << "'path.slices[j].spath' should be a hierarchical reference";
  ASSERT_NE(init->getActual(), nullptr);
  EXPECT_EQ(init->getActual(), getSpathMember()) << "'.spath' selects the struct member (Sec 7.2)";
  ASSERT_NE(init->getPathElems(), nullptr);
  ASSERT_EQ(init->getPathElems()->size(), 3u);
  EXPECT_EQ(init->getPathElems()->at(0)->getName(), std::string_view("path"));
  const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(init->getPathElems()->at(1));
  ASSERT_NE(sel, nullptr) << "'slices[j]' should be a BitSelect";
  const hldb::RefObj *const idx = sel->getIndex<hldb::RefObj>();
  ASSERT_NE(idx, nullptr);
  EXPECT_EQ(idx->getName(), std::string_view("j"));
  EXPECT_EQ(init->getPathElems()->at(2)->getName(), std::string_view("spath"));
}

// ---------------------------------------------------------------------------
// Diagnostics -- the source is legal
// ---------------------------------------------------------------------------

TEST_F(LocalScopeHierPathTest, NoBindFailures) {
  for (const std::string_view name : {"path", "slices", "spath", "paths", "p", "j"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, name), nullptr) << name;
  }
}

TEST_F(LocalScopeHierPathTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
