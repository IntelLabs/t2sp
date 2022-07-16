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

#ifndef T2SPREPROCESSOR_H_
#define T2SPREPROCESSOR_H_

// clang include
#include <clang-c/Index.h>

// // LLVM includes
#include <llvm/Support/CommandLine.h>
#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/CompilerInstance.h"

// Clang includes
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/ASTContext.h"

// Clang Parse Attributes
#include "clang/AST/Attr.h"
#include "clang/Sema/ParsedAttr.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/SemaDiagnostic.h"
#include "llvm/IR/Attributes.h"

// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Tooling.h"

// // Find matching statements
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

// For rewritting
#include "clang/Rewrite/Core/Rewriter.h"

// For llvm Inner
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/StringRef.h"

// Standard includes
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>
#include <stdlib.h>

using namespace clang::tooling;
using namespace llvm;
using namespace clang;
using namespace clang::ast_matchers;

// =============== Helpers ===============

const char *USAGE_EXAMPLE = R"(
/* filename.cpp */
int main(){
	// Initialize data 
	const int TOTAL_I = III * II * I;
	const int TOTAL_J = JJJ * JJ * J;
	const int TOTAL_K = KKK * KK * K;
	float *a = new float[TOTAL_K * TOTAL_I];
	float *b = new float[TOTAL_J * TOTAL_K];
	float *c = new float[TOTAL_J * TOTAL_I];
	for(unsigned int i = 0; i < (TOTAL_K * TOTAL_I); i++){ a[i] = random(); }
	for(unsigned int i = 0; i < (TOTAL_J * TOTAL_K); i++){ b[i] = random(); }
	for(unsigned int i = 0; i < (TOTAL_J * TOTAL_I); i++){ c[i] = 0.0f; }

	// Dimensions of the data
	int a_dim[2] = {TOTAL_K, TOTAL_I};
	int b_dim[2] = {TOTAL_J, TOTAL_K};
	int c_dim[6] = {JJJ, III, JJ, II, J, I};


#pragma t2s_spec_start
	// ...
	ImageParam A("A", TTYPE, 2), B("B", TTYPE, 2);
	// T2S Speficiations ... 
	C.compile_to_oneapi( { A, B }, "gemm", IntelFPGA);

#pragma t2s_spec_end

#pragma t2s_submit gemm (A, a, a_dim) (B, b, b_dim) (C, c, c_dim)

}
)";

namespace t2sprinter
{

  const std::string t2sprinterName = "t2spreprocessor: ";

  const std::string HEADER = "\033[95m";
  const std::string OKBLUE = "\033[94m";
  const std::string OKCYAN = "\033[96m";
  const std::string OKGREEN = "\033[92m";
  const std::string WARNING = "\033[93m";
  const std::string FAIL = "\033[91m";
  const std::string ENDC = "\033[0m";
  const std::string BOLD = "\033[1m";
  const std::string UNDERLINE = "\033[4m";

  std::string simple()
  {
    std::ostringstream rhs;
    rhs << t2sprinterName;
    return rhs.str();
  }

  std::string simple_tab()
  {
    std::ostringstream rhs;
    rhs << "\t" << t2sprinterName;
    return rhs.str();
  }

  std::string header()
  {
    std::ostringstream rhs;
    rhs << OKGREEN << t2sprinterName << ENDC;
    return rhs.str();
  }

  std::string warning()
  {
    std::ostringstream rhs;
    rhs << WARNING << t2sprinterName << ENDC;
    return rhs.str();
  }

  std::string error()
  {
    std::ostringstream rhs;
    rhs << FAIL << t2sprinterName << ENDC;
    return rhs.str();
  }

  std::string base_name(std::string const &path)
  {
    const std::string delims = "/\\";
    std::size_t pos = path.find_last_of(delims);
    return path.substr(pos + 1);
  }

  std::string path_only(std::string const &path)
  {
    const std::string delims = "/\\";
    std::size_t pos = path.find_last_of(delims);
    return path.substr(0, pos + 1);
  }

  std::string usage()
  {
    std::ostringstream rhs;
    std::string space = " ";
    rhs << header() << "Expected usage goes as follows: "
        << "\n\n";

    rhs << USAGE_EXAMPLE;
    // << "int main(){\n\n"
    // << space   << "float *a = new float[DIM_1 * DIM_2];" << "\n"
    // << space   << "float *b = new float[DIM_3 * DIM_4 * DIM_5];" << "\n\n"
    // << space   << "int a_dim[2] = {DIM_1, DIM_2};" << "\n"
    // << space   << "int b_dim[3] = {DIM_3, DIM_4, DIM_5};" << "\n\n"
    // << space   << "#pragma t2s_arg A, a, a_dim" << "\n"
    // << space   << "#pragma t2s_arg C, c, c_dim" << "\n"
    // << space   << "#pragma t2s_arg B, b, b_dim" << "\n"
    // << space   << "#pragma t2s_spec_start" << "\n\n"
    // << space << space << "ImageParam A(\"A\", TTYPE, 2), B(\"B\", TTYPE, 2);" << "\n"
    // << space << space << "// T2S Speficiations ... " << "\n"
    // << space << space << "C.compile_to_oneapi( { A, B }, \"gemm\", IntelFPGA);" << "\n\n"
    // << space   << "#pragma t2s_spec_end" << "\n\n"
    // << "}\n\n";
    return rhs.str();
  }

  std::string get_base_type(std::string const &arg_type)
  {
    std::vector<std::string> arg_type_vec;
    size_t start;
    size_t end = 0;
    while ((start = arg_type.find_first_not_of(" ", end)) != std::string::npos)
    {
      end = arg_type.find(" ", start);
      arg_type_vec.push_back(arg_type.substr(start, end - start));
    }
    std::string ret = arg_type_vec[0];
    return ret;
  }

  std::string parseOneAPIName(std::string const &name)
  {
    std::string funcName = name;
    // Check to make sure that the second paramater, the function name, is not a variable
    if (funcName.find("\"") == std::string::npos && funcName.find("'") == std::string::npos)
    {
      std::cout << t2sprinter::error() << "Error: unable to parse the name passed provided i.e. `" << funcName << "`.\n";
      std::cout << t2sprinter::usage();
      exit(1);
    }

    // str.erase(std::remove(str.begin(), str.end(), 'a'), str.end());
    while (funcName.find("\"") != std::string::npos)
    {
      funcName.replace(funcName.find("\""), 1, "");
    }
    while (funcName.find("'") != std::string::npos)
    {
      funcName.replace(funcName.find("'"), 1, "");
    }
    return funcName;
  }

  std::vector<std::string> parseOneAPIVector(std::string const &vec)
  {
    std::string vec_tmp = vec;
    std::vector<std::string> args;
    // Check to make sure we can parse the vector directly
    if (vec_tmp.find("{") == std::string::npos && vec_tmp.find("}") == std::string::npos)
    {
      std::cout << t2sprinter::error() << "Error: unable to parse the vector passed provided i.e. `" << vec << "`.\n";
      std::cout << t2sprinter::usage();
      exit(1);
    }

    while (vec_tmp.find("{") != std::string::npos)
    {
      vec_tmp.replace(vec_tmp.find("{"), 1, "");
    }
    while (vec_tmp.find("}") != std::string::npos)
    {
      vec_tmp.replace(vec_tmp.find("}"), 1, "");
    }
    while (vec_tmp.find(" ") != std::string::npos)
    {
      vec_tmp.replace(vec_tmp.find(" "), 1, "");
    }

    size_t pos = 0;
    while (vec_tmp.find(",") != std::string::npos)
    {
      pos = vec_tmp.find(",");
      args.push_back(vec_tmp.substr(0, pos));
      // std::cout << t2sprinter::warning() << "\t parseVector @ arg: " << vec_tmp.substr(0,pos) << "\n";
      vec_tmp.erase(0, pos + 1);
    }
    // std::cout << t2sprinter::warning() << "\t parseVector @ arg: " << vec_tmp << "\n";
    args.push_back(vec_tmp);

    return args;
  }

  std::string parseOneAPIOutputArg(std::string const &func)
  {
    std::string arg = func;
    // Check to make sure that the second paramater, the function name, is not a variable
    if (arg.find(".") == std::string::npos)
    {
      std::cout << t2sprinter::error() << "Error: unable to parse the output argument name passed provided i.e. `" << func << "`.\n";
      std::cout << t2sprinter::usage();
      exit(1);
    }

    size_t pos = arg.find(".");
    std::string ret = arg.substr(0, pos);

    return ret;
  }

}

struct OneAPIArgStruct
{
  std::string arg_name;
  std::string ptr_name;
  std::string dim_name;

  std::string full_args;

  void ParseArgs(std::string s)
  {
    full_args = s;
    std::vector<std::string> args;

    // Remove all white spaces
    s.erase(remove_if(s.begin(), s.end(), isspace), s.end());

    // Seperate the args
    std::string tmp;
    size_t pos;
    while ((pos = s.find(',')) != std::string::npos)
    {
      tmp = s.substr(0, pos);
      args.push_back(tmp);
      s.erase(0, pos + 1);
    }
    args.push_back(s);

    // Check that we have 3 args
    assert(args.size() == 3);

    // Allocate them to members accordingly
    arg_name = args[0];
    ptr_name = args[1];
    dim_name = args[2];
  }

  std::string str()
  {
    std::ostringstream rhs;
    rhs << "\t" << arg_name << " -> ptr: [" << ptr_name << "] , dim: [" << dim_name << "]";
    return rhs.str();
  }
};

struct OneAPIFuncStruct
{
  std::string funcName;
  std::vector<OneAPIArgStruct> SubmitArgs;
  std::vector<std::string> oneapi_args_names;
  std::string oneapi_output_arg;

  std::vector<const clang::Expr *> args_expr_pntrs;

  // OneAPI Location i.e. `compile_to_onepai()` method
  SourceLocation oneapi_start;
  SourceLocation oneapi_end;

  // Pragma t2s_submit pragma location
  SourceLocation submit_start;
  SourceLocation submit_end;

  std::string str()
  {
    std::ostringstream rhs;
    rhs << "\n";
    rhs << "funcName: " << funcName << "\n";
    rhs << "OutputArg: " << oneapi_output_arg << "\n";

    rhs << "oneapi_args_names: [";
    for (unsigned int i = 0; i < oneapi_args_names.size(); i++)
    {
      rhs << oneapi_args_names[i];
      if (i < oneapi_args_names.size() - 1)
      {
        rhs << ", ";
      }
    }
    rhs << "]\n";
    for (unsigned int i = 0; i < SubmitArgs.size(); i++)
    {
      rhs << SubmitArgs[i].str() << "\n";
    }

    return rhs.str();
  }

  // For parsing the `compile_to_oneapi()` method
  void parseExprPntrs(Rewriter &R, SourceLocation start, SourceLocation end)
  {

    if (args_expr_pntrs.size() < 3)
    {
      std::cout << t2sprinter::error() << "Error: args_expr_pntrs is size zero.\n";
    }

    if (funcName.size() == 0)
    {
      std::string ret = t2sprinter::parseOneAPIName(
          R.getRewrittenText(SourceRange(args_expr_pntrs[1]->getBeginLoc(), args_expr_pntrs[1]->getEndLoc())));
      funcName = ret;
    }

    if (oneapi_args_names.size() == 0)
    {
      std::vector<std::string> ret = t2sprinter::parseOneAPIVector(
          R.getRewrittenText(SourceRange(args_expr_pntrs[0]->getBeginLoc(), args_expr_pntrs[0]->getEndLoc())));
      oneapi_args_names = ret;
    }

    if (oneapi_output_arg.size() == 0)
    {
      std::string ret = t2sprinter::parseOneAPIOutputArg(
          R.getRewrittenText(SourceRange(start, end)));
      oneapi_output_arg = ret;
    }
  }

  // For parsing the arugments themselves
  void parseArgPntrs(Rewriter &R, SourceLocation start, SourceLocation end)
  {
    // Get the t2s_submit pragma line as a string
    std::string Args = R.getRewrittenText(SourceRange(start, end));
    parseArgPntrsHelper(Args);
  }

  void parseArgPntrsHelper(std::string Args)
  {
    // Get the open & close bracket index
    std::vector<int> open_bracket;
    std::vector<int> close_bracket;
    for (unsigned int i; i < Args.size(); i++)
    {
      if (Args.at(i) == '(')
      {
        open_bracket.push_back(i);
      }
    }
    for (unsigned int i; i < Args.size(); i++)
    {
      if (Args.at(i) == ')')
      {
        close_bracket.push_back(i);
      }
    }

    // Create and Push back the OneAPIArgStruct
    for (unsigned int i = 0; i < open_bracket.size(); i++)
    {
      std::string sub_str = Args.substr(open_bracket[i] + 1, close_bracket[i] - (open_bracket[i] + 1));
      OneAPIArgStruct tmp;
      tmp.ParseArgs(sub_str);
      SubmitArgs.push_back(tmp);
    }
  }
};

struct ParamLocs
{
  SourceLocation start;
  SourceLocation end;
  Token *tkn;

  std::string args;

  std::vector<QualType> arg_types;

  OneAPIFuncStruct oneapiStruct;

  ParamLocs(SourceLocation s, SourceLocation e) : start(s), end(e) {}
  ParamLocs(SourceLocation s, SourceLocation e, Token *t) : start(s), end(e), tkn(t) {}

  std::vector<std::string> SplitArgs()
  {
    // Get the argument names and put them into a string vector
    std::string tmp_args = args;
    std::vector<std::string> args_vector;
    std::string delim = ",";
    size_t pos = 0;
    bool last_arg = false;
    while ((pos = tmp_args.find(delim)) != std::string::npos || !last_arg)
    {
      std::string token = tmp_args.substr(0, pos);
      args_vector.push_back(token);
      tmp_args.erase(0, pos + delim.length());

      if (pos == std::string::npos)
        last_arg = true;
    }
    return args_vector;
  }

  void initOneAPIStruct(Rewriter &orig_Rewriter)
  {
    oneapiStruct.parseExprPntrs(orig_Rewriter, start, end);
  }
};

std::string GenRunPostCode(OneAPIFuncStruct oneapiStruct, std::vector<std::string> args_name, std::vector<QualType> args_types, std::string funcName)
{
  std::ostringstream rhs;  
  // check that there is an even number i.e. paris of args and their dimension vector
  if (((args_name.size() % 3) != 0) || ((args_types.size() % 3) != 0))
  {
    std::cout << t2sprinter::error() << "Uneven match of argument names and argument types i.e. [" << args_name.size() << "] arg names and [" << args_types.size() << "] arg types found.\n";
    std::cout << t2sprinter::usage();
    exit(1);
  }

  rhs << "\n\n";

  std::vector<std::string> new_halide_names_vector;
  std::string new_halide_output_name;

  for (unsigned int i = 0; i < args_name.size(); i = i + 3)
  {
    unsigned int FuncArgName_index = i;
    unsigned int pointer_index = i + 1;
    unsigned int dim_index = i + 2;

    std::string func_arg_name = args_name[FuncArgName_index];
    std::string new_halide_name = func_arg_name + "_h";

    if (func_arg_name == oneapiStruct.oneapi_output_arg)
    {
      new_halide_output_name = new_halide_name;
    }

    std::string arg_ptr_name = args_name[pointer_index];
    new_halide_names_vector.push_back(new_halide_name);
    std::string arg_dim_vector_name = args_name[dim_index];

    // Retrieve the QualTypes
    QualType t_ptr = args_types[pointer_index]; // pointer type
    QualType t_dim = args_types[dim_index];     // array type

    // // Get the type of Halide::Runtime::Buffer by Splitting the type to get the base type
    std::string arg_type = t_ptr.getAsString();
    arg_type = t2sprinter::get_base_type(arg_type);

    // // Get the number of dimensions
    std::string arg_dim_type = t_dim.getAsString();
    arg_dim_type = t2sprinter::get_base_type(arg_dim_type);
    std::string arg_dim_vector_size = "(sizeof(" + arg_dim_vector_name + ")/sizeof(" + arg_dim_type + "))";

    // // Get the dimensions as an std::vector
    std::string arg_dims_as_vector = "std::vector<" + arg_dim_type + ">(std::begin(" + arg_dim_vector_name + "), std::end(" + arg_dim_vector_name + "))";

    // // For debugging purposes
    // rhs << "// func_arg_name: " << func_arg_name << "\n";
    // rhs << "// arg_ptr_name: " << arg_ptr_name << "\n";
    // rhs << "// arg_type: " << arg_type << "\n";
    // rhs << "// new_halide_name: " << new_halide_name << "\n";
    // rhs << "// arg_dim_vector_name: " << arg_dim_vector_name << "\n";
    // rhs << "// arg_dim_type: " << arg_dim_type << "\n";
    // rhs << "// arg_dim_vector_size: " << arg_dim_vector_size << "\n";
    // rhs << "// arg_dims_as_vector: " << arg_dims_as_vector << "\n";
    // rhs << "\n\n";

    // // Write the initalization of the halide runtime buffer
    rhs << "#ifdef FPGA\n";
    rhs << "Halide::Runtime::Buffer<" << arg_type << ", " << arg_dim_vector_size << "> " << new_halide_name << "(" << arg_ptr_name << ", " << arg_dims_as_vector << ");\n";
    rhs << "#endif\n";
    // ATTENTION:only 2-D image is supported by preprocessor
    /*
    HERE WE GOT FOLLOWING CL DATA TYPES:
      snorm_int8 = 0,
      snorm_int16 = 1,
      unorm_int8 = 2,
      unorm_int16 = 3,
      unorm_short_565 = 4,
      unorm_short_555 = 5,
      unorm_int_101010 = 6,
      signed_int8 = 7,
      signed_int16 = 8,
      signed_int32 = 9,
      unsigned_int8 = 10,
      unsigned_int16 = 11,
      unsigned_int32 = 12,
      fp16 = 13,
      fp32 = 14
    */
    std::string image_type;
    if (arg_type == "float")
      image_type = "fp32";
    else if (arg_type == "int")
      image_type = "signed_int32";
    else if (arg_type == "unsigned int")
      image_type = "unsigned_int32";
    else
    {
      std::cout << t2sprinter::error() << "currently, image type does not support arg type:" << arg_type << ",please check t2sp/t2s/preprocessor/t2spreprocessor.h line 555 for detailed info";
      exit(0);
    }
    rhs << "#ifdef GPU\n";
    //currently only 2-D image is supported
    //here we must explicitly print out T2S image extension
    //std::cout << t2sprinter::header() << "debugging purpose:loop variable i = " << i <<'\n';
    rhs << "int _"<<func_arg_name << "_extent_" << "0 = "<< arg_dim_vector_name << "[0];\n";
    rhs << "int _"<<func_arg_name << "_extent_" << "1 = "<< arg_dim_vector_name << "[1];\n";
    rhs << "sycl::image<2> img" << "_" << func_arg_name << "(" << arg_ptr_name << ", image_channel_order::rgba, image_channel_type::" << image_type << ",\n"
        << "range<2>{" << arg_dim_vector_name << "[0]/4 , " << arg_dim_vector_name << "[1]});\n";
    rhs << "#endif\n";
  }

  // Write out the device selector options
  // preprocessor for FPGA
  rhs << "#ifdef FPGA\n";
  rhs << "#if defined(FPGA_EMULATOR)\n";
  rhs << "std::cout << \"USING FPGA EMULATOR\\n\";\n";
  rhs << "sycl::ext::intel::fpga_emulator_selector device_selector; // (NOTE) for emulation\n";
  rhs << "#else\n";
  rhs << "std::cout << \"USING REAL FPGA\\n\";\n";
  rhs << "sycl::ext::intel::fpga_selector device_selector; // (NOTE) for full compile and hardware profiling \n";
  rhs << "#endif\n";

  // Write out the execution of the function
  rhs << "std::cout << \"Start Run\\n\";\n";
  rhs << "double exec_time = 0;\n";
  rhs << "exec_time = " + funcName + "(device_selector, ";
  for (unsigned int i = 0; i < new_halide_names_vector.size(); i++)
  {
    if (new_halide_names_vector[i] != new_halide_output_name)
    {
      rhs << new_halide_names_vector[i] << ".raw_buffer()";
      if (i != new_halide_names_vector.size() - 1)
      {
        rhs << ", ";
      }
    }
  }
  rhs << new_halide_output_name << ".raw_buffer()";
  rhs << ");\n";
  rhs << "std::cout << \"Run completed!\\n\";\n";
  rhs << "std::cout << \"kernel exec time: \" << exec_time << \"\\n\";\n";
  rhs << "#endif\n";
  return rhs.str();
}

#endif // T2SPREPROCESSOR_H_