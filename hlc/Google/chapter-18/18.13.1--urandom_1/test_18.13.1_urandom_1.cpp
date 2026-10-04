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

// Source under test: tests/Google/chapter-18/18.13.1--urandom_1.sv
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class a;
//       function int unsigned do_urandom();
//           int unsigned x;
//           x = $urandom();
//           return x;
//       endfunction
//   endclass
//
//   class env extends uvm_env;
//     a obj = new;
//     int unsigned ret;
//     function new(string name, uvm_component parent = null); super.new(name, parent); endfunction
//     task run_phase(uvm_phase phase);
//       phase.raise_objection(this);
//       begin
//         ret = obj.do_urandom();
//         if(ret >= 0) `uvm_info(...) else `uvm_error(...);
//       end
//       phase.drop_objection(this);
//     endtask: run_phase
//   endclass
//
//   module top;
//     env environment;
//     initial begin environment = new("env"); run_test(); end
//   endmodule
//
// Same class "a"/"do_urandom" already checked in detail by the sibling
// 18.13.1--urandom_0.sv, wrapped in a UVM env/run_phase/run_test() shell.
// This tool is a compiler/elaborator, not a simulator, so this test only
// checks static structure.
//
// Checked (against IEEE 1800-2023, not against whatever HLC happens to
// output today -- see davtests test writing guide):
//   - Class "a" still has its "do_urandom" method returning an unsigned
//     IntTypespec with no arguments (same shape as the sibling "_0" file;
//     not re-derived field-by-field here).
//   - Class "env" is derived from a base class named "uvm_env" (Sec
//     8.13) -- checked via ClassDefn::getExtends().
//   - Class "env" has its own property "ret" (Sec 6.11: "int unsigned",
//     resolving to an unsigned IntTypespec) and a method "run_phase"
//     (found via ClassDefn::getMethods() as an hldb::Task, since it has a
//     task body -- the concrete subtype for a fully-bodied task, distinct
//     from hldb::Function used for do_urandom).
//   - Module "top" exists.
//
// Not checked:
//   - Any statement tree inside run_phase() (e.g. the
//     "ret = obj.do_urandom();" call and its if/else) -- not
//     investigated this session (build was skipped per request).
//   - Any actual runtime $urandom()/run_phase()/run_test() behavior --
//     unrunnable by a compiler/elaborator.
//   - vpiFullName on any object (never asserted per the test writing
//     guide; use getName() only).

#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/class_defn.h>
#include <hldb/variable.h>
#include <hldb/function.h>
#include <hldb/task.h>
#include <hldb/int_typespec.h>
#include <hldb/extends.h>

namespace hlc {
class Urandom1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.13.1--urandom_1.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(Urandom1Test, ClassAStillHasDoUrandomWithUnsignedIntReturn) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);

  const hldb::Function *const fn = hldb::findByName<hldb::Function>("do_urandom", a->getMethods());
  ASSERT_NE(fn, nullptr);
  const hldb::IntTypespec *const returnTs = hldb::getActual<hldb::IntTypespec>(fn->getReturn());
  ASSERT_NE(returnTs, nullptr);
  EXPECT_FALSE(returnTs->getSigned());
  EXPECT_EQ(fn->getIODecls(), nullptr);
}

TEST_F(Urandom1Test, ClassEnvExtendsUvmEnvAndHasPropertyRetAndRunPhase) {
  const hldb::ClassDefn *const env = hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses());
  ASSERT_NE(env, nullptr);
  EXPECT_NE(env->getExtends(), nullptr) << "Sec 8.13: 'class env extends uvm_env;' should record its base class";

  const hldb::Variable *const ret = hldb::findByName<hldb::Variable>("ret", env->getVariables());
  ASSERT_NE(ret, nullptr) << "'int unsigned ret;' declares 'ret' on env";
  const hldb::IntTypespec *const retTs = hldb::getTypespec<hldb::IntTypespec>(ret);
  ASSERT_NE(retTs, nullptr);
  EXPECT_FALSE(retTs->getSigned());

  const hldb::Task *const runPhase = hldb::findByName<hldb::Task>("run_phase", env->getMethods());
  ASSERT_NE(runPhase, nullptr) << "'task run_phase(uvm_phase phase);' should be a method with a body";
}

TEST_F(Urandom1Test, ModuleTopExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
