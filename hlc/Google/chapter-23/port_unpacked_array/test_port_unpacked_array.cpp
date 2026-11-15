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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2--port-unpacked-array.sv
//
// THE SOURCE
//
//   20: module m(input logic [7:0] mem [0:3]);
//   21: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// A port can be "a variable or net of any allowed data type, including an
// array" (23.2.2). The ANSI declaration "input logic [7:0] mem [0:3]" gives
// the port identifier mem an unpacked dimension ({ unpacked_dimension } in
// ansi_port_declaration, Syntax 23-4), so mem is an input port that is an
// unpacked array whose elements are logic [7:0]. Its port index is 0 (37.14
// detail 9).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error diagnostic
//   PortMemIsAnInput           one port mem, index 0, vpiInput
//   PortMemIsAnUnpackedArray   its typespec is an array typespec whose
//                              element typespec is a logic typespec
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The bounds [0:3] and [7:0]. Range modeling belongs to the data type
//     clauses (7.4), not to the port declaration.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/port.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typespec.h>
#include <hldb/vpi_user.h>

#include <algorithm>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.2--port-unpacked-array.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

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
}  // namespace

class PortUnpackedArrayTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "port_unpacked_array.hlc"}); }
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

  static const hldb::Port *findPortMem() {
    if ((m_design == nullptr) || (m_design->getAllModules() == nullptr)) return nullptr;
    for (const hldb::Module *module : *m_design->getAllModules()) {
      if (endsWith(module->getFile(), kSourceFileName) && (module->getPorts() != nullptr) &&
          (module->getPorts()->size() == 1)) {
        return module->getPorts()->front();
      }
    }
    return nullptr;
  }
};

TEST_F(PortUnpackedArrayTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(PortUnpackedArrayTest, PortMemIsAnInput) {
  const hldb::Port *const port = findPortMem();
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "mem");
  EXPECT_EQ(port->getPortIndex(), 0);
  EXPECT_EQ(port->getDirection(), vpiInput);
}

TEST_F(PortUnpackedArrayTest, PortMemIsAnUnpackedArray) {
  const hldb::Port *const port = findPortMem();
  ASSERT_NE(port, nullptr);
  const hldb::Typespec *const typespec = hldb::getTypespec(port);
  ASSERT_NE(typespec, nullptr);
  ASSERT_EQ(typespec->getAnyType(), hldb::AnyType::ArrayTypespec);

  const hldb::ArrayTypespec *const array = any_cast<hldb::ArrayTypespec>(typespec);
  ASSERT_NE(array, nullptr);
  const hldb::RefTypespec *const element = array->getElemTypespec();
  ASSERT_NE(element, nullptr);
  ASSERT_NE(element->getActual(), nullptr);
  EXPECT_EQ(element->getActual()->getAnyType(), hldb::AnyType::LogicTypespec);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
