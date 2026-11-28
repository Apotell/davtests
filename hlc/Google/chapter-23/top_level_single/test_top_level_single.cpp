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

// Test for tests/Google/chapter-23/23.3.1--top-level-single.sv
//
// THE SOURCE
//
//   19: module top;
//   20:   wire w;
//   21: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "Top-level modules are modules that are included in the SystemVerilog
// source text, but do not appear in any module instantiation statement"
// (23.3.1). "A design shall contain at least one top-level module. A
// top-level module is implicitly instantiated once, and its instance name is
// the same as the module name. Such an instance is called a top-level
// instance." (23.3.1). The object model of an instance (37.10) has a name, a
// definition name and a top-module flag, which HLDB gives as getName(),
// getDefName() and getTopModule(); getTopModule() is TRUE for a top-level
// module instance. The elaborated instance holds its own copy of
// the module's nets (37.10, relation net).
//
// THE MODEL
//
// HLC's graph is a definition model: it holds everything the source says,
// so that an instance model can be built from it. An instantiation is an
// unelaborated instance (Module::getRefInstances()) bound through its
// typespec to the definition it names, with its parameter and port
// connections bound to that definition's parameters and ports. Those
// bindings are checked unconditionally. The top-level instances and the
// instance tree exist only in an elaborated model (23.3); those checks are
// gated on m_design->getElaborated(), so they run once HLC elaborates.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors         no fatal, syntax or error event
//   TopIsNeverInstantiated        no instantiation names top (23.3.1)
//   TopDeclaresNetW               the definition top declares the net w
//   OneTopLevelInstance           elaborated model only: exactly one
//                                 top-level instance
//   TopInstanceIsNamedTop         elaborated model only: its getName() is
//                                 top, the module name
//   TopInstanceIsOfModuleTop      elaborated model only: its getDefName() is
//                                 top
//   TopInstanceIsATopModule       elaborated model only: its getTopModule()
//                                 is TRUE
//   TopInstanceHasNetW            elaborated model only: the instance holds
//                                 net w

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/case_item.h>
#include <hldb/design.h>
#include <hldb/gen_block.h>
#include <hldb/gen_case.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if.h>
#include <hldb/gen_if_else.h>
#include <hldb/gen_region.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_typespec.h>

#include <algorithm>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
// True for an error-class event: fatal, syntax or error severity.
bool isErrorClass(const Event &event) {
  const EventDefinition *const eventDefinition = hldb::findEventDefinition(hlc::getEventTable(), event.getType());
  if (eventDefinition == nullptr) return false;
  const EventSeverity severity = eventDefinition->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}

// One event as hldb::renderEvent() prints it, one line per output line, so that a
// failing test shows what was reported and where.
std::string describeEvent(const Event &event) {
  std::ostringstream oss;
  const std::vector<std::string> lines = hldb::renderEvent(event);
  std::copy(lines.cbegin(), lines.cend(), std::ostream_iterator<std::string>(oss, "\n"));
  return oss.str();
}
// The name of the definition an unelaborated instance is bound to (the module its typespec names), or "" when it
// is not bound to a module.
std::string_view boundDefinition(const hldb::RefInstance *instance) {
  if ((instance == nullptr) || (instance->getTypespec() == nullptr)) return "";
  const hldb::ModuleTypespec *const definition = instance->getTypespec()->getActual<hldb::ModuleTypespec>();
  return (definition == nullptr) ? std::string_view() : definition->getDefName();
}

// Adds to 'instances' the unelaborated instances that 'object' declares: a definition's own and those in its
// generate constructs, at any depth.
void collectRefInstances(const hldb::Any *object, std::vector<const hldb::RefInstance *> &instances) {
  if (object == nullptr) return;
  if (const hldb::RefInstance *const instance = any_cast<hldb::RefInstance>(object)) {
    instances.emplace_back(instance);
  } else if (const hldb::Module *const module = any_cast<hldb::Module>(object)) {
    if (module->getRefInstances() != nullptr) {
      for (const hldb::RefInstance *child : *module->getRefInstances()) collectRefInstances(child, instances);
    }
    if (module->getGenStmts() != nullptr) {
      for (const hldb::Any *statement : *module->getGenStmts()) collectRefInstances(statement, instances);
    }
  } else if (const hldb::GenRegion *const region = any_cast<hldb::GenRegion>(object)) {
    if (region->getStmts() != nullptr) {
      for (const hldb::Any *statement : *region->getStmts()) collectRefInstances(statement, instances);
    }
  } else if (const hldb::GenBlock *const block = any_cast<hldb::GenBlock>(object)) {
    if (block->getStmts() != nullptr) {
      for (const hldb::Any *statement : *block->getStmts()) collectRefInstances(statement, instances);
    }
  } else if (const hldb::GenIf *const genIf = any_cast<hldb::GenIf>(object)) {
    collectRefInstances(genIf->getStmt(), instances);
  } else if (const hldb::GenIfElse *const genIfElse = any_cast<hldb::GenIfElse>(object)) {
    collectRefInstances(genIfElse->getStmt(), instances);
    collectRefInstances(genIfElse->getElseStmt(), instances);
  } else if (const hldb::GenFor *const genFor = any_cast<hldb::GenFor>(object)) {
    collectRefInstances(genFor->getStmt(), instances);
  } else if (const hldb::GenCase *const genCase = any_cast<hldb::GenCase>(object)) {
    if (genCase->getCaseItems() != nullptr) {
      for (const hldb::CaseItem *item : *genCase->getCaseItems()) collectRefInstances(item->getStmt(), instances);
    }
  }
}

// The definitions that no module instantiation statement names (23.3.1), comma-separated in the order of
// getAllModules().
std::string neverInstantiated(const hldb::Design *design) {
  if ((design == nullptr) || (design->getAllModules() == nullptr)) return "<no design>";
  std::vector<const hldb::RefInstance *> instances;
  for (const hldb::Module *module : *design->getAllModules()) collectRefInstances(module, instances);
  std::string result;
  for (const hldb::Module *module : *design->getAllModules()) {
    const std::string_view name = module->getDefName();
    const bool named = std::any_of(instances.cbegin(), instances.cend(), [name](const hldb::RefInstance *instance) {
      return boundDefinition(instance) == name;
    });
    if (named) continue;
    if (!result.empty()) result += ", ";
    result += std::string(name);
  }
  return result;
}

}  // namespace

class TopLevelSingleTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "top_level_single.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string allEvents() {
    hlc::EventListener *const listener = m_session->getEventListener();
    std::ostringstream oss;
    for (const Event &event : listener->getEvents()) {
      if (isErrorClass(event)) oss << describeEvent(event);
    }
    return oss.str();
  }

  // The definition of the module named 'name', or nullptr.
  static const hldb::Module *findModule(std::string_view name) {
    if (m_design == nullptr) return nullptr;
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  // The unelaborated instance 'name' that the definition 'parent' declares, or nullptr.
  static const hldb::RefInstance *findRefInstance(std::string_view parent, std::string_view name) {
    const hldb::Module *const module = findModule(parent);
    return (module == nullptr) ? nullptr : hldb::findByName<hldb::RefInstance>(name, module->getRefInstances());
  }

  // The top-level instances (37.5 detail 1); there are some only in an elaborated model.
  static std::vector<const hldb::Module *> topInstances() {
    std::vector<const hldb::Module *> result;
    if ((m_design == nullptr) || (m_design->getTopModules() == nullptr)) return result;
    for (const hldb::Module *module : *m_design->getTopModules()) result.emplace_back(module);
    return result;
  }

  // The names of 'instances', comma-separated, for failure messages.
  static std::string namesOf(const std::vector<const hldb::Module *> &instances) {
    std::string result;
    for (const hldb::Module *instance : instances) {
      if (!result.empty()) result += ", ";
      result += std::string(instance->getName());
    }
    return result;
  }
};

TEST_F(TopLevelSingleTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

// "Top-level modules are modules that ... do not appear in any module instantiation statement" (23.3.1).
TEST_F(TopLevelSingleTest, TopIsNeverInstantiated) { EXPECT_EQ(neverInstantiated(m_design), "top"); }

TEST_F(TopLevelSingleTest, TopDeclaresNetW) {
  const hldb::Module *const top = findModule("top");
  ASSERT_NE(top, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("w", top->getNets()), nullptr);
}

TEST_F(TopLevelSingleTest, OneTopLevelInstance) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    const std::vector<const hldb::Module *> tops = topInstances();
    EXPECT_EQ(tops.size(), 1u) << "top-level instances: " << namesOf(tops);
  }
}

TEST_F(TopLevelSingleTest, TopInstanceIsNamedTop) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    const std::vector<const hldb::Module *> tops = topInstances();
    ASSERT_EQ(tops.size(), 1u) << "top-level instances: " << namesOf(tops);
    EXPECT_EQ(tops.front()->getName(), std::string_view("top"));
  }
}

TEST_F(TopLevelSingleTest, TopInstanceIsOfModuleTop) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    const std::vector<const hldb::Module *> tops = topInstances();
    ASSERT_EQ(tops.size(), 1u) << "top-level instances: " << namesOf(tops);
    EXPECT_EQ(tops.front()->getDefName(), std::string_view("top"));
  }
}

TEST_F(TopLevelSingleTest, TopInstanceIsATopModule) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    const std::vector<const hldb::Module *> tops = topInstances();
    ASSERT_EQ(tops.size(), 1u) << "top-level instances: " << namesOf(tops);
    EXPECT_TRUE(tops.front()->getTopModule());
  }
}

TEST_F(TopLevelSingleTest, TopInstanceHasNetW) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    const std::vector<const hldb::Module *> tops = topInstances();
    ASSERT_EQ(tops.size(), 1u) << "top-level instances: " << namesOf(tops);
    const hldb::NetCollection *const nets = tops.front()->getNets();
    ASSERT_NE(nets, nullptr);
    const bool found =
        std::any_of(nets->cbegin(), nets->cend(), [](const hldb::Net *net) { return net->getName() == "w"; });
    EXPECT_TRUE(found);
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
