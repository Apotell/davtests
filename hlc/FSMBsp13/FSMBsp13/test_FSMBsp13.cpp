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

// Tests for tests/FSMBsp13/top.v (FSMBsp13.hlc compiles top.v together with
// fsm1.v, fsm2.v and fsm3.v so that top's instantiations resolve).
//
//   module top;
//     reg fsm1clk, fsm2clk, fsm3clk, fsm1rst, SlowRam, fsm2rst;
//     reg ctrl;
//     reg keys, brake, a, b, c, accelerate, m, n, o, p, q, r, s, t, u, v;
//     wire rd, wr;
//     wire [2:0] Fsm2Out;
//     wire [3:0] speed;
//
//     FSM1 F1(.Clk(fsm1clk), .Reset(fsm1rst), .SlowRam(SlowRam),
//             .Read(rd), .Write(wr));
//     FSM2 F2(.clock(fsm2clk), .reset(fsm2rst), .control(ctrl), .y(Fsm2Out));
//     FSM3 F3(.clock(fsm3clk), .keys(keys), .brake(brake),
//             .accelerate(accelerate), .Speed(speed));
//
//     initial begin fsm1clk = 0; forever #20 fsm1clk = ~fsm1clk; end
//     initial begin a = 0; ... v = 0; fork forever begin ... end
//                                          forever begin ... end join end
//     initial begin #1; fsm2clk = 0; forever #30 fsm2clk = ~fsm2clk; end
//     ... (10 initial blocks in total)
//     initial begin $vtDumpvars(1,top.F2); #3000000 $finish; end
//     ...
//   endmodule
//
// Object model used here (unelaborated design):
//   - Module DEFINITIONS (top, FSM1, FSM2, FSM3) are hldb::Module objects in
//     Design::getAllModules(), looked up by getDefName().
//   - Module INSTANTIATIONS inside a definition (F1, F2, F3 inside top) are
//     hldb::RefInstance objects in Module::getRefInstances(). A RefInstance
//     names the instance (getName()) and refers to the instantiated
//     definition through getTypespec() -> RefTypespec -> ModuleTypespec ->
//     getModule(). Its port connections are hldb::Port objects in
//     RefInstance::getPorts() (highConn = actual expression in top).
//
// What is checked (IEEE 1800-2023 citations):
//   - 23.2/23.3.1: all four definitions exist; "top" is the only top-level
//     module (FSM1/FSM2/FSM3 are instantiated, so they are not top-level);
//     top has no ports (empty list_of_ports).
//   - 6.8 / 6.7: the 23 "reg" declarations are variables (not nets) and the
//     4 "wire" declarations are nets of type wire; Fsm2Out/speed carry their
//     [2:0]/[3:0] packed ranges.
//   - 23.3.2: top holds exactly 3 RefInstances F1/F2/F3, each resolving to the
//     FSM1/FSM2/FSM3 definition Module (the very object in getAllModules()).
//   - 23.3.2.2: every connection is a named port connection; each port's
//     name is the formal port name, its direction matches the formal
//     declaration, and its highConn is a RefObj bound to the top-level
//     variable/net being connected.
//   - 9.2.1: top has exactly 10 processes, all "initial" (no always).
//   - 9.4.1 / 12.7.2: clock generator "fsm1clk = 0; forever #20 fsm1clk =
//     ~fsm1clk;" -- Forever wrapping a DelayControl wrapping a blocking
//     Assignment whose rhs is a vpiBitNegOp Operation.
//   - 9.4.1: "#1;" is a delay control with a null statement.
//   - 9.3.2: "fork ... join" with two forever branches, join type vpiJoin.
//   - 11.4.8: "a = m | n | o" -- binary bitwise OR is left-associative, so
//     the rhs is ((m | n) | o).
//   - 20.1 / 21: system task calls "$vtDumpvars(1, top.F2)" and the delayed
//     "$finish".
//
// What is NOT checked and why:
//   - Internals of FSM1/FSM2/FSM3 -- this file targets top.v; the FSM modules
//     are only compiled so top's instantiations resolve.
//   - Every individual initial block in depth -- one representative of each
//     shape (clock generator, delayed clock generator, fork/join stimulus,
//     system task calls) is walked; the remaining blocks repeat those shapes.
//   - Runtime simulation behavior -- HLC is an elaborator, not a simulator.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/delay_control.h>
#include <hldb/design.h>
#include <hldb/forever_stmt.h>
#include <hldb/fork_stmt.h>
#include <hldb/initial.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/port.h>
#include <hldb/process_stmt.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <tuple>

namespace hldb {
template <>
inline Port *findByName<Port, Port>(std::string_view name, const PortCollection *collection) {
  if (collection == nullptr) return nullptr;
  for (Port *p : *collection) {
    if (Any *any = p->getLowConn()) {
      if (name == any->getName()) {
        return p;
      }
    }
  }
  return nullptr;
}
}

namespace hlc {

class FSMBsp13Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FSMBsp13.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Module definition lookup (Design::getAllModules() holds definitions).
  static const hldb::Module *getDefinition(std::string_view defName) {
    return hldb::findByDefName<hldb::Module>(defName, m_design->getAllModules());
  }

  static const hldb::Module *getTop() { return getDefinition("top"); }

  // Instantiation lookup inside top (RefInstance, not Module).
  static const hldb::RefInstance *getInstance(std::string_view instName) {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>(instName, top->getRefInstances());
  }

  // The definition Module a RefInstance instantiates.
  static const hldb::Module *getInstantiatedDefinition(const hldb::RefInstance *inst) {
    if ((inst == nullptr) || (inst->getTypespec() == nullptr)) return nullptr;
    const hldb::ModuleTypespec *const mts = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
    if (mts == nullptr) return nullptr;
    return mts->getModule();
  }

  // Body (Begin) of top's index-th initial block, in source order.
  static const hldb::Begin *getInitialBody(size_t index) {
    const hldb::Module *const top = getTop();
    if ((top == nullptr) || (top->getProcesses() == nullptr) || (index >= top->getProcesses()->size())) {
      return nullptr;
    }
    const hldb::Initial *const initial = any_cast<hldb::Initial>(top->getProcesses()->at(index));
    if (initial == nullptr) return nullptr;
    return initial->getStmt<hldb::Begin>();
  }

  // Checks "<lhs> = <constant>;" (blocking, no intra-assignment delay).
  static void expectBlockingConstAssign(const hldb::Any *stmt, std::string_view lhs, std::string_view value) {
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(stmt);
    ASSERT_NE(assign, nullptr) << lhs << " = " << value;
    EXPECT_TRUE(assign->getBlocking()) << lhs;
    EXPECT_EQ(assign->getDelayControl(), nullptr) << lhs;
    const hldb::RefObj *const ref = assign->getLhs<hldb::RefObj>();
    ASSERT_NE(ref, nullptr) << lhs;
    EXPECT_EQ(ref->getName(), lhs);
    const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr) << lhs;
    EXPECT_EQ(rhs->getDecompile(), value) << lhs;
  }

  // Checks "#<delay> <name> = ~<name>;" (a toggle under a delay control).
  static void expectDelayedToggle(const hldb::Any *stmt, std::string_view delay, std::string_view name) {
    const hldb::DelayControl *const dc = any_cast<hldb::DelayControl>(stmt);
    ASSERT_NE(dc, nullptr) << "#" << delay << " " << name << " = ~" << name;
    const hldb::Constant *const amount = dc->getDelay<hldb::Constant>();
    ASSERT_NE(amount, nullptr) << name;
    EXPECT_EQ(amount->getDecompile(), delay) << name;

    const hldb::Assignment *const assign = dc->getStmt<hldb::Assignment>();
    ASSERT_NE(assign, nullptr) << name;
    EXPECT_TRUE(assign->getBlocking()) << name;
    const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr) << name;
    EXPECT_EQ(lhs->getName(), name);

    const hldb::Operation *const neg = assign->getRhs<hldb::Operation>();
    ASSERT_NE(neg, nullptr) << name;
    EXPECT_EQ(neg->getOpType(), vpiBitNegOp) << name;
    ASSERT_NE(neg->getOperands(), nullptr) << name;
    ASSERT_EQ(neg->getOperands()->size(), 1u) << name;
    const hldb::RefObj *const operand = any_cast<hldb::RefObj>(neg->getOperands()->at(0));
    ASSERT_NE(operand, nullptr) << name;
    EXPECT_EQ(operand->getName(), name);
  }

  // Checks one named port connection ".<formal>(<actual>)" on an instance.
  static void expectNamedConnection(const hldb::RefInstance *inst, std::string_view formal, int32_t direction,
                                    std::string_view actual, const hldb::Any *actualDecl) {
    ASSERT_NE(inst, nullptr);
    const hldb::NamedArgument *const arg = hldb::findByName<hldb::NamedArgument>(formal, inst->getArguments());
    ASSERT_NE(arg, nullptr) << inst->getName() << "." << formal;
    EXPECT_TRUE(arg->getConnByName()) << inst->getName() << "." << formal;

    ASSERT_NE(arg->getLowConn(), nullptr) << inst->getName() << "." << formal;
    const hldb::RefObj *const low = arg->getLowConn<hldb::RefObj>();
    ASSERT_NE(low, nullptr)  << inst->getName() << "." << formal;
    EXPECT_EQ(low->getName(), formal);
    const hldb::Port *const port = low->getActual<hldb::Port>();
    ASSERT_NE(port, nullptr);
    EXPECT_EQ(port->getName(), formal);
    EXPECT_EQ(port->getDirection(), direction) << inst->getName() << "." << formal;

    ASSERT_NE(arg->getHighConn(), nullptr) << inst->getName() << "." << formal;
    const hldb::RefObj *const high = arg->getHighConn<hldb::RefObj>();
    ASSERT_NE(high, nullptr) << inst->getName() << "." << formal;
    EXPECT_EQ(high->getName(), actual) << inst->getName() << "." << formal;
    ASSERT_NE(actualDecl, nullptr) << actual;
    EXPECT_EQ(high->getActual(), actualDecl) << inst->getName() << "." << formal << " -> " << actual;
  }
};

// --- definitions ------------------------------------------------------------

TEST_F(FSMBsp13Test, AllFourModuleDefinitionsExist) {
  for (std::string_view defName : {"top", "FSM1", "FSM2", "FSM3"}) {
    EXPECT_NE(getDefinition(defName), nullptr) << defName;
  }
}

TEST_F(FSMBsp13Test, OnlyTopIsATopLevelModule) {
  // 23.3.1: modules that are not instantiated anywhere are top-level.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  if (m_design->getElaborated()) {
    EXPECT_TRUE(top->getTopModule());
  }
  for (std::string_view defName : {"FSM1", "FSM2", "FSM3"}) {
    const hldb::Module *const def = getDefinition(defName);
    ASSERT_NE(def, nullptr) << defName;
    EXPECT_FALSE(def->getTopModule()) << defName << " is instantiated by top";
  }
}

TEST_F(FSMBsp13Test, TopHasNoPorts) {
  // "module top;" -- no list_of_ports.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getPorts(), nullptr);
}

// --- top: declarations ------------------------------------------------------

TEST_F(FSMBsp13Test, TopRegsAreLogicVariables) {
  // 6.8: "reg" declares a variable of 4-state type logic, never a net.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 23u);

  for (std::string_view name :
       {"fsm1clk",    "fsm2clk", "fsm3clk", "fsm1rst", "SlowRam", "fsm2rst", "ctrl", "keys", "brake", "a", "b", "c",
        "accelerate", "m",       "n",       "o",       "p",       "q",       "r",    "s",    "t",     "u", "v"}) {
    const hldb::Variable *const var = hldb::findByName<hldb::Variable>(name, top->getVariables());
    ASSERT_NE(var, nullptr) << name;
    EXPECT_EQ(hldb::findByName<hldb::Net>(name, top->getNets()), nullptr) << name << " must not also be a net";
    ASSERT_NE(var->getTypespec(), nullptr) << name;
    const hldb::LogicTypespec *const ts = var->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(ts, nullptr) << name;
    EXPECT_EQ(ts->getRanges(), nullptr) << name << " is a 1-bit scalar";
  }
}

TEST_F(FSMBsp13Test, TopWiresAreWireNets) {
  // 6.7: "wire" declares a net of net type wire.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 4u);

  for (std::string_view name : {"rd", "wr", "Fsm2Out", "speed"}) {
    const hldb::Net *const net = hldb::findByName<hldb::Net>(name, top->getNets());
    ASSERT_NE(net, nullptr) << name;
    EXPECT_EQ(net->getNetType(), vpiWire) << name;
    EXPECT_FALSE(net->getImplicitDecl()) << name << " is explicitly declared";
    EXPECT_EQ(hldb::findByName<hldb::Variable>(name, top->getVariables()), nullptr)
        << name << " must not also be a variable";
  }
}

TEST_F(FSMBsp13Test, TopVectorWiresCarryPackedRanges) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);

  const std::tuple<std::string_view, std::string_view, std::string_view> expected[2] = {
      {"Fsm2Out", "2", "0"},
      {"speed", "3", "0"},
  };
  for (const std::tuple<std::string_view, std::string_view, std::string_view> &entry : expected) {
    const std::string_view name = std::get<0>(entry);
    const hldb::Net *const net = hldb::findByName<hldb::Net>(name, top->getNets());
    ASSERT_NE(net, nullptr) << name;
    ASSERT_NE(net->getTypespec(), nullptr) << name;
    const hldb::LogicTypespec *const ts = net->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(ts, nullptr) << name;
    ASSERT_NE(ts->getRanges(), nullptr) << name;
    ASSERT_EQ(ts->getRanges()->size(), 1u) << name;
    const hldb::Range *const range = ts->getRanges()->at(0);
    const hldb::Constant *const left = range->getLeftExpr<hldb::Constant>();
    ASSERT_NE(left, nullptr) << name;
    EXPECT_EQ(left->getDecompile(), std::get<1>(entry)) << name;
    const hldb::Constant *const right = range->getRightExpr<hldb::Constant>();
    ASSERT_NE(right, nullptr) << name;
    EXPECT_EQ(right->getDecompile(), std::get<2>(entry)) << name;
  }
}

// --- top: instantiations ----------------------------------------------------

TEST_F(FSMBsp13Test, TopHasThreeRefInstancesBoundToDefinitions) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  EXPECT_EQ(top->getRefInstances()->size(), 3u);

  const std::pair<std::string_view, std::string_view> expected[3] = {
      {"F1", "FSM1"},
      {"F2", "FSM2"},
      {"F3", "FSM3"},
  };
  for (const std::pair<std::string_view, std::string_view> &entry : expected) {
    const hldb::RefInstance *const inst = getInstance(entry.first);
    ASSERT_NE(inst, nullptr) << entry.first;
    ASSERT_NE(inst->getTypespec(), nullptr) << entry.first;
    const hldb::ModuleTypespec *const mts = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
    ASSERT_NE(mts, nullptr) << entry.first << ": typespec is not a ModuleTypespec";

    const hldb::Module *const def = getDefinition(entry.second);
    ASSERT_NE(def, nullptr) << entry.second;
    EXPECT_EQ(mts->getModule(), def) << entry.first << " must resolve to the " << entry.second << " definition";
    EXPECT_EQ(inst->getDelay(), nullptr) << entry.first << " has no instance delay";
  }
}

TEST_F(FSMBsp13Test, InstantiatedDefinitionsDoNotInstantiateAnything) {
  // fsm1.v/fsm2.v/fsm3.v contain no module instantiations.
  for (std::string_view instName : {"F1", "F2", "F3"}) {
    const hldb::Module *const def = getInstantiatedDefinition(getInstance(instName));
    ASSERT_NE(def, nullptr) << instName;
    EXPECT_EQ(def->getRefInstances(), nullptr) << instName;
  }
}

TEST_F(FSMBsp13Test, F1NamedPortConnections) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const f1 = getInstance("F1");
  ASSERT_NE(f1, nullptr);
  ASSERT_NE(f1->getArguments(), nullptr);
  EXPECT_EQ(f1->getArguments()->size(), 5u);

  expectNamedConnection(f1, "Clk", vpiInput, "fsm1clk",
                        hldb::findByName<hldb::Variable>("fsm1clk", top->getVariables()));
  expectNamedConnection(f1, "Reset", vpiInput, "fsm1rst",
                        hldb::findByName<hldb::Variable>("fsm1rst", top->getVariables()));
  expectNamedConnection(f1, "SlowRam", vpiInput, "SlowRam",
                        hldb::findByName<hldb::Variable>("SlowRam", top->getVariables()));
  expectNamedConnection(f1, "Read", vpiOutput, "rd", hldb::findByName<hldb::Net>("rd", top->getNets()));
  expectNamedConnection(f1, "Write", vpiOutput, "wr", hldb::findByName<hldb::Net>("wr", top->getNets()));
}

TEST_F(FSMBsp13Test, F2NamedPortConnections) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const f2 = getInstance("F2");
  ASSERT_NE(f2, nullptr);
  ASSERT_NE(f2->getArguments(), nullptr);
  EXPECT_EQ(f2->getArguments()->size(), 4u);

  expectNamedConnection(f2, "clock", vpiInput, "fsm2clk",
                        hldb::findByName<hldb::Variable>("fsm2clk", top->getVariables()));
  expectNamedConnection(f2, "reset", vpiInput, "fsm2rst",
                        hldb::findByName<hldb::Variable>("fsm2rst", top->getVariables()));
  expectNamedConnection(f2, "control", vpiInput, "ctrl", hldb::findByName<hldb::Variable>("ctrl", top->getVariables()));
  expectNamedConnection(f2, "y", vpiOutput, "Fsm2Out", hldb::findByName<hldb::Net>("Fsm2Out", top->getNets()));
}

TEST_F(FSMBsp13Test, F3NamedPortConnections) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const f3 = getInstance("F3");
  ASSERT_NE(f3, nullptr);
  ASSERT_NE(f3->getArguments(), nullptr);
  EXPECT_EQ(f3->getArguments()->size(), 5u);

  expectNamedConnection(f3, "clock", vpiInput, "fsm3clk",
                        hldb::findByName<hldb::Variable>("fsm3clk", top->getVariables()));
  expectNamedConnection(f3, "keys", vpiInput, "keys", hldb::findByName<hldb::Variable>("keys", top->getVariables()));
  expectNamedConnection(f3, "brake", vpiInput, "brake", hldb::findByName<hldb::Variable>("brake", top->getVariables()));
  expectNamedConnection(f3, "accelerate", vpiInput, "accelerate",
                        hldb::findByName<hldb::Variable>("accelerate", top->getVariables()));
  expectNamedConnection(f3, "Speed", vpiOutput, "speed", hldb::findByName<hldb::Net>("speed", top->getNets()));
}

// --- top: initial processes -------------------------------------------------

TEST_F(FSMBsp13Test, TopHasTenInitialProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 10u);
  for (const hldb::Process *const process : *top->getProcesses()) {
    ASSERT_NE(process, nullptr);
    EXPECT_EQ(process->getAnyType(), hldb::AnyType::Initial);
    EXPECT_NE(process->getStmt<hldb::Begin>(), nullptr) << "every initial body is a begin...end block";
  }
}

TEST_F(FSMBsp13Test, Fsm1ClkGeneratorIsForeverDelayedToggle) {
  // initial begin fsm1clk = 0; forever #20 fsm1clk = ~fsm1clk; end
  const hldb::Begin *const body = getInitialBody(0);
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u);

  expectBlockingConstAssign(body->getStmts()->at(0), "fsm1clk", "0");

  const hldb::ForeverStmt *const forever = any_cast<hldb::ForeverStmt>(body->getStmts()->at(1));
  ASSERT_NE(forever, nullptr);
  expectDelayedToggle(forever->getStmt(), "20", "fsm1clk");
}

TEST_F(FSMBsp13Test, Fsm2ClkGeneratorStartsWithNullDelayStatement) {
  // initial begin #1; fsm2clk = 0; forever #30 fsm2clk = ~fsm2clk; end
  // 9.4.1: "#1;" is a delay control whose statement_or_null is null; the
  // following assignment is a separate statement of the begin block.
  const hldb::Begin *const body = getInitialBody(2);
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 3u);

  const hldb::DelayControl *const dc = any_cast<hldb::DelayControl>(body->getStmts()->at(0));
  ASSERT_NE(dc, nullptr) << "'#1;'";
  const hldb::Constant *const amount = dc->getDelay<hldb::Constant>();
  ASSERT_NE(amount, nullptr);
  EXPECT_EQ(amount->getDecompile(), std::string_view{"1"});
  ASSERT_NE(dc->getStmt(), nullptr) << "'#1;' controls a null statement";
  EXPECT_EQ(dc->getStmt()->getAnyType(), hldb::AnyType::NullStmt);

  expectBlockingConstAssign(body->getStmts()->at(1), "fsm2clk", "0");

  const hldb::ForeverStmt *const forever = any_cast<hldb::ForeverStmt>(body->getStmts()->at(2));
  ASSERT_NE(forever, nullptr);
  expectDelayedToggle(forever->getStmt(), "30", "fsm2clk");
}

TEST_F(FSMBsp13Test, StimulusBlockInitializesThirteenRegsThenForks) {
  // initial begin a = 0; ... v = 0; fork ... join end
  const hldb::Begin *const body = getInitialBody(1);
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 14u);

  const std::string_view names[13] = {"a", "b", "c", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v"};
  for (size_t i = 0; i < 13; ++i) {
    expectBlockingConstAssign(body->getStmts()->at(i), names[i], "0");
  }

  const hldb::ForkStmt *const fork = any_cast<hldb::ForkStmt>(body->getStmts()->at(13));
  ASSERT_NE(fork, nullptr);
  EXPECT_EQ(fork->getJoinType(), vpiJoin);
  ASSERT_NE(fork->getStmts(), nullptr);
  ASSERT_EQ(fork->getStmts()->size(), 2u);
  for (const hldb::Any *const branch : *fork->getStmts()) {
    const hldb::ForeverStmt *const forever = any_cast<hldb::ForeverStmt>(branch);
    ASSERT_NE(forever, nullptr) << "both fork branches are 'forever begin ... end'";
    EXPECT_NE(forever->getStmt<hldb::Begin>(), nullptr);
  }
}

TEST_F(FSMBsp13Test, FirstForkBranchComputesAccelerateFromOrChains) {
  // forever begin
  //   #10 a = m | n | o;  accelerate = a | b | c;
  //   #10 b = p | q | r;  accelerate = a | b | c;
  //   #10 c = t | u | v;  accelerate = a | b | c;
  // end
  const hldb::Begin *const body = getInitialBody(1);
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 14u);
  const hldb::ForkStmt *const fork = any_cast<hldb::ForkStmt>(body->getStmts()->at(13));
  ASSERT_NE(fork, nullptr);
  ASSERT_NE(fork->getStmts(), nullptr);
  ASSERT_EQ(fork->getStmts()->size(), 2u);
  const hldb::ForeverStmt *const forever = any_cast<hldb::ForeverStmt>(fork->getStmts()->at(0));
  ASSERT_NE(forever, nullptr);
  const hldb::Begin *const loop = forever->getStmt<hldb::Begin>();
  ASSERT_NE(loop, nullptr);
  ASSERT_NE(loop->getStmts(), nullptr);
  ASSERT_EQ(loop->getStmts()->size(), 6u);

  // "#10 a = m | n | o;" -- 11.4.8: '|' is a left-associative binary operator.
  const hldb::DelayControl *const dc = any_cast<hldb::DelayControl>(loop->getStmts()->at(0));
  ASSERT_NE(dc, nullptr);
  const hldb::Constant *const amount = dc->getDelay<hldb::Constant>();
  ASSERT_NE(amount, nullptr);
  EXPECT_EQ(amount->getDecompile(), std::string_view{"10"});
  const hldb::Assignment *const aAssign = dc->getStmt<hldb::Assignment>();
  ASSERT_NE(aAssign, nullptr);
  EXPECT_TRUE(aAssign->getBlocking());
  const hldb::RefObj *const aLhs = aAssign->getLhs<hldb::RefObj>();
  ASSERT_NE(aLhs, nullptr);
  EXPECT_EQ(aLhs->getName(), std::string_view{"a"});

  const hldb::Operation *const outer = aAssign->getRhs<hldb::Operation>();
  ASSERT_NE(outer, nullptr);
  EXPECT_EQ(outer->getOpType(), vpiBitOrOp);
  ASSERT_NE(outer->getOperands(), nullptr);
  ASSERT_EQ(outer->getOperands()->size(), 2u) << "binary '|' has exactly two operands: (m | n) | o";
  const hldb::Operation *const inner = any_cast<hldb::Operation>(outer->getOperands()->at(0));
  ASSERT_NE(inner, nullptr);
  EXPECT_EQ(inner->getOpType(), vpiBitOrOp);
  ASSERT_NE(inner->getOperands(), nullptr);
  ASSERT_EQ(inner->getOperands()->size(), 2u);
  const hldb::RefObj *const m = any_cast<hldb::RefObj>(inner->getOperands()->at(0));
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getName(), std::string_view{"m"});
  const hldb::RefObj *const n = any_cast<hldb::RefObj>(inner->getOperands()->at(1));
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->getName(), std::string_view{"n"});
  const hldb::RefObj *const o = any_cast<hldb::RefObj>(outer->getOperands()->at(1));
  ASSERT_NE(o, nullptr);
  EXPECT_EQ(o->getName(), std::string_view{"o"});

  // "accelerate = a | b | c;" -- undelayed, at indices 1, 3, 5.
  for (const size_t i : {size_t{1}, size_t{3}, size_t{5}}) {
    const hldb::Assignment *const acc = any_cast<hldb::Assignment>(loop->getStmts()->at(i));
    ASSERT_NE(acc, nullptr) << "index " << i;
    EXPECT_TRUE(acc->getBlocking()) << "index " << i;
    EXPECT_EQ(acc->getDelayControl(), nullptr) << "index " << i;
    const hldb::RefObj *const lhs = acc->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr) << "index " << i;
    EXPECT_EQ(lhs->getName(), std::string_view{"accelerate"});
    EXPECT_EQ(lhs->getActual(), hldb::findByName<hldb::Variable>("accelerate", getTop()->getVariables()));
    const hldb::Operation *const rhs = acc->getRhs<hldb::Operation>();
    ASSERT_NE(rhs, nullptr) << "index " << i;
    EXPECT_EQ(rhs->getOpType(), vpiBitOrOp) << "index " << i;
  }
}

TEST_F(FSMBsp13Test, SecondForkBranchTogglesSevenRegs) {
  // forever begin #4 p = ~p; #4 q = ~q; #3 r = ~r; #2 s = ~s;
  //               #5 t = ~t; #3 u = ~u; #5 v = ~v; end
  const hldb::Begin *const body = getInitialBody(1);
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 14u);
  const hldb::ForkStmt *const fork = any_cast<hldb::ForkStmt>(body->getStmts()->at(13));
  ASSERT_NE(fork, nullptr);
  ASSERT_NE(fork->getStmts(), nullptr);
  ASSERT_EQ(fork->getStmts()->size(), 2u);
  const hldb::ForeverStmt *const forever = any_cast<hldb::ForeverStmt>(fork->getStmts()->at(1));
  ASSERT_NE(forever, nullptr);
  const hldb::Begin *const loop = forever->getStmt<hldb::Begin>();
  ASSERT_NE(loop, nullptr);
  ASSERT_NE(loop->getStmts(), nullptr);
  ASSERT_EQ(loop->getStmts()->size(), 7u);

  const std::pair<std::string_view, std::string_view> expected[7] = {
      {"4", "p"}, {"4", "q"}, {"3", "r"}, {"2", "s"}, {"5", "t"}, {"3", "u"}, {"5", "v"},
  };
  for (size_t i = 0; i < 7; ++i) {
    expectDelayedToggle(loop->getStmts()->at(i), expected[i].first, expected[i].second);
  }
}

TEST_F(FSMBsp13Test, DumpBlockCallsSystemTasks) {
  // initial begin $vtDumpvars(1,top.F2); #3000000 $finish; end
  const hldb::Begin *const body = getInitialBody(8);
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u);

  const hldb::SysTaskCall *const dump = any_cast<hldb::SysTaskCall>(body->getStmts()->at(0));
  ASSERT_NE(dump, nullptr) << "'$vtDumpvars(1,top.F2);'";
  EXPECT_EQ(dump->getName(), std::string_view{"$vtDumpvars"});
  ASSERT_NE(dump->getArguments(), nullptr);
  ASSERT_EQ(dump->getArguments()->size(), 2u);
  const hldb::NamedArgument *const arg0 = dump->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const level = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(level, nullptr);
  EXPECT_EQ(level->getDecompile(), std::string_view{"1"});
  EXPECT_NE(dump->getArguments()->at(1), nullptr) << "'top.F2' hierarchical reference";

  const hldb::DelayControl *const dc = any_cast<hldb::DelayControl>(body->getStmts()->at(1));
  ASSERT_NE(dc, nullptr) << "'#3000000 $finish;'";
  const hldb::Constant *const amount = dc->getDelay<hldb::Constant>();
  ASSERT_NE(amount, nullptr);
  EXPECT_EQ(amount->getDecompile(), std::string_view{"3000000"});
  const hldb::SysTaskCall *const finish = dc->getStmt<hldb::SysTaskCall>();
  ASSERT_NE(finish, nullptr);
  EXPECT_EQ(finish->getName(), std::string_view{"$finish"});
  EXPECT_EQ(finish->getArguments(), nullptr) << "'$finish' is called with no arguments";
}

// --- compiler diagnostics ---------------------------------------------------

TEST_F(FSMBsp13Test, NoFailedBindsForTopSignals) {
  for (std::string_view name : {"fsm1clk", "fsm2clk", "fsm3clk", "fsm1rst", "fsm2rst", "SlowRam", "ctrl", "keys",
                                "brake", "accelerate", "rd", "wr", "Fsm2Out", "speed"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, name), nullptr) << "'" << name << "' must bind";
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
