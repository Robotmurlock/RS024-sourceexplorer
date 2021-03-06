#include "clang_interface.h"

#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Tooling/Tooling.h"

#include <fstream>
#include <iostream>
#include <sstream>

namespace clang_interface {

std::ostream& operator<<(std::ostream& out, const ParamVarDecl& param_decl) {
  DUMP(out, param_decl.ID());
  DUMP(out, param_decl.NameAsString());
  DUMP(out, param_decl.TypeAsString());
  return out;
}
std::ostream& operator<<(std::ostream& out, const FunctionDecl& func_decl) {
  DUMP(out, func_decl.ID());
  DUMP(out, func_decl.NameAsString());
  DUMP(out, func_decl.ReturnTypeAsString());
  for (auto param = func_decl.ParamBegin(); param != func_decl.ParamEnd();
       ++param) {
    out << *param;
  }
  return out;
}
std::ostream& operator<<(std::ostream& out, const Edge& edge) {
  DUMP(out, edge.caller->ID());
  DUMP(out, edge.callee->ID());
  return out;
}
std::ostream& operator<<(std::ostream& out, const CallGraph& call_graph) {
  out << "--NODES--\n";
  for (const auto& node : call_graph.nodes) {
    out << "Node:\n";
    out << *node << '\n';
  }
  out << "\n--EDGES--\n";
  for (const auto& edge : call_graph.edges) {
    out << "Edge:\n";
    out << edge << '\n';
  }
  return out;
}

void AddEdge(CallGraph& call_graph, Edge edge) {
  call_graph.edges.emplace_back(std::move(edge));
}

class CallerCalleeFinderCallback
    : public clang::ast_matchers::MatchFinder::MatchCallback {
 private:
  CallGraph& call_graph;
  clang::ASTContext& ast_context;
 public:
  CallerCalleeFinderCallback(clang_interface::CallGraph& cg, clang::ASTContext& ast_context) : call_graph(cg), ast_context(ast_context) {}
  virtual void run(
      const clang::ast_matchers::MatchFinder::MatchResult& Results) {
    auto caller_decl = Results.Nodes.getNodeAs<clang::FunctionDecl>("caller");
    auto callee_call_expr_decl =
        Results.Nodes.getNodeAs<clang::CallExpr>("callee");
    if (!caller_decl || !callee_call_expr_decl) {
      return;
    }
    auto callee_decl = callee_call_expr_decl->getDirectCallee();
    if (callee_decl == nullptr) {
      return;
    }
    auto existing_caller_node = std::find_if(
        begin(call_graph.nodes), end(call_graph.nodes),
        [id = caller_decl->getID()](const auto& n) { return n->ID() == id; });

    FunctionDecl* new_caller_node;
    if (existing_caller_node == end(call_graph.nodes)) {
      call_graph.nodes.emplace_back(
          std::make_unique<clang_interface::FunctionDecl>(caller_decl, ast_context.getFullLoc(caller_decl->getBeginLoc())));
      new_caller_node = call_graph.nodes.back().get();
    } else {
      new_caller_node = existing_caller_node->get();
    }

    auto existing_callee_node = std::find_if(
        begin(call_graph.nodes), end(call_graph.nodes),
        [id = callee_decl->getID()](const auto& n) { return n->ID() == id; });

    FunctionDecl* new_callee_node;
    if (existing_callee_node == end(call_graph.nodes)) {
      call_graph.nodes.emplace_back(
          std::make_unique<clang_interface::FunctionDecl>(callee_decl, ast_context.getFullLoc(callee_decl->getBeginLoc())));
      new_callee_node = call_graph.nodes.back().get();
    } else {
      new_callee_node = existing_callee_node->get();
    }

    AddEdge(call_graph, {new_caller_node, new_callee_node});
  }

};  // CallerCalleeCallBack

ASTUnit BuildASTFromSource(const std::string& source,
                           std::vector<std::string> compiler_args) {
  compiler_args.push_back("-std=c++17");
  compiler_args.push_back("-nostdinc++");
  compiler_args.push_back("-v");
  ASTUnit ast(clang::tooling::buildASTFromCodeWithArgs(source, compiler_args));
  return ast;
}

clang_interface::CallGraph ExtractCallGraphFromAST(ASTUnit& ast) {
  CallGraph call_graph;
  clang_interface::CallerCalleeFinderCallback Callback(call_graph, ast.ASTContext());
  clang::ast_matchers::MatchFinder Finder;

  using clang::ast_matchers::callExpr;
  using clang::ast_matchers::functionDecl;
  using clang::ast_matchers::hasAncestor;

  Finder.addMatcher(
      callExpr(hasAncestor(functionDecl().bind("caller"))).bind("callee"),
      &Callback);

  Finder.matchAST(ast.ASTContext());
  return call_graph;
}

clang_interface::CallGraph ExtractCallGraphFromSource(
    const std::string& source) {
  ASTUnit ast = BuildASTFromSource(source);
  return ExtractCallGraphFromAST(ast);
}

};  // namespace clang_interface
