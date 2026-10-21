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

// Validates the HLDB model built for tests/ParamInFunc/dut.sv:
//
//   `default_nettype none
//
//   module func_block_top(inp, out1, out2, out3);
//           function automatic [31:0] func3;
//                   localparam A = 31;
//                   parameter B = 1 - 0;
//                   input [31:0] inp;
//                   func3[A:B] = inp[A:B];
//           endfunction
//   endmodule
//
// The point of the fixture is a function that declares a localparam and a
// parameter among its block items (A.2.8) and uses both as the bounds of
// part-selects (11.5.1) on the function's implicit result variable and on
// its old-style input. The regression this file exists to catch is HLC
// failing to scope A and B to the function, so that the part-select bounds
// resolve to nothing or to a module-level name.
//
// The module itself is not legal. Its non-ANSI list of ports names inp, out1,
// out2 and out3, but the module body declares none of them, and 23.2.2.1
// requires: "Each port_identifier in a port_expression in the list of ports
// for the module declaration shall also be declared in the body of the
// module as one of the following port declarations: input, output, inout
// (bidirectional), ref, or as an interface port". The 'input [31:0] inp;'
// inside the function declares the function's argument, not the module
// port. In addition, 22.8: "When the `default_nettype is set to none, all
// nets shall be explicitly declared. If a net is not explicitly declared, an
// error is generated."
//
// What is checked, and why:
//   Module func_block_top
//     - its default net type is none (22.8, 37.10 vpiDefNetType)
//     - lists 4 ports, inp, out1, out2 and out3, in that order, with port
//       indexes 0 to 3 (37.14 detail 9)
//     - declares exactly 1 function, func3
//   Function func3
//     - is automatic (13.4.2)
//     - returns logic [31:0]: 13.4, "When the implicit syntax is used, the
//       return type is the same as if the implicit syntax had been
//       immediately preceded by the logic keyword"
//     - declares exactly 1 argument, inp, an input of logic [31:0] (13.4)
//     - declares A, a localparam with the value 31, and B, whose unreduced
//       value is Operation vpiSubOp over the Constants 1 and 0
//     - its body is one blocking assignment (10.4.1) whose LHS is the
//       part-select func3[A:B] of the implicit result variable (13.4.1) and
//       whose RHS is the part-select inp[A:B]; A and B are bound to the
//       function's own parameters and inp to its argument
//   Diagnostics
//     - each of the 4 undeclared ports is reported as an error (23.2.2.1,
//       22.8)
//     - there is no fatal and no syntax diagnostic: the source is
//       syntactically well formed
//
// Reduction and elaboration: none is checked. The design is illegal, so the
// standard gives no elaborated value for it, and func3 is never called.
//
// KNOWN COMPILER BUG (port index not set), not a defect in this test: HLC
// does not give ports their index in the order of the list of ports, although
// 37.14 detail 9 says "vpiPortIndex can be used to determine the port order.
// The first port has a port index of zero." PortIndexesFollowListOrder is
// expected to fail until HLC is fixed; it is intentionally not skipped or
// relaxed.
//
// KNOWN COMPILER BUG (undeclared non-ANSI ports unreported), not a defect in
// this test: HLC accepts the list of ports '(inp, out1, out2, out3)' with no
// port declaration in the module body and reports nothing, although 23.2.2.1
// requires each port_identifier to "also be declared in the body of the
// module" and 22.8 requires an error for an undeclared net under
// '`default_nettype none'. HLC records the four ports with no direction.
// UndeclaredPortsAreErrors is expected to fail until HLC is fixed; it is
// intentionally not skipped or relaxed.
//
// Model convention: HLC stores the formal argument 'inp' both as an IODecl and
// as a Variable of func3, and a reference to it may bind to either.
// RhsPrefixIsFunc3Argument accepts both; what it pins is that the reference
// resolves inside func3 and not to the module port of the same name.
//
// What is NOT checked, and why:
//   - Whether B is a local parameter. 6.20.1 makes 'parameter' a synonym
//     for 'localparam' only in a design element with a parameter port list,
//     in a class body, in a generate block, in a package and in the
//     compilation-unit scope, and says nothing of a function body, so the
//     standard does not decide it.
//   - The direction of the module ports. The source writes none, which is
//     the error above, so there is no value to assert.
//   - Which error identifier HLC uses for an undeclared port is a tool
//     convention, so any error-severity diagnostic naming the port is
//     accepted.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/part_select.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class ParamInFuncTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamInFunc.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Whether a diagnostic of error severity names 'symbol'. Which error
  // identifier HLC picks for it is a tool convention, so any is accepted.
  bool reportsErrorNaming(std::string_view symbol) {
    for (const ErrorDefinition::ErrorMap::value_type &entry : ErrorDefinition::getErrorInfoMap()) {
      if ((entry.second.m_severity == ErrorDefinition::ERROR) && (findError(entry.first, symbol) != nullptr)) {
        return true;
      }
    }
    return false;
  }

  static const hldb::Module *getTop() {
    return hldb::findByDefName<hldb::Module>("func_block_top", m_design->getAllModules());
  }

  static const hldb::Function *getFunc3() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("func3", top->getTaskFuncs());
  }

  static const hldb::Parameter *getParam(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, scope->getParameters());
  }

  // The ParamAssign in 'scope' whose LHS names 'name'.
  static const hldb::ParamAssign *getParamAssign(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr || scope->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *scope->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == name)) return pa;
      const hldb::Parameter *const p = pa->getLhs<hldb::Parameter>();
      if ((p != nullptr) && (p->getName() == name)) return pa;
    }
    return nullptr;
  }

  // The only statement of func3, looking through a Begin that holds just it.
  static const hldb::Assignment *getBodyAssignment() {
    const hldb::Function *const f = getFunc3();
    if (f == nullptr) return nullptr;
    const hldb::Any *stmt = f->getStmt();
    if (const hldb::Begin *const block = any_cast<hldb::Begin>(stmt)) {
      if (block->getStmts() == nullptr || block->getStmts()->size() != 1u) return nullptr;
      stmt = block->getStmts()->at(0);
    }
    return any_cast<hldb::Assignment>(stmt);
  }

  // Verifies 'rts' is logic [31:0].
  static void ExpectLogic31To0(const hldb::RefTypespec *rts, std::string_view what) {
    ASSERT_NE(rts, nullptr) << what << " has no type";
    const hldb::LogicTypespec *const lt = rts->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << "13.4: " << what << " is a logic";
    ASSERT_NE(lt->getRanges(), nullptr) << what;
    ASSERT_EQ(lt->getRanges()->size(), 1u) << what;
    const hldb::Constant *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
    ASSERT_NE(left, nullptr) << what;
    EXPECT_EQ(left->getDecompile(), "31") << what;
    const hldb::Constant *const right = lt->getRanges()->at(0)->getRightExpr<hldb::Constant>();
    ASSERT_NE(right, nullptr) << what;
    EXPECT_EQ(right->getDecompile(), "0") << what;
  }

  // Verifies 'sel' is '<prefix>[A:B]' with A and B bound to func3's own
  // parameters.
  static void ExpectPartSelectAToB(const hldb::PartSelect *sel, std::string_view prefix) {
    ASSERT_NE(sel, nullptr) << "'" << prefix << "[A:B]' is a part-select";
    const hldb::RefObj *const base = sel->getPrefix<hldb::RefObj>();
    ASSERT_NE(base, nullptr) << "the part-select is taken of a name";
    EXPECT_EQ(base->getName(), prefix);
    ASSERT_NE(sel->getRange(), nullptr);
    const hldb::RefObj *const a = sel->getRange()->getLeftExpr<hldb::RefObj>();
    ASSERT_NE(a, nullptr) << "the left bound is the name 'A'";
    EXPECT_EQ(a->getName(), "A");
    ASSERT_NE(getParam(getFunc3(), "A"), nullptr);
    EXPECT_EQ(a->getActual(), getParam(getFunc3(), "A")) << "'A' is func3's own localparam";
    const hldb::RefObj *const b = sel->getRange()->getRightExpr<hldb::RefObj>();
    ASSERT_NE(b, nullptr) << "the right bound is the name 'B'";
    EXPECT_EQ(b->getName(), "B");
    ASSERT_NE(getParam(getFunc3(), "B"), nullptr);
    EXPECT_EQ(b->getActual(), getParam(getFunc3(), "B")) << "'B' is func3's own parameter";
  }
};

// ---------------------------------------------------------------------------
// module func_block_top
// ---------------------------------------------------------------------------

TEST_F(ParamInFuncTest, DefaultNetTypeIsNone) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module 'func_block_top' not found";
  EXPECT_EQ(top->getDefNetType(), vpiNone) << "22.8: '`default_nettype none' precedes the module";
}

TEST_F(ParamInFuncTest, ListsFourPorts) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 4u) << "23.2.2.1: the list of ports names 4 ports";
  const std::string_view names[] = {"inp", "out1", "out2", "out3"};
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(top->getPorts()->at(i)->getName(), names[i]) << "port " << i;
  }
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (port index
// not set) in the file header.
TEST_F(ParamInFuncTest, PortIndexesFollowListOrder) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 4u);
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(top->getPorts()->at(i)->getPortIndex(), static_cast<int32_t>(i))
        << "37.14 detail 9: port '" << top->getPorts()->at(i)->getName() << "' is at position " << i;
  }
}

TEST_F(ParamInFuncTest, DeclaresOneFunctionFunc3) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  EXPECT_EQ(top->getTaskFuncs()->size(), 1u);
  EXPECT_NE(getFunc3(), nullptr) << "function 'func3' not found";
}

// ---------------------------------------------------------------------------
// function automatic [31:0] func3
// ---------------------------------------------------------------------------

TEST_F(ParamInFuncTest, Func3IsAutomatic) {
  const hldb::Function *const f = getFunc3();
  ASSERT_NE(f, nullptr);
  EXPECT_TRUE(f->getAutomatic()) << "13.4.2: func3 is declared 'automatic'";
}

TEST_F(ParamInFuncTest, Func3ReturnsLogic31To0) {
  const hldb::Function *const f = getFunc3();
  ASSERT_NE(f, nullptr);
  ExpectLogic31To0(f->getReturn(), "func3's return type");
}

TEST_F(ParamInFuncTest, Func3DeclaresInputInp) {
  const hldb::Function *const f = getFunc3();
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->getIODecls(), nullptr);
  ASSERT_EQ(f->getIODecls()->size(), 1u) << "func3 declares one argument";
  const hldb::IODecl *const inp = f->getIODecls()->at(0);
  EXPECT_EQ(inp->getName(), "inp");
  EXPECT_EQ(inp->getDirection(), vpiInput) << "13.4: 'input [31:0] inp;' is an input";
  ExpectLogic31To0(inp->getTypespec(), "inp");
}

TEST_F(ParamInFuncTest, Func3DeclaresLocalparamAOf31) {
  const hldb::Function *const f = getFunc3();
  ASSERT_NE(f, nullptr);
  const hldb::Parameter *const a = getParam(f, "A");
  ASSERT_NE(a, nullptr) << "func3's localparam 'A' not found";
  EXPECT_TRUE(a->getLocalParam()) << "6.20.4: 'A' is declared 'localparam'";
  const hldb::ParamAssign *const pa = getParamAssign(f, "A");
  ASSERT_NE(pa, nullptr);
  ASSERT_NE(pa->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(pa->getRhs<hldb::Constant>()->getDecompile(), "31");
}

TEST_F(ParamInFuncTest, Func3DeclaresParameterBOfOneMinusZero) {
  const hldb::Function *const f = getFunc3();
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->getParameters(), nullptr);
  EXPECT_EQ(f->getParameters()->size(), 2u) << "func3 declares A and B";
  ASSERT_NE(getParam(f, "B"), nullptr) << "func3's parameter 'B' not found";
  const hldb::ParamAssign *const pa = getParamAssign(f, "B");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const sub = pa->getRhs<hldb::Operation>();
  ASSERT_NE(sub, nullptr) << "unreduced, '1 - 0' is an Operation";
  EXPECT_EQ(sub->getOpType(), vpiSubOp) << "11.4.3: a subtraction";
  ASSERT_NE(sub->getOperands(), nullptr);
  ASSERT_EQ(sub->getOperands()->size(), 2u);
  const hldb::Constant *const one = any_cast<hldb::Constant>(sub->getOperands()->at(0));
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), "1");
  const hldb::Constant *const zero = any_cast<hldb::Constant>(sub->getOperands()->at(1));
  ASSERT_NE(zero, nullptr);
  EXPECT_EQ(zero->getDecompile(), "0");
}

TEST_F(ParamInFuncTest, BodyAssignsPartSelectOfInpToPartSelectOfFunc3) {
  const hldb::Assignment *const assign = getBodyAssignment();
  ASSERT_NE(assign, nullptr) << "func3's body is one assignment";
  EXPECT_TRUE(assign->getBlocking()) << "10.4.1: '=' is a blocking assignment";
  ExpectPartSelectAToB(assign->getLhs<hldb::PartSelect>(), "func3");
  ExpectPartSelectAToB(assign->getRhs<hldb::PartSelect>(), "inp");
}

TEST_F(ParamInFuncTest, RhsPrefixIsFunc3Argument) {
  const hldb::Assignment *const assign = getBodyAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::PartSelect *const sel = assign->getRhs<hldb::PartSelect>();
  ASSERT_NE(sel, nullptr);
  const hldb::RefObj *const prefix = sel->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr) << "the part-select is taken of the name 'inp'";
  EXPECT_EQ(prefix->getName(), "inp");
  const hldb::Function *const f = getFunc3();
  ASSERT_NE(f, nullptr);
  ASSERT_NE(f->getIODecls(), nullptr);
  ASSERT_EQ(f->getIODecls()->size(), 1u);
  // Model convention: HLC stores a formal argument both as an IODecl and as a
  // Variable of the function, and a reference may bind to either.
  const hldb::Variable *const argVar = hldb::findByName<hldb::Variable>("inp", f->getVariables());
  EXPECT_TRUE((prefix->getActual() == f->getIODecls()->at(0)) ||
              ((argVar != nullptr) && (prefix->getActual() == argVar)))
      << "13.4: 'inp' is func3's argument, not the module port";
  EXPECT_EQ(any_cast<hldb::Port>(prefix->getActual()), nullptr) << "'inp' does not resolve to the module port";
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (undeclared
// non-ANSI ports unreported) in the file header.
TEST_F(ParamInFuncTest, UndeclaredPortsAreErrors) {
  for (std::string_view port : {"inp", "out1", "out2", "out3"}) {
    EXPECT_TRUE(reportsErrorNaming(port))
        << "23.2.2.1: port '" << port
        << "' shall also be declared in the body of the module as an input, output, inout, "
           "ref or interface port; 22.8: with '`default_nettype none' an undeclared net is "
           "an error";
  }
}

TEST_F(ParamInFuncTest, NoFatalOrSyntaxDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "the file is syntactically well formed";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
