// -*- c++ -*-

/*

 Copyright 2019-2020 Alain Dargelas

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

/*
 * File:   uhdm_forward_decl.h
 * Author:
 *
 * Created on May 06, 2020, 10:03 PM
 */

#ifndef UHDM_FORWARD_DECL_H
#define UHDM_FORWARD_DECL_H

namespace uhdm {
  class Any;
  class Alias;
  class Always;
  class AnyPattern;
  class ArrayExpr;
  class ArrayTypespec;
  class Assert;
  class AssignStmt;
  class Assignment;
  class Assume;
  class AtomicStmt;
  class Attribute;
  class Begin;
  class BindDirective;
  class BitSelect;
  class BitTypespec;
  class BreakStmt;
  class ByteTypespec;
  class CaseItem;
  class CaseProperty;
  class CasePropertyItem;
  class CaseStmt;
  class ChandleTypespec;
  class CheckerDecl;
  class CheckerInst;
  class CheckerInstPort;
  class CheckerPort;
  class ClassDefn;
  class ClassObj;
  class ClassTypespec;
  class Clause;
  class ClockedProperty;
  class ClockedSeq;
  class ClockingBlock;
  class ClockingIODecl;
  class Comment;
  class ConcurrentAssertions;
  class ConfigDecl;
  class ConfigRule;
  class Constant;
  class ConstrForeach;
  class ConstrIf;
  class ConstrIfElse;
  class Constraint;
  class ConstraintExpr;
  class ConstraintOrdering;
  class ContAssign;
  class ContAssignBit;
  class ContinueStmt;
  class Cover;
  class CoverBin;
  class CoverCross;
  class CoverGroup;
  class CoverPoint;
  class CoverageOption;
  class Deassign;
  class DefParam;
  class DelayControl;
  class DelayTerm;
  class Design;
  class Disable;
  class DisableFork;
  class Disables;
  class DistItem;
  class Distribution;
  class DoWhile;
  class EnumConst;
  class EnumTypespec;
  class EventControl;
  class EventStmt;
  class EventTypespec;
  class ExpectStmt;
  class Expr;
  class Extends;
  class FinalStmt;
  class ForStmt;
  class Force;
  class ForeachStmt;
  class ForeverStmt;
  class ForkStmt;
  class FuncCall;
  class Function;
  class FunctionDecl;
  class Gate;
  class GateArray;
  class GenCase;
  class GenFor;
  class GenIf;
  class GenIfElse;
  class GenRegion;
  class GenScope;
  class GenScopeArray;
  class GenStmt;
  class HierPath;
  class IODecl;
  class Identifier;
  class IfElse;
  class IfStmt;
  class ImmediateAssert;
  class ImmediateAssume;
  class ImmediateCover;
  class Implements;
  class Implication;
  class ImportTypespec;
  class IncludeStmt;
  class IndexedPartSelect;
  class Initial;
  class Instance;
  class InstanceArray;
  class IntTypespec;
  class IntegerTypespec;
  class Interface;
  class InterfaceArray;
  class InterfaceTFDecl;
  class InterfaceTypespec;
  class LetDecl;
  class LetExpr;
  class Library;
  class LogicTypespec;
  class LongIntTypespec;
  class MethodFuncCall;
  class MethodTaskCall;
  class ModPath;
  class Modport;
  class Module;
  class ModuleArray;
  class ModuleTypespec;
  class MulticlockSequenceExpr;
  class NamedEvent;
  class NamedEventArray;
  class Net;
  class NullStmt;
  class Operation;
  class OrderedWait;
  class Package;
  class PackageTypespec;
  class ParamAssign;
  class Parameter;
  class PartSelect;
  class PathTerm;
  class Port;
  class PortBit;
  class Ports;
  class PreprocMacroDefinition;
  class PreprocMacroInstance;
  class PrimTerm;
  class Primitive;
  class PrimitiveArray;
  class Process;
  class Program;
  class ProgramArray;
  class ProgramTypespec;
  class PropFormalDecl;
  class PropertyDecl;
  class PropertyInst;
  class PropertySpec;
  class PropertyTypespec;
  class Range;
  class RealTypespec;
  class RefInstance;
  class RefObj;
  class RefTypespec;
  class Reg;
  class RegArray;
  class Release;
  class Repeat;
  class RepeatControl;
  class Restrict;
  class ReturnStmt;
  class Scope;
  class Select;
  class SeqFormalDecl;
  class SequenceDecl;
  class SequenceInst;
  class SequenceTypespec;
  class ShortIntTypespec;
  class ShortRealTypespec;
  class SimpleExpr;
  class SoftDisable;
  class SourceFile;
  class SpecParam;
  class StringTypespec;
  class StructPattern;
  class StructTypespec;
  class SwitchArray;
  class SwitchTran;
  class SysFuncCall;
  class SysTaskCall;
  class TFCall;
  class TableEntry;
  class TaggedPattern;
  class Task;
  class TaskCall;
  class TaskDecl;
  class TaskFunc;
  class TaskFuncDecl;
  class Tchk;
  class TchkTerm;
  class Thread;
  class TimeTypespec;
  class TypeParameter;
  class TypedefTypespec;
  class Typespec;
  class TypespecMember;
  class Udp;
  class UdpArray;
  class UdpDefn;
  class UdpDefnTypespec;
  class UnionTypespec;
  class Uniqueness;
  class UnsupportedExpr;
  class UnsupportedStmt;
  class UnsupportedTypespec;
  class UserSystf;
  class VarSelect;
  class Variable;
  class VoidTypespec;
  class WaitFork;
  class WaitStmt;
  class Waits;
  class WhileStmt;
} // namespace uhdm

#endif  // UHDM_FORWARD_DECL_H
