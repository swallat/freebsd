//===-- SPUAsmPrinter.cpp - Print machine instrs to Cell SPU assembly -------=//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains a printer that converts from our internal representation
// of machine-dependent LLVM code to Cell SPU assembly language. This printer
// is the output mechanism used by `llc'.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "asmprinter"
#include "SPU.h"
#include "SPUTargetMachine.h"
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Module.h"
#include "llvm/Assembly/Writer.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/DwarfWriter.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Target/TargetLoweringObjectFile.h"
#include "llvm/Target/TargetInstrInfo.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Target/TargetRegisterInfo.h"
#include "llvm/Target/TargetRegistry.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/Mangler.h"
#include "llvm/Support/MathExtras.h"
#include <set>
using namespace llvm;

namespace {
  STATISTIC(EmittedInsts, "Number of machine instrs printed");

  const std::string bss_section(".bss");

  class SPUAsmPrinter : public AsmPrinter {
    std::set<std::string> FnStubs, GVStubs;
  public:
    explicit SPUAsmPrinter(formatted_raw_ostream &O, TargetMachine &TM,
                           const MCAsmInfo *T, bool V) :
      AsmPrinter(O, TM, T, V) {}

    virtual const char *getPassName() const {
      return "STI CBEA SPU Assembly Printer";
    }

    SPUTargetMachine &getTM() {
      return static_cast<SPUTargetMachine&>(TM);
    }

    /// printInstruction - This method is automatically generated by tablegen
    /// from the instruction set description.
    void printInstruction(const MachineInstr *MI);
    static const char *getRegisterName(unsigned RegNo);


    void printMachineInstruction(const MachineInstr *MI);
    void printOp(const MachineOperand &MO);

    /// printRegister - Print register according to target requirements.
    ///
    void printRegister(const MachineOperand &MO, bool R0AsZero) {
      unsigned RegNo = MO.getReg();
      assert(TargetRegisterInfo::isPhysicalRegister(RegNo) &&
             "Not physreg??");
      O << getRegisterName(RegNo);
    }

    void printOperand(const MachineInstr *MI, unsigned OpNo) {
      const MachineOperand &MO = MI->getOperand(OpNo);
      if (MO.isReg()) {
        O << getRegisterName(MO.getReg());
      } else if (MO.isImm()) {
        O << MO.getImm();
      } else {
        printOp(MO);
      }
    }

    bool PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                         unsigned AsmVariant, const char *ExtraCode);
    bool PrintAsmMemoryOperand(const MachineInstr *MI, unsigned OpNo,
                               unsigned AsmVariant, const char *ExtraCode);


    void
    printS7ImmOperand(const MachineInstr *MI, unsigned OpNo)
    {
      int value = MI->getOperand(OpNo).getImm();
      value = (value << (32 - 7)) >> (32 - 7);

      assert((value >= -(1 << 8) && value <= (1 << 7) - 1)
             && "Invalid s7 argument");
      O << value;
    }

    void
    printU7ImmOperand(const MachineInstr *MI, unsigned OpNo)
    {
      unsigned int value = MI->getOperand(OpNo).getImm();
      assert(value < (1 << 8) && "Invalid u7 argument");
      O << value;
    }

    void
    printShufAddr(const MachineInstr *MI, unsigned OpNo)
    {
      char value = MI->getOperand(OpNo).getImm();
      O << (int) value;
      O << "(";
      printOperand(MI, OpNo+1);
      O << ")";
    }

    void
    printS16ImmOperand(const MachineInstr *MI, unsigned OpNo)
    {
      O << (short) MI->getOperand(OpNo).getImm();
    }

    void
    printU16ImmOperand(const MachineInstr *MI, unsigned OpNo)
    {
      O << (unsigned short)MI->getOperand(OpNo).getImm();
    }

    void
    printU32ImmOperand(const MachineInstr *MI, unsigned OpNo)
    {
      O << (unsigned)MI->getOperand(OpNo).getImm();
    }

    void
    printMemRegReg(const MachineInstr *MI, unsigned OpNo) {
      // When used as the base register, r0 reads constant zero rather than
      // the value contained in the register.  For this reason, the darwin
      // assembler requires that we print r0 as 0 (no r) when used as the base.
      const MachineOperand &MO = MI->getOperand(OpNo);
      O << getRegisterName(MO.getReg()) << ", ";
      printOperand(MI, OpNo+1);
    }

    void
    printU18ImmOperand(const MachineInstr *MI, unsigned OpNo)
    {
      unsigned int value = MI->getOperand(OpNo).getImm();
      assert(value <= (1 << 19) - 1 && "Invalid u18 argument");
      O << value;
    }

    void
    printS10ImmOperand(const MachineInstr *MI, unsigned OpNo)
    {
      short value = (short) (((int) MI->getOperand(OpNo).getImm() << 16)
                             >> 16);
      assert((value >= -(1 << 9) && value <= (1 << 9) - 1)
             && "Invalid s10 argument");
      O << value;
    }

    void
    printU10ImmOperand(const MachineInstr *MI, unsigned OpNo)
    {
      short value = (short) (((int) MI->getOperand(OpNo).getImm() << 16)
                             >> 16);
      assert((value <= (1 << 10) - 1) && "Invalid u10 argument");
      O << value;
    }

    void
    printDFormAddr(const MachineInstr *MI, unsigned OpNo)
    {
      assert(MI->getOperand(OpNo).isImm() &&
             "printDFormAddr first operand is not immediate");
      int64_t value = int64_t(MI->getOperand(OpNo).getImm());
      int16_t value16 = int16_t(value);
      assert((value16 >= -(1 << (9+4)) && value16 <= (1 << (9+4)) - 1)
             && "Invalid dform s10 offset argument");
      O << (value16 & ~0xf) << "(";
      printOperand(MI, OpNo+1);
      O << ")";
    }

    void
    printAddr256K(const MachineInstr *MI, unsigned OpNo)
    {
      /* Note: operand 1 is an offset or symbol name. */
      if (MI->getOperand(OpNo).isImm()) {
        printS16ImmOperand(MI, OpNo);
      } else {
        printOp(MI->getOperand(OpNo));
        if (MI->getOperand(OpNo+1).isImm()) {
          int displ = int(MI->getOperand(OpNo+1).getImm());
          if (displ > 0)
            O << "+" << displ;
          else if (displ < 0)
            O << displ;
        }
      }
    }

    void printCallOperand(const MachineInstr *MI, unsigned OpNo) {
      printOp(MI->getOperand(OpNo));
    }

    void printPCRelativeOperand(const MachineInstr *MI, unsigned OpNo) {
      // Used to generate a ".-<target>", but it turns out that the assembler
      // really wants the target.
      //
      // N.B.: This operand is used for call targets. Branch hints are another
      // animal entirely.
      printOp(MI->getOperand(OpNo));
    }

    void printHBROperand(const MachineInstr *MI, unsigned OpNo) {
      // HBR operands are generated in front of branches, hence, the
      // program counter plus the target.
      O << ".+";
      printOp(MI->getOperand(OpNo));
    }

    void printSymbolHi(const MachineInstr *MI, unsigned OpNo) {
      if (MI->getOperand(OpNo).isImm()) {
        printS16ImmOperand(MI, OpNo);
      } else {
        printOp(MI->getOperand(OpNo));
        O << "@h";
      }
    }

    void printSymbolLo(const MachineInstr *MI, unsigned OpNo) {
      if (MI->getOperand(OpNo).isImm()) {
        printS16ImmOperand(MI, OpNo);
      } else {
        printOp(MI->getOperand(OpNo));
        O << "@l";
      }
    }

    /// Print local store address
    void printSymbolLSA(const MachineInstr *MI, unsigned OpNo) {
      printOp(MI->getOperand(OpNo));
    }

    void printROTHNeg7Imm(const MachineInstr *MI, unsigned OpNo) {
      if (MI->getOperand(OpNo).isImm()) {
        int value = (int) MI->getOperand(OpNo).getImm();
        assert((value >= 0 && value < 16)
               && "Invalid negated immediate rotate 7-bit argument");
        O << -value;
      } else {
        llvm_unreachable("Invalid/non-immediate rotate amount in printRotateNeg7Imm");
      }
    }

    void printROTNeg7Imm(const MachineInstr *MI, unsigned OpNo) {
      if (MI->getOperand(OpNo).isImm()) {
        int value = (int) MI->getOperand(OpNo).getImm();
        assert((value >= 0 && value <= 32)
               && "Invalid negated immediate rotate 7-bit argument");
        O << -value;
      } else {
        llvm_unreachable("Invalid/non-immediate rotate amount in printRotateNeg7Imm");
      }
    }

    virtual bool runOnMachineFunction(MachineFunction &F) = 0;
  };

  /// LinuxAsmPrinter - SPU assembly printer, customized for Linux
  class LinuxAsmPrinter : public SPUAsmPrinter {
  public:
    explicit LinuxAsmPrinter(formatted_raw_ostream &O, TargetMachine &TM,
                             const MCAsmInfo *T, bool V)
      : SPUAsmPrinter(O, TM, T, V) {}

    virtual const char *getPassName() const {
      return "STI CBEA SPU Assembly Printer";
    }

    bool runOnMachineFunction(MachineFunction &F);

    void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesAll();
      AU.addRequired<MachineModuleInfo>();
      AU.addRequired<DwarfWriter>();
      SPUAsmPrinter::getAnalysisUsage(AU);
    }

    //! Emit a global variable according to its section and type
    void PrintGlobalVariable(const GlobalVariable* GVar);
  };
} // end of anonymous namespace

// Include the auto-generated portion of the assembly writer
#include "SPUGenAsmWriter.inc"

void SPUAsmPrinter::printOp(const MachineOperand &MO) {
  switch (MO.getType()) {
  case MachineOperand::MO_Immediate:
    llvm_report_error("printOp() does not handle immediate values");
    return;

  case MachineOperand::MO_MachineBasicBlock:
    GetMBBSymbol(MO.getMBB()->getNumber())->print(O, MAI);
    return;
  case MachineOperand::MO_JumpTableIndex:
    O << MAI->getPrivateGlobalPrefix() << "JTI" << getFunctionNumber()
      << '_' << MO.getIndex();
    return;
  case MachineOperand::MO_ConstantPoolIndex:
    O << MAI->getPrivateGlobalPrefix() << "CPI" << getFunctionNumber()
      << '_' << MO.getIndex();
    return;
  case MachineOperand::MO_ExternalSymbol:
    // Computing the address of an external symbol, not calling it.
    if (TM.getRelocationModel() != Reloc::Static) {
      std::string Name(MAI->getGlobalPrefix()); Name += MO.getSymbolName();
      GVStubs.insert(Name);
      O << "L" << Name << "$non_lazy_ptr";
      return;
    }
    O << MAI->getGlobalPrefix() << MO.getSymbolName();
    return;
  case MachineOperand::MO_GlobalAddress: {
    // Computing the address of a global symbol, not calling it.
    GlobalValue *GV = MO.getGlobal();
    std::string Name = Mang->getMangledName(GV);

    // External or weakly linked global variables need non-lazily-resolved
    // stubs
    if (TM.getRelocationModel() != Reloc::Static) {
      if (((GV->isDeclaration() || GV->hasWeakLinkage() ||
            GV->hasLinkOnceLinkage() || GV->hasCommonLinkage()))) {
        GVStubs.insert(Name);
        O << "L" << Name << "$non_lazy_ptr";
        return;
      }
    }
    O << Name;
    return;
  }

  default:
    O << "<unknown operand type: " << MO.getType() << ">";
    return;
  }
}

/// PrintAsmOperand - Print out an operand for an inline asm expression.
///
bool SPUAsmPrinter::PrintAsmOperand(const MachineInstr *MI, unsigned OpNo,
                                    unsigned AsmVariant,
                                    const char *ExtraCode) {
  // Does this asm operand have a single letter operand modifier?
  if (ExtraCode && ExtraCode[0]) {
    if (ExtraCode[1] != 0) return true; // Unknown modifier.

    switch (ExtraCode[0]) {
    default: return true;  // Unknown modifier.
    case 'L': // Write second word of DImode reference.
      // Verify that this operand has two consecutive registers.
      if (!MI->getOperand(OpNo).isReg() ||
          OpNo+1 == MI->getNumOperands() ||
          !MI->getOperand(OpNo+1).isReg())
        return true;
      ++OpNo;   // Return the high-part.
      break;
    }
  }

  printOperand(MI, OpNo);
  return false;
}

bool SPUAsmPrinter::PrintAsmMemoryOperand(const MachineInstr *MI,
                                          unsigned OpNo,
                                          unsigned AsmVariant,
                                          const char *ExtraCode) {
  if (ExtraCode && ExtraCode[0])
    return true; // Unknown modifier.
  printMemRegReg(MI, OpNo);
  return false;
}

/// printMachineInstruction -- Print out a single PowerPC MI in Darwin syntax
/// to the current output stream.
///
void SPUAsmPrinter::printMachineInstruction(const MachineInstr *MI) {
  ++EmittedInsts;
  processDebugLoc(MI, true);
  printInstruction(MI);
  if (VerboseAsm)
    EmitComments(*MI);
  processDebugLoc(MI, false);
  O << '\n';
}

/// runOnMachineFunction - This uses the printMachineInstruction()
/// method to print assembly for each instruction.
///
bool LinuxAsmPrinter::runOnMachineFunction(MachineFunction &MF) {
  this->MF = &MF;

  SetupMachineFunction(MF);
  O << "\n\n";

  // Print out constants referenced by the function
  EmitConstantPool(MF.getConstantPool());

  // Print out labels for the function.
  const Function *F = MF.getFunction();

  OutStreamer.SwitchSection(getObjFileLowering().SectionForGlobal(F, Mang, TM));
  EmitAlignment(MF.getAlignment(), F);

  switch (F->getLinkage()) {
  default: llvm_unreachable("Unknown linkage type!");
  case Function::PrivateLinkage:
  case Function::LinkerPrivateLinkage:
  case Function::InternalLinkage:  // Symbols default to internal.
    break;
  case Function::ExternalLinkage:
    O << "\t.global\t" << CurrentFnName << "\n"
      << "\t.type\t" << CurrentFnName << ", @function\n";
    break;
  case Function::WeakAnyLinkage:
  case Function::WeakODRLinkage:
  case Function::LinkOnceAnyLinkage:
  case Function::LinkOnceODRLinkage:
    O << "\t.global\t" << CurrentFnName << "\n";
    O << "\t.weak_definition\t" << CurrentFnName << "\n";
    break;
  }
  O << CurrentFnName << ":\n";

  // Emit pre-function debug information.
  DW->BeginFunction(&MF);

  // Print out code for the function.
  for (MachineFunction::const_iterator I = MF.begin(), E = MF.end();
       I != E; ++I) {
    // Print a label for the basic block.
    if (I != MF.begin()) {
      EmitBasicBlockStart(I);
    }
    for (MachineBasicBlock::const_iterator II = I->begin(), E = I->end();
         II != E; ++II) {
      // Print the assembly for the instruction.
      printMachineInstruction(II);
    }
  }

  O << "\t.size\t" << CurrentFnName << ",.-" << CurrentFnName << "\n";

  // Print out jump tables referenced by the function.
  EmitJumpTableInfo(MF.getJumpTableInfo(), MF);

  // Emit post-function debug information.
  DW->EndFunction(&MF);

  // We didn't modify anything.
  return false;
}


/*!
  Emit a global variable according to its section, alignment, etc.

  \note This code was shamelessly copied from the PowerPC's assembly printer,
  which sort of screams for some kind of refactorization of common code.
 */
void LinuxAsmPrinter::PrintGlobalVariable(const GlobalVariable *GVar) {
  const TargetData *TD = TM.getTargetData();

  if (!GVar->hasInitializer())
    return;

  // Check to see if this is a special global used by LLVM, if so, emit it.
  if (EmitSpecialLLVMGlobal(GVar))
    return;

  std::string name = Mang->getMangledName(GVar);

  printVisibility(name, GVar->getVisibility());

  Constant *C = GVar->getInitializer();
  const Type *Type = C->getType();
  unsigned Size = TD->getTypeAllocSize(Type);
  unsigned Align = TD->getPreferredAlignmentLog(GVar);

  OutStreamer.SwitchSection(getObjFileLowering().SectionForGlobal(GVar, Mang,
                                                                  TM));

  if (C->isNullValue() && /* FIXME: Verify correct */
      !GVar->hasSection() &&
      (GVar->hasLocalLinkage() || GVar->hasExternalLinkage() ||
       GVar->isWeakForLinker())) {
      if (Size == 0) Size = 1;   // .comm Foo, 0 is undefined, avoid it.

      if (GVar->hasExternalLinkage()) {
        O << "\t.global " << name << '\n';
        O << "\t.type " << name << ", @object\n";
        O << name << ":\n";
        O << "\t.zero " << Size << '\n';
      } else if (GVar->hasLocalLinkage()) {
        O << MAI->getLCOMMDirective() << name << ',' << Size;
      } else {
        O << ".comm " << name << ',' << Size;
      }
      O << "\t\t" << MAI->getCommentString() << " '";
      WriteAsOperand(O, GVar, /*PrintType=*/false, GVar->getParent());
      O << "'\n";
      return;
  }

  switch (GVar->getLinkage()) {
    // Should never be seen for the CellSPU platform...
   case GlobalValue::LinkOnceAnyLinkage:
   case GlobalValue::LinkOnceODRLinkage:
   case GlobalValue::WeakAnyLinkage:
   case GlobalValue::WeakODRLinkage:
   case GlobalValue::CommonLinkage:
    O << "\t.global " << name << '\n'
      << "\t.type " << name << ", @object\n"
      << "\t.weak " << name << '\n';
    break;
   case GlobalValue::AppendingLinkage:
    // FIXME: appending linkage variables should go into a section of
    // their name or something.  For now, just emit them as external.
   case GlobalValue::ExternalLinkage:
    // If external or appending, declare as a global symbol
    O << "\t.global " << name << '\n'
      << "\t.type " << name << ", @object\n";
    // FALL THROUGH
   case GlobalValue::PrivateLinkage:
   case GlobalValue::LinkerPrivateLinkage:
   case GlobalValue::InternalLinkage:
    break;
   default:
    llvm_report_error("Unknown linkage type!");
  }

  EmitAlignment(Align, GVar);
  O << name << ":\t\t\t\t" << MAI->getCommentString() << " '";
  WriteAsOperand(O, GVar, /*PrintType=*/false, GVar->getParent());
  O << "'\n";

  EmitGlobalConstant(C);
  O << '\n';
}

// Force static initialization.
extern "C" void LLVMInitializeCellSPUAsmPrinter() { 
  RegisterAsmPrinter<LinuxAsmPrinter> X(TheCellSPUTarget);
}
