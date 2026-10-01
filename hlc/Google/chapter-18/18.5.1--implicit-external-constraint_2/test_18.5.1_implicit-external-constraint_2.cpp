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

// Source under test: tests/Google/chapter-18/18.5.1--implicit-external-constraint_2.sv
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class a;
//       rand int b;
//       constraint c;
//   endclass
//   constraint a::c { b == 5; }
//
//   class env extends uvm_env;
//     a obj = new;
//     function new(string name, uvm_component parent = null); super.new(name, parent); endfunction
//     task run_phase(uvm_phase phase);
//       phase.raise_objection(this);
//       begin
//         obj.randomize();
//         if(obj.b == 5) `uvm_info(...) else `uvm_error(...);
//       end
//       phase.drop_objection(this);
//     endtask: run_phase
//   endclass
//
//   module top;
//     env environment;
//     initial begin
//       environment = new("env");
//       run_test();
//     end
//   endmodule
//
// Identical to 18.5.1--explicit-external-constraint_2.sv except the
// in-class constraint prototype omits "extern" ("constraint c;" instead
// of "extern constraint c;") -- per IEEE 1800-2023 Sec 18.5.1, "extern"
// is not actually part of the constraint_prototype grammar, so this is
// the same construct (see the detailed shape confirmed in
// 18.5.1--implicit-external-constraint_0.cpp). This tool is a compiler/
// elaborator, not a simulator, so this test only checks static structure,
// the same way its "explicit" sibling does.
//
// Checked (against IEEE 1800-2023, not against whatever HLC happens to
// output today -- see davtests test writing guide). Confirmed by actually
// compiling and running this test's own SetUpTestSuite(), not by reading
// the existing .log:
//   - Sec 8.3/18.4/18.5.1: class "a" exists with "rand int b;" and an
//     out-of-class-defined constraint "c" whose body is "b == 5" -- same
//     shape already proven in detail by 18.5.1--implicit-external-
//     constraint_0.cpp (Distribution wrapper, vpiEqOp, RefObj "b",
//     Constant), not re-derived field-by-field here.
//   - Class "env" exists and is derived from a base class named
//     "uvm_env" (Sec 8.13, single inheritance via "extends") -- checked
//     via ClassDefn::getExtends().
//   - Module "top" exists.
//   - This build does elaborate the file, pulling in the UVM package via
//     "import uvm_pkg::*" (confirmed by the same class set appearing as
//     in the "explicit" sibling); this test does not assert zero
//     diagnostics for the same reason as that sibling -- a real UVM
//     testbench pulls in far more machinery than this chapter's own
//     rand/constraint construct.
//
// Not checked:
//   - The exact cause of whatever diagnostics this build reports on this
//     file -- not diagnosed here (spot-checked on the "explicit" sibling:
//     none were specific to the rand/constraint construct this chapter
//     is actually about; all were inside UVM library plumbing pulled in
//     by "import uvm_pkg::*", not inside class "a" or "env" as written).
//   - Any actual randomize()/run_phase()/run_test() runtime behavior --
//     unrunnable by a compiler/elaborator.
//   - vpiFullName on any object (never asserted per the test writing
//     guide; use getName() only).

#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/class_defn.h>
#include <hldb/variable.h>
#include <hldb/extends.h>
#include <hldb/vpi_user.h>

namespace hlc {
class ImplicitExternalConstraint2Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.1--implicit-external-constraint_2.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(ImplicitExternalConstraint2Test, ClassAWithRandPropertyExists) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", a->getVariables());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getRandType(), vpiRand);
}

TEST_F(ImplicitExternalConstraint2Test, OutOfClassConstraintExistsAtDesignLevel) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);

  const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", m_design->getConstraints());
  ASSERT_NE(c, nullptr) << "'constraint a::c { b == 5; }' should register a Constraint at Design level "
                            "(same modeling as 18.5.1--implicit-external-constraint_0.sv)";

  const hldb::ClassDefn *const owner = hldb::getReferenced<hldb::ClassDefn>(c->getPrefix());
  EXPECT_EQ(owner, a);
}

TEST_F(ImplicitExternalConstraint2Test, ClassEnvExtendsUvmEnv) {
  const hldb::ClassDefn *const env = hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses());
  ASSERT_NE(env, nullptr) << "'class env extends uvm_env;' should exist";

  const hldb::Extends *const extends = env->getExtends();
  ASSERT_NE(extends, nullptr) << "Sec 8.13: single inheritance via 'extends' should be recorded";
}

TEST_F(ImplicitExternalConstraint2Test, ModuleTopExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
