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

// Tests for dut.sv (tags: LhsHierPath)
//   package my_pkg;
//      typedef struct packed {
//         logic p;
//      } ast_dif_t;
//      typedef struct packed {
//         ast_dif_t [1:0] alerts_ack;
//      } ast_alert_rsp_t;
//   endpackage
//
//   module top(output o);
//      my_pkg::ast_alert_rsp_t ast_alert_o;
//      always_comb begin
//         ast_alert_o.alerts_ack[0].p = 1'b1;
//      end
//      assign o = ast_alert_o.alerts_ack[0].p;
//   endmodule
//
// What is checked (IEEE 1800-2023):
//   - package 'my_pkg' declares two typedefs (6.18), each aliasing a packed
//     struct (7.2.1) with a single member: ast_dif_t { logic p; } and
//     ast_alert_rsp_t { ast_dif_t [1:0] alerts_ack; }
//   - 'alerts_ack' is a packed array [1:0] (7.4.1) whose element type is the
//     typedef ast_dif_t
//   - 'ast_alert_o' is a variable (6.8: declared with a data type and no
//     net type keyword) whose type is the package-scoped typedef
//     my_pkg::ast_alert_rsp_t (26.3)
//   - 'output o' (ANSI port with neither data type nor net type) is a net of
//     the default net type wire (23.2.2.3, 6.10)
//   - always_comb (9.2.2.2) body is a blocking assignment (10.4.1) whose LHS
//     is the member/select path ast_alert_o.alerts_ack[0].p (7.2, 7.4.3):
//     a reference rooted at variable ast_alert_o, a bit-select [0] of member
//     alerts_ack, then member p; RHS is the 1-bit binary constant 1'b1
//   - the continuous assignment (10.3.2) drives net o from the same path
//
// What is NOT checked and why:
//   - the object HLC binds a struct-member reference to (e.g. a
//     TypespecMember vs. a per-variable member object): the standard's object
//     model does not prescribe this for non-elaborated designs.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/package.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class LhsHierPathTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LhsHierPath.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() { return hldb::findByName<hldb::Package>("my_pkg", m_design->getAllPackages()); }
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Typedef *findTypedef(std::string_view name) {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr || pkg->getTypedefs() == nullptr) return nullptr;
    for (const hldb::Typedef *const td : *pkg->getTypedefs()) {
      if (td->getName() == name) return td;
    }
    return nullptr;
  }

  static const hldb::Struct *getTypedefStruct(std::string_view name) {
    const hldb::Typedef *const td = findTypedef(name);
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
    return (st == nullptr) ? nullptr : st->getStruct();
  }

  static const hldb::Variable *getAstAlertO() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getVariables() == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("ast_alert_o", top->getVariables());
  }

  // ast_alert_o.alerts_ack[0].p
  static void checkPath(const hldb::Any *any) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(any);
    ASSERT_NE(ref, nullptr) << "'ast_alert_o.alerts_ack[0].p' must be a reference";
    ASSERT_NE(ref->getPathElems(), nullptr) << "member/select path must carry its path elements";
    ASSERT_EQ(ref->getPathElems()->size(), 3u) << "ast_alert_o . alerts_ack[0] . p";

    const hldb::RefObj *const root = any_cast<hldb::RefObj>(ref->getPathElems()->at(0));
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->getName(), "ast_alert_o");
    EXPECT_EQ(root->getActual(), getAstAlertO()) << "path root must bind to variable 'ast_alert_o'";

    const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(ref->getPathElems()->at(1));
    ASSERT_NE(sel, nullptr) << "'alerts_ack[0]' must be a bit-select";
    const hldb::RefObj *const prefix = sel->getPrefix<hldb::RefObj>();
    ASSERT_NE(prefix, nullptr);
    EXPECT_EQ(prefix->getName(), "alerts_ack");
    ASSERT_NE(prefix->getActual(), nullptr) << "'alerts_ack' must resolve to the struct member";
    const hldb::Constant *const idx = sel->getIndex<hldb::Constant>();
    ASSERT_NE(idx, nullptr);
    EXPECT_EQ(idx->getValue(), "0");

    const hldb::RefObj *const leaf = any_cast<hldb::RefObj>(ref->getPathElems()->at(2));
    ASSERT_NE(leaf, nullptr);
    EXPECT_EQ(leaf->getName(), "p");
    ASSERT_NE(leaf->getActual(), nullptr) << "'p' must resolve to the struct member";
  }
};

// ---------------------------------------------------------------------------
// Package typedefs -- 6.18, 7.2.1, 7.4.1
// ---------------------------------------------------------------------------

TEST_F(LhsHierPathTest, PackageHasTwoTypedefs) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'my_pkg' not found";
  ASSERT_NE(pkg->getTypedefs(), nullptr);
  EXPECT_EQ(pkg->getTypedefs()->size(), 2u);
  EXPECT_NE(findTypedef("ast_dif_t"), nullptr);
  EXPECT_NE(findTypedef("ast_alert_rsp_t"), nullptr);
}

TEST_F(LhsHierPathTest, AstDifTIsPackedStructWithLogicP) {
  const hldb::Struct *const s = getTypedefStruct("ast_dif_t");
  ASSERT_NE(s, nullptr) << "ast_dif_t must alias a struct";
  EXPECT_TRUE(s->getPacked());
  ASSERT_NE(s->getMembers(), nullptr);
  ASSERT_EQ(s->getMembers()->size(), 1u);
  const hldb::TypespecMember *const p = s->getMembers()->at(0);
  EXPECT_EQ(p->getName(), "p");
  ASSERT_NE(p->getTypespec(), nullptr);
  ASSERT_NE(p->getTypespec()->getActual(), nullptr);
  EXPECT_EQ(p->getTypespec()->getActual()->getAnyType(), hldb::AnyType::LogicTypespec);
}

TEST_F(LhsHierPathTest, AstAlertRspTIsPackedStructWithPackedArrayMember) {
  const hldb::Struct *const s = getTypedefStruct("ast_alert_rsp_t");
  ASSERT_NE(s, nullptr) << "ast_alert_rsp_t must alias a struct";
  EXPECT_TRUE(s->getPacked());
  ASSERT_NE(s->getMembers(), nullptr);
  ASSERT_EQ(s->getMembers()->size(), 1u);
  const hldb::TypespecMember *const m = s->getMembers()->at(0);
  EXPECT_EQ(m->getName(), "alerts_ack");
  ASSERT_NE(m->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = m->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr) << "'ast_dif_t [1:0]' is a packed array of ast_dif_t";
  EXPECT_TRUE(at->getPacked()) << "7.4.1: dimensions before the identifier are packed";
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const left = at->getRange()->getLeftExpr<hldb::Constant>();
  const hldb::Constant *const right = at->getRange()->getRightExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->getValue(), "1");
  EXPECT_EQ(right->getValue(), "0");
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::TypedefTypespec *const elem = at->getElemTypespec()->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(elem, nullptr) << "element type is the typedef 'ast_dif_t'";
  EXPECT_EQ(elem->getName(), "ast_dif_t");
}

// ---------------------------------------------------------------------------
// top declarations -- 6.8, 23.2.2.3, 26.3
// ---------------------------------------------------------------------------

TEST_F(LhsHierPathTest, AstAlertOIsVariableOfPackageTypedef) {
  const hldb::Variable *const v = getAstAlertO();
  ASSERT_NE(v, nullptr) << "6.8: 'my_pkg::ast_alert_rsp_t ast_alert_o;' declares a variable";
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::TypedefTypespec *const tt = v->getTypespec()->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(tt, nullptr) << "type must resolve to the typedef my_pkg::ast_alert_rsp_t";
  EXPECT_EQ(tt->getName(), "ast_alert_rsp_t");
  EXPECT_EQ(tt->getTypedef(), findTypedef("ast_alert_rsp_t"));
}

TEST_F(LhsHierPathTest, OutputOIsImplicitWireNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 1u);
  const hldb::Port *const port = top->getPorts()->at(0);
  EXPECT_EQ(port->getName(), "o");
  EXPECT_EQ(port->getDirection(), vpiOutput);
  const hldb::RefObj *const low = port->getLowConn<hldb::RefObj>();
  ASSERT_NE(low, nullptr);
  const hldb::Net *const net = low->getActual<hldb::Net>();
  ASSERT_NE(low->getActual(), nullptr);
  ASSERT_NE(net, nullptr) << "23.2.2.3: an output port with no data type or net type is a net";
  EXPECT_EQ(net->getNetType(), vpiWire) << "default_nettype is wire";
}

// ---------------------------------------------------------------------------
// always_comb LHS path -- 9.2.2.2, 10.4.1, 7.2, 7.4.3
// ---------------------------------------------------------------------------

TEST_F(LhsHierPathTest, AlwaysCombBlockingAssignToMemberPath) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Always *const al = any_cast<hldb::Always>(top->getProcesses()->at(0));
  ASSERT_NE(al, nullptr);
  EXPECT_EQ(al->getAlwaysType(), vpiAlwaysComb);
  const hldb::Begin *const b = al->getStmt<hldb::Begin>();
  ASSERT_NE(b, nullptr);
  ASSERT_NE(b->getStmts(), nullptr);
  ASSERT_EQ(b->getStmts()->size(), 1u);
  const hldb::Assignment *const asg = any_cast<hldb::Assignment>(b->getStmts()->at(0));
  ASSERT_NE(asg, nullptr);
  EXPECT_TRUE(asg->getBlocking()) << "'=' is a blocking assignment";
  checkPath(asg->getLhs());
  const hldb::Constant *const rhs = asg->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getSize(), 1);
  EXPECT_EQ(rhs->getConstType(), vpiBinaryConst);
}

// ---------------------------------------------------------------------------
// continuous assignment RHS path -- 10.3.2
// ---------------------------------------------------------------------------

TEST_F(LhsHierPathTest, ContAssignDrivesOFromMemberPath) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "o");
  ASSERT_NE(lhs->getActual(), nullptr);
  EXPECT_EQ(lhs->getActual()->getAnyType(), hldb::AnyType::Net);
  checkPath(ca->getRhs());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
