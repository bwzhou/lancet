#include <iostream>
#include <fstream>

#include "expr/Lexer.h"
#include "expr/Parser.h"

#include "klee/Config/Version.h"
#include "klee/Constraints.h"
#include "klee/Expr.h"
#include "klee/ExprBuilder.h"
#include "klee/Solver.h"
#include "klee/SolverImpl.h"
#include "klee/Statistics.h"
#include "klee/CommandLine.h"
#include "klee/Common.h"
#include "klee/util/ExprPPrinter.h"
#include "klee/util/ExprVisitor.h"

#include "klee/util/ExprSMTLIBLetPrinter.h"

#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#include <sys/stat.h>
#include <unistd.h>

// FIXME: Ugh, this is gross. But otherwise our config.h conflicts with LLVMs.
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#include "llvm/Support/Signals.h"
#include "llvm/Support/system_error.h"

using namespace llvm;
using namespace klee;
using namespace klee::expr;

#ifdef SUPPORT_METASMT

#include <metaSMT/DirectSolver_Context.hpp>
#include <metaSMT/backend/Z3_Backend.hpp>
#include <metaSMT/backend/Boolector.hpp>

#define Expr VCExpr
#define Type VCType
#define STP STP_Backend
#include <metaSMT/backend/STP.hpp>
#undef Expr
#undef Type
#undef STP

using namespace metaSMT;
using namespace metaSMT::solver;

#endif /* SUPPORT_METASMT */




namespace {
  llvm::cl::opt<std::string>
  InputFile(llvm::cl::desc("<input query log>"), llvm::cl::Positional,
            llvm::cl::init("-"));

  llvm::cl::opt<std::string>
  SecondFile(llvm::cl::desc("<second query log>"), llvm::cl::Positional,
             llvm::cl::init("-"));

  llvm::cl::opt<std::string>
  FeatureFile(llvm::cl::desc("<feature output>"), llvm::cl::Positional,
              llvm::cl::init("-"));

  enum ToolActions {
    PrintTokens,
    PrintAST,
    PrintSMTLIBv2,
    Evaluate,
    Compare
  };

  static llvm::cl::opt<ToolActions> 
  ToolAction(llvm::cl::desc("Tool actions:"),
             llvm::cl::init(Evaluate),
             llvm::cl::values(
             clEnumValN(PrintTokens, "print-tokens",
                        "Print tokens from the input file."),
             clEnumValN(PrintSMTLIBv2, "print-smtlib",
                        "Print parsed input file as SMT-LIBv2 query."),
             clEnumValN(PrintAST, "print-ast",
                        "Print parsed AST nodes from the input file."),
             clEnumValN(Evaluate, "evaluate",
                        "Print parsed AST nodes from the input file."),
             clEnumValN(Compare, "compare",
                        "Compare two input query files."),
             clEnumValEnd));


  enum BuilderKinds {
    DefaultBuilder,
    ConstantFoldingBuilder,
    SimplifyingBuilder
  };

  static llvm::cl::opt<BuilderKinds> 
  BuilderKind("builder",
              llvm::cl::desc("Expression builder:"),
              llvm::cl::init(DefaultBuilder),
              llvm::cl::values(
              clEnumValN(DefaultBuilder, "default",
                         "Default expression construction."),
              clEnumValN(ConstantFoldingBuilder, "constant-folding",
                         "Fold constant expressions."),
              clEnumValN(SimplifyingBuilder, "simplify",
                         "Fold constants and simplify expressions."),
              clEnumValEnd));

  cl::opt<bool>
  UseDummySolver("use-dummy-solver",
                 cl::init(false));

  llvm::cl::opt<std::string> directoryToWriteQueryLogs("query-log-dir",
      llvm::cl::desc("The folder to write query logs to. Defaults is current working directory."),
      llvm::cl::init("."));

}

static std::string getQueryLogPath(const char filename[])
{
	//check directoryToWriteLogs exists
	struct stat s;
	if( !(stat(directoryToWriteQueryLogs.c_str(),&s) == 0 && S_ISDIR(s.st_mode)) )
	{
		std::cerr << "Directory to log queries \"" << directoryToWriteQueryLogs << "\" does not exist!" << std::endl;
		exit(1);
	}

	//check permissions okay
	if( !( (s.st_mode & S_IWUSR) && getuid() == s.st_uid) &&
	    !( (s.st_mode & S_IWGRP) && getgid() == s.st_gid) &&
	    !( s.st_mode & S_IWOTH)
	)
	{
		std::cerr << "Directory to log queries \"" << directoryToWriteQueryLogs << "\" is not writable!" << std::endl;
		exit(1);
	}

	std::string path=directoryToWriteQueryLogs;
	path+="/";
	path+=filename;
	return path;
}

static std::string escapedString(const char *start, unsigned length) {
  std::string Str;
  llvm::raw_string_ostream s(Str);
  for (unsigned i=0; i<length; ++i) {
    char c = start[i];
    if (isprint(c)) {
      s << c;
    } else if (c == '\n') {
      s << "\\n";
    } else {
      s << "\\x" 
        << hexdigit(((unsigned char) c >> 4) & 0xF) 
        << hexdigit((unsigned char) c & 0xF);
    }
  }
  return s.str();
}

static void PrintInputTokens(const MemoryBuffer *MB) {
  Lexer L(MB);
  Token T;
  do {
    L.Lex(T);
    std::cout << "(Token \"" << T.getKindName() << "\" "
               << "\"" << escapedString(T.start, T.length) << "\" "
               << T.length << " "
               << T.line << " " << T.column << ")\n";
  } while (T.kind != Token::EndOfFile);
}

static bool PrintInputAST(const char *Filename,
                          const MemoryBuffer *MB,
                          ExprBuilder *Builder) {
  std::vector<Decl*> Decls;
  Parser *P = Parser::Create(Filename, MB, Builder);
  P->SetMaxErrors(20);

  unsigned NumQueries = 0;
  while (Decl *D = P->ParseTopLevelDecl()) {
    if (!P->GetNumErrors()) {
      if (isa<QueryCommand>(D))
        std::cout << "# Query " << ++NumQueries << "\n";

      D->dump();
    }
    Decls.push_back(D);
  }

  bool success = true;
  if (unsigned N = P->GetNumErrors()) {
    std::cerr << Filename << ": parse failure: "
               << N << " errors.\n";
    success = false;
  }

  for (std::vector<Decl*>::iterator it = Decls.begin(),
         ie = Decls.end(); it != ie; ++it)
    delete *it;

  delete P;

  return success;
}

static bool EvaluateInputAST(const char *Filename,
                             const MemoryBuffer *MB,
                             ExprBuilder *Builder) {
  std::vector<Decl*> Decls;
  Parser *P = Parser::Create(Filename, MB, Builder);
  P->SetMaxErrors(20);
  while (Decl *D = P->ParseTopLevelDecl()) {
    Decls.push_back(D);
  }

  bool success = true;
  if (unsigned N = P->GetNumErrors()) {
    std::cerr << Filename << ": parse failure: "
               << N << " errors.\n";
    success = false;
  }  

  if (!success)
    return false;

  // FIXME: Support choice of solver.
  Solver *coreSolver = NULL; // 
  
#ifdef SUPPORT_METASMT
  if (UseMetaSMT != METASMT_BACKEND_NONE) {
    
    std::string backend;
    
    switch (UseMetaSMT) {
          case METASMT_BACKEND_STP:
              backend = "STP"; 
              coreSolver = new MetaSMTSolver< DirectSolver_Context < STP_Backend > >(UseForkedCoreSolver, CoreSolverOptimizeDivides);
              break;
          case METASMT_BACKEND_Z3:
              backend = "Z3";
              coreSolver = new MetaSMTSolver< DirectSolver_Context < Z3_Backend > >(UseForkedCoreSolver, CoreSolverOptimizeDivides);
              break;
          case METASMT_BACKEND_BOOLECTOR:
              backend = "Boolector";
              coreSolver = new MetaSMTSolver< DirectSolver_Context < Boolector > >(UseForkedCoreSolver, CoreSolverOptimizeDivides);
              break;
          default:
              assert(false);
              break;
    };
    std::cerr << "Starting MetaSMTSolver(" << backend << ") ...\n";
  }
  else {
    coreSolver = UseDummySolver ? createDummySolver() : new STPSolver(UseForkedCoreSolver);
  }
#else
  coreSolver = UseDummySolver ? createDummySolver() : new STPSolver(UseForkedCoreSolver);
#endif /* SUPPORT_METASMT */
  
  
  if (!UseDummySolver) {
    if (0 != MaxCoreSolverTime) {
      coreSolver->setCoreSolverTimeout(MaxCoreSolverTime);
    }
  }

  Solver *S = constructSolverChain(coreSolver,
                                   getQueryLogPath(ALL_QUERIES_SMT2_FILE_NAME),
                                   getQueryLogPath(SOLVER_QUERIES_SMT2_FILE_NAME),
                                   getQueryLogPath(ALL_QUERIES_PC_FILE_NAME),
                                   getQueryLogPath(SOLVER_QUERIES_PC_FILE_NAME));

  unsigned Index = 0;
  for (std::vector<Decl*>::iterator it = Decls.begin(),
         ie = Decls.end(); it != ie; ++it) {
    Decl *D = *it;
    if (QueryCommand *QC = dyn_cast<QueryCommand>(D)) {
      std::cout << "Query " << Index << ":\t";

      assert("FIXME: Support counterexample query commands!");
      if (QC->Values.empty() && QC->Objects.empty()) {
        bool result;
        if (S->mustBeTrue(Query(ConstraintManager(QC->Constraints), QC->Query),
                          result)) {
          std::cout << (result ? "VALID" : "INVALID");
        } else {
          std::cout << "FAIL (reason: " 
                    << SolverImpl::getOperationStatusString(S->impl->getOperationStatusCode())
                    << ")";
        }
      } else if (!QC->Values.empty()) {
        assert(QC->Objects.empty() && 
               "FIXME: Support counterexamples for values and objects!");
        assert(QC->Values.size() == 1 &&
               "FIXME: Support counterexamples for multiple values!");
        assert(QC->Query->isFalse() &&
               "FIXME: Support counterexamples with non-trivial query!");
        ref<ConstantExpr> result;
        if (S->getValue(Query(ConstraintManager(QC->Constraints), 
                              QC->Values[0]),
                        result)) {
          std::cout << "INVALID\n";
          std::cout << "\tExpr 0:\t" << result;
        } else {
          std::cout << "FAIL (reason: " 
                    << SolverImpl::getOperationStatusString(S->impl->getOperationStatusCode())
                    << ")";
        }
      } else {
        std::vector< std::vector<unsigned char> > result;
        
        if (S->getInitialValues(Query(ConstraintManager(QC->Constraints), 
                                      QC->Query),
                                QC->Objects, result)) {
          std::cout << "INVALID\n";

          for (unsigned i = 0, e = result.size(); i != e; ++i) {
            std::cout << "\tArray " << i << ":\t"
                       << QC->Objects[i]->name
                       << "[";
            for (unsigned j = 0; j != QC->Objects[i]->size; ++j) {
              std::cout << (unsigned) result[i][j];
              if (j + 1 != QC->Objects[i]->size)
                std::cout << ", ";
            }
            std::cout << "]";
            if (i + 1 != e)
              std::cout << "\n";
          }
        } else {
          SolverImpl::SolverRunStatus retCode = S->impl->getOperationStatusCode();
          if (SolverImpl::SOLVER_RUN_STATUS_TIMEOUT == retCode) {
            std::cout << " FAIL (reason: " 
                      << SolverImpl::getOperationStatusString(retCode)
                      << ")";
          }           
          else {
            std::cout << "VALID (counterexample request ignored)";
          }
        }
      }

      std::cout << "\n";
      ++Index;
    }
  }

  for (std::vector<Decl*>::iterator it = Decls.begin(),
         ie = Decls.end(); it != ie; ++it)
    delete *it;
  delete P;

  delete S;

  if (uint64_t queries = *theStatisticManager->getStatisticByName("Queries")) {
    std::cout 
      << "--\n"
      << "total queries = " << queries << "\n"
      << "total queries constructs = " 
      << *theStatisticManager->getStatisticByName("QueriesConstructs") << "\n"
      << "valid queries = " 
      << *theStatisticManager->getStatisticByName("QueriesValid") << "\n"
      << "invalid queries = " 
      << *theStatisticManager->getStatisticByName("QueriesInvalid") << "\n"
      << "query cex = " 
      << *theStatisticManager->getStatisticByName("QueriesCEX") << "\n";
  }

  return success;
}

static bool ParseDecls(std::vector<Decl*> *Decls,
                       const char *Filename,
                       const MemoryBuffer *MB,
                       ExprBuilder *Builder) {
  Parser *P = Parser::Create(Filename, MB, Builder);
  P->SetMaxErrors(20);
  while (Decl *D = P->ParseTopLevelDecl()) {
    Decls->push_back(D);
  }

  bool success = true;
  if (unsigned N = P->GetNumErrors()) {
    std::cerr << Filename << ": parse failure: "
              << N << " errors.\n";
    success = false;
  }  

  delete P;

  return success;
}

static bool SimplifyConstraints(const std::vector<ExprHandle> &constraints,
                                std::vector<ExprHandle> *simplified,
                                Solver *S) {
  std::set<ExprHandle> subsumed;
  for (std::vector<ExprHandle>::const_iterator it = constraints.begin(),
       ie = constraints.end(); it != ie; ++it) {
    //std::cerr << "it:" << std::endl;
    //(*it)->dump();
    std::vector<ExprHandle> assumption;
    assumption.push_back(*it);
    ConstraintManager assume(assumption);
    // subsequent constraints is stronger by definition
    for (std::vector<ExprHandle>::const_iterator jt = constraints.begin();
         jt != it; ++jt) {
      //std::cerr << "jt:" << std::endl;
      //(*jt)->dump();
      bool res = false;
      if (S->mustBeTrue(Query(assume, *jt), res) && res)
        subsumed.insert(*jt);
    }
  }

  for (std::vector<ExprHandle>::const_iterator it = constraints.begin(),
       ie = constraints.end(); it != ie; ++it) {
    if (subsumed.find(*it) != subsumed.end())
      continue;
    simplified->push_back(*it);
  }

  return true;
}

static bool CompareInputAST(const char *Filename1,
                            const char *Filename2,
                            const MemoryBuffer *MB1,
                            const MemoryBuffer *MB2,
                            ExprBuilder *Builder,
                            std::ostream *Feature) {
  if (!Filename1 || !Filename2 || !Feature)
    return false;

  // Set up the solver
  Solver *coreSolver = NULL;
  coreSolver = UseDummySolver ? createDummySolver() : new STPSolver(UseForkedCoreSolver);
  if (!UseDummySolver) {
    if (0 != MaxCoreSolverTime) {
      coreSolver->setCoreSolverTimeout(MaxCoreSolverTime);
    }
  }
  Solver *S = constructSolverChain(coreSolver,
                                   getQueryLogPath(ALL_QUERIES_SMT2_FILE_NAME),
                                   getQueryLogPath(SOLVER_QUERIES_SMT2_FILE_NAME),
                                   getQueryLogPath(ALL_QUERIES_PC_FILE_NAME),
                                   getQueryLogPath(SOLVER_QUERIES_PC_FILE_NAME));

  // Parse the input
  bool success = true;
  std::vector<Decl*> Decls1, Decls2;
  ParseDecls(&Decls1, Filename1, MB1, Builder);
  ParseDecls(&Decls2, Filename2, MB2, Builder);
  QueryCommand *QC1 = NULL;
  for (std::vector<Decl*>::iterator it = Decls1.begin(), ie = Decls1.end();
       it != ie; ++it) {
    if (QC1 = dyn_cast<QueryCommand>(*it)) {
      break;
    }
    (*it)->dump();
  }
  QueryCommand *QC2 = NULL;
  for (std::vector<Decl*>::iterator it = Decls2.begin(), ie = Decls2.end();
       it != ie; ++it) {
    if (QC2 = dyn_cast<QueryCommand>(*it)) {
      break;
    }
  }

  std::cout << "(query [";
  // Simplify and match the two sets of constraints
  if (QC1 && QC2) {
    std::vector<ExprHandle> constraints1, constraints2;
    if (SimplifyConstraints(QC1->Constraints, &constraints1, S) &&
        SimplifyConstraints(QC2->Constraints, &constraints2, S)) {
      std::vector<ExprHandle>::const_iterator
        it1 = constraints1.begin(), ie1 = constraints1.end();
      std::vector<ExprHandle>::const_iterator
        it2 = constraints2.begin(), ie2 = constraints2.end();
      for (; it1 != ie1; ++it1) {
        std::vector<ExprHandle>::const_iterator dup2 = it2;

        Expr::ExprEquivSet equivs;
        Expr::ExprConstantVec constants;
        for (; dup2 != ie2 && (*it1)->compareSkipConstant(*(*dup2).get(), equivs, constants); ++dup2) {
          equivs.clear();
          constants.clear();
        }
        if (dup2 == ie2) {
          continue;
        }

        for (Expr::ExprConstantVec::const_iterator it = constants.begin(),
             ie = constants.end(); it != ie; ++it) {
          //std::cerr << it->first << " ";
          *Feature << it->second << " ";
        }

        it2 = dup2;
        (*it1)->dump();
        //(*it2)->dump();
        ++it2;
      }
      *Feature << std::endl;
    }
  }
  std::cout << "]" << std::endl << "false)" << std::endl;

  // Release resources
  for (std::vector<Decl*>::iterator it = Decls1.begin(),
       ie = Decls1.end(); it != ie; ++it)
    delete *it;
  for (std::vector<Decl*>::iterator it = Decls2.begin(),
       ie = Decls2.end(); it != ie; ++it)
    delete *it;
  delete S;

  return success;
}

static bool printInputAsSMTLIBv2(const char *Filename,
                             const MemoryBuffer *MB,
                             ExprBuilder *Builder)
{
	//Parse the input file
	std::vector<Decl*> Decls;
	Parser *P = Parser::Create(Filename, MB, Builder);
	P->SetMaxErrors(20);
	while (Decl *D = P->ParseTopLevelDecl())
	{
		Decls.push_back(D);
	}

	bool success = true;
	if (unsigned N = P->GetNumErrors())
	{
		std::cerr << Filename << ": parse failure: "
				   << N << " errors.\n";
		success = false;
	}

	if (!success)
	return false;

	ExprSMTLIBPrinter* printer = createSMTLIBPrinter();
	printer->setOutput(std::cout);

	unsigned int queryNumber = 0;
	//Loop over the declarations
	for (std::vector<Decl*>::iterator it = Decls.begin(), ie = Decls.end(); it != ie; ++it)
	{
		Decl *D = *it;
		if (QueryCommand *QC = dyn_cast<QueryCommand>(D))
		{
			//print line break to separate from previous query
			if(queryNumber!=0) 	std::cout << std::endl;

			//Output header for this query as a SMT-LIBv2 comment
			std::cout << ";SMTLIBv2 Query " << queryNumber << std::endl;

			/* Can't pass ConstraintManager constructor directly
			 * as argument to Query object. Like...
			 * query(ConstraintManager(QC->Constraints),QC->Query);
			 *
			 * For some reason if constructed this way the first
			 * constraint in the constraint set is set to NULL and
			 * will later cause a NULL pointer dereference.
			 */
			ConstraintManager constraintM(QC->Constraints);
			Query query(constraintM,QC->Query);
			printer->setQuery(query);

			if(!QC->Objects.empty())
				printer->setArrayValuesToGet(QC->Objects);

			printer->generateOutput();


			queryNumber++;
		}
	}

	//Clean up
	for (std::vector<Decl*>::iterator it = Decls.begin(),
			ie = Decls.end(); it != ie; ++it)
		delete *it;
	delete P;

	delete printer;

	return true;
}

int main(int argc, char **argv) {
  bool success = true;

  llvm::sys::PrintStackTraceOnErrorSignal();
  llvm::cl::ParseCommandLineOptions(argc, argv);

  std::string ErrorStr;
  
#if LLVM_VERSION_CODE < LLVM_VERSION(2, 9)
  MemoryBuffer *MB = MemoryBuffer::getFileOrSTDIN(InputFile.c_str(), &ErrorStr);
  if (!MB) {
    std::cerr << argv[0] << ": error: " << ErrorStr << "\n";
    return 1;
  }
  MemoryBuffer *MB2;
#else
  OwningPtr<MemoryBuffer> MB;
  error_code ec = MemoryBuffer::getFileOrSTDIN(InputFile.c_str(), MB);
  if (ec) {
    std::cerr << argv[0] << ": error: " << ec.message() << "\n";
    return 1;
  }
  OwningPtr<MemoryBuffer> MB2;
#endif
  std::ostream *Feature;
  
  ExprBuilder *Builder = 0;
  switch (BuilderKind) {
  case DefaultBuilder:
    Builder = createDefaultExprBuilder();
    break;
  case ConstantFoldingBuilder:
    Builder = createDefaultExprBuilder();
    Builder = createConstantFoldingExprBuilder(Builder);
    break;
  case SimplifyingBuilder:
    Builder = createDefaultExprBuilder();
    Builder = createConstantFoldingExprBuilder(Builder);
    Builder = createSimplifyingExprBuilder(Builder);
    break;
  }

  switch (ToolAction) {
  case PrintTokens:
    PrintInputTokens(MB.get());
    break;
  case PrintAST:
    success = PrintInputAST(InputFile=="-" ? "<stdin>" : InputFile.c_str(), MB.get(),
                            Builder);
    break;
  case Evaluate:
    success = EvaluateInputAST(InputFile=="-" ? "<stdin>" : InputFile.c_str(),
                               MB.get(), Builder);
    break;
  case Compare:
    Feature = new std::ofstream(
        FeatureFile.c_str(), std::ios::out | std::ios::binary | std::ios::app);
#if LLVM_VERSION_CODE < LLVM_VERSION(2, 9)
    MB2 = MemoryBuffer::getFileOrSTDIN(SecondFile.c_str(), &ErrorStr);
    if (!MB2) {
      std::cerr << argv[0] << ": error: " << ErrorStr << "\n";
      return 1;
    }
    success = CompareInputAST(InputFile=="-" ? NULL : InputFile.c_str(),
                              SecondFile=="-" ? NULL : SecondFile.c_str(),
                              MB, MB2, Builder, Feature);
#else
    ec = MemoryBuffer::getFileOrSTDIN(SecondFile.c_str(), MB2);
    if (ec) {
      std::cerr << argv[0] << ": error: " << ec.message() << "\n";
      return 1;
    }
    success = CompareInputAST(InputFile=="-" ? NULL : InputFile.c_str(),
                              SecondFile=="-" ? NULL : SecondFile.c_str(),
                              MB.get(), MB2.get(), Builder, Feature);
#endif
    delete Feature;
    break;
  case PrintSMTLIBv2:
    success = printInputAsSMTLIBv2(InputFile=="-"? "<stdin>" : InputFile.c_str(), MB.get(),Builder);
    break;
  default:
    std::cerr << argv[0] << ": error: Unknown program action!\n";
  }

  delete Builder;
  llvm::llvm_shutdown();
  return success ? 0 : 1;
}
