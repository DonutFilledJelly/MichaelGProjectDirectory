#include <stdio.h>

extern "C" {
    #include "cc.h"
    #include "scan.h"
    #include "semutil.h"
    #include "sem.h"
    #include "sym.h"
}

#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Function.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include <utility>
#include <cstdlib>
#include <memory>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>
#include <list>
#include <map>

# define MAXLOOPNEST 50
# define MAXLABELS 50
# define MAXGOTOS 50

using std::map;
using std::string;
using std::vector;
using llvm::outs;
using llvm::Type;
using llvm::Value;
using llvm::Module;
using llvm::Function;
using llvm::Constant;
using llvm::IRBuilder;
using llvm::ArrayType;
using llvm::BasicBlock;
using llvm::AllocaInst;
using llvm::BranchInst;
using llvm::Instruction;
using llvm::LLVMContext;
using llvm::ConstantInt;
using llvm::GlobalValue;
using llvm::IntegerType;
using llvm::PointerType;
using llvm::FunctionType;
using llvm::GlobalVariable;
using llvm::ConstantAggregateZero;

extern int formalnum;                         /* number of formal arguments */
extern struct id_entry* formalvars[MAXLOCS];  /* entries for parameters */
extern int localnum;                          /* number of local variables  */
extern struct id_entry* localvars[MAXLOCS];   /* entries for local variables */

static LLVMContext TheContext;
static IRBuilder<> Builder(TheContext);
static std::unique_ptr<Module> TheModule;

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

static int label_index = 0;
int relexpr = 0;

struct loopscope {
  struct sem_rec* breaks;
  struct sem_rec* conts;
} lscopes[MAXLOOPNEST];

static int looplevel = 0;
struct loopscope *looptop = (struct loopscope *) NULL;

struct labelnode {
   const char *id;    /* label string    */
   BasicBlock *bb;    /* basic block for label */
} labels[MAXLABELS];

struct gotonode {
   const char *id;     /* label string in goto statement */
   BranchInst *branch; /* branch to temporary label */
} gotos[MAXGOTOS];     /* list of gotos to be backpatched */

int numgotos = 0;    /* number of gotos to be backpatched */
int numlabelids = 0; /* total label ids in function */

std::string new_label()
{
  return ("L" + std::to_string(label_index++));
}

BasicBlock*
create_tmp_label()
{
  
  return BasicBlock::Create(TheContext);
}

BasicBlock*
create_named_label(std::string label)
{
  Function *curr_func = Builder.GetInsertBlock()->getParent();
  BasicBlock *new_block = BasicBlock::Create(TheContext, label, curr_func);
  return new_block;
}

/*
 * convert an internal csem type (s_type or i_type) to an LLVM Type*
 */
Type *get_llvm_type(int type){
  switch( type &~(T_ARRAY|T_ADDR) ){
  case T_INT:
    return Type::getInt32Ty(TheContext);
    break;
  case T_DOUBLE:
    return Type::getDoubleTy(TheContext);
    break;
  default:
    fprintf(stderr,"get_llvm_type: invalid type %x\n", type);
    exit(1);
    break;
  }
}

/*
 * startloopscope - start the scope for a loop
 */
void
startloopscope()
{
   looptop = &lscopes[looplevel++];
   if (looptop > lscopes+MAXLOOPNEST) {
      fprintf(stderr, "loop nest too great\n");
      exit(1);
   }
   looptop->breaks = (struct sem_rec *) NULL;
   looptop->conts = (struct sem_rec *) NULL;
}

/*
 * endloopscope - end the scope for a loop
 */
void
endloopscope()
{
  looplevel--;
  looptop--;
}


/*
 * Global allocations. Globals are initialized to 0.
 */
void
global_alloc (struct id_entry *p, int width)
{
  string name(p->i_name);
  GlobalVariable *var;
  Type *type;
  Constant *init;

  if (p->i_type & T_ARRAY) {
    type = ArrayType::get(get_llvm_type(p->i_type), width);
    init = ConstantAggregateZero::get(type);
  }
  else {
    type = get_llvm_type(p->i_type);
    init = ConstantInt::get(get_llvm_type(T_INT), 0);
  }

  TheModule->getOrInsertGlobal(name, type);
  var = TheModule->getNamedGlobal(name);
  var->setInitializer(init);
  p->i_value = (void*) var;
}


/*
 * backpatch - set temporary labels in the sem_rec to real labels
 *
 * LLVM API calls:
 *
 * llvm::dyn_cast<BranchInst>(Value*)
 * BranchInst::getNumSuccessors()
 * BranchInst::getSuccessor(unsigned)
 * BranchInst::setSuccessor(unsigned, BasicBlock*)
 */
void backpatch(struct sem_rec *rec, void *bb)
{
  unsigned i;
  BranchInst *br_inst;
  do{
  if( (br_inst = llvm::dyn_cast<BranchInst>((Value*)rec->s_value)) ){
    for(i = 0; i < br_inst->getNumSuccessors(); i++){
      if( br_inst->getSuccessor(i) == ((BasicBlock *) rec->s_bb) ) {
        br_inst->setSuccessor(i, (BasicBlock *)bb);
      }
    }

  }else {
    fprintf(stderr, "error: backpatch with non-branch instruction\n");
    exit(1);
  }
  rec = rec->s_link;
  }while(rec != NULL);
  for(int i = 0; i < numgotos; i++){
    if (gotos[i].id != NULL && strcmp(gotos[i].id, ((BasicBlock*)bb)->getName().str().c_str()) == 0) {
      gotos[i].branch->setSuccessor(0, (BasicBlock*)bb);
    }
    }
  }

  //fprintf(stderr, "sem: backpatch not implemented\n");
  //return;



/*
 * call - procedure invocation
 *
 * Grammar:
 * lval -> ID '(' ')'            { $$ = call($1, (struct sem_rec *) NULL); }
 * lval -> ID '(' exprs ')'      { $$ = call($1, $3); }
 *
 * LLVM API calls:
 * makeArrayRef(vector<Value*>)
 * IRBuilder::CreateCall(Function *, ArrayRef<Value*>)
 */
struct sem_rec*
call(char *f, struct sem_rec *args)
{ 
  Function *FT;
  FT = TheModule->getFunction(std::string(f));
  vector<Value*> Argvec;
  Argvec.push_back((Value*)args->s_value);
  while(args->s_link != NULL){
    Argvec.push_back((Value *)args->s_link->s_value);
    args = args->s_link;
  }
  Value* val;

  val = Builder.CreateCall(FT, makeArrayRef(Argvec));

  return (s_node (val, args->s_type));
  fprintf(stderr, "sem: call not implemented\n");
  return ((struct sem_rec*) NULL);
}

/*
 * ccand - logical and
 *
 * Grammar:
 * cexpr -> cexpr AND m cexpr     { $$ = ccand($1, $3, $4); }
 *
 * LLVM API calls:
 * None
 */
struct sem_rec*
ccand(struct sem_rec *e1, void *m, struct sem_rec *e2)
{
  
  backpatch(e1->s_true, m);
  merge(e2->s_false, e1->s_false);
  return e2;
  fprintf(stderr, "sem: ccand not implemented\n");
  return ((struct sem_rec*) NULL);
}

/*
 * ccexpr - convert arithmetic expression to logical expression
 *
 * Grammar:
 * cexpr -> expr                  { $$ = ccexpr($1); }
 *
 * LLVM API calls:
 *
 * IRBuilder::CreateICmpNE(Value *, Value *)
 * IRBuilder::CreateFCmpONE(Value *, Value *)
 * IRBuilder::CreateCondBr(Value *, BasicBlock *, BasicBlock *)
 */
struct sem_rec*
ccexpr(struct sem_rec *e)
{
  BasicBlock *tmp_true, *tmp_false;
  Value *val;
  tmp_true = create_tmp_label();
  tmp_false = create_tmp_label();
  val = Builder.CreateCondBr((Value*)e->s_value, tmp_true, tmp_false );
  return (node ( (void*) NULL, (void *) NULL, 0, (struct sem_rec*) NULL, 
        (node (val, tmp_true, 0, (struct sem_rec*) NULL, (struct sem_rec*) NULL, 
        (struct sem_rec*) NULL)),
        (node (val, tmp_false, 0, (struct sem_rec*) NULL, (struct sem_rec*) NULL,
        (struct sem_rec*) NULL))));
  //fprintf(stderr, "sem: ccexpr not implemented\n");
  //return ((struct sem_rec *) NULL);
}

/*
 * ccnot - logical not
 *
 * Grammar:
 * cexpr -> NOT cexpr             { $$ = ccnot($2); }
 *
 * LLVM API calls:
 * None
 */
struct sem_rec*
ccnot(struct sem_rec *e)
{
  struct sem_rec *tmpfalse, *tmptrue;
  tmpfalse = e->s_true;
  tmptrue = e->s_false;
  return (node (e->s_value, e->s_bb, e->s_type, e->s_link, tmptrue, tmpfalse));
  fprintf(stderr, "sem: ccnot not implemented\n");
  return ((struct sem_rec *) NULL);
}

/*
 * ccor - logical or
 *
 * Grammar:
 * cexpr -> cexpr OR m cexpr      { $$ = ccor($1, $3, $4); }
 *
 * LLVM API calls:
 * None -- but uses backpatch
 */
struct sem_rec*
ccor(struct sem_rec *e1, void *m, struct sem_rec *e2)
{
  backpatch(e1->s_false, m);
  merge(e2->s_true, e1->s_true);
  return e2;
  fprintf(stderr, "sem: ccor not implemented\n");
  return NULL;
}

/*
 * con - constant reference in an expression
 *
 * Grammar:
 * expr -> CON                   { $$ = con($1); }
 *
 * LLVM API calls:
 * ConstantInt::get(Type*, int)
 */
struct sem_rec*
con(const char *x)
{
  struct id_entry *entry;
  if ((entry = lookup(x, 0)) == NULL) {
    entry = install (x, 0);
    entry->i_type = T_INT;
    entry->i_scope = GLOBAL;
    entry->i_defined = 1;
  } 
    


    //printf(stderr, "sem: remainder of con not implemented\n");
    //return ((struct sem_rec *) NULL);
  
  entry->i_value = (void *) ConstantInt::get(get_llvm_type(T_INT), std::stoi(x));
  return (s_node ((void *) entry->i_value, entry->i_type) );
  
}

/*
 * dobreak - break statement
 *
 * Grammar:
 * stmt -> BREAK ';'                { dobreak(); }
 *
 * LLVM API calls:
 * None -- but uses n
 */
void
dobreak()
{
  if(looptop == NULL){
    fprintf(stderr, "Error: 'break' not within a loop\n");
    return;
  }
  struct sem_rec *break_lab = n();
  break_lab->s_link = looptop->breaks;
  looptop->breaks = break_lab;
  fprintf(stderr, "sem: dobreak not implemented\n");
  return;
}

/*
 * docontinue - continue statement
 *
 * Grammar:
 * stmt -> CONTINUE ';'              { docontinue(); }
 *
 * LLVM API calls:
 * None -- but uses n
 */
void
docontinue()
{
  if(looptop == NULL){
    fprintf(stderr, "Error: 'continue' not within a loop\n");
    return;
  }
  struct sem_rec *cont_lab = n();
  cont_lab->s_link = looptop->conts;
  looptop->conts = cont_lab;
  fprintf(stderr, "sem: docontinue not implemented\n");
  return;
}

/*
 * dodo - do statement
 *
 * Grammar:
 * stmt -> DO m s lblstmt WHILE '(' m cexpr ')' ';' m
 *                { dodo($2, $7, $8, $11); }
 *
 * LLVM API calls:
 * None -- but uses backpatch
 */
void
dodo(void *m1, void *m2, struct sem_rec *cond, void *m3)
{
  backpatch(cond->s_true ,m1);
  backpatch(cond->s_false, m3);
  startloopscope();
  endloopscope();
  if(looptop->conts != NULL){
    backpatch(looptop->conts, m1);
  }
  if(looptop->breaks != NULL){
    backpatch(looptop->breaks, m3);
  }
  return;
  fprintf(stderr, "sem: dodo not implemented\n");
  return;
}

/*
 * dofor - for statement
 *
 * Grammar:
 * stmt -> FOR '(' expro ';' m cexpro ';' m expro n ')' m s lblstmt n m
 *               { dofor($5, $6, $8, $10, $12, $15, $16); }
 *
 * LLVM API calls:
 * None -- but uses backpatch
 */
void
dofor(void *m1, struct sem_rec *cond, void *m2, struct sem_rec *n1, void *m3,
  struct sem_rec *n2, void *m4)
{ 
  backpatch(cond->s_true, m3);
  backpatch(cond->s_false, m4);
  backpatch(n1, m1);
  backpatch(n2, m2);
  /*
  backpatch break and continue and scoping
  */
  fprintf(stderr, "sem: dofor not implemented\n");
  return;
}

/*
 * dogoto - goto statement
 *
 * Grammar:
 * stmt -> GOTO ID ';'              { dogoto($2); }
 *
 * LLVM API calls:
 * IRBuilder::CreateBr(BasicBlock *)
 */
void
dogoto(char *id)
{
  fprintf(stderr, "sem: dogoto not implemented\n");
  char userlbl[256]; 
  snprintf(userlbl, sizeof(userlbl), "userlbl%s", id);

  BasicBlock *lbb = NULL;
  for(int i = 0; i < numlabelids; i++){
       if(strcmp(userlbl, labels[i].id) == 0){
          fprintf(stderr, "found \n");
          lbb = labels[i].bb;
          
       }

  }
  fprintf(stderr, "after first for\n");
  if(lbb != NULL){
    fprintf(stderr, "We are in top\n");
    Builder.CreateBr(lbb);
    fprintf(stderr, "After Builder\n");
  }
  else{
    fprintf(stderr, "We are in bottom\n");
    lbb = create_tmp_label();
    gotos[numgotos].id = strdup(userlbl);
    gotos[numgotos].branch = Builder.CreateBr(lbb);
    numgotos++;
  }
  fprintf(stderr, "we're returning\n");

  return;
}

/*
 * doif - one-arm if statement
 *
 * Grammar:
 * stmt -> IF '(' cexpr ')' m lblstmt m
 *         { doif($3, $5, $7); }
 *
 * LLVM API calls:
 * None -- but uses backpatch
 */
void
doif(struct sem_rec *cond, void *m1, void *m2)
{
  backpatch(cond->s_true, m1);
  backpatch(cond->s_false, m2);
  
  
  //fprintf(stderr, "sem: doif not implemented\n");
  //return;
}

/*
 * doifelse - if then else statement
 *
 * Grammar:
 * stmt -> IF '(' cexpr ')' m lblstmt ELSE n m lblstmt m
 *                { doifelse($3, $5, $8, $9, $11); }
 *
 * LLVM API calls:
 * None -- but uses backpatch
 */
void
doifelse(struct sem_rec *cond, void *m1, struct sem_rec *n,
  void *m2, void *m3)
{/*
  backpatch(cond->s_true, m1);
  backpatch(cond->s_false, m2);
*/
backpatch(cond->s_true, m1);
backpatch(n, m3);
backpatch(cond->s_false, m2);
return;
  fprintf(stderr, "sem: doifelse not implemented\n");
  return;
}

/*
 * doret - return statement
 *
 * Grammar:
 * stmt -> RETURN ';'            { doret((struct sem_rec *) NULL); }
 * stmt -> RETURN expr ';'       { doret($2); }
 *
 * LLVM API calls:
 * IRBuilder::CreateRetVoid();
 * IRBuilder::CreateRet(Value *);
 */
void
doret(struct sem_rec *e)
{
  if(!e){
    Builder.CreateRetVoid();
    return;
  }
  Builder.CreateRet( ((Value*) e->s_value) );
  //fprintf(stderr, "sem: doret not implemented\n");
  //return;
}

/*
 * dowhile - while statement
 *
 * Grammar:
 * stmt -> WHILE '(' m cexpr ')' m s lblstmt n m
 *                { dowhile($3, $4, $6, $9, $10); }
 *
 * LLVM API calls:
 * None -- but uses backpatch
 */
void
dowhile(void *m1, struct sem_rec *cond, void *m2,
  struct sem_rec *n, void *m3)
{

  backpatch(cond->s_true, m2);
  backpatch(cond->s_false, m3);
  backpatch(n, m1);
  startloopscope();
  endloopscope();
  if(looptop->conts != NULL){
  backpatch(looptop->conts, m1);
  }
  if(looptop->breaks != NULL){
  backpatch(looptop->breaks, m3);
  }
  return;
  fprintf(stderr, "sem: dowhile not implemented\n");
  return;
}

/*
 * exprs - form a list of expressions
 *
 * Grammar:
 * exprs -> exprs ',' expr        { $$ = exprs($1, $3); }
 *
 * LLVM API calls:
 * None
 */
struct sem_rec*
exprs(struct sem_rec *l, struct sem_rec *e)
{
  //printf("express\n");
  if(l == NULL){
    return e;
  }else{
    struct sem_rec *current = l;
    while(current->s_link != NULL){
      current = current->s_link;
    }
    current->s_link = e;
    return l;
    }
  


  return merge(l, e);
  fprintf(stderr, "sem: exprs not implemented\n");
  return ((struct sem_rec *) NULL);
}

/*
 * fhead - beginning of function body
 *
 * Grammar:
 * fhead -> fname fargs '{' dcls  { fhead($1); }
 */
void
fhead(struct id_entry *p)
{
  Type *func_type, *var_type;
  Value *arr_size;
  vector<Type*> func_args;
  GlobalValue::LinkageTypes linkage;
  FunctionType *FT;
  Function *F;
  BasicBlock *B;
  int i;
  struct id_entry *v;

  /* get function return type */
  func_type = get_llvm_type(p->i_type);

  /* get function argument types */
  for (i = 0; i < formalnum; i++) {
    func_args.push_back(get_llvm_type(formalvars[i]->i_type));
  }

  FT = FunctionType::get(func_type, makeArrayRef(func_args), false);

  /* linkage is external if function is main */
  linkage = (strcmp(p->i_name, "main") == 0) ?
            Function::ExternalLinkage :
            Function::InternalLinkage ;

  F = Function::Create(FT, linkage, p->i_name, TheModule.get());
  p->i_value = (void*) F;


  B = BasicBlock::Create(TheContext, "", F);
  Builder.SetInsertPoint(B);

  /*
   * Allocate instances of each parameter and local so they can be referenced
   * and mutated.
   */
  i = 0;
  for (auto &arg : F->args()) {

    v = formalvars[i++];
    arg.setName(v->i_name);
    var_type = get_llvm_type(v->i_type);
    arr_size = (v->i_width > 1) ?
               (ConstantInt::get(get_llvm_type(T_INT), v->i_width)) :
               NULL;

    v->i_value = Builder.CreateAlloca(var_type, arr_size, arg.getName());
    Builder.CreateStore(&arg, (Value*)v->i_value);
  }

  /* Create the instance of stack memory for each local variable */
  for (i = 0; i < localnum; i++) {
    v = localvars[i];
    var_type = get_llvm_type(v->i_type);
    arr_size = (v->i_width > 1) ?
               (ConstantInt::get(get_llvm_type(T_INT), v->i_width)) :
               NULL;

    v->i_value = Builder.CreateAlloca(var_type, arr_size, std::string(v->i_name));
  }
}

/*
 * fname - function declaration
 *
 * Grammar:
 * fname -> type ID               { $$ = fname($1, $2); }
 * fname -> ID                    { $$ = fname(T_INT, $1); }
 */
struct id_entry*
fname(int t, char *id)
{
  struct id_entry *entry = lookup(id, 0);

  // add function to hash table if it doesn't exist
  if (!entry) {
    entry = install(id, 0);
  }

  // cannot have two functions of the same name
  if (entry->i_defined) {
    yyerror("cannot declare function more than once");
  }

  entry->i_type = t;
  entry->i_scope = GLOBAL;
  entry->i_defined = true;

  // need to enter the block to let hash table do internal work
  enterblock();
  // then need to reset argument count variables

  formalnum = 0;
  localnum = 0;

  return entry;
}

/*
 * ftail - end of function body
 *
 * Grammar:
 * func -> fhead stmts '}'       { ftail(); }
 */
void
ftail()
{
  numgotos = 0;
  numlabelids = 0;
  leaveblock();
}

/*
 * id - variable reference
 *
 * Grammar:
 * lval -> ID                    { $$ = id($1); }
 * lval -> ID '[' expr ']'       { $$ = indx(id($1), $3); }
 *
 * LLVM API calls:
 * None
 */
struct sem_rec*
id(char *x)
{
  struct id_entry *entry;
  if ((entry = lookup(x, 0)) == NULL) {
    yyerror("undeclared identifier");
    entry = install(x, -1);
    entry->i_type = T_INT;
    entry->i_scope = LOCAL;
    entry->i_defined = 1;
  } 
    //fprintf(stderr, "sem: remainder of id not implemented\n");
  //return ((struct sem_rec *) NULL);
  

  return (s_node( (void *)entry->i_value, entry->i_type|T_ADDR));

  }
/*
 * indx - subscript
 *
 * Grammar:
 * lval -> ID '[' expr ']'       { $$ = indx(id($1), $3); }
 *
 * LLVM API calls:
 * makeArrayRef(vector<Value*>)
 * IRBuilder::CreateGEP(Type, Value *, ArrayRef<Value*>)
 */
struct sem_rec*
indx(struct sem_rec *x, struct sem_rec *i)
{
  Type *Array_type;
  Array_type = get_llvm_type(x->s_type);
  Value* baseAddr = static_cast<Value*>(x->s_value);
  vector<Value*> Argvec;
  Argvec.push_back(static_cast<Value*>(i->s_value));
  Value* elementptr =  Builder.CreateGEP(Array_type, baseAddr, makeArrayRef(Argvec));
  return (s_node (elementptr, x->s_type & ~T_ARRAY & ~T_ADDR));
  
  //return (s_node ( Builder.CreateGEP(Array_type, baseAddr, makeArrayRef(Argvec)), i->s_type & ~T_ADDR));

  fprintf(stderr, "sem: indx not implemented\n");
  return ((struct sem_rec *) NULL);
}

/*
 * labeldcl - process a label declaration
 *
 * Grammar:
 * labels -> ID ':'                { labeldcl($1); }
 * labels -> labels ID ':'         { labeldcl($2); }
 *
 * NOTE: All blocks in LLVM must have a terminating instruction (i.e., branch
 * or return statement -- fall-throughs are not allowed). This code must
 * ensure that each block ends with a terminating instruction.
 *
 * LLVM API calls:
 * IRBuilder::GetInsertBlock()
 * BasicBlock::getTerminator()
 * IRBuilder::CreateBr(BasicBlock*)
 * IRBuilder::SetInsertPoint(BasicBlock*)
 * BranchInst::setSuccessor(unsigned, BasicBlock*)
 */
void
labeldcl(const char *id)
{ 
  fprintf(stderr, "sem: labeldcl not implemented\n");
  
    
    char userlbl[256]; 
    snprintf(userlbl, sizeof(userlbl), "userlbl%s", id);

    BasicBlock *lbb = nullptr;
    for(int i =0; i < numlabelids; i++){
      if(strcmp(userlbl, labels[i].id) == 1){
          lbb = labels[i].bb;
          break;
      }
    }
    if(!lbb){
        fprintf(stderr, "After builder\n");
      lbb = create_named_label(userlbl);
      labels[numlabelids].id = strdup(userlbl);
      labels[numlabelids].bb = lbb;
      numlabelids++;
    }
    BasicBlock *bb2 = Builder.GetInsertBlock();
    if(!bb2->getTerminator()){
        Builder.CreateBr(lbb);
    }
    Builder.SetInsertPoint(lbb);
    for(int i = 0; i < numgotos; i++){
      if(gotos[i].id && strcmp(gotos[i].id, userlbl)==0){
        gotos[i].branch->setSuccessor(0, lbb);
        gotos[i].id = NULL;
      }
    }
  return;
}

/*
 * m - generate label and return next temporary number
 *
 * NOTE: All blocks in LLVM must have a terminating instruction (i.e., branch
 * or return statement -- fall-throughs are not allowed). This code must
 * ensure that each block ends with a terminating instruction.
 *
 * LLVM API calls:
 * IRBuilder::GetInsertBlock()
 * BasicBlock::getTerminator()
 * IRBuilder::CreateBr(BasicBlock*)
 * IRBuilder::SetInsertPoint(BasicBlock*)
 */
void*
m ()
{
  BasicBlock *bb;
  
  std::string label = new_label();
  bb = create_named_label(label);

  if(Builder.GetInsertBlock()->getTerminator() == NULL){
    Builder.CreateBr(bb);
  }
  Builder.SetInsertPoint(bb);
  return (void *) bb;
  fprintf(stderr, "sem: m not implemented\n");
  return (void *) NULL;
}

/*
 * n - generate goto and return backpatch pointer
 *
 * LLVM API calls:
 * IRBuilder::CreateBr(BasicBlock *)
 */
struct sem_rec *n()
{
  BasicBlock *bb;
  bb = create_tmp_label();
  Value *val;
  val = Builder.CreateBr(bb);
  struct sem_rec *jerry;
  jerry = node ( val, bb, 0, (struct sem_rec*) NULL, (struct sem_rec*) NULL, (struct sem_rec*) NULL);
  return jerry;

  fprintf(stderr, "sem: n not implemented\n");
  return NULL;
}

/*
 * op1 - unary operators
 *
 * LLVM API calls:
 * IRBuilder::CreateLoad(Type, Value *)
 * IRBuilder::CreateNot(Value *)
 * IRBuilder::CreateNeg(Value *)
 * IRBuilder::CreateFNeg(Value *)
 */
struct sem_rec*
op1(const char *op, struct sem_rec *y)
{
  
  struct sem_rec *rec;
  if (*op == '@'){
    if(!(y->s_type & T_ARRAY)){
      y->s_type &= ~T_ADDR;
      rec = s_node (Builder.CreateLoad ( get_llvm_type(y->s_type),((Value*)y->s_value)), y->s_type);
    }
  }else{

  fprintf(stderr, "sem: op1 not implemented\n");
  return ((struct sem_rec *) NULL);
  }
  return rec;
  
}

/*
 * op2 - arithmetic operators
 *
 * No LLVM API calls, but most functionality is abstracted to a separate
 * method used by op2, opb, and set.
 *
 * The separate method uses the following API calls:
 * IRBuilder::CreateAdd(Value *, Value *)
 * IRBuilder::CreateFAdd(Value *, Value *)
 * IRBuilder::CreateSub(Value *, Value *)
 * IRBuilder::CreateFSub(Value *, Value *)
 * IRBuilder::CreateMul(Value *, Value *)
 * IRBuilder::CreateFMul(Value *, Value *)
 * IRBuilder::CreateSDiv(Value *, Value *)
 * IRBuilder::CreateFDiv(Value *, Value *)
 * IRBuilder::CreateSRem(Value *, Value *)
 * IRBuilder::CreateAnd(Value *, Value *)
 * IRBuilder::CreateOr(Value *, Value *)
 * IRBuilder::CreateXOr(Value *, Value *)
 * IRBuilder::CreateShl(Value *, Value *)
 * IRBuilder::CreateAShr(Value *, Value *)
 */
struct sem_rec*
op2(const char *op, struct sem_rec *x, struct sem_rec *y)
{

  y = cast(y, x->s_type & ~T_ADDR);
  if((x->s_type & ~T_ADDR) == T_DOUBLE){
  if(op[0] == '+'){
    x->s_value = Builder.CreateFAdd((Value*)x->s_value, (Value*)y->s_value);
  }
  else if(op[0] == '*'){
    x->s_value = Builder.CreateFMul((Value*)x->s_value, (Value*)y->s_value);
  }
  else if(op[0] == '-'){
    x->s_value = Builder.CreateFSub((Value*)x->s_value, (Value*)y->s_value);
  }
  } else if((x->s_type & ~T_ADDR) == T_INT){
    if(op[0] == '+'){
    x->s_value = Builder.CreateAdd((Value*)x->s_value, (Value*)y->s_value);
  }
  else if(op[0] == '*'){
    x->s_value = Builder.CreateMul((Value*)x->s_value, (Value*)y->s_value);
  }
  else if(op[0] == '-'){
    x->s_value = Builder.CreateSub((Value*)x->s_value, (Value*)y->s_value);
  }
  else if(op[0] == '%'){
    x->s_value = Builder.CreateSRem((Value*)x->s_value, (Value*)y->s_value);
  }
  }
  return x;
  fprintf(stderr, "sem: op2 not implemented\n");
  return NULL;
}

/*
 * opb - bitwise operators
 *
 * No LLVM API calls, but most functionality is abstracted to a separate
 * method used by op2, opb, and set. The comment above op2 lists the LLVM API
 * calls for this method.
 */
struct sem_rec*
opb(const char *op, struct sem_rec *x, struct sem_rec *y)
{
  fprintf(stderr, "sem: opb not implemented\n");
  return ((struct sem_rec *) NULL);
}

/*
 * rel - relational operators
 *
 * Grammar:
 * cexpr -> expr EQ expr          { $$ = rel((char*) "==", $1, $3); }
 * cexpr -> expr NE expr          { $$ = rel((char*) "!=", $1, $3); }
 * cexpr -> expr LE expr          { $$ = rel((char*) "<=", $1, $3); }
 * cexpr -> expr GE expr          { $$ = rel((char*) ">=", $1, $3); }
 * cexpr -> expr LT expr          { $$ = rel((char*) "<",  $1, $3); }
 * cexpr -> expr GT expr          { $$ = rel((char*) ">",  $1, $3); }
 *
 * LLVM API calls:
 * IRBuilder::CreateICmpEq(Value *, Value *)
 * IRBuilder::CreateFCmpOEq(Value *, Value *)
 * IRBuilder::CreateICmpNE(Value *, Value *)
 * IRBuilder::CreateFCmpONE(Value *, Value *)
 * IRBuilder::CreateICmpSLT(Value *, Value *)
 * IRBuilder::CreateFCmpOLT(Value *, Value *)
 * IRBuilder::CreateICmpSLE(Value *, Value *)
 * IRBuilder::CreateFCmpOLE(Value *, Value *)
 * IRBuilder::CreateICmpSGT(Value *, Value *)
 * IRBuilder::CreateFCmpOGT(Value *, Value *)
 * IRBuilder::CreateICmpSGE(Value *, Value *)
 * IRBuilder::CreateFCmpOGE(Value *, Value *)
 */
struct sem_rec*
rel(const char *op, struct sem_rec *x, struct sem_rec *y)
{
  Value *val;
  bool isDouble = false;
  if (x->s_type == T_DOUBLE || y->s_type == T_DOUBLE){ 
      isDouble = true;
      if(y->s_type == T_INT){
        y = cast(y, T_DOUBLE & ~T_ADDR);
      }
      else if(x->s_type == T_INT){
        x = cast(x, T_DOUBLE & ~T_ADDR);
      }
  }
  if (*op == '<'){
      if(isDouble){
        val = Builder.CreateFCmpOLT( (Value *)x->s_value, (Value *)y->s_value);
      }
      else{
        val = Builder.CreateICmpSLT( (Value *)x->s_value, (Value *)y->s_value);
      }
  }else if (*op == '>'){
      if(isDouble){
        val = Builder.CreateFCmpOGT( (Value *)x->s_value, (Value *)y->s_value);
      }
      else{
        val = Builder.CreateICmpSGT( (Value *)x->s_value, (Value *)y->s_value);
      }
  }else if (op[0] == '=' && op[1] == '='){
      if(isDouble){
        val = Builder.CreateFCmpOEQ( (Value *)x->s_value, (Value *)y->s_value);
      }
      else{
        val = Builder.CreateICmpEQ( (Value *)x->s_value, (Value *)y->s_value);
      }
  
    }
  return (ccexpr (s_node ((void*)val, T_INT)));
  
    fprintf(stderr, "sem: rel not implemented\n");
    return ((struct sem_rec *) NULL);
  
}

/*
 * cast - cast value to a different type
 *
 * LLVM API calls:
 * IRBuilder::CreateSIToFP(Value *, Type *)
 * IRBuilder::CreateFPToSI(Value *, Type *)
 */
struct sem_rec*
cast (struct sem_rec *y, int t)
{
  if((y->s_type & ~T_ADDR) == t){
    return y;
  }
  if(t == T_DOUBLE ){
    return (s_node (Builder.CreateSIToFP((Value*) y->s_value, get_llvm_type(t)), t));
  }
  if(t == T_INT){
    return (s_node (Builder.CreateFPToSI((Value*) y->s_value, get_llvm_type(t)), t));
  }
  fprintf(stderr, "end cast\n");
  return y;
  //fprintf(stderr, "sem: cast not implemented\n");
  //return ((struct sem_rec *) NULL);
}

/*
 * set - assignment operators
 *
 * Grammar:
 * expr -> lval SET expr         { $$ = set((char*) "",   $1, $3); }
 * expr -> lval SETOR expr       { $$ = set((char*) "|",  $1, $3); }
 * expr -> lval SETXOR expr      { $$ = set((char*) "^",  $1, $3); }
 * expr -> lval SETAND expr      { $$ = set((char*) "&",  $1, $3); }
 * expr -> lval SETLSH expr      { $$ = set((char*) "<<", $1, $3); }
 * expr -> lval SETRSH expr      { $$ = set((char*) ">>", $1, $3); }
 * expr -> lval SETADD expr      { $$ = set((char*) "+",  $1, $3); }
 * expr -> lval SETSUB expr      { $$ = set((char*) "-",  $1, $3); }
 * expr -> lval SETMUL expr      { $$ = set((char*) "*",  $1, $3); }
 * expr -> lval SETDIV expr      { $$ = set((char*) "/",  $1, $3); }
 * expr -> lval SETMOD expr      { $$ = set((char*) "%",  $1, $3); }
 *
 * Much of the functionality in this method is abstracted to a separate method
 * used by op2, opb, and set. The comment above op2 lists the LLVM API calls
 * for this method.
 *
 * Additional LLVM API calls:
 * IRBuilder::CreateLoad(Type, Value *)
 * IRBuilder::CreateStore(Value *, Value *)
 */
struct sem_rec*
set(const char *op, struct sem_rec *x, struct sem_rec *y)
{
  
  if((op != NULL) && (op[0] == '\0')){
    struct sem_rec *tmpy = s_node (y->s_value, y->s_type);
    struct sem_rec *tmpy2 = cast(tmpy, x->s_type & ~T_ADDR);
    
    return s_node(Builder.CreateStore((Value*)tmpy2->s_value, (Value*)x->s_value ), x->s_type & ~(T_ADDR));
  } 
    struct sem_rec *tmp, *tmpret, *tmpret2;
    tmp = s_node(Builder.CreateLoad(get_llvm_type(x->s_type & ~T_ADDR), (Value*)x->s_value), x->s_type & ~(T_ADDR));
    tmpret = op2(op, tmp, y);
    tmpret2 = cast(tmpret, x->s_type & ~(T_ADDR));
    Builder.CreateStore((Value*)tmpret2->s_value, (Value*)x->s_value);
    return tmpret2;
  

  return x;
}

/*
 * genstring - generate code for a string
 *
 * Grammar:
 * expr ->  STR                   { $$ = genstring($1); }
 *
 * Use parse_escape_chars (in semutil.c) to handle escape characters
 *
 * LLVM API calls:
 * IRBuilder::CreateGlobalStringPtr(char *)
 */
struct sem_rec*
genstring(char *s)
{
  struct sem_rec *stringthing;
  char *tmpstr;
  tmpstr = parse_escape_chars(s); 
  stringthing = s_node (Builder.CreateGlobalStringPtr(tmpstr), T_STR);
  return stringthing;
  fprintf(stderr, "sem: genstring not implemented\n");
  return (struct sem_rec *) NULL;
}

void
declare_print ()
{
  struct id_entry *entry;
  FunctionType *var_arg;
  Value *F;
  std::string fname = "print";

  /* Add print to our internal data structure */
  var_arg = FunctionType::get(IntegerType::getInt32Ty(TheContext),
                              PointerType::get(Type::getInt8Ty(TheContext), 0), true);
  F = TheModule->getOrInsertFunction(fname, var_arg).getCallee();

  entry = install( slookup(fname.c_str()), 0 );
  entry->i_type = T_INT | T_PROC;
  entry->i_value = (void*) F;
}

void
init_IR ()
{
  TheModule = make_unique<Module>("<stdin>", TheContext);
  declare_print();
}


void
emit_IR ()
{
  TheModule->print(outs(), nullptr);
}
