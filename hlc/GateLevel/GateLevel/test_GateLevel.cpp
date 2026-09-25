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

// Tests for tests/GateLevel/dut.sv:
//   module LogicGates(a,b,y1,y2,y3,y4,y5,y6,y7, y8, y9, y10);
//     input a,b;
//     output y1, y2,y3,y4,y5,y6,y7,y8,y9;
//     and(y1,a,b);
//     or(y2,a,b);
//     not(y3,a);
//     nand(y4,a,b);
//     nor(y5,a,b);
//     xor(y6,a,b);
//     xnor(y7,a,b);
//     and #(1)   a1 (y8,a,b);
//     or #(1,2) a2 (y9,a,b, a | b);
//     nand #(2:3:4, 3:4:5)  a3 (nn, a, b);
//     bufif0 #(5, 6, 7) a4 (out2, a, b);
//     bufif0 #(5:6:7, 6:7:8, 7:8:9) a5 (out3, a, b);
//     pmos a6 (p1,p2,p3);
//     pullup a7 (p1);
//   endmodule
//
// IEEE 1800-2023 constructs under test (Sec 28: gate and switch level
// modeling):
//   - Sec 28.4/23.3: gate_instantiation. Unlike a module_instantiation, a
//     gate_instance name is OPTIONAL ("... unlike a module instance, no name
//     needs to be specified for a primitive instance" -- Sec 28.4). Both the
//     unnamed form ("and(y1,a,b);") and the named form ("and #(1) a1
//     (y8,a,b);") are legal.
//   - Sec 28.4: n-input gates (and, nand, or, nor, xor, xnor) and n-output
//     gates (not) each produce one terminal per port connection, the first
//     terminal always being the (sole, for these gate kinds) output.
//   - Sec 28.14/29.4 (delay specification, "drive_strength / delay2 /
//     delay3" grammar): a gate_instantiation may carry 0, 1, 2, or 3 delay
//     values, and each individual delay value may itself be a
//     "mintypmax_expression" (min:typ:max, three sub-values joined by ':').
//     Plain (non mintypmax) delay values are modeled as a flat Constant per
//     Primitive::getDelays() element; a min:typ:max delay value is modeled
//     as a DelayTerm whose getValues() holds the three Constants.
//   - Sec 28.9 (MOS switches) / 28.12 (pull gates): "pmos a6 (p1,p2,p3);" is
//     a 3-terminal MOS switch (output, input, control); "pullup a7 (p1);"
//     is a 1-terminal (output only) pull gate.
//
// Checked:
//   - module "LogicGates" exists.
//   - each unnamed n-input/n-output gate (y1..y7) has the correct
//     vpiPrimType and, per Sec 28.4, an empty instance name.
//   - each named gate/switch instance (a1..a7) has the correct name and
//     vpiPrimType.
//   - delay values are captured for every delay-specification form present:
//     single value (a1), two values (a2), min:typ:max pair (a3), three
//     plain values (a4), three min:typ:max values (a5).
//
// NOT CHECKED (out of scope; no assertion locks in current tool output as
// "correct"):
//   - net/port declaration completeness of y8/y9/y10/nn/out2/out3/p1/p2/p3
//     (several are implicit/undeclared nets or an unused port "y10"; that is
//     a nets-and-variables concern, not a gate-primitive concern).
//   - elaborated logic-value simulation of any gate.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/delay_term.h>
#include <hldb/design.h>
#include <hldb/gate.h>
#include <hldb/module.h>
#include <hldb/prim_term.h>
#include <hldb/primitive.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

namespace hlc {

class GateLevelTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "GateLevel.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Gate *getNamedGate(const hldb::Module *m, std::string_view name) {
    return (m == nullptr) ? nullptr : hldb::findByName<hldb::Gate>(name, m->getPrimitives());
  }

  // Finds an unnamed gate by the name of the net connected to its first
  // (output) terminal -- e.g. "and(y1,a,b);" is found via "y1".
  static const hldb::Gate *getGateByFirstTermNet(const hldb::Module *m, std::string_view firstTermNetName) {
    if ((m == nullptr) || (m->getPrimitives() == nullptr)) return nullptr;
    for (const hldb::Primitive *const prim : *m->getPrimitives()) {
      const hldb::Gate *const gate = any_cast<hldb::Gate>(prim);
      if (gate == nullptr) continue;
      if ((gate->getPrimTerms() == nullptr) || gate->getPrimTerms()->empty()) continue;
      const hldb::PrimTerm *const firstTerm = gate->getPrimTerms()->at(0);
      if (firstTerm == nullptr) continue;
      const hldb::RefObj *const ref = firstTerm->getExpr<hldb::RefObj>();
      if ((ref != nullptr) && (ref->getName() == firstTermNetName)) return gate;
    }
    return nullptr;
  }

  // Expects 'delays->at(index)' to be a flat Constant with the given
  // decompiled text (the plain, non mintypmax delay-value form).
  static void ExpectPlainDelay(const hldb::ExprCollection *delays, size_t index, std::string_view value) {
    ASSERT_NE(delays, nullptr);
    ASSERT_LT(index, delays->size());
    const hldb::Constant *const c = delays->at(index)->getVpiType() == vpiConstant
                                         ? any_cast<hldb::Constant>(delays->at(index))
                                         : nullptr;
    ASSERT_NE(c, nullptr) << "delay[" << index << "] is not a plain Constant";
    EXPECT_EQ(c->getDecompile(), value);
  }

  // Expects 'delays->at(index)' to be a DelayTerm (min:typ:max) with values
  // {minVal, typVal, maxVal}.
  static void ExpectMinTypMaxDelay(const hldb::ExprCollection *delays, size_t index, std::string_view minVal,
                                    std::string_view typVal, std::string_view maxVal) {
    ASSERT_NE(delays, nullptr);
    ASSERT_LT(index, delays->size());
    const hldb::DelayTerm *const dt = any_cast<hldb::DelayTerm>(delays->at(index));
    ASSERT_NE(dt, nullptr) << "delay[" << index << "] is not a DelayTerm (min:typ:max)";
    ASSERT_NE(dt->getValues(), nullptr);
    ASSERT_EQ(dt->getValues()->size(), 3u);
    const hldb::Constant *const minC = any_cast<hldb::Constant>(dt->getValues()->at(0));
    const hldb::Constant *const typC = any_cast<hldb::Constant>(dt->getValues()->at(1));
    const hldb::Constant *const maxC = any_cast<hldb::Constant>(dt->getValues()->at(2));
    ASSERT_NE(minC, nullptr);
    ASSERT_NE(typC, nullptr);
    ASSERT_NE(maxC, nullptr);
    EXPECT_EQ(minC->getDecompile(), minVal);
    EXPECT_EQ(typC->getDecompile(), typVal);
    EXPECT_EQ(maxC->getDecompile(), maxVal);
  }
};

// --- module existence ----

TEST_F(GateLevelTest, ModuleExists) { ASSERT_NE(getModule("LogicGates"), nullptr) << "module 'LogicGates' not found"; }

// --- unnamed gates: correct vpiPrimType and, per Sec 28.4, no name ----

TEST_F(GateLevelTest, UnnamedAndGateY1) {
  const hldb::Module *const m = getModule("LogicGates");
  ASSERT_NE(m, nullptr);
  const hldb::Gate *const g = getGateByFirstTermNet(m, "y1");
  ASSERT_NE(g, nullptr) << "'and(y1,a,b);' not found";
  EXPECT_EQ(g->getPrimType(), vpiAndPrim);
  EXPECT_TRUE(g->getName().empty()) << "Sec 28.4: an unnamed gate instance carries no instance name";
  ASSERT_NE(g->getPrimTerms(), nullptr);
  EXPECT_EQ(g->getPrimTerms()->size(), 3u) << "output + 2 inputs";
}

TEST_F(GateLevelTest, UnnamedOrGateY2) {
  const hldb::Gate *const g = getGateByFirstTermNet(getModule("LogicGates"), "y2");
  ASSERT_NE(g, nullptr) << "'or(y2,a,b);' not found";
  EXPECT_EQ(g->getPrimType(), vpiOrPrim);
  EXPECT_TRUE(g->getName().empty());
}

TEST_F(GateLevelTest, UnnamedNotGateY3) {
  const hldb::Gate *const g = getGateByFirstTermNet(getModule("LogicGates"), "y3");
  ASSERT_NE(g, nullptr) << "'not(y3,a);' not found";
  EXPECT_EQ(g->getPrimType(), vpiNotPrim);
  EXPECT_TRUE(g->getName().empty());
  ASSERT_NE(g->getPrimTerms(), nullptr);
  EXPECT_EQ(g->getPrimTerms()->size(), 2u) << "output + 1 input";
}

TEST_F(GateLevelTest, UnnamedNandGateY4) {
  const hldb::Gate *const g = getGateByFirstTermNet(getModule("LogicGates"), "y4");
  ASSERT_NE(g, nullptr) << "'nand(y4,a,b);' not found";
  EXPECT_EQ(g->getPrimType(), vpiNandPrim);
  EXPECT_TRUE(g->getName().empty());
}

TEST_F(GateLevelTest, UnnamedNorGateY5) {
  const hldb::Gate *const g = getGateByFirstTermNet(getModule("LogicGates"), "y5");
  ASSERT_NE(g, nullptr) << "'nor(y5,a,b);' not found";
  EXPECT_EQ(g->getPrimType(), vpiNorPrim);
  EXPECT_TRUE(g->getName().empty());
}

TEST_F(GateLevelTest, UnnamedXorGateY6) {
  const hldb::Gate *const g = getGateByFirstTermNet(getModule("LogicGates"), "y6");
  ASSERT_NE(g, nullptr) << "'xor(y6,a,b);' not found";
  EXPECT_EQ(g->getPrimType(), vpiXorPrim);
  EXPECT_TRUE(g->getName().empty());
}

TEST_F(GateLevelTest, UnnamedXnorGateY7) {
  const hldb::Gate *const g = getGateByFirstTermNet(getModule("LogicGates"), "y7");
  ASSERT_NE(g, nullptr) << "'xnor(y7,a,b);' not found";
  EXPECT_EQ(g->getPrimType(), vpiXnorPrim);
  EXPECT_TRUE(g->getName().empty());
}

// --- named gates: correct name and vpiPrimType ----

TEST_F(GateLevelTest, NamedAndGateA1) {
  const hldb::Gate *const g = getNamedGate(getModule("LogicGates"), "a1");
  ASSERT_NE(g, nullptr) << "'and #(1) a1 (y8,a,b);' not found";
  EXPECT_EQ(g->getName(), std::string_view{"a1"});
  EXPECT_EQ(g->getPrimType(), vpiAndPrim);
}

TEST_F(GateLevelTest, NamedOrGateA2) {
  const hldb::Gate *const g = getNamedGate(getModule("LogicGates"), "a2");
  ASSERT_NE(g, nullptr) << "'or #(1,2) a2 (y9,a,b, a | b);' not found";
  EXPECT_EQ(g->getName(), std::string_view{"a2"});
  EXPECT_EQ(g->getPrimType(), vpiOrPrim);
}

TEST_F(GateLevelTest, NamedNandGateA3) {
  const hldb::Gate *const g = getNamedGate(getModule("LogicGates"), "a3");
  ASSERT_NE(g, nullptr) << "'nand #(2:3:4, 3:4:5) a3 (nn, a, b);' not found";
  EXPECT_EQ(g->getName(), std::string_view{"a3"});
  EXPECT_EQ(g->getPrimType(), vpiNandPrim);
}

TEST_F(GateLevelTest, NamedBufif0GateA4) {
  const hldb::Gate *const g = getNamedGate(getModule("LogicGates"), "a4");
  ASSERT_NE(g, nullptr) << "'bufif0 #(5, 6, 7) a4 (out2, a, b);' not found";
  EXPECT_EQ(g->getName(), std::string_view{"a4"});
  EXPECT_EQ(g->getPrimType(), vpiBufif0Prim);
}

TEST_F(GateLevelTest, NamedBufif0GateA5) {
  const hldb::Gate *const g = getNamedGate(getModule("LogicGates"), "a5");
  ASSERT_NE(g, nullptr) << "'bufif0 #(5:6:7, 6:7:8, 7:8:9) a5 (out3, a, b);' not found";
  EXPECT_EQ(g->getName(), std::string_view{"a5"});
  EXPECT_EQ(g->getPrimType(), vpiBufif0Prim);
}

TEST_F(GateLevelTest, NamedPmosSwitchA6) {
  const hldb::Gate *const g = getNamedGate(getModule("LogicGates"), "a6");
  ASSERT_NE(g, nullptr) << "'pmos a6 (p1,p2,p3);' not found";
  EXPECT_EQ(g->getName(), std::string_view{"a6"});
  EXPECT_EQ(g->getPrimType(), vpiPmosPrim);
  ASSERT_NE(g->getPrimTerms(), nullptr);
  EXPECT_EQ(g->getPrimTerms()->size(), 3u) << "Sec 28.9: MOS switch has 3 terminals (output, input, control)";
}

TEST_F(GateLevelTest, NamedPullupGateA7) {
  const hldb::Gate *const g = getNamedGate(getModule("LogicGates"), "a7");
  ASSERT_NE(g, nullptr) << "'pullup a7 (p1);' not found";
  EXPECT_EQ(g->getName(), std::string_view{"a7"});
  EXPECT_EQ(g->getPrimType(), vpiPullupPrim);
  ASSERT_NE(g->getPrimTerms(), nullptr);
  EXPECT_EQ(g->getPrimTerms()->size(), 1u) << "Sec 28.12: a pull gate has a single (output) terminal";
}

// --- delay-specification grammar breadth (Sec 28.14/29.4) ----

TEST_F(GateLevelTest, A1SingleDelayValue) {
  const hldb::Gate *const g = getNamedGate(getModule("LogicGates"), "a1");
  ASSERT_NE(g, nullptr);
  ASSERT_NE(g->getDelays(), nullptr) << "'#(1)' must be captured";
  ASSERT_EQ(g->getDelays()->size(), 1u) << "a single delay value applies uniformly to all transitions";
  ExpectPlainDelay(g->getDelays(), 0, "1");
}

TEST_F(GateLevelTest, A2TwoDelayValues) {
  const hldb::Gate *const g = getNamedGate(getModule("LogicGates"), "a2");
  ASSERT_NE(g, nullptr);
  ASSERT_NE(g->getDelays(), nullptr) << "'#(1,2)' must be captured";
  ASSERT_EQ(g->getDelays()->size(), 2u) << "rise/fall delay2 form";
  ExpectPlainDelay(g->getDelays(), 0, "1");
  ExpectPlainDelay(g->getDelays(), 1, "2");
}

TEST_F(GateLevelTest, A3TwoMinTypMaxDelayValues) {
  const hldb::Gate *const g = getNamedGate(getModule("LogicGates"), "a3");
  ASSERT_NE(g, nullptr);
  ASSERT_NE(g->getDelays(), nullptr) << "'#(2:3:4, 3:4:5)' must be captured";
  ASSERT_EQ(g->getDelays()->size(), 2u) << "rise/fall delay2 form, each a min:typ:max triple";
  ExpectMinTypMaxDelay(g->getDelays(), 0, "2", "3", "4");
  ExpectMinTypMaxDelay(g->getDelays(), 1, "3", "4", "5");
}

TEST_F(GateLevelTest, A4ThreePlainDelayValues) {
  const hldb::Gate *const g = getNamedGate(getModule("LogicGates"), "a4");
  ASSERT_NE(g, nullptr);
  ASSERT_NE(g->getDelays(), nullptr) << "'#(5, 6, 7)' must be captured";
  ASSERT_EQ(g->getDelays()->size(), 3u) << "rise/fall/turn-off delay3 form";
  ExpectPlainDelay(g->getDelays(), 0, "5");
  ExpectPlainDelay(g->getDelays(), 1, "6");
  ExpectPlainDelay(g->getDelays(), 2, "7");
}

TEST_F(GateLevelTest, A5ThreeMinTypMaxDelayValues) {
  const hldb::Gate *const g = getNamedGate(getModule("LogicGates"), "a5");
  ASSERT_NE(g, nullptr);
  ASSERT_NE(g->getDelays(), nullptr) << "'#(5:6:7, 6:7:8, 7:8:9)' must be captured";
  ASSERT_EQ(g->getDelays()->size(), 3u) << "rise/fall/turn-off delay3 form, each a min:typ:max triple";
  ExpectMinTypMaxDelay(g->getDelays(), 0, "5", "6", "7");
  ExpectMinTypMaxDelay(g->getDelays(), 1, "6", "7", "8");
  ExpectMinTypMaxDelay(g->getDelays(), 2, "7", "8", "9");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
