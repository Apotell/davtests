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

// Validates that the grammar predicates (isModuleElem / isInterfaceElem /
// isProgramElem / isCheckerElem / isPrimitiveElem / isUnsupportedElem)
// correctly classify each instantiation and produce the right HLDB objects.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/design.h>
#include <hldb/gen_for.h>
#include <hldb/gen_region.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/primitive.h>
#include <hldb/program.h>
#include <hldb/program_typespec.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_typespec.h>
#include <hldb/udp.h>
#include <hldb/udp_defn.h>
#include <hldb/unsupported_typespec.h>

namespace hlc {

class Instantiations : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Instantiations.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ----
// 1. Top-level definitions are present with correct types
// ----
TEST_F(Instantiations, ModuleDefinitions) {
  ASSERT_NE(hldb::findByDefName<hldb::Module>("passthru", m_design->getAllModules()), nullptr);
  ASSERT_NE(hldb::findByDefName<hldb::Module>("adder", m_design->getAllModules()), nullptr);
  ASSERT_NE(hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()), nullptr);
  ASSERT_NE(hldb::findByDefName<hldb::Module>("gen_top", m_design->getAllModules()), nullptr);
}

TEST_F(Instantiations, InterfaceDefinitions) {
  ASSERT_NE(hldb::findByDefName<hldb::Interface>("simple_if", m_design->getAllInterfaces()), nullptr);
  ASSERT_NE(hldb::findByDefName<hldb::Interface>("bus_if", m_design->getAllInterfaces()), nullptr);
}

TEST_F(Instantiations, ProgramDefinition) {
  ASSERT_NE(hldb::findByDefName<hldb::Program>("my_prog", m_design->getAllPrograms()), nullptr);
}

TEST_F(Instantiations, UdpDefinition) {
  ASSERT_NE(hldb::findByName<hldb::UdpDefn>("my_udp", m_design->getAllUdps()), nullptr);
}

// ----
// Helper: return the RefInstance named 'instName' inside 'parent', or null.
// ----
static const hldb::RefInstance *findRefInst(std::string_view instName, const hldb::Module *parent) {
  return hldb::findByName<hldb::RefInstance>(instName, parent->getRefInstances());
}

// ----
// 2. RefInstances inside 'top' carry the right typespec type
// ----
TEST_F(Instantiations, InterfaceInstantiation) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  // Plain interface instantiation  -- isInterfaceElem("simple_if")
  const hldb::RefInstance *const u_simple_if = findRefInst("u_simple_if", top);
  ASSERT_NE(u_simple_if, nullptr) << "u_simple_if RefInstance not found";
  ASSERT_NE(u_simple_if->getTypespec(), nullptr) << "u_simple_if has no typespec";
  EXPECT_NE(u_simple_if->getTypespec()->getActual<hldb::InterfaceTypespec>(), nullptr)
      << "u_simple_if typespec is not InterfaceTypespec";

  // Parameterized interface instantiation -- isInterfaceElem("bus_if")
  const hldb::RefInstance *const u_bus = findRefInst("u_bus", top);
  ASSERT_NE(u_bus, nullptr) << "u_bus RefInstance not found";
  ASSERT_NE(u_bus->getTypespec(), nullptr) << "u_bus has no typespec";
  EXPECT_NE(u_bus->getTypespec()->getActual<hldb::InterfaceTypespec>(), nullptr)
      << "u_bus typespec is not InterfaceTypespec";
}

TEST_F(Instantiations, ModuleInstantiation) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  // Plain module instantiation -- isModuleElem("passthru")
  const hldb::RefInstance *const u_pt = findRefInst("u_pt", top);
  ASSERT_NE(u_pt, nullptr) << "u_pt RefInstance not found";
  ASSERT_NE(u_pt->getTypespec(), nullptr) << "u_pt has no typespec";
  EXPECT_NE(u_pt->getTypespec()->getActual<hldb::ModuleTypespec>(), nullptr) << "u_pt typespec is not ModuleTypespec";

  // Parameterized module instantiation -- isModuleElem("adder")
  const hldb::RefInstance *const u_add = findRefInst("u_add", top);
  ASSERT_NE(u_add, nullptr) << "u_add RefInstance not found";
  ASSERT_NE(u_add->getTypespec(), nullptr) << "u_add has no typespec";
  EXPECT_NE(u_add->getTypespec()->getActual<hldb::ModuleTypespec>(), nullptr) << "u_add typespec is not ModuleTypespec";
}

TEST_F(Instantiations, ProgramInstantiation) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  // Program instantiation -- isProgramElem("my_prog")
  const hldb::RefInstance *const u_prog = findRefInst("u_prog", top);
  ASSERT_NE(u_prog, nullptr) << "u_prog RefInstance not found";
  ASSERT_NE(u_prog->getTypespec(), nullptr) << "u_prog has no typespec";
  EXPECT_NE(u_prog->getTypespec()->getActual<hldb::ProgramTypespec>(), nullptr)
      << "u_prog typespec is not ProgramTypespec";
}

TEST_F(Instantiations, UnsupportedInstantiation) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  // Unsupported / black-box -- isUnsupportedElem("black_box")
  const hldb::RefInstance *const u_bb = findRefInst("u_bb", top);
  ASSERT_NE(u_bb, nullptr) << "u_bb RefInstance not found";
  ASSERT_NE(u_bb->getTypespec(), nullptr) << "u_bb has no typespec";
  EXPECT_NE(u_bb->getTypespec()->getActual<hldb::UnsupportedTypespec>(), nullptr)
      << "u_bb typespec is not UnsupportedTypespec";
}

// ----
// Macro-defined module: 'module macro_mod(); endmodule' is entirely inside a
// macro expansion so the preprocessor callbacks never fire.  The parser-side
// retroactive insertDesignElement must register it, making isModuleElem()
// return true so the instantiation gets ModuleTypespec (not UnsupportedTypespec).
// ----
TEST_F(Instantiations, MacroDefinedModuleInstantiation) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const hldb::RefInstance *const u_macro_mod = findRefInst("u_macro_mod", top);
  ASSERT_NE(u_macro_mod, nullptr) << "u_macro_mod RefInstance not found";
  ASSERT_NE(u_macro_mod->getTypespec(), nullptr) << "u_macro_mod has no typespec";
  EXPECT_NE(u_macro_mod->getTypespec()->getActual<hldb::ModuleTypespec>(), nullptr)
      << "u_macro_mod typespec is not ModuleTypespec -- "
         "retroactive insertDesignElement may have failed";
}

// ----
// 3. UDP instantiation -- Udp (extends Primitive) in top's primitives
// ----
TEST_F(Instantiations, UdpInstantiation) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPrimitives(), nullptr) << "top has no primitives";

  const hldb::Udp *u_udp = nullptr;
  for (const hldb::Primitive *const prim : *top->getPrimitives()) {
    if (const hldb::Udp *const udp = any_cast<const hldb::Udp *>(prim)) {
      if (udp->getName() == "u_udp") {
        u_udp = udp;
        break;
      }
    }
  }
  ASSERT_NE(u_udp, nullptr) << "u_udp Udp primitive not found in top";
  EXPECT_EQ(u_udp->getDefName(), "my_udp");
}

// ----
// 4. Gate primitives are present (keyword-driven, no predicate)
// ----
TEST_F(Instantiations, GatePrimitives) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPrimitives(), nullptr) << "top has no primitives";

  bool found_and = false, found_or = false, found_not = false;
  for (const hldb::Primitive *const prim : *top->getPrimitives()) {
    const std::string_view name = prim->getName();
    if (name == "g_and") found_and = true;
    if (name == "g_or") found_or = true;
    if (name == "g_not") found_not = true;
  }
  EXPECT_TRUE(found_and) << "gate 'and' instance g_and not found";
  EXPECT_TRUE(found_or) << "gate 'or'  instance g_or  not found";
  EXPECT_TRUE(found_not) << "gate 'not' instance g_not not found";
}

// ----
// 5. generate-for block:
//    gen_top->getGenStmts() -> GenRegion -> getStmt<GenFor>()
//                           -> getStmt<Begin>() -> getStmts() -> RefInstances
// ----
TEST_F(Instantiations, GenerateBlockInstantiations) {
  const hldb::Module *const gen_top = hldb::findByDefName<hldb::Module>("gen_top", m_design->getAllModules());
  ASSERT_NE(gen_top, nullptr);
  ASSERT_NE(gen_top->getGenStmts(), nullptr) << "gen_top has no gen statements";

  // GenRegion wraps the generate block
  const hldb::GenRegion *gen_region = nullptr;
  for (const hldb::Any *const stmt : *gen_top->getGenStmts()) {
    gen_region = any_cast<const hldb::GenRegion *>(stmt);
    if (gen_region != nullptr) break;
  }
  ASSERT_NE(gen_region, nullptr) << "No GenRegion in gen_top gen statements";

  // GenFor is the stmt of the GenRegion
  const hldb::GenFor *const gen_for = gen_region->getStmt<hldb::GenFor>();
  ASSERT_NE(gen_for, nullptr) << "GenRegion stmt is not a GenFor";

  // The named begin block (gen_lane) is the stmt of the GenFor
  const hldb::Begin *const body = gen_for->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "GenFor stmt is not a Begin";
  ASSERT_NE(body->getStmts(), nullptr) << "Begin body has no stmts";

  // RefInstances for u_if (interface) and u_pt (module) are in body->getStmts()
  bool found_if = false, found_mod = false;
  for (const hldb::Any *const s : *body->getStmts()) {
    const hldb::RefInstance *const ri = any_cast<const hldb::RefInstance *>(s);
    if (ri == nullptr || ri->getTypespec() == nullptr) continue;
    if (ri->getTypespec()->getActual<hldb::InterfaceTypespec>() != nullptr) found_if = true;
    if (ri->getTypespec()->getActual<hldb::ModuleTypespec>() != nullptr) found_mod = true;
  }
  EXPECT_TRUE(found_if) << "No InterfaceTypespec RefInstance in generate body";
  EXPECT_TRUE(found_mod) << "No ModuleTypespec RefInstance in generate body";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
