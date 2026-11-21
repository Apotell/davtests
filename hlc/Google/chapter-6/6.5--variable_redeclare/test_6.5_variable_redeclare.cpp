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

// Tests for 6.5--variable_redeclare.sv (tags: 6.5,
// :should_fail_because: 'v' is declared twice in the same scope, once as
// a variable (reg) and once as a net (wire))
//   module top();
//     reg v;
//     wire v;
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.5 "Declaration data types" /
// namespace rules, checked before any test code was written):
//   "reg" is an integer_vector_type keyword (IEEE 1800-2023 6.8: "bit |
//   logic | reg"), NOT a net_type keyword (6.7's list is wire, tri,
//   triand, trior, trireg, tri0, tri1, uwire, wand, wor, supply0,
//   supply1) -- "reg v;" declares a VARIABLE. "wire v;" then declares a
//   NET with the same name in the same scope. A single identifier cannot
//   simultaneously be two fundamentally different kinds of thing (a
//   variable AND a net) in one scope -- this is what makes the file
//   illegal, distinct from an ordinary "same-type" redeclaration.
//
//   Besides reporting the redeclaration, HLC, confirmed by running the
//   compiler directly on this file, MERGES both declarations into a
//   single Net object (current HLC behaviour; the standard defines
//   no model for illegal code, so these are not standard
//   requirements): vpiNetType comes out as vpiWire (the second/"wire"
//   declaration wins), and the typespec comes out as LogicTypespec (the
//   "reg"/first declaration's type survives). The structural tests below
//   describe this actual merged-object shape as ground truth for what
//   HLC currently produces -- they are not claiming this Net/Variable
//   split is correct, only documenting current behavior. The real
//   point of the file -- the error HLC reports for this conflicting
//   redeclaration -- is checked by the last test.
//
// What is checked:
//   - module top exists, has exactly 1 Net named "v" (vpiNetType=vpiWire)
//   - the Net has a RefTypespec whose actual is LogicTypespec (the "reg"
//     declaration's type)
//   - top has no continuous assignments, no processes
//   - THE POINT OF THIS FILE: redeclaring "v" as both reg (variable) and wire
//     (net) in the same scope is an error (IEEE 1800-2023 6.5), reported as
//     CompDuplicateDeclaration at the later declaration, the wire on line 19.
//
// What is NOT checked and why:
//   - none: every corner above is fully structural and checkable without
//     simulation.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class VariableRedeclareTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.5--variable_redeclare.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }
};

TEST_F(VariableRedeclareTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

// ---------------------------------------------------------------------------
// Net -- reg v and wire v merge into a single Net named 'v' (HLC's current
// shape for this illegal code, documented as is, not required by the standard)
// ---------------------------------------------------------------------------
TEST_F(VariableRedeclareTest, OneNetExists) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr) << "module has no nets";
  EXPECT_EQ(top->getNets()->size(), 1u) << "reg v and wire v currently collapse to exactly one net";
}

TEST_F(VariableRedeclareTest, NetNameIsV) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("v", top->getNets());
  ASSERT_NE(v, nullptr) << "net 'v' not found in module";
}

// ---------------------------------------------------------------------------
// wire wins -- vpiNetType should be vpiWire (1)
// ---------------------------------------------------------------------------
TEST_F(VariableRedeclareTest, NetTypeIsWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("v", top->getNets());
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getNetType(), vpiWire) << "expected vpiNetType wire (1) -- wire declaration wins over reg";
}

// ---------------------------------------------------------------------------
// Typespec -- reg maps to LogicTypespec referenced via RefTypespec
// ---------------------------------------------------------------------------
TEST_F(VariableRedeclareTest, NetHasRefTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("v", top->getNets());
  ASSERT_NE(v, nullptr);
  EXPECT_NE(v->getTypespec(), nullptr) << "net 'v' has no typespec";
}

TEST_F(VariableRedeclareTest, NetTypespecIsLogic) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const v = hldb::findByName<hldb::Net>("v", top->getNets());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr) << "net 'v' has no RefTypespec";
  const hldb::LogicTypespec *const lts = rts->getActual<hldb::LogicTypespec>();
  EXPECT_NE(lts, nullptr) << "RefTypespec actual is not a LogicTypespec (expected from reg declaration)";
}

// ---------------------------------------------------------------------------
// No continuous assignments or processes -- the module only has declarations
// ---------------------------------------------------------------------------
TEST_F(VariableRedeclareTest, NoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty())
      << "unexpected continuous assignments in redeclaration-only module";
}

TEST_F(VariableRedeclareTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

// ---------------------------------------------------------------------------
// The actual point of this file: 'v' is redeclared as both reg and wire
// ---------------------------------------------------------------------------
TEST_F(VariableRedeclareTest, RedeclarationOfVAsNetIsRejected) {
  // 6.5: "Within a name space (see 3.13), it shall be illegal to redeclare a name already declared by a net,
  // variable, or other declaration." 'reg v;' (line 18) is the first declaration; 'wire v;' (line 19, column 7)
  // redeclares it and is the one reported.
  EXPECT_NE(findEvent(EventType::CompDuplicateDeclaration, "v", 19, 7), nullptr)
      << "IEEE 1800-2023 6.5: 'v' is declared twice in one scope, first as a variable via "
         "'reg' and then as a net via 'wire'.";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
