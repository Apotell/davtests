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

// Source under test: tests/Google/chapter-18/18.13.1--urandom_3.sv
//
//   import uvm_pkg::*;
//   `include "uvm_macros.svh"
//
//   class a;
//       function int unsigned do_urandom(int seed);
//           int unsigned x;
//           x = $urandom(seed);
//           return x;
//       endfunction
//   endclass
//
//   class env extends uvm_env;
//     a obj = new;
//     int unsigned ret1, ret2;
//     int seed = 254;
//     function new(string name, uvm_component parent = null); super.new(name, parent); endfunction
//     task run_phase(uvm_phase phase);
//       phase.raise_objection(this);
//       begin
//         ret1 = obj.do_urandom(seed);
//         ret2 = obj.do_urandom(seed);
//         if(ret1 == ret2) `uvm_info(...) else `uvm_error(...);
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
// Same class "a"/"do_urandom(int seed)" already checked in detail by the
// sibling 18.13.1--urandom_2.sv, wrapped in a UVM env/run_phase/
// run_test() shell that calls it twice with the same seed. This tool is
// a compiler/elaborator, not a simulator, so this test only checks
// static structure -- it cannot verify the file's actual point (Sec
// 18.13.1: seeding $urandom with the same value deterministically
// reproduces the same sequence, hence "ret1 == ret2" is the file's own
// expected-success condition).
//
// Checked (against IEEE 1800-2023, not against whatever HLC happens to
// output today -- see davtests test writing guide):
//   - Class "a" still has its "do_urandom" method returning an unsigned
//     IntTypespec with a signed "int seed" argument (same shape as the
//     sibling "_2" file; not re-derived field-by-field here).
//   - Class "env" is derived from a base class named "uvm_env" (Sec
//     8.13) -- checked via ClassDefn::getExtends().
//   - Class "env" declares two unsigned-int properties in one statement
//     ("int unsigned ret1, ret2;" -- Sec 6.8: a single data_declaration
//     can declare multiple variables of the same type) and a signed-int
//     property "seed", plus a method "run_phase" (found via
//     ClassDefn::getMethods() as an hldb::Task).
//   - Module "top" exists.
//
// Not checked:
//   - The declaration-time initializer (254) of env::seed -- not
//     asserted (see the sibling chapter tests' shared note on the
//     unresolved Variable::getExpr()-vs-getValue() ambiguity for plain
//     initializers).
//   - Any statement tree inside run_phase() (the two do_urandom() calls
//     and the ret1==ret2 comparison) -- not investigated this session
//     (build was skipped per request).
//   - Any actual runtime $urandom(seed)/run_phase()/run_test() behavior
//     -- unrunnable by a compiler/elaborator.
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
#include <hldb/io_decl.h>
#include <hldb/int_typespec.h>
#include <hldb/extends.h>
#include <hldb/vpi_user.h>

namespace hlc {
class Urandom3Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.13.1--urandom_3.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(Urandom3Test, ClassAStillHasDoUrandomWithSeedArgument) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);

  const hldb::Function *const fn = hldb::findByName<hldb::Function>("do_urandom", a->getMethods());
  ASSERT_NE(fn, nullptr);
  const hldb::IntTypespec *const returnTs = hldb::getActual<hldb::IntTypespec>(fn->getReturn());
  ASSERT_NE(returnTs, nullptr);
  EXPECT_FALSE(returnTs->getSigned());

  ASSERT_NE(fn->getIODecls(), nullptr);
  const hldb::IODecl *const seedArg = hldb::findByName<hldb::IODecl>("seed", fn->getIODecls());
  ASSERT_NE(seedArg, nullptr);
  EXPECT_EQ(seedArg->getDirection(), vpiInput);
}

TEST_F(Urandom3Test, ClassEnvExtendsUvmEnvAndHasRet1Ret2SeedAndRunPhase) {
  const hldb::ClassDefn *const env = hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses());
  ASSERT_NE(env, nullptr);
  EXPECT_NE(env->getExtends(), nullptr) << "Sec 8.13: 'class env extends uvm_env;' should record its base class";

  const hldb::Variable *const ret1 = hldb::findByName<hldb::Variable>("ret1", env->getVariables());
  const hldb::Variable *const ret2 = hldb::findByName<hldb::Variable>("ret2", env->getVariables());
  const hldb::Variable *const seed = hldb::findByName<hldb::Variable>("seed", env->getVariables());
  ASSERT_NE(ret1, nullptr) << "'int unsigned ret1, ret2;' declares both in one statement";
  ASSERT_NE(ret2, nullptr);
  ASSERT_NE(seed, nullptr) << "'int seed = 254;' declares 'seed' on env";

  const hldb::IntTypespec *const ret1Ts = hldb::getTypespec<hldb::IntTypespec>(ret1);
  const hldb::IntTypespec *const ret2Ts = hldb::getTypespec<hldb::IntTypespec>(ret2);
  const hldb::IntTypespec *const seedTs = hldb::getTypespec<hldb::IntTypespec>(seed);
  ASSERT_NE(ret1Ts, nullptr);
  ASSERT_NE(ret2Ts, nullptr);
  ASSERT_NE(seedTs, nullptr);
  EXPECT_FALSE(ret1Ts->getSigned());
  EXPECT_FALSE(ret2Ts->getSigned());
  EXPECT_TRUE(seedTs->getSigned()) << "'int seed' (no 'unsigned' qualifier) is signed";

  const hldb::Task *const runPhase = hldb::findByName<hldb::Task>("run_phase", env->getMethods());
  ASSERT_NE(runPhase, nullptr) << "'task run_phase(uvm_phase phase);' should be a method with a body";
}

TEST_F(Urandom3Test, ModuleTopExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
