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

// Tests for dut.sv (tags: NamedEventHierPath)
//   package pack;
//   virtual class uvm_event_base;
//     protected event m_event;
//   virtual function void do_copy (uvm_object rhs);
//   uvm_event_base e;
//   m_event = e.m_event;
//   endfunction
//   endclass
//   endpackage // pack
//
// What is checked (IEEE 1800-2023):
//   - 8.21: 'uvm_event_base' is declared 'virtual class' (abstract) in
//     package 'pack'.
//   - 6.17: the class property 'm_event' is a named event (event data type),
//     typed by an event typespec.
//   - 8.20 / 8.18: 'do_copy' is a virtual method, a function returning
//     'void', with default (public) visibility, and one input argument
//     'rhs' (13.5: default direction input).
//   - 'uvm_object' is never declared (this file is compiled with
//     -nobuiltin and no UVM package), so the argument type cannot be
//     resolved: a COMP_FAILED_TO_BIND diagnostic naming 'uvm_object' is
//     expected.
//   - A.2.6 / 13.4: the function body has one block_item_declaration
//     ('uvm_event_base e;', a local variable of class type) followed by a
//     single statement, the blocking assignment. (HLDB also lists the
//     declaration itself among the Begin's getStmts() entries, in source
//     order; that entry is the Variable 'e' and is not counted.)
//   - 6.17: 'm_event = e.m_event;' assigns one event to another. The LHS
//     binds to the class's named event 'm_event'. The RHS is the
//     hierarchical / member path 'e.m_event': 'e' binds to the local
//     variable and 'm_event' binds to the named event of class
//     'uvm_event_base' (the class type of 'e'). 8.18: a protected member
//     is visible inside the class's own methods, so accessing it through
//     another handle of the same class is legal -- no binding error for
//     'm_event' or 'e'.
//
// What is NOT checked and why:
//   - the 'protected' qualifier on 'm_event': HLDB's NamedEvent exposes no
//     visibility accessor, so there is nothing in the object model to
//     assert against.
//   - runtime event aliasing semantics of the assignment (15.5.5.1).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/design.h>
#include <hldb/event_typespec.h>
#include <hldb/function.h>
#include <hldb/io_decl.h>
#include <hldb/named_event.h>
#include <hldb/package.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class NamedEventHierPathTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "NamedEventHierPath.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPack() { return hldb::findByName<hldb::Package>("pack", m_design->getAllPackages()); }

  static const hldb::ClassDefn *getClass() {
    const hldb::Package *const p = getPack();
    return (p == nullptr) ? nullptr : hldb::findByName<hldb::ClassDefn>("uvm_event_base", p->getClassDefns());
  }

  static const hldb::NamedEvent *getEvent() {
    const hldb::ClassDefn *const c = getClass();
    return (c == nullptr) ? nullptr : hldb::findByName<hldb::NamedEvent>("m_event", c->getNamedEvents());
  }

  static const hldb::Function *getDoCopy() {
    const hldb::ClassDefn *const c = getClass();
    return (c == nullptr) ? nullptr : hldb::findByName<hldb::Function>("do_copy", c->getMethods());
  }

  static const hldb::Begin *getBody() {
    const hldb::Function *const f = getDoCopy();
    return (f == nullptr) ? nullptr : any_cast<hldb::Begin>(f->getStmt());
  }

  static const hldb::Variable *getLocalE() {
    const hldb::Begin *const b = getBody();
    return (b == nullptr) ? nullptr : hldb::findByName<hldb::Variable>("e", b->getVariables());
  }

  static const hldb::Assignment *getAssign() {
    const hldb::Begin *const b = getBody();
    if (b == nullptr || b->getStmts() == nullptr) return nullptr;
    for (const hldb::Any *const s : *b->getStmts()) {
      if (const hldb::Assignment *const a = any_cast<hldb::Assignment>(s)) return a;
    }
    return nullptr;
  }
};

// ===========================================================================
// 8.21 / 6.17: class and event property
// ===========================================================================

TEST_F(NamedEventHierPathTest, VirtualClassInPackage) {
  ASSERT_NE(getPack(), nullptr);
  const hldb::ClassDefn *const c = getClass();
  ASSERT_NE(c, nullptr) << "class 'uvm_event_base' not found in package 'pack'";
  EXPECT_TRUE(c->getVirtual()) << "8.21: 'virtual class'";
}

TEST_F(NamedEventHierPathTest, MEventIsNamedEvent) {
  const hldb::NamedEvent *const ev = getEvent();
  ASSERT_NE(ev, nullptr) << "6.17: 'event m_event' is a named event";
  ASSERT_NE(ev->getTypespec(), nullptr);
  ASSERT_NE(ev->getTypespec()->getActual(), nullptr);
  EXPECT_EQ(ev->getTypespec()->getActual()->getAnyType(), hldb::AnyType::EventTypespec);
}

// ===========================================================================
// 8.20: virtual function void do_copy (uvm_object rhs)
// ===========================================================================

TEST_F(NamedEventHierPathTest, DoCopyIsVirtualVoidPublicMethod) {
  const hldb::Function *const f = getDoCopy();
  ASSERT_NE(f, nullptr);
  EXPECT_TRUE(f->getMethod());
  EXPECT_TRUE(f->getVirtual()) << "8.20: 'virtual function'";
  EXPECT_EQ(f->getVisibility(), vpiPublicVis) << "8.18: members are public unless qualified";
  ASSERT_NE(f->getReturn(), nullptr);
  ASSERT_NE(f->getReturn()->getActual(), nullptr);
  EXPECT_EQ(f->getReturn()->getActual()->getAnyType(), hldb::AnyType::VoidTypespec);
}

TEST_F(NamedEventHierPathTest, DoCopyHasOneInputArgRhs) {
  const hldb::Function *const f = getDoCopy();
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->getIODecls(), nullptr);
  ASSERT_EQ(f->getIODecls()->size(), 1u);
  const hldb::IODecl *const io = f->getIODecls()->at(0);
  EXPECT_EQ(io->getName(), "rhs");
  EXPECT_EQ(io->getDirection(), vpiInput) << "13.5: default argument direction is input";
  ASSERT_NE(io->getTypespec(), nullptr);
  EXPECT_EQ(io->getTypespec()->getName(), "uvm_object");
}

TEST_F(NamedEventHierPathTest, UndeclaredUvmObjectIsDiagnosed) {
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "uvm_object"), nullptr)
      << "'uvm_object' is never declared; the argument type cannot be resolved";
}

// ===========================================================================
// A.2.6: body = one declaration + one statement
// ===========================================================================

TEST_F(NamedEventHierPathTest, LocalVariableEIsOfClassType) {
  const hldb::Variable *const e = getLocalE();
  ASSERT_NE(e, nullptr) << "'uvm_event_base e;' is a local variable of do_copy";
  ASSERT_NE(e->getTypespec(), nullptr);
  const hldb::ClassTypespec *const cts = any_cast<hldb::ClassTypespec>(e->getTypespec()->getActual());
  ASSERT_NE(cts, nullptr);
  EXPECT_EQ(cts->getClassDefn(), getClass());
}

TEST_F(NamedEventHierPathTest, BodyHasExactlyOneStatement) {
  const hldb::Begin *const b = getBody();
  ASSERT_NE(b, nullptr);
  ASSERT_NE(b->getStmts(), nullptr);
  // HLDB lists block item declarations in getStmts() (in source order) next
  // to the real statements; only the non-declaration entries are statements.
  size_t nbStatements = 0;
  for (const hldb::Any *const s : *b->getStmts()) {
    if (s->getAnyType() == hldb::AnyType::Variable) {
      EXPECT_EQ(s, getLocalE()) << "the only declaration is 'uvm_event_base e;'";
    } else {
      ++nbStatements;
      EXPECT_EQ(s->getAnyType(), hldb::AnyType::Assignment);
    }
  }
  EXPECT_EQ(nbStatements, 1u) << "A.2.6: the body has exactly one statement, 'm_event = e.m_event;'";
  EXPECT_NE(getAssign(), nullptr);
}

// ===========================================================================
// 6.17: m_event = e.m_event;
// ===========================================================================

TEST_F(NamedEventHierPathTest, AssignmentIsBlocking) {
  const hldb::Assignment *const a = getAssign();
  ASSERT_NE(a, nullptr);
  EXPECT_TRUE(a->getBlocking());
}

TEST_F(NamedEventHierPathTest, LhsBindsToClassEvent) {
  const hldb::Assignment *const a = getAssign();
  ASSERT_NE(a, nullptr);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(a->getLhs());
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "m_event");
  ASSERT_NE(lhs->getActual(), nullptr);
  EXPECT_EQ(lhs->getActual(), getEvent());
}

TEST_F(NamedEventHierPathTest, RhsPathEThenMEvent) {
  const hldb::Assignment *const a = getAssign();
  ASSERT_NE(a, nullptr);
  const hldb::RefObj *const rhs = any_cast<hldb::RefObj>(a->getRhs());
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getPathElems(), nullptr);
  ASSERT_EQ(rhs->getPathElems()->size(), 2u);

  const hldb::RefObj *const head = any_cast<hldb::RefObj>(rhs->getPathElems()->at(0));
  ASSERT_NE(head, nullptr);
  EXPECT_EQ(head->getName(), "e");
  ASSERT_NE(head->getActual(), nullptr);
  EXPECT_EQ(head->getActual(), getLocalE());

  const hldb::RefObj *const tail = any_cast<hldb::RefObj>(rhs->getPathElems()->at(1));
  ASSERT_NE(tail, nullptr);
  EXPECT_EQ(tail->getName(), "m_event");
  ASSERT_NE(tail->getActual(), nullptr);
  EXPECT_EQ(tail->getActual(), getEvent()) << "'m_event' of class uvm_event_base (type of 'e')";
}

TEST_F(NamedEventHierPathTest, ProtectedAccessWithinClassIsLegal) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "m_event"), nullptr)
      << "8.18: protected members are visible within the class's own methods";
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "e"), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
