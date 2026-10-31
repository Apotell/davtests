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

// Validates the HLDB model built for tests/PartSelectHierPath/dut.sv:
//
//   package swerv_types;
//
//   typedef struct packed {
//                          logic [2:0] trace_rv_i_valid_ip;
//                          logic [95:0] trace_rv_i_insn_ip;
//                          } trace_pkt_t;
//
//   endpackage // swerv_types
//
//   module top();
//     import swerv_types::*;
//     trace_pkt_t  trace_rv_trace_pkt;
//     assign trace_rv_i_insn_ip[63:0]     = trace_rv_trace_pkt.trace_rv_i_insn_ip[63:0];
//   endmodule
//
// The fixture reads a part-select (11.5.1) of a packed structure member
// (7.2) through a variable whose type comes from a wildcard package import
// (26.3). The LHS names trace_rv_i_insn_ip, which top never declares; the
// member of the same name is not visible as a plain identifier.
//
// What is checked, and why:
//   - top imports swerv_types::* (26.3) and trace_rv_trace_pkt's type binds
//     to the package typedef trace_pkt_t through it
//   - the structure has two members in declaration order, logic [2:0] and
//     logic [95:0] (7.2)
//   - the continuous assignment's LHS is a part-select [63:0] of the name
//     trace_rv_i_insn_ip, and that name does NOT bind to the structure
//     member: a member is only reachable through the structure (7.2)
//   - the RHS names trace_rv_trace_pkt, then the part-select [63:0] of its
//     member trace_rv_i_insn_ip
//   - no fatal or syntax diagnostic
//
// Model convention: the hierarchical RHS is one RefObj whose path elements
// are the RefObj trace_rv_trace_pkt and the PartSelect of the member; the
// RefObj binds to that PartSelect.
//
// What is NOT checked, and why:
//   - What the undeclared LHS name resolves to, and whether it is an error.
//     6.10 says "If an identifier appears on the left-hand side of a
//     continuous assignment statement, and that identifier has not been
//     declared previously in the scope where the continuous assignment
//     statement appears ... then an implicit scalar net of default net type
//     shall be assumed", but the identifier here is part-selected, and a
//     [63:0] select of a scalar net is not meaningful. The standard does not
//     plainly settle this case, so neither an implicit net nor an error is
//     asserted; HLC reports a "Failed to bind" warning. Left for a human to
//     decide. LhsIsPartSelectOfUndeclaredName pins what is certain.
//   - The value assigned is a runtime property; HLC is not a simulator, so
//     this is out of scope permanently.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/import_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/part_select.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class PartSelectHierPathTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PartSelectHierPath.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::ContAssign *getAssign() {
    const hldb::Module *const m = getTop();
    if ((m == nullptr) || (m->getContAssigns() == nullptr) || (m->getContAssigns()->size() != 1)) return nullptr;
    return m->getContAssigns()->at(0);
  }

  static const hldb::TypedefTypespec *getTraceType() {
    const hldb::Package *const p = hldb::findByName<hldb::Package>("swerv_types", m_design->getAllPackages());
    if (p == nullptr) return nullptr;
    return hldb::findByName<hldb::TypedefTypespec>("trace_pkt_t", p->getTypespecs());
  }

  static void ExpectConstant(const hldb::Expr *e, std::string_view text, std::string_view why) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(e);
    ASSERT_NE(c, nullptr) << why << ": a literal";
    EXPECT_EQ(c->getDecompile(), text) << why;
  }

  static void ExpectRange63To0(const hldb::PartSelect *ps) {
    ASSERT_NE(ps->getRange(), nullptr) << "11.5.1: a part-select has a range";
    ExpectConstant(ps->getRange()->getLeftExpr(), "63", "11.5.1: msb expression");
    ExpectConstant(ps->getRange()->getRightExpr(), "0", "11.5.1: lsb expression");
  }
};

TEST_F(PartSelectHierPathTest, TopImportsSwervTypes) {
  const hldb::Module *const m = getTop();
  ASSERT_NE(m, nullptr);
  const hldb::ImportTypespec *const imp = hldb::findByName<hldb::ImportTypespec>("swerv_types", m->getTypespecs());
  ASSERT_NE(imp, nullptr) << "26.3: 'import swerv_types::*'";
  const hldb::Constant *const item = imp->getItem<hldb::Constant>();
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->getDecompile(), "*") << "26.3: a wildcard import";
}

TEST_F(PartSelectHierPathTest, TracePktTHasTwoMembers) {
  const hldb::TypedefTypespec *const td = getTraceType();
  ASSERT_NE(td, nullptr) << "6.18: swerv_types declares trace_pkt_t";
  ASSERT_NE(td->getTypedef(), nullptr);
  ASSERT_NE(td->getTypedef()->getAlias(), nullptr);
  const hldb::StructTypespec *const sts = td->getTypedef()->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(sts, nullptr) << "7.2: a structure";
  const hldb::Struct *const st = sts->getStruct();
  ASSERT_NE(st, nullptr);
  EXPECT_TRUE(st->getPacked()) << "7.2.1: 'struct packed'";
  ASSERT_NE(st->getMembers(), nullptr);
  ASSERT_EQ(st->getMembers()->size(), 2u);
  const char *const names[] = {"trace_rv_i_valid_ip", "trace_rv_i_insn_ip"};
  const char *const lefts[] = {"2", "95"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::TypespecMember *const mem = st->getMembers()->at(i);
    EXPECT_EQ(mem->getName(), names[i]) << "7.2: members in declaration order";
    ASSERT_NE(mem->getTypespec(), nullptr);
    const hldb::LogicTypespec *const lt = mem->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr);
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u);
    ExpectConstant(lt->getRanges()->at(0)->getLeftExpr(), lefts[i], "7.4.1: left bound");
    ExpectConstant(lt->getRanges()->at(0)->getRightExpr(), "0", "7.4.1: right bound");
  }
}

TEST_F(PartSelectHierPathTest, TraceRvTracePktIsImportedTypedef) {
  const hldb::Module *const m = getTop();
  ASSERT_NE(m, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("trace_rv_trace_pkt", m->getVariables());
  ASSERT_NE(v, nullptr);
  ASSERT_NE(v->getTypespec(), nullptr);
  EXPECT_EQ(v->getTypespec()->getActual(), getTraceType()) << "26.3: trace_pkt_t is visible through the import";
}

TEST_F(PartSelectHierPathTest, LhsIsPartSelectOfUndeclaredName) {
  const hldb::ContAssign *const ca = getAssign();
  ASSERT_NE(ca, nullptr) << "10.3: one continuous assignment";
  const hldb::PartSelect *const ps = ca->getLhs<hldb::PartSelect>();
  ASSERT_NE(ps, nullptr) << "11.5.1: the LHS is a part-select";
  const hldb::RefObj *const prefix = ps->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), "trace_rv_i_insn_ip");
  EXPECT_EQ(prefix->getActual<hldb::TypespecMember>(), nullptr)
      << "7.2: a structure member is not visible as a plain identifier";
  ExpectRange63To0(ps);
}

TEST_F(PartSelectHierPathTest, RhsIsPartSelectOfStructMember) {
  const hldb::ContAssign *const ca = getAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const rhs = ca->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getPathElems(), nullptr);
  ASSERT_EQ(rhs->getPathElems()->size(), 2u);
  const hldb::RefObj *const head = any_cast<hldb::RefObj>(rhs->getPathElems()->at(0));
  ASSERT_NE(head, nullptr);
  EXPECT_EQ(head->getActual(), hldb::findByName<hldb::Variable>("trace_rv_trace_pkt", getTop()->getVariables()));
  const hldb::PartSelect *const ps = any_cast<hldb::PartSelect>(rhs->getPathElems()->at(1));
  ASSERT_NE(ps, nullptr) << "11.5.1: a part-select of the member";
  const hldb::RefObj *const prefix = ps->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  const hldb::TypespecMember *const mem = prefix->getActual<hldb::TypespecMember>();
  ASSERT_NE(mem, nullptr) << "7.2: binds to the structure member";
  EXPECT_EQ(mem->getName(), "trace_rv_i_insn_ip");
  ExpectRange63To0(ps);
}

TEST_F(PartSelectHierPathTest, NoFatalOrSyntaxDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
