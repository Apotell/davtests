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

// Validates the HLDB model built for tests/PackedArrayStruct/dut.sv:
//
//   module top(output logic [31:0] o);
//      typedef struct packed {
//         logic x;
//      } [31:0] struct_array_t;
//      struct_array_t a = '1;
//      assign o = a;
//   endmodule
//
// The point of the fixture is a typedef whose data type is a packed
// structure followed directly by a packed dimension (IEEE 1800-2023 A.2.2.1:
// data_type ::= struct_union [ packed [ signing ] ] { ... } { packed_dimension }),
// so the typedef names a 32-element packed array of a 1-bit structure. The
// regression this file exists to catch is HLC dropping the packed dimension
// written after the structure body.
//
// What is checked, and why:
//   Module top (23.2)
//     - exactly 1 port, 'o', with direction output. 'output logic [31:0] o'
//       writes its data type with the explicit data_type syntax, so the port
//       is a variable (23.2.2.3): the module's Variable o, a LogicTypespec
//       with the single packed range [31:0], is the port's low connection
//     - exactly 2 variables, o and a, and no nets
//   typedef struct packed { logic x; } [31:0] struct_array_t; (6.18)
//     - the module declares exactly 1 Typedef, whose alias is a packed array
//       (7.4.1) with the single packed dimension [31:0] whose element type is
//       a packed StructTypespec (7.2.1) with exactly 1 member, 'x', a
//       LogicTypespec with no packed range
//   struct_array_t a = '1;
//     - a is typed by struct_array_t and initialized with the unbased
//       unsized literal '1 (5.7.1)
//   assign o = a; (10.3)
//     - exactly 1 ContAssign; LHS bound to the variable o, RHS bound to the
//       variable a
//   Elaboration (23.3.1)
//     - top appears in no instantiation, so on an elaborated design it is
//       the only top-level instance, named "top"
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: nothing here is a constant-expression context,
// so only the instance tree is checked under getElaborated().
//
// What is NOT checked, and why:
//   - The values a and o hold (all 32 bits 1) only exist while simulation
//     runs. Permanently out of scope.
//   - How a RefTypespec refers to struct_array_t: it may resolve to the
//     TypedefTypespec or to the ArrayTypespec it aliases. Both are that type
//     (6.18), so either is accepted.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/port.h>
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

class PackedArrayStructTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackedArrayStruct.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Variable *getVar(std::string_view name) {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, top->getVariables());
  }

  static const hldb::Typedef *getStructArrayT() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("struct_array_t", top->getTypedefs());
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
};

TEST_F(PackedArrayStructTest, TopHasOneOutputVariablePortO) {
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
  ASSERT_NE(o->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = o->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'o' is declared 'logic [31:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  ExpectConstRange(lt->getRanges()->at(0), "31", "0");
  const hldb::RefObj *const low = port->getLowConn<hldb::RefObj>();
  ASSERT_NE(low, nullptr);
  EXPECT_EQ(low->getActual(), o) << "37.14 detail 4: the port connects to its own variable";
}

TEST_F(PackedArrayStructTest, TopHasVariablesOAndAOnly) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 2u) << "'o' and 'a'";
  EXPECT_NE(getVar("a"), nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty()) << "the module declares no net";
}

TEST_F(PackedArrayStructTest, StructArrayTIsPackedArray31To0OfOneBitStruct) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypedefs(), nullptr);
  EXPECT_EQ(top->getTypedefs()->size(), 1u) << "'struct_array_t' is the module's only typedef";
  const hldb::Typedef *const td = getStructArrayT();
  ASSERT_NE(td, nullptr) << "typedef 'struct_array_t' not found";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::ArrayTypespec *const at = td->getAlias()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr) << "A.2.2.1: the packed dimension after the structure body makes it an array type";
  EXPECT_TRUE(at->getPacked()) << "7.4.1: '[31:0]' is a packed dimension";
  ExpectConstRange(at->getRange(), "31", "0");
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::StructTypespec *const st = at->getElemTypespec()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr) << "the element type is the structure";
  ASSERT_NE(st->getStruct(), nullptr);
  EXPECT_TRUE(st->getStruct()->getPacked()) << "7.2.1: declared 'struct packed'";
  ASSERT_NE(st->getStruct()->getMembers(), nullptr);
  ASSERT_EQ(st->getStruct()->getMembers()->size(), 1u);
  const hldb::TypespecMember *const x = st->getStruct()->getMembers()->at(0);
  ASSERT_NE(x, nullptr);
  EXPECT_EQ(x->getName(), "x");
  ASSERT_NE(x->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = x->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'x' is declared 'logic'";
  EXPECT_TRUE(lt->getRanges() == nullptr || lt->getRanges()->empty()) << "'logic x' has no packed dimension";
}

TEST_F(PackedArrayStructTest, AIsTypedByStructArrayTAndSetToAllOnes) {
  const hldb::Variable *const a = getVar("a");
  ASSERT_NE(a, nullptr) << "variable 'a' not found";
  const hldb::Typedef *const td = getStructArrayT();
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  ASSERT_NE(a->getTypespec(), nullptr);
  const hldb::Typespec *const actual = a->getTypespec()->getActual();
  ASSERT_NE(actual, nullptr) << "'a''s type must resolve";
  const hldb::TypedefTypespec *const viaTypedef = any_cast<hldb::TypedefTypespec>(actual);
  EXPECT_TRUE(((viaTypedef != nullptr) && (viaTypedef->getTypedef() == td)) || (actual == td->getAlias()->getActual()))
      << "6.18: 'a' is declared with the type struct_array_t";
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "'a' is initialized with a literal";
  EXPECT_EQ(init->getDecompile(), "'1") << "5.7.1: '1 is the unbased unsized literal that sets all bits to 1";
}

TEST_F(PackedArrayStructTest, ContAssignDrivesOWithA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "o");
  EXPECT_EQ(lhs->getActual(), getVar("o"));
  const hldb::RefObj *const rhs = ca->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "a");
  EXPECT_EQ(rhs->getActual(), getVar("a"));
}

TEST_F(PackedArrayStructTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: 'top' appears in no instantiation";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getName(), "top");
  }
}

TEST_F(PackedArrayStructTest, NoFatalSyntaxOrErrorDiagnostics) {
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
