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

// Validates the HLDB model built for tests/PackedArrayAssign/dut.sv:
//
//   module top(output logic [9:0] o);
//      typedef struct packed {
//         logic [9:0] min_v;
//      } filter_ctl_t;
//      filter_ctl_t [1:0] a = '{10'd15, 10'd0};
//      assign o = a[1];
//   endmodule
//
// The point of the fixture is a packed array of a packed structure (IEEE
// 1800-2023 7.4.1), initialized with an array assignment pattern (10.9.1),
// one of whose elements drives an output port. The regression this file
// exists to catch is HLC mis-modeling the packed dimension on the structure
// typedef or the element select.
//
// What is checked, and why:
//   Module top (23.2)
//     - exactly 1 port, 'o', with direction output. 'output logic [9:0] o'
//       writes its data type with the explicit data_type syntax, so the port
//       is a variable (23.2.2.3): the module's Variable o, a LogicTypespec
//       with exactly 1 packed range [9:0], is the port's low connection
//     - exactly 2 variables, o and a, and no nets
//   typedef struct packed { logic [9:0] min_v; } filter_ctl_t; (6.18, 7.2.1)
//     - the module declares exactly 1 Typedef, 'filter_ctl_t', whose alias
//       is a packed StructTypespec with exactly 1 member, min_v, a
//       LogicTypespec with the packed range [9:0]
//   filter_ctl_t [1:0] a = '{10'd15, 10'd0};
//     - a's type is a packed array (7.4.1) with the single packed dimension
//       [1:0] whose element type is filter_ctl_t
//     - its initializer is Operation vpiAssignmentPatternOp (10.9.1) with
//       exactly 2 items, in source order the Constants "10'd15" and "10'd0":
//       one item per element, as 10.9.1 requires
//   assign o = a[1]; (10.3)
//     - exactly 1 ContAssign; LHS bound to the variable o; RHS a bit-select
//       (11.5.1) whose prefix is bound to the variable a and whose index is
//       the Constant "1"
//   Elaboration (23.3.1)
//     - top appears in no instantiation, so on an elaborated design it is
//       the only top-level instance, named "top"
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: nothing here is a constant-expression context
// (a variable's initializer and the RHS of a continuous assignment are not),
// so only the instance tree is checked under getElaborated().
//
// What is NOT checked, and why:
//   - The values a and o hold (a[1] is 10'd15, so o is 15) only exist while
//     simulation runs. Permanently out of scope; the static half, which
//     declarations the assignment reads and writes, is covered by
//     ContAssignDrivesOWithElementOneOfA.
//   - How a's type refers to filter_ctl_t: the element RefTypespec may
//     resolve to the TypedefTypespec or to the StructTypespec it aliases.
//     Both are that type (6.18), so either is accepted.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
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

#include <string_view>

namespace hlc {

class PackedArrayAssignTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackedArrayAssign.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Variable *getVar(std::string_view name) {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, top->getVariables());
  }

  static const hldb::Typedef *getFilterCtlT() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("filter_ctl_t", top->getTypedefs());
  }

  // Verifies 'range' is [left:right] with Constant bounds.
  static void ExpectConstRange(const hldb::Range *range, std::string_view left, std::string_view right) {
    ASSERT_NE(range, nullptr);
    const hldb::Constant *const l = range->getLeftExpr<hldb::Constant>();
    const hldb::Constant *const r = range->getRightExpr<hldb::Constant>();
    ASSERT_NE(l, nullptr);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(l->getDecompile(), left);
    EXPECT_EQ(r->getDecompile(), right);
  }

  // Verifies 'type' is a LogicTypespec with the single packed range [9:0].
  static void ExpectLogic9To0(const hldb::RefTypespec *type, std::string_view what) {
    ASSERT_NE(type, nullptr) << what << " has no typespec";
    const hldb::LogicTypespec *const lt = type->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << what << " is declared 'logic [9:0]'";
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u) << what;
    ExpectConstRange(lt->getRanges()->at(0), "9", "0");
  }
};

// ---------------------------------------------------------------------------
// module top(output logic [9:0] o);
// ---------------------------------------------------------------------------

TEST_F(PackedArrayAssignTest, TopHasOneOutputVariablePortO) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 1u);
  const hldb::Port *const port = top->getPorts()->at(0);
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "o");
  EXPECT_EQ(port->getDirection(), vpiOutput);
  const hldb::Variable *const o = getVar("o");
  ASSERT_NE(o, nullptr) << "23.2.2.3: an output port whose data type uses the explicit data_type syntax is a variable";
  ExpectLogic9To0(o->getTypespec(), "'o'");
  const hldb::RefObj *const low = port->getLowConn<hldb::RefObj>();
  ASSERT_NE(low, nullptr);
  EXPECT_EQ(low->getActual(), o) << "37.14 detail 4: the port connects to its own variable";
}

TEST_F(PackedArrayAssignTest, TopHasVariablesOAndAOnly) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 2u) << "'o' and 'a'";
  EXPECT_NE(getVar("a"), nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty()) << "the module declares no net";
}

// ---------------------------------------------------------------------------
// typedef struct packed { logic [9:0] min_v; } filter_ctl_t;
// ---------------------------------------------------------------------------

TEST_F(PackedArrayAssignTest, FilterCtlTIsPackedStructWithMemberMinV) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypedefs(), nullptr);
  EXPECT_EQ(top->getTypedefs()->size(), 1u) << "'filter_ctl_t' is the module's only typedef";
  const hldb::Typedef *const td = getFilterCtlT();
  ASSERT_NE(td, nullptr) << "typedef 'filter_ctl_t' not found";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr) << "6.18: 'filter_ctl_t' names a structure type";
  ASSERT_NE(st->getStruct(), nullptr);
  EXPECT_TRUE(st->getStruct()->getPacked()) << "7.2.1: declared 'struct packed'";
  ASSERT_NE(st->getStruct()->getMembers(), nullptr);
  ASSERT_EQ(st->getStruct()->getMembers()->size(), 1u);
  const hldb::TypespecMember *const minV = st->getStruct()->getMembers()->at(0);
  ASSERT_NE(minV, nullptr);
  EXPECT_EQ(minV->getName(), "min_v");
  ExpectLogic9To0(minV->getTypespec(), "'min_v'");
}

// ---------------------------------------------------------------------------
// filter_ctl_t [1:0] a = '{10'd15, 10'd0};
// ---------------------------------------------------------------------------

TEST_F(PackedArrayAssignTest, AIsPackedArray1To0OfFilterCtlT) {
  const hldb::Variable *const a = getVar("a");
  ASSERT_NE(a, nullptr) << "variable 'a' not found";
  ASSERT_NE(a->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = a->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr) << "'filter_ctl_t [1:0]' is an array type";
  EXPECT_TRUE(at->getPacked()) << "7.4.1: '[1:0]' is written before the name, so it is packed";
  ExpectConstRange(at->getRange(), "1", "0");
  const hldb::RefTypespec *const elem = at->getElemTypespec();
  ASSERT_NE(elem, nullptr);
  const hldb::Typedef *const td = getFilterCtlT();
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::Typespec *const actual = elem->getActual();
  ASSERT_NE(actual, nullptr) << "the element type must resolve";
  const hldb::TypedefTypespec *const viaTypedef = any_cast<hldb::TypedefTypespec>(actual);
  EXPECT_TRUE(((viaTypedef != nullptr) && (viaTypedef->getTypedef() == td)) || (actual == td->getAlias()->getActual()))
      << "7.4.1: the element type is filter_ctl_t";
}

TEST_F(PackedArrayAssignTest, AIsInitializedWithTwoItemPattern) {
  const hldb::Variable *const a = getVar("a");
  ASSERT_NE(a, nullptr);
  const hldb::Operation *const pattern = a->getValue<hldb::Operation>();
  ASSERT_NE(pattern, nullptr) << "'{10'd15, 10'd0} is an Operation";
  EXPECT_EQ(pattern->getOpType(), vpiAssignmentPatternOp) << "10.9.1: an array assignment pattern";
  ASSERT_NE(pattern->getOperands(), nullptr);
  ASSERT_EQ(pattern->getOperands()->size(), 2u) << "10.9.1: one item per element of the [1:0] array";
  const char *const items[] = {"10'd15", "10'd0"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(pattern->getOperands()->at(i));
    ASSERT_NE(c, nullptr) << "item " << i;
    EXPECT_EQ(c->getDecompile(), items[i]) << "item " << i << ", in source order";
  }
}

// ---------------------------------------------------------------------------
// assign o = a[1];
// ---------------------------------------------------------------------------

TEST_F(PackedArrayAssignTest, ContAssignDrivesOWithElementOneOfA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "o");
  ASSERT_NE(getVar("o"), nullptr);
  EXPECT_EQ(lhs->getActual(), getVar("o"));
  const hldb::BitSelect *const sel = ca->getRhs<hldb::BitSelect>();
  ASSERT_NE(sel, nullptr) << "11.5.1: 'a[1]' selects one element of the packed array";
  const hldb::Constant *const index = sel->getIndex<hldb::Constant>();
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->getDecompile(), "1");
  const hldb::RefObj *const prefix = sel->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr) << "the select applies to 'a'";
  EXPECT_EQ(prefix->getName(), "a");
  ASSERT_NE(getVar("a"), nullptr);
  EXPECT_EQ(prefix->getActual(), getVar("a"));
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(PackedArrayAssignTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: 'top' appears in no instantiation";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getName(), "top");
  }
}

TEST_F(PackedArrayAssignTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
