/*******************************************************************************
* Copyright 2021 Intel Corporation
*
* Licensed under the BSD-2-Clause Plus Patent License (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* https://opensource.org/licenses/BSDplusPatent
*
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions
* and limitations under the License.
*
*
* SPDX-License-Identifier: BSD-2-Clause-Patent
*******************************************************************************/

// Header file
#include "t2spreprocessor.h"

using namespace clang::tooling;
using namespace llvm;
using namespace clang;
using namespace clang::ast_matchers;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory T2SCategory("t2spreprocessor options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
std::string MoreHelpMsg = "t2spreprocessor is inteded to convert mixed t2s specification and execution code into compiliable code\n" + t2sprinter::usage();
static cl::extrahelp MoreHelp( MoreHelpMsg  );


// =============== Helpers =============== 

// For the Pragma Handlers
std::vector<ParamLocs> T2S_START_LOCS;
std::vector<ParamLocs> T2S_END_LOCS; 
std::vector<ParamLocs> T2S_SUBMIT_LOCS;

// For the AST Consumer 
std::vector<ParamLocs> T2S_ARGS_LOCS;
std::vector<ParamLocs> T2S_ONEAPI_LOCS;




// =============== Pragma Handlers =============== 

// #pragma t2s_submit_start
const char* t2s_submit_start_name = "t2s_spec_start";
const char* t2s_submit_start_expl = "#pragma t2s_submit_start // indicates where the begining of the T2S Specifications";
class T2SubmitStartPragmaHandler : public PragmaHandler {
  public:
    T2SubmitStartPragmaHandler() : PragmaHandler( StringRef(t2s_submit_start_name) ) { }
    void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer, Token &PragmaTok) {
      ParamLocs locs( Introducer.Loc , PragmaTok.getEndLoc() );
      T2S_START_LOCS.push_back( locs );

      // T2S_START_LOCS.push_back( Introducer.Loc );
      // Handle the pragma
      std::cout << t2sprinter::header() << "Found [" << T2S_START_LOCS.size() << "] " << t2s_submit_start_name << "\n";
    }
};


// #pragma t2s_submit_end
const char* t2s_submit_end_name = "t2s_spec_end";
const char* t2s_submit_end_expl = "#pragma t2s_submit_end // indicates where the ending of the T2S Specifications";
class T2SubmitEndPragmaHandler : public PragmaHandler {
  public:
    T2SubmitEndPragmaHandler() : PragmaHandler( StringRef(t2s_submit_end_name) ) { }
    void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer, Token &PragmaTok) {
      // ParamLocs locs( PragmaTok.getLocation() , PragmaTok.getEndLoc() );
      ParamLocs locs( Introducer.Loc , PragmaTok.getEndLoc() );
      T2S_END_LOCS.push_back( locs );

      // T2S_END_LOCS.push_back( Introducer.Loc );
      // SourceLocation loc = Introducer.Loc 
      // SourceLocation loc = PragmaTok.getLastLoc()
      // SourceLocation loc = PragmaTok.getEndLoc()

      // Handle the pragma
      std::cout << t2sprinter::header() << "Found [" << T2S_END_LOCS.size() << "] " << t2s_submit_end_name << "\n";
    }
};


// #pragma t2s_submit
const char* t2s_submit_name = "t2s_submit";
const char* t2s_submit_expl = "#pragma t2s_submit FuncNname ([Input Arg], [Memory Pointer], [Input Dimensions]) ..\n // maps an input argument variable a user defined memory pointer and int array for the dimensions. Data type is infered from the memory pointer.";
class T2SubmitPragmaHandler : public PragmaHandler {
  public:
    T2SubmitPragmaHandler() : PragmaHandler( StringRef(t2s_submit_name) ) { }
    void HandlePragma(Preprocessor &PP, PragmaIntroducer Introducer, Token &PragmaTok) {
      // -------- T2S_SUBMIT_LOCS dealing with the submission itself -------- //

      ParamLocs locs( Introducer.Loc , PragmaTok.getEndLoc(), &PragmaTok );

      // get the param arguments
      Token Tok;
      std::ostringstream AnnotateDirective;
      while(Tok.isNot(tok::eod)) {
        PP.Lex(Tok);
        if(Tok.isNot(tok::eod))
          AnnotateDirective << PP.getSpelling(Tok);
      }

      locs.end = Tok.getLocation();
      locs.args = AnnotateDirective.str();

      T2S_SUBMIT_LOCS.push_back( locs );


      // -------- T2S_ARGS_LOCS dealing with the arguments submitted -------- //

      // Print info about the pragma
      std::cout << t2sprinter::header() << "Found [" << T2S_SUBMIT_LOCS.size() << "] "<< t2s_submit_name << " submit: " << AnnotateDirective.str() << " \n";

      // Create a OneAPIFuncStruct to Parse out the Arugment informaiton from the #pragma t2s_submit
      OneAPIFuncStruct tmp;
      tmp.parseArgPntrsHelper( AnnotateDirective.str()  );
      
      for( unsigned int i = 0; i < tmp.SubmitArgs.size(); i++){
        std::cout << t2sprinter::warning() << "T2SubmitPragmaHandler adding to T2S_ARGS_LOCS: " 
                  << tmp.SubmitArgs[i].full_args << "\n";
        ParamLocs arg( Introducer.Loc , PragmaTok.getEndLoc(), &PragmaTok );
        arg.args = tmp.SubmitArgs[i].full_args;
        T2S_ARGS_LOCS.push_back( arg );
      }
    }
};



// =========== Match Handlers Class' ===========
class ArgsMatchHandler : public MatchFinder::MatchCallback {
  public :
    unsigned int arg_index;
    ArgsMatchHandler(unsigned int i) : arg_index(i) {}

    virtual void run(const MatchFinder::MatchResult &Result) {

      auto *id = Result.Nodes.getNodeAs<clang::VarDecl>("var"); // i.e. Decl child

      const llvm::StringRef matchName = id->getName();
      const QualType matchType = id->getType();
      if ( matchName.empty() ) return;
      std::cout << t2sprinter::header() << "found " <<  matchType.getAsString() << " " << matchName.str() << "\n";

      // Add the types the T2S_ARGS_LOCS's arg_types of vector QualType
      T2S_ARGS_LOCS[arg_index].arg_types.push_back( matchType );
    }
};

class OneAPIMatchHandler : public MatchFinder::MatchCallback {
  public :
    virtual void run(const MatchFinder::MatchResult &Result) {
      // i.e. FunctionDecl 
      auto *id = Result.Nodes.getNodeAs<clang::CXXMemberCallExpr>("compile_to_oneapi"); 
      if( !id ){ return; }

      // // Verify that there are at least 2 arguments
      if( id->getNumArgs() < 2){
        std::cout << t2sprinter::error() << "Error: unable to find compile_to_oneapi with function name passed in.\n";
        std::cout << t2sprinter::usage();
        exit(1);
      }

      // // Verify that we have not already added this
      bool alreadyExists = false;
      if( !T2S_ONEAPI_LOCS.empty() ){
        for(unsigned int i = 0; i < T2S_ONEAPI_LOCS.size(); i++){
          if( T2S_ONEAPI_LOCS[i].oneapiStruct.args_expr_pntrs[1] == id->getArg(1) ){
            return;
          }
        }
      }

      // std::cout << t2sprinter::header() << "found compile_to_oneapi() w/ " <<  id->getNumArgs() << " args \n";
      std::cout << t2sprinter::warning() << "found compile_to_oneapi() w/ " <<  id->getNumArgs() << " args \n";

      // Store the locaiton of the compile_to_oneapi function
      // Todo. Eventually make sure that the argument types of the ones we need are correct, (1) Vector (2) String
      ParamLocs locs( id->getBeginLoc() , id->getEndLoc() );
      for(unsigned int i = 0; i < id->getNumArgs(); i++){
        locs.oneapiStruct.args_expr_pntrs.push_back( id->getArg(i) );
      }

      // ParamLocs locs( funcNameExpr->getBeginLoc() , funcNameExpr->getEndLoc() );
      T2S_ONEAPI_LOCS.push_back( locs );
    }
};



// =============== MatchFinder/Consumer =============== 

// (2) ASTConsumer FrontendConsumer
// ASTConsumer is an interface used to write generic actions on an AST, regardless of how the AST was produced.
// ASTConsumer provides many different entry points
// We can use a RecursiveASTVisitor to visit the AST
// We can use a StatementMatcher to visit the AST
// here we use HandleTranslationUnit, which is called with the ASTContext for the translation unit.
class Consumer : public clang::ASTConsumer {
  public:
    virtual void HandleTranslationUnit(clang::ASTContext &Context) {
 
      // Loop through the arguments found in the t2s_submit pragma and retrieve their type to store
      std::cout << t2sprinter::warning() << "===T2S_ARGS_LOCS AST Searcher===\n";
      for(unsigned int i = 0; i < T2S_ARGS_LOCS.size(); i++){
        ParamLocs params = T2S_ARGS_LOCS[i];
        std::vector<std::string> arg_names = params.SplitArgs();
        
        // Print out info
        std::cout << t2sprinter::warning() << "Checking args: ";
        for(unsigned int j = 0; j < arg_names.size() ; j++){
          std::cout << arg_names[j] << " ";
        }
        std::cout << "\n";

        // Run a AST matcher to get each of the t2s arguments informations 
        for(unsigned int j = 0; j < arg_names.size(); j ++){
          MatchFinder finder;
          auto matcher = varDecl( isExpansionInMainFile(), hasName( arg_names[j] ) ).bind("var");
          ArgsMatchHandler handler(i);
          finder.addMatcher(matcher, &handler );
          finder.matchAST(Context);
        }
      }



      // Run a matcher to find the location of the compile_to_oneapi() MemberExpr
      MatchFinder oneapiFinder;
      // (NOTE) Need to fine tune matcher, Currently finds all Calls to compile_to_oneapi which occurs in multiple locations
      // inside of the halide source code
      auto oneapiMatcher = cxxMemberCallExpr( isExpansionInMainFile(), callee(cxxMethodDecl(hasName("compile_to_oneapi"))) ).bind("compile_to_oneapi");  
      OneAPIMatchHandler oneapiHandler;
      oneapiFinder.addMatcher(oneapiMatcher, &oneapiHandler );
      oneapiFinder.matchAST(Context);

    }
};



// =============== FrontendAction =============== 

// (1) FrontendAction
// FrontendAction is an interface that allows execution of user specific actions as part of the compilation
// To run tools over the AST clang provides the convenience interface ASTFrontendAction 
// which takes care of executing the action
// The only part left is to implement the `CreateASTConsumer` method that returns an ASTConsumer per translation unit.
class T2SPreprocessorFrontendAction : public clang::ASTFrontendAction {
  private:

    void T2S_RemoveStart(Rewriter &R){
      for(unsigned int i = 0; i < T2S_START_LOCS.size(); i++){
        R.ReplaceText( SourceRange(T2S_START_LOCS[i].start, T2S_START_LOCS[i].end) , "\n");
      }
    }
    
    void T2S_RemoveEnd(Rewriter &R){
      for(unsigned int i = 0; i < T2S_END_LOCS.size(); i++){
        R.ReplaceText( SourceRange(T2S_END_LOCS[i].start, T2S_END_LOCS[i].end) , "\n");
      }
    }

    // (TODO) Remove b/c made redudent when using the OneAPIFuncStruct w/ locations & poping T2S_SUBMIT_LOCS
    void T2S_RemoveSubmit(Rewriter &R){
      for(unsigned int i = 0; i < T2S_SUBMIT_LOCS.size(); i++){
        std::cout << t2sprinter::error() << " Removing T2S_SUBMIT i.e.:\n\t"
                  << R.getRewrittenText( SourceRange(T2S_SUBMIT_LOCS[i].start, T2S_SUBMIT_LOCS[i].end) ) << "\n";
        R.ReplaceText( SourceRange(T2S_SUBMIT_LOCS[i].start, T2S_SUBMIT_LOCS[i].end) , "\n");
      }
    }

    void T2S_RemoveSubmit(Rewriter &R, SourceLocation start, SourceLocation end){
      // (NOTE) print statment for debugging purposes
      // std::cout << t2sprinter::error() << " Removing T2S_SUBMIT i.e.:\n\t"
      //           << R.getRewrittenText( SourceRange(start, end) ) << "\n";
      R.ReplaceText( SourceRange(start, end) , "\n");
    }

    void T2S_CleanArea(Rewriter &R){
      if(  T2S_START_LOCS.size() != T2S_END_LOCS.size()){
        std::cout << t2sprinter::error() << "Error: mismatch in number of T2S start and end pragmas "
                  << "i.e. [" << T2S_START_LOCS.size() << "] t2s start pragmas != ["
                  << T2S_END_LOCS.size() << "] t2s end pragma.\n";
        std::cout << t2sprinter::usage();
        exit(1);
      }

      for(unsigned int i = 0; i < T2S_START_LOCS.size(); i++){
        ParamLocs start_loc = T2S_START_LOCS[i];
        ParamLocs end_loc = T2S_END_LOCS[i];

        R.ReplaceText(SourceRange(start_loc.end, end_loc.start), "\n");
      }
    }

    void T2S_InsertRunArgs(Rewriter &R, OneAPIFuncStruct oneapiStruct,std::string funcName, SourceLocation includeLoc){
      std::vector<std::string> all_args_vector;
      std::vector<QualType> all_types_vecstor;
      ParamLocs start_loc = T2S_START_LOCS[ T2S_START_LOCS.size() -1];

      // Check the argument & Types. Placing them into the vectors
      // all_args_vector & all_types_vecstor to be passed to another function below
      std::cout << t2sprinter::warning() << "===FINAL ARGS CHECK===\n";
      for(unsigned int i = 0; i < T2S_ARGS_LOCS.size(); i++){
        ParamLocs arg_loc = T2S_ARGS_LOCS[i];

        Token *tkn = arg_loc.tkn;

        // Get the argument names and put them into a string
        std::vector<std::string> args_vector = arg_loc.SplitArgs();
        for(auto elem : args_vector){ all_args_vector.push_back( elem );}


        // Get the argument types found by the ASTMacher 
        std::vector<QualType> types_vecstor = arg_loc.arg_types;
        for(auto elem : types_vecstor){ all_types_vecstor.push_back( elem );}

        // Verify that the sizes are equal
        if(args_vector.size() != types_vecstor.size()){
          std::cout << t2sprinter::error() << "Error: cannot match arguments passed into `#pragma t2s_submit_args` with their types."
                    << " Make sure you have defined all these variables within the same file.\n";
          std::cout << t2sprinter::usage();
          exit(1);
        }


        // Verify that for each set of variables passed into `#pragma t2s_submit_args`
        //    index%2 == 0 are pointer types to memory 
        //    index%2 == 1 are of int[#] which define the dimensions of the Halide Runtime Buffer
        for(int i = 0; i < types_vecstor.size() / 3; i++){
          QualType t_ImageParam = types_vecstor[i];
          const clang::Type* t_ptr_ImageParam = t_ImageParam.getTypePtr();
          QualType t_data = types_vecstor[i+1];
          const clang::Type* t_ptr_data = t_data.getTypePtr();

          QualType t_dim = types_vecstor[i+2];
          const clang::Type* t_ptr_dim = t_dim.getTypePtr();

          std::cout << t2sprinter::header() << " checking " <<  args_vector[i]  << " of type " << t_ImageParam.getAsString() << "\n";
          std::cout << t2sprinter::header() << " checking " <<  args_vector[i+1]  << " of type " << t_data.getAsString() << "\n";
          std::cout << t2sprinter::header() << " checking " <<  args_vector[i+2]  << " of type " << t_dim.getAsString() << "\n";

        }
      
      }


      // Insert necessary includes at the `SourceLocation` includeLoc 
      std::ostringstream rhs_insert;
      rhs_insert << "\n\n";
      rhs_insert << "#ifdef FPGA\n";
      rhs_insert << "#include \"HalideBuffer.h\"\n";
      rhs_insert << "#include \"" << funcName << ".sycl.h\"\n";
      rhs_insert << "#endif\n";
      rhs_insert << "#ifdef GPU\n";
      // rhs_insert << "#include \"HalideBuffer.h\"\n";
      rhs_insert << "#include \"esimd_test_utils.hpp\"\n";
      rhs_insert << "#include <iostream>\n";
      rhs_insert << "#include <sycl/ext/intel/esimd.hpp>\n";
      rhs_insert << "#include <CL/sycl.hpp>\n";
      rhs_insert << "#endif\n";
      R.InsertTextAfterToken( includeLoc , rhs_insert.str() );
      
      // Insert the internal code to run the generated OneAPI Function
      std::ostringstream rhs;

      std::cout << t2sprinter::error() << "all_args_vector.size(): " << all_args_vector.size() << "\n"; 
      std::cout << t2sprinter::error() << "all_types_vecstor.size(): " << all_types_vecstor.size() << "\n"; 


      // Pass in the arguments/argument types/oneapiStruct/& function name into the `GenRunPostCode()`
      // Then finally insert the string into the original location of the #pragma t2s_spec_start
      rhs << GenRunPostCode(oneapiStruct, all_args_vector, all_types_vecstor, funcName); // (TODO) Reimplement
      R.ReplaceText( SourceRange(start_loc.end, start_loc.end) , rhs.str() ); // (TODO) Reimplement/Check
    }

    OneAPIFuncStruct GetNextFuncStruct(){ // (CHECK) good
      if( T2S_ONEAPI_LOCS.empty() ){
        std::cout << t2sprinter::error() << "Error: unable to find the function name passed into the compile_to_oneapi method.\n";
        std::cout << t2sprinter::usage();
        exit(1);
      }
      if( T2S_SUBMIT_LOCS.empty() ){
        std::cout << t2sprinter::error() << "Error: unable to find the t2s_submit function pragma.\n";
        std::cout << t2sprinter::usage();
        exit(1);
      }

      ParamLocs oneapi_locs = T2S_ONEAPI_LOCS[ T2S_ONEAPI_LOCS.size() -1 ];
      T2S_ONEAPI_LOCS.pop_back();

      ParamLocs submit_locs = T2S_SUBMIT_LOCS[ T2S_SUBMIT_LOCS.size() -1 ];
      T2S_SUBMIT_LOCS.pop_back();

      std::cout << t2sprinter::warning() << "\tGetNextFuncStruct @: " << orig_Rewriter.getRewrittenText( SourceRange(oneapi_locs.start, oneapi_locs.end) ) << "\n";
      oneapi_locs.initOneAPIStruct(orig_Rewriter);
      oneapi_locs.oneapiStruct.parseArgPntrs(orig_Rewriter, submit_locs.start , submit_locs.end );

      // Adding the start & end locations for further functionality
      oneapi_locs.oneapiStruct.oneapi_start = oneapi_locs.start;
      oneapi_locs.oneapiStruct.oneapi_end = oneapi_locs.end;

      oneapi_locs.oneapiStruct.submit_start = submit_locs.start;
      oneapi_locs.oneapiStruct.submit_end = submit_locs.end;

      return oneapi_locs.oneapiStruct;
    }


  public:

    bool BeginInvocation (CompilerInstance &CI) override {

      static PragmaHandlerRegistry::Add<T2SubmitStartPragmaHandler>StartPragma( t2s_submit_start_name , t2s_submit_start_expl );
      static PragmaHandlerRegistry::Add<T2SubmitEndPragmaHandler>EndPragma( t2s_submit_end_name , t2s_submit_end_expl );
      static PragmaHandlerRegistry::Add<T2SubmitPragmaHandler>SubmitPragma( t2s_submit_name, t2s_submit_expl);

      return true;
    }

    void EndSourceFileAction() override {
      std::cout << t2sprinter::header() << "final num(T2S_START_LOCS) : " << T2S_START_LOCS.size() << "\n";
      std::cout << t2sprinter::header() << "final num(T2S_END_LOCS)   : " << T2S_END_LOCS.size() << "\n";

      // (NOTE) This is not set within the t2s_submit_gemm pragma handler instead of any other
      std::cout << t2sprinter::header() << "final num(T2S_ARGS_LOCS)  : " << T2S_ARGS_LOCS.size() << "\n"; 

      std::cout << t2sprinter::header() << "final num(T2S_SUBMIT_LOCS)  : " << T2S_SUBMIT_LOCS.size() << "\n";
      std::cout << t2sprinter::header() << "final num(T2S_ONEAPI_LOCS)  : " << T2S_ONEAPI_LOCS.size() << "\n";


      // // Retrieve the current function name i.e. using T2S_ONEAPI_LOCS helping vector
      OneAPIFuncStruct oneapiStruct = GetNextFuncStruct();
      std::cout << t2sprinter::warning() << "compile_to_oneapi: " << oneapiStruct.funcName <<"\n";
      std::cout << oneapiStruct.str();


      // Verifying that there is only one T2S to OneAPI Spec
      if( !T2S_ONEAPI_LOCS.empty() ){
        std::cout << t2sprinter::error() 
                  << "The T2S Preprocessor is intended to be used for 1 T2s specification per file.\n"
                  << "Found the following other T2S OneAPI secifications:\n";
        while( !T2S_ONEAPI_LOCS.empty() ){
          OneAPIFuncStruct oneapiStructTmp = GetNextFuncStruct();
          std::cout << t2sprinter::warning() << "compile_to_oneapi: " << oneapiStructTmp.funcName <<"\n";
          std::cout << oneapiStructTmp.str();        
        }
        assert(false);
      }


      // 1. Clear inbetween 
      // (NOTE) Should be ran before replacing the Start and End Pragmas 
      T2S_CleanArea(run_Rewriter);

      
      // Find the location of the last include
      SourceManager *srcMngr = &compilerPntr->getSourceManager();
      SourceLocation *includeLoc = NULL;
      SourceLocation includeLoc2;
      unsigned includeLineNumber;
      for(auto it = srcMngr->fileinfo_begin(); it != srcMngr->fileinfo_end(); it++){
        auto fst_fileEntry = it->getFirst(); // clang::FileEntry
        auto snd_contentCache = it->getSecond(); // clang::SrcMgr::ContentCache

        FileID fid = srcMngr->translateFile( fst_fileEntry );

        if( fid.isValid() ){
          SourceLocation tmpLoc = srcMngr->getIncludeLoc( fid );
          unsigned tmpLineNumber = srcMngr->getSpellingLineNumber( tmpLoc );

          if( srcMngr->isInMainFile( tmpLoc )  ){
            // (TODO) Remove, for debuging purposes only
            // std::cout << t2sprinter::error() << "!!! CHECKING FILESOURCE: " << fst_fileEntry->getName().str() << "\n";
            // std::cout << t2sprinter::error() << "\t FileID: " << fid.isValid() << "\n";
            // std::cout << t2sprinter::error() << "\t includeLoc: " << tmpLoc.isValid() << "\n";
            // std::cout << t2sprinter::error() << "\t getSpellingLineNumber: " << tmpLineNumber << "\n";

            std::string rawText = run_Rewriter.getRewrittenText( SourceRange(tmpLoc) );
            unsigned offset = rawText.size();
            SourceLocation includeLoc3 = srcMngr->getComposedLoc(fid,  offset);

            // (TODO) Remove, for debuging purposes only
            // std::cout << t2sprinter::warning() << "Line: " << run_Rewriter.getRewrittenText( SourceRange(tmpLoc) ) << "\n";

            if( !includeLoc ){
              includeLoc = &tmpLoc;
              includeLineNumber = tmpLineNumber;
              includeLoc2 = tmpLoc;
            } else if(  includeLineNumber <  tmpLineNumber ){
              includeLoc = &tmpLoc;
              includeLineNumber = tmpLineNumber;                
              includeLoc2 = tmpLoc;
            }

          }
        }
      }


      // Execute insurtion into the runfile 
      T2S_InsertRunArgs(run_Rewriter, oneapiStruct, oneapiStruct.funcName, includeLoc2 ); 
      
      //--------------- Remove Start,End, and Submit Pragmas ---------------//


      // 1. Remove start 
      T2S_RemoveStart(t2s_Rewriter);
      T2S_RemoveStart(run_Rewriter);

      // 1. Remove end 
      T2S_RemoveEnd(t2s_Rewriter);
      T2S_RemoveEnd(run_Rewriter);

      // 1. Remove Submit
      T2S_RemoveSubmit(t2s_Rewriter, oneapiStruct.submit_start, oneapiStruct.submit_end);
      T2S_RemoveSubmit(run_Rewriter, oneapiStruct.submit_start, oneapiStruct.submit_end);


      //--------------- writing to file ---------------//


      // Creating Files
      RewriteBuffer *RB;

      std::cout << t2sprinter::header() << "Creating " << t2sprinter::base_name( t2sfilename.str() )  << " file (1/2) \n";
      std::ofstream t2sfile;
      t2sfile.open( t2sfilename.str().c_str() );
      RB = &t2s_Rewriter.getEditBuffer(t2s_Rewriter.getSourceMgr().getMainFileID());
      t2sfile << std::string(RB->begin(), RB->end());

      std::cout << t2sprinter::header() << "Creating " << t2sprinter::base_name( runfilename.str() ) << " file (2/2) \n";
      std::ofstream runfile;
      runfile.open( runfilename.str().c_str() );
      RB = &run_Rewriter.getEditBuffer(run_Rewriter.getSourceMgr().getMainFileID());
      runfile << std::string(RB->begin(), RB->end());


      //-----------------------------------------------//
    }

    // Implementing the AST Searcher
    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance &Compiler, llvm::StringRef InFile) override {
      orig_Rewriter.setSourceMgr(Compiler.getSourceManager(), Compiler.getLangOpts());
      t2s_Rewriter.setSourceMgr(Compiler.getSourceManager(), Compiler.getLangOpts());
      run_Rewriter.setSourceMgr(Compiler.getSourceManager(), Compiler.getLangOpts());
      compilerPntr = &Compiler;

      std::string filenamefullpath = InFile.str();
      std::string filename = t2sprinter::base_name( filenamefullpath );
      std::string pathname = t2sprinter::path_only( filenamefullpath );

      if( !InFile.endswith(".cpp")  ){
        std::cout << t2sprinter::error() << "Error: expected source file to be <filename>.cpp format. Recieved " << filename << "\n";
        exit(1);
      }
    
      std::cout << t2sprinter::header() << "orig filename: " << filename << "\n";
      std::cout << t2sprinter::header() << "orig pathname: " << pathname << "\n";
      filename = filename.substr(0,filename.length()-4);
      t2sfilename << pathname << filename << ".spec.cpp";
      runfilename << pathname << filename << ".run.cpp";
      std::cout << t2sprinter::header() << "new t2sfilename: " << t2sfilename.str() << "\n";
      std::cout << t2sprinter::header() << "new runfilename: " << runfilename.str() << "\n";

      return llvm::make_unique<Consumer>();
    }

  private:
    std::ostringstream t2sfilename;
    std::ostringstream runfilename;
    Rewriter orig_Rewriter;
    Rewriter t2s_Rewriter;
    Rewriter run_Rewriter;
    clang::CompilerInstance* compilerPntr;
};

int main(int argc, char* argv[]){

  std::cout << t2sprinter::header() << "started! ...\n";

  CommonOptionsParser OptionsParser(argc, (const char **)argv, T2SCategory);
  ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());

  // // Check for the T2S_PATH enviornment variable
  char *T2S_PATH = getenv("T2S_PATH");
  if( !T2S_PATH ){
    std::cout << t2sprinter::error() << "Error: cannot find enviornment variable `T2S_PATH`.\n";
    std::cout << t2sprinter::error() << "Make sure you have sourced `setenv.sh` from the t2s base directory\n";
    exit(1);
  }

  // // Add necessary arguments
  Tool.appendArgumentsAdjuster( getInsertArgumentAdjuster(std::string("-I" + std::string(T2S_PATH) + "/t2s/src/").c_str()) );
  Tool.appendArgumentsAdjuster( getInsertArgumentAdjuster(std::string("-I" + std::string(T2S_PATH) + "/Halide/include/").c_str()) );
  Tool.appendArgumentsAdjuster( getInsertArgumentAdjuster(std::string("-I" + std::string(T2S_PATH) + "/install/lib/clang/9.0.1/include").c_str()) );
  Tool.appendArgumentsAdjuster( getInsertArgumentAdjuster(std::string("-I" + std::string(T2S_PATH) + "/t2s/tests/correctness/util").c_str()) );
  Tool.appendArgumentsAdjuster( getInsertArgumentAdjuster(std::string("-std=c++11").c_str()) );

  int ret = Tool.run(newFrontendActionFactory<T2SPreprocessorFrontendAction>().get());
  std::cout << t2sprinter::header() << "finished! ...\n";
  return ret;
}