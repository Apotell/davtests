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

// Validates the HLDB model built for tests/ParamFile/dut.sv:
//
//   `define PARAM_FILE ""
//
//   module dut (output logic o);
//      parameter SRAMInitFile = `PARAM_FILE;
//      ram_1p #(
//        .MemInitFile(SRAMInitFile)
//      ) u_ram (
//              .out(o)
//      );
//   endmodule
//
//   module ram_1p #(
//           parameter     MemInitFile = ""
//   ) (
//           output logic out
//   );
//
//   initial begin
//     if (MemInitFile != "") begin : gen_meminit
//         $display("Initializing memory %m from file '%s'.", MemInitFile);
//         assign out = 1'b1;
//     end
//
//     if (MemInitFile == "") begin : gen_nomeminit
//         $display("Initializing memory file is empty.");
//         assign out = 1'b0;
//     end
//   end
//
//   endmodule
//
// The point of the fixture is a string-valued parameter whose default comes
// from a text macro (22.5.1) and is passed down by an override, then tested
// in two 'if' statements. The 'if' statements sit inside an initial
// procedure (9.2.1), so they are procedural conditional statements (12.4),
// not conditional generate constructs (27.5), even though their named blocks
// are called gen_meminit and gen_nomeminit; each block holds a system task
// call and a procedural continuous assignment (10.6.1) to the variable out.
// The regression this file exists to catch is HLC losing the empty-string
// value on its way through the macro and the override, or treating the
// procedural 'if' statements as generate constructs.
//
// Values: `PARAM_FILE expands to "", so SRAMInitFile = "" and u_ram's
// MemInitFile = "".
//
// What is checked, and why:
//   Module dut
//     - exactly 1 port, o, an output variable of logic (23.2.2.3: an output
//       declared with an explicit data type defaults to a variable)
//     - exactly 1 parameter, SRAMInitFile, overridable (dut has no parameter
//       port list, 6.20.1), whose value is the string literal "" that the
//       macro expands to (5.9, 22.5.1)
//     - u_ram instantiates ram_1p, overrides MemInitFile with the RefObj
//       SRAMInitFile bound to dut's Parameter, and connects out to o
//   Module ram_1p
//     - exactly 1 parameter, MemInitFile, overridable (6.20.4), defaulting
//       to the string literal ""
//     - exactly 1 port, out, an output variable of logic (23.2.2.3)
//     - no generate construct
//     - exactly 1 process, an initial procedure whose body is a begin-end
//       block of 2 'if' statements (12.4)
//     - the first tests 'MemInitFile != ""' (11.4.5) and its block
//       gen_meminit holds $display with 2 arguments and the procedural
//       continuous assignment 'assign out = 1'b1;' (10.6.1)
//     - the second tests 'MemInitFile == ""' and its block gen_nomeminit
//       holds $display with 1 argument and 'assign out = 1'b0;'
//     - MemInitFile and out in those statements are bound to ram_1p's
//       declarations
//   Elaboration
//     - u_ram's MemInitFile is the empty string
//     - the elaborated u_ram has no generate scope
//     - dut is the only top-level instance (23.3.1)
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the elaborated value is checked only when the
// design is elaborated, on Design::getTopModules(); the declarations,
// statements and bindings are asserted on the definitions in every run.
//
// What is NOT checked, and why:
//   - Which branch runs, the text $display would print, and the value out
//     takes. They exist only when the design runs; HLC is a static
//     compiler/elaborator and is not a simulator, so this is out of scope
//     permanently rather than a gap waiting to be filled.
//     IfStatementsTestMemInitFileAgainstEmptyString and
//     ElaboratedMemInitFileIsEmptyString are the static evidence of the
//     conditions and of the value they compare.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assign_stmt.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/if_stmt.h>
#include <hldb/initial.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/named_argument.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class ParamFileTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamFile.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
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

  // The variable the lowConn of module 'name''s only port is bound to.
  static const hldb::Variable *getPortVariable(std::string_view name) {
    const hldb::Module *const m = getModule(name);
    if (m == nullptr || m->getPorts() == nullptr || m->getPorts()->size() != 1u) return nullptr;
    const hldb::RefObj *const low = m->getPorts()->at(0)->getLowConn<hldb::RefObj>();
    if (low == nullptr) return nullptr;
    return low->getActual<hldb::Variable>();
  }

  static const hldb::RefInstance *getURam() {
    const hldb::Module *const dut = getModule("dut");
    if (dut == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>("u_ram", dut->getRefInstances());
  }

  // The 'index'-th 'if' statement of ram_1p's initial procedure.
  static const hldb::IfStmt *getIf(size_t index) {
    const hldb::Module *const ram = getModule("ram_1p");
    if (ram == nullptr || ram->getProcesses() == nullptr || ram->getProcesses()->size() != 1u) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(ram->getProcesses()->at(0));
    if (init == nullptr) return nullptr;
    const hldb::Begin *const body = init->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->size() <= index) return nullptr;
    return any_cast<hldb::IfStmt>(body->getStmts()->at(index));
  }

  // Verifies 'c' is the empty string literal.
  static void ExpectEmptyString(const hldb::Constant *c, std::string_view what) {
    ASSERT_NE(c, nullptr) << what << " is a literal";
    EXPECT_EQ(c->getConstType(), vpiStringConst) << "5.9: " << what << " is a string literal";
    EXPECT_EQ(c->getDecompile(), "\"\"") << what << " is the empty string";
  }

  // Verifies module 'name' has exactly 1 port, 'port', an output logic
  // variable.
  static void ExpectOutputLogicVariablePort(std::string_view name, std::string_view port) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    ASSERT_NE(m->getPorts(), nullptr);
    ASSERT_EQ(m->getPorts()->size(), 1u) << name << " declares one port";
    EXPECT_EQ(m->getPorts()->at(0)->getName(), port);
    EXPECT_EQ(m->getPorts()->at(0)->getDirection(), vpiOutput) << "'output logic " << port << "'";
    const hldb::Variable *const v = getPortVariable(name);
    ASSERT_NE(v, nullptr) << "23.2.2.3: an output with an explicit data type defaults to a variable";
    ASSERT_NE(v->getTypespec(), nullptr);
    EXPECT_NE(v->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr) << "'" << port << "' is a logic";
  }

  // Verifies the block of 'ifStmt' is named 'name' and holds $display with
  // 'nargs' arguments followed by 'assign out = <value>;'.
  static void ExpectBlock(const hldb::IfStmt *ifStmt, std::string_view name, size_t nargs, std::string_view value) {
    ASSERT_NE(ifStmt, nullptr);
    const hldb::Begin *const block = ifStmt->getStmt<hldb::Begin>();
    ASSERT_NE(block, nullptr) << "9.3.1: the 'if' writes a begin-end block";
    EXPECT_EQ(block->getName(), name) << "9.3.4: the block is named";
    ASSERT_NE(block->getStmts(), nullptr);
    ASSERT_EQ(block->getStmts()->size(), 2u) << name << " holds 2 statements";
    const hldb::SysTaskCall *const display = any_cast<hldb::SysTaskCall>(block->getStmts()->at(0));
    ASSERT_NE(display, nullptr) << "the first statement is a system task call";
    EXPECT_EQ(display->getName(), "$display");
    ASSERT_NE(display->getArguments(), nullptr);
    EXPECT_EQ(display->getArguments()->size(), nargs);
    const hldb::AssignStmt *const assign = any_cast<hldb::AssignStmt>(block->getStmts()->at(1));
    ASSERT_NE(assign, nullptr) << "10.6.1: 'assign out = ...;' is a procedural continuous assignment";
    const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), "out");
    EXPECT_EQ(lhs->getActual(), getPortVariable("ram_1p")) << "10.6.1: the LHS is the variable 'out'";
    const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(rhs->getDecompile(), value);
  }
};

// ---------------------------------------------------------------------------
// module dut
// ---------------------------------------------------------------------------

TEST_F(ParamFileTest, DutPortOIsOutputLogicVariable) { ExpectOutputLogicVariablePort("dut", "o"); }

TEST_F(ParamFileTest, SramInitFileIsMacroExpandedEmptyString) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getParameters(), nullptr);
  EXPECT_EQ(dut->getParameters()->size(), 1u);
  const hldb::Parameter *const p = getParam(dut, "SRAMInitFile");
  ASSERT_NE(p, nullptr) << "parameter 'SRAMInitFile' not found";
  EXPECT_FALSE(p->getLocalParam()) << "6.20.1: dut has no parameter port list, so 'SRAMInitFile' is overridable";
  const hldb::ParamAssign *const pa = getParamAssign(dut, "SRAMInitFile");
  ASSERT_NE(pa, nullptr);
  ExpectEmptyString(pa->getRhs<hldb::Constant>(), "22.5.1: the expansion of `PARAM_FILE");
}

TEST_F(ParamFileTest, URamOverridesMemInitFileWithSramInitFile) {
  const hldb::RefInstance *const inst = getURam();
  ASSERT_NE(inst, nullptr) << "instance 'u_ram' not found";
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getModule(), getModule("ram_1p"));
  ASSERT_NE(mt->getParamAssigns(), nullptr);
  ASSERT_EQ(mt->getParamAssigns()->size(), 1u);
  const hldb::RefObj *const lhs = mt->getParamAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "MemInitFile");
  const hldb::RefObj *const rhs = mt->getParamAssigns()->at(0)->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "the override value is the name 'SRAMInitFile'";
  EXPECT_EQ(rhs->getName(), "SRAMInitFile");
  ASSERT_NE(getParam(getModule("dut"), "SRAMInitFile"), nullptr);
  EXPECT_EQ(rhs->getActual(), getParam(getModule("dut"), "SRAMInitFile")) << "dut's own parameter";
}

TEST_F(ParamFileTest, URamConnectsOutToO) {
  const hldb::RefInstance *const inst = getURam();
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getArguments(), nullptr);
  ASSERT_EQ(inst->getArguments()->size(), 1u) << "'.out(o)'";
  const hldb::NamedArgument *const conn = inst->getArguments()->at(0);
  const hldb::RefObj *const formal = conn->getLowConn<hldb::RefObj>();
  ASSERT_NE(formal, nullptr);
  EXPECT_EQ(formal->getName(), "out");
  const hldb::RefObj *const actual = conn->getHighConn<hldb::RefObj>();
  ASSERT_NE(actual, nullptr);
  EXPECT_EQ(actual->getName(), "o");
  EXPECT_EQ(actual->getActual(), getPortVariable("dut")) << "'o' is dut's output variable";
}

// ---------------------------------------------------------------------------
// module ram_1p
// ---------------------------------------------------------------------------

TEST_F(ParamFileTest, RamDeclaresMemInitFileDefaultingToEmptyString) {
  const hldb::Module *const ram = getModule("ram_1p");
  ASSERT_NE(ram, nullptr) << "module 'ram_1p' not found";
  ASSERT_NE(ram->getParameters(), nullptr);
  EXPECT_EQ(ram->getParameters()->size(), 1u);
  const hldb::Parameter *const p = getParam(ram, "MemInitFile");
  ASSERT_NE(p, nullptr) << "parameter 'MemInitFile' not found";
  EXPECT_FALSE(p->getLocalParam()) << "6.20.4: 'MemInitFile' is a parameter port, not a localparam";
  const hldb::ParamAssign *const pa = getParamAssign(ram, "MemInitFile");
  ASSERT_NE(pa, nullptr);
  ExpectEmptyString(pa->getRhs<hldb::Constant>(), "the default of MemInitFile");
}

TEST_F(ParamFileTest, RamPortOutIsOutputLogicVariable) { ExpectOutputLogicVariablePort("ram_1p", "out"); }

TEST_F(ParamFileTest, RamHasNoGenerateConstruct) {
  const hldb::Module *const ram = getModule("ram_1p");
  ASSERT_NE(ram, nullptr);
  EXPECT_TRUE(ram->getGenStmts() == nullptr || ram->getGenStmts()->empty())
      << "12.4: an 'if' inside an initial procedure is a procedural statement, not a generate construct";
}

TEST_F(ParamFileTest, RamInitialHoldsTwoIfStatements) {
  const hldb::Module *const ram = getModule("ram_1p");
  ASSERT_NE(ram, nullptr);
  ASSERT_NE(ram->getProcesses(), nullptr);
  ASSERT_EQ(ram->getProcesses()->size(), 1u) << "ram_1p has one procedure";
  const hldb::Initial *const init = any_cast<hldb::Initial>(ram->getProcesses()->at(0));
  ASSERT_NE(init, nullptr) << "9.2.1: the procedure is an initial procedure";
  const hldb::Begin *const body = init->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "the body is a begin-end block";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u);
  EXPECT_NE(getIf(0), nullptr) << "12.4: the first statement is an 'if'";
  EXPECT_NE(getIf(1), nullptr) << "12.4: the second statement is an 'if'";
}

TEST_F(ParamFileTest, IfStatementsTestMemInitFileAgainstEmptyString) {
  const int ops[] = {vpiNeqOp, vpiEqOp};
  for (size_t i = 0; i < 2; ++i) {
    SCOPED_TRACE(testing::Message() << "if statement " << i);
    const hldb::IfStmt *const ifStmt = getIf(i);
    ASSERT_NE(ifStmt, nullptr);
    const hldb::Operation *const cond = ifStmt->getCondition<hldb::Operation>();
    ASSERT_NE(cond, nullptr);
    EXPECT_EQ(cond->getOpType(), ops[i]) << "11.4.5";
    ASSERT_NE(cond->getOperands(), nullptr);
    ASSERT_EQ(cond->getOperands()->size(), 2u);
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
    ASSERT_NE(ref, nullptr);
    EXPECT_EQ(ref->getName(), "MemInitFile");
    ASSERT_NE(getParam(getModule("ram_1p"), "MemInitFile"), nullptr);
    EXPECT_EQ(ref->getActual(), getParam(getModule("ram_1p"), "MemInitFile")) << "ram_1p's own parameter";
    ExpectEmptyString(any_cast<hldb::Constant>(cond->getOperands()->at(1)), "the compared value");
  }
}

TEST_F(ParamFileTest, MemInitBlockDisplaysAndAssignsOne) { ExpectBlock(getIf(0), "gen_meminit", 2u, "1'b1"); }

TEST_F(ParamFileTest, NoMemInitBlockDisplaysAndAssignsZero) { ExpectBlock(getIf(1), "gen_nomeminit", 1u, "1'b0"); }

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(ParamFileTest, ElaboratedMemInitFileIsEmptyString) {
  if (m_design->getElaborated()) {
    const hldb::Module *const dut = hldb::findByDefName<hldb::Module>("dut", m_design->getTopModules());
    ASSERT_NE(dut, nullptr) << "the elaborated dut not found";
    const hldb::Module *const ram = hldb::findByName<hldb::Module>("u_ram", dut->getModules());
    ASSERT_NE(ram, nullptr) << "the instance u_ram is elaborated";
    const hldb::ParamAssign *const pa = getParamAssign(ram, "MemInitFile");
    ASSERT_NE(pa, nullptr);
    ExpectEmptyString(pa->getRhs<hldb::Constant>(), "23.10.2.2: u_ram's MemInitFile = SRAMInitFile");
    EXPECT_TRUE(ram->getGenScopeArrays() == nullptr || ram->getGenScopeArrays()->empty())
        << "12.4: the procedural 'if' statements create no generate scope";
  }
}

TEST_F(ParamFileTest, DutIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: ram_1p is instantiated by dut";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "dut");
  }
}

TEST_F(ParamFileTest, NoFatalSyntaxOrErrorDiagnostics) {
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
