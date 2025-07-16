/*
  OMPi OpenMP Compiler
  == Copyright since 2001 the OMPi Team
  == Dept. of Computer Science & Engineering, University of Ioannina

  This file is part of OMPi.

  OMPi is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  OMPi is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OMPi; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

/* dfa.c */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "ompi.h"
#include "symtab.h"
#include "tac.h"
#include "parser.h"
#include "ast_traverse.h"
#include "dfa.h"
#include "ast_vars.h"
#include "x_clauses.h"
#include "x_parallel.h"
#include "x_teams.h"
#include "x_task.h"
#include "x_target.h"
#include "cfg.h"
#include "ast_show.h"
#include "ast_print.h"


ast2cfg_opt_t opts = {   // options for cfg transform
  true,  // opts.expandomp
	false,  // opts.keepjoiners
	false,  // opts.keepjmpstmt
	false,   // opts.basicblocks
	false,  // opts.keepparedge
};


void vars_set_show(set(vars) s){  // print the vars set
  setelem(vars) e;
  e = s->first;
  for (e = s->first; e; e = e->next){
    fprintf(stderr, " %s\n", e->key->name);
  }
}

void accvar_set_show(set(accvar) s){  // print the accvar set
  setelem(accvar) e;
  for (e = s->first; e; e = e->next){
    fprintf(stderr, "%s\n", e->key->name);
  }
}

void find_no_clause_vars(aststmt tree,void * used_vars){ // function to find implicitly determined variables
  setelem(vars) e;
  dfa_base_t* base = (dfa_base_t*)used_vars;

  base->usedvars = analyze_used_vars(tree);

  for (e = base->usedvars[DCT_UNSPECIFIED]->first; e; e = e->next){
    switch (tree->u.omp->type) 
    {
      case DCPARALLEL:
        set_put(base->usedvars[xp_implicitDefault(e)], e->key)->value = e->value;
        break;
      case DCTASK:
        set_put(base->usedvars[xt_implicitDefault(e)], e->key)->value = e->value;
        break;
      case DCTARGET:
        set_put(base->usedvars[xtarget_implicitDefault(e)], e->key)->value = e->value;
        break;
      case DCTEAMS:
        set_put(base->usedvars[xtm_implicitDefault(e)], e->key)->value = e->value;
        break;    
      default:
        break;
    }
  }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                   *
 *    DFA OPERATIONS                                                 *
 *                                                                   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void find_read_private_vars(void* user_data){
  dfa_base_t* dfa_vars = (dfa_base_t*)user_data;
  stentry elem;
  int thislevel = stab->scopelevel;
  setelem(vars) e;
  setelem(accvar) e1;

  dfa_vars->acclist_vars_set = acclist2vars(&(dfa_vars->l)); 

  for (e = dfa_vars->clause_vars_set->first; e; e = e->next){
    for(e1 = dfa_vars->acclist_vars_set->first; e1; e1 = e1->next){
      if(e1->key == e->key){  
        //fprintf(stderr,"e1->key->name: %s\n",e1->key->name);
        elem = symtab_get(stab,e1->key,IDNAME); 
        if(!elem || elem->scopelevel < thislevel){
          if(e1->value == xR || e1->value == xRW){  // check for xR or xRW
            set_put_unique(dfa_vars->final_set,e1->key);
          }  
        }        
      }  
    } 
  } 
}

void find_written_firstprivate_vars(void* user_data){
  dfa_base_t* dfa_vars = (dfa_base_t*)user_data;
  stentry elem;
  int thislevel = stab->scopelevel;
  setelem(vars) e;
  setelem(accvar) e1;

  dfa_vars->acclist_vars_set = acclist2vars(&(dfa_vars->l)); 

  for (e = dfa_vars->clause_vars_set->first; e; e = e->next){
    for(e1 = dfa_vars->acclist_vars_set->first; e1; e1 = e1->next){
      if(e1->key == e->key){
        elem = symtab_get(stab,e1->key,IDNAME); 
        if(!elem || elem->scopelevel < thislevel){
          if(e1->value == xW || e1->value == xWR){  // check for Wr or WR
            set_put_unique(dfa_vars->final_set,e1->key);
          }  
        }        
      } 
    } 
  } 
}

void find_used_vars(void* user_data){
  dfa_base_t* dfa_vars = (dfa_base_t*)user_data;
  int i=0;
  while(i < dfa_vars->l.nelems){
    set_put_unique(dfa_vars->final_set,dfa_vars->l.list[i].var);
    i++;
  }  
}

void find_read_only_vars(void* user_data){
  
  dfa_base_t* dfa_vars = (dfa_base_t*)user_data;
  int i=0;

  while(i < dfa_vars->l.nelems){
    if(dfa_vars->l.list[i].way == ACC_READ){
      set_put_unique(dfa_vars->read_set,dfa_vars->l.list[i].var);
    }
    else if(dfa_vars->l.list[i].way == ACC_WRITE){
      set_put_unique(dfa_vars->write_set,dfa_vars->l.list[i].var);
    }
    i++;
  }
  dfa_vars->final_set = set_subtract(dfa_vars->read_set, dfa_vars->write_set);
  dfa_vars->l.nelems = 0;
}

void find_write_only_vars(void* user_data){
  dfa_base_t* dfa_vars = (dfa_base_t*)user_data;
  int i=0;
  while(i < dfa_vars->l.nelems){
    if(dfa_vars->l.list[i].way == ACC_READ){
      set_put_unique(dfa_vars->read_set,dfa_vars->l.list[i].var);
    }
    else if(dfa_vars->l.list[i].way == ACC_WRITE){
      set_put_unique(dfa_vars->write_set,dfa_vars->l.list[i].var);
    }
    i++;
  }
  dfa_vars->final_set = set_subtract(dfa_vars->write_set,dfa_vars->read_set);
  dfa_vars->l.nelems = 0;
}

void find_wr_vars(void* user_data){
  dfa_base_t* dfa_vars = (dfa_base_t*)user_data;
  int i=0;
  while(i < dfa_vars->l.nelems){
    if(dfa_vars->l.list[i].way == ACC_WRITE){
      set_put_unique(dfa_vars->write_set,dfa_vars->l.list[i].var);
    }
    else if(dfa_vars->l.list[i].way == ACC_READ){
      if (set_get(dfa_vars->write_set, dfa_vars->l.list[i].var) && !set_get(dfa_vars->read_set, dfa_vars->l.list[i].var)) {
        set_put_unique(dfa_vars->final_set,dfa_vars->l.list[i].var);          
      } 
      else {
        set_put_unique(dfa_vars->read_set,dfa_vars->l.list[i].var);
      }
    }
    i++;
  }
  dfa_vars->l.nelems = 0;
}

void find_rw_vars(void* user_data){
  dfa_base_t* dfa_vars = (dfa_base_t*)user_data;
  int i=0;
  while(i < dfa_vars->l.nelems){
    if(dfa_vars->l.list[i].way == ACC_READ){
      set_put_unique(dfa_vars->read_set,dfa_vars->l.list[i].var);
    }
    else if(dfa_vars->l.list[i].way == ACC_WRITE){
      if (set_get(dfa_vars->read_set, dfa_vars->l.list[i].var) && !set_get(dfa_vars->write_set, dfa_vars->l.list[i].var)) {
        set_put_unique(dfa_vars->final_set,dfa_vars->l.list[i].var);          
      } 
      else {
        set_put_unique(dfa_vars->write_set,dfa_vars->l.list[i].var);
      }
    }
    i++;
  }
  dfa_vars->l.nelems = 0;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                   *
 *    CALLBACKS FOR OMPCLAUSE NODES                                *
 *                                                                   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void compound_c(aststmt stmt, void *user_data, int vistime){ // handle scope the right way
  if ((vistime & PREVISIT) != 0){
	  scope_start(stab);
  }
	if ((vistime & POSTVISIT) != 0){
		scope_end(stab);
  }
}


void vars_ompclvars_c(ompclause t, void *user_data, int vistime){ // decl_flag seperates newly defined variables from those already present within a clause.
  dfa_base_t* base = (dfa_base_t*)user_data;
  if ((vistime & PREVISIT) != 0){
    base->decl_flag = 1;
  }
   
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                   *
 *    CALLBACKS FOR DECLARATION NODES                                *
 *                                                                   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void dlist_c(astdecl t, void *user_data, int vistime){
  // fprintf(stderr,"11111111111 %s\n",t->decl->u.id->name);
  // fprintf(stderr,"2222222 %d \n",t->decl->spec->type);
  // if(t->u.id){
  //   fprintf(stderr,"222222222 %s\n",t->u.id->name);
  // }
}

void dinit_c(astdecl t, void *user_data, int vistime){
  dfa_base_t* base = (dfa_base_t*)user_data;
  if ((vistime & PREVISIT) != 0){
    expr2acclist(t->u.expr,&(base->l));
  
    if(base->l.nelems > 0){ // need to check for the initializer part
      base->dinit_flag = 1;
    }
  }
  
}

void dparam_c(astdecl t, void *user_data, int vistime){
  dfa_base_t* base = (dfa_base_t*)user_data;
  if ((vistime & PREVISIT) != 0){
    if(t->decl){
      if(t->decl->type == DECLARATOR){ // make sure dparam refers to an argument
        base->param_flag = 1;
      } 
  }
  }
  
}


void dident_c(astdecl t, void *user_data, int vistime){
  dfa_base_t* base = (dfa_base_t*)user_data;
  if ((vistime & PREVISIT) != 0){
    if(base->param_flag == 0 ){ // check to avoid arguments
      if (t->u.id){
        if(base->decl_flag == 0){ // take only the new variables inside the construct
          symtab_put(stab,t->u.id,IDNAME);
          set_put(base->decl_vars,t->u.id);
          if(base->dinit_flag == 1){
            base->func_ptr(user_data);
            base->dinit_flag = 0;
          }
        }
      }
    }  
    base->decl_flag=0;
    base->param_flag = 0; 
  }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                   *
 *    CALLBACKS FOR EVERY OPERATION IN A BLOCK                       *
 *                                                                   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void return_c(aststmt stmt, void *user_data, int vistime){
  dfa_base_t* base = (dfa_base_t*)user_data;
  if ((vistime & PREVISIT) != 0){
    if(stmt->u.expr){
      expr2acclist(stmt->u.expr,&(base->l));
      base->func_ptr(user_data);
    }
  }
  
}

void switch_c(aststmt stmt, void *user_data, int vistime){
  dfa_base_t* base = (dfa_base_t*)user_data;
  if ((vistime & PREVISIT) != 0){
    if(stmt->u.selection.cond){
      expr2acclist(stmt->u.selection.cond,&(base->l));
      base->func_ptr(user_data);
    }
  }
  
}

void do_c(aststmt stmt, void *user_data, int vistime){
  dfa_base_t* base = (dfa_base_t*)user_data;
  if ((vistime & POSTVISIT) != 0){
    if(stmt->u.iteration.cond){
      //fprintf(stderr,"111\n");
      expr2acclist(stmt->u.iteration.cond,&(base->l));
      base->func_ptr(user_data);
    }
  }
  
}

void for_c(aststmt stmt, void *user_data, int vistime){
  dfa_base_t* base = (dfa_base_t*)user_data;
  if ((vistime & PREVISIT) != 0){
    if(stmt->u.iteration.init->u.expr){
      expr2acclist(stmt->u.iteration.init->u.expr,&(base->l));
    }
    if(stmt->u.iteration.cond){
      expr2acclist(stmt->u.iteration.cond,&(base->l));
    }
    if(stmt->u.iteration.incr){
      expr2acclist(stmt->u.iteration.incr,&(base->l));
    }
    base->func_ptr(user_data);
  }
  
}

void while_c(aststmt stmt, void *user_data, int vistime){
  dfa_base_t* base = (dfa_base_t*)user_data;
  if ((vistime & PREVISIT) != 0){
    if(stmt->u.iteration.cond){
      expr2acclist(stmt->u.iteration.cond,&(base->l));      
      base->func_ptr(user_data);
    }
  }
  
}

void if_c(aststmt stmt, void *user_data, int vistime){
  dfa_base_t* base = (dfa_base_t*)user_data;
  if ((vistime & PREVISIT) != 0){
    if(stmt->u.selection.cond){
      expr2acclist(stmt->u.selection.cond,&(base->l));
      base->func_ptr(user_data);
    }
  }
  
}

void expression_c(aststmt stmt,void *user_data, int vistime){ 
  dfa_base_t* base = (dfa_base_t*)user_data;
  if ((vistime & PREVISIT) != 0){
    if(stmt->u.expr){
      //fprintf(stderr,"222 %s\n",stmt->u.expr->left->u.sym->name);
      expr2acclist(stmt->u.expr,&(base->l));
      base->func_ptr(user_data);
    }
  }
  
} 

void dfa(aststmt tree,void* dfa_vars)
{
	travopts_t asopts;
	
	travopts_init_noop(&asopts);
	
	asopts.stmtc.expression_c = expression_c;
  asopts.stmtc.if_c = if_c;
  asopts.stmtc.while_c = while_c;
  asopts.stmtc.for_c = for_c;
  asopts.stmtc.do_c = do_c;
  asopts.stmtc.switch_c = switch_c;
  asopts.stmtc.return_c = return_c;  
  asopts.stmtc.compound_c = compound_c; 
  asopts.when = PREPOSTVISIT;

  asopts.ompclausec.ompclvars_c = vars_ompclvars_c;

  
  asopts.declc.dlist_c = dlist_c;
  asopts.declc.dinit_c = dinit_c;
  asopts.declc.dparam_c = dparam_c;
  asopts.declc.dident_c = dident_c;
  
  asopts.starg = dfa_vars;
	ast_stmt_traverse(tree, &asopts);

}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                   *
 *    WRAPPER FUNCTIONS                                              *
 *                                                                   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

set(vars) dfa_find_used_vars(aststmt tree,int keep_only_previous_defined_vars) {
  //cfgnode n;
  
  dfa_base_t dfa_vars;
  dfa_vars.final_set = set_new(vars);
  dfa_vars.decl_vars = set_new(vars);
  dfa_vars.func_ptr = find_used_vars;
  dfa_vars.decl_flag = 0;
  dfa_vars.param_flag = 0;
  dfa_vars.l.nelems = 0;

  dfa(tree,&dfa_vars);

  if(keep_only_previous_defined_vars){
    dfa_vars.final_set = set_subtract(dfa_vars.final_set,dfa_vars.decl_vars);
  }

  //print the set
  if(enableDFAdebug){
    fprintf(stderr,"usedvars_set:  \n");
    vars_set_show(dfa_vars.final_set);
  }
  

  //n = cfg_from_ast(tree,opts);
  //dfa_find_cfg_used_vars(n,keep_only_previous_defined_vars);

  return dfa_vars.final_set;

}

set(vars) dfa_find_read_only_vars(aststmt tree,int keep_only_previous_defined_vars) {
  //cfgnode n;

  dfa_base_t dfa_vars;
  dfa_vars.final_set = set_new(vars);
  dfa_vars.read_set = set_new(vars);
  dfa_vars.write_set = set_new(vars);
  dfa_vars.decl_vars = set_new(vars); // gia na vgalw tis metavlhtes ektos t block gia to read-only
  dfa_vars.func_ptr = find_read_only_vars;
  dfa_vars.decl_flag = 0;
  dfa_vars.param_flag = 0;
  dfa_vars.l.nelems = 0;
  
  dfa(tree,&dfa_vars);
  
  if(keep_only_previous_defined_vars){
    dfa_vars.final_set = set_subtract(dfa_vars.final_set,dfa_vars.decl_vars);
  }

  //print the set
  if(enableDFAdebug){
    fprintf(stderr,"read_only_set:  \n");
    vars_set_show(dfa_vars.final_set);
  }
  

  // for cfg
  //n = cfg_from_ast(tree,opts);
  //dfa_find_cfg_read_only_vars(n,keep_only_previous_defined_vars);

  return dfa_vars.final_set;
}

set(vars) dfa_find_write_only_vars(aststmt tree,int keep_only_previous_defined_vars) {
  //cfgnode n;

  dfa_base_t dfa_vars;
  dfa_vars.decl_vars = set_new(vars);
  dfa_vars.final_set = set_new(vars);
  dfa_vars.read_set = set_new(vars);
  dfa_vars.write_set = set_new(vars);
  dfa_vars.func_ptr = find_write_only_vars;
  dfa_vars.decl_flag = 0;
  dfa_vars.param_flag = 0;
  dfa_vars.l.nelems = 0;

  dfa(tree,&dfa_vars);

  
  if(keep_only_previous_defined_vars){
    dfa_vars.final_set = set_subtract(dfa_vars.final_set,dfa_vars.decl_vars);
  }

  //print the set
  if(enableDFAdebug){
    fprintf(stderr,"write_only_set:  \n");
    vars_set_show(dfa_vars.final_set);
  }


  //n = cfg_from_ast(tree,opts);
  //dfa_find_cfg_write_only_vars(n,keep_only_previous_defined_vars);
  

  return dfa_vars.final_set;
}

set(vars) dfa_find_wr_vars(aststmt tree,int keep_only_previous_defined_vars) {
  //cfgnode n;

  dfa_base_t dfa_vars;
  dfa_vars.decl_vars = set_new(vars);
  dfa_vars.final_set = set_new(vars);
  dfa_vars.read_set = set_new(vars);
  dfa_vars.write_set = set_new(vars);
  dfa_vars.func_ptr = find_wr_vars;
  dfa_vars.decl_flag = 0;
  dfa_vars.param_flag = 0;
  dfa_vars.l.nelems = 0;

  dfa(tree,&dfa_vars);

  
  if(keep_only_previous_defined_vars){
    dfa_vars.final_set = set_subtract(dfa_vars.final_set,dfa_vars.decl_vars);
  }

  //print the set
  if(enableDFAdebug){
    fprintf(stderr,"wr_set:  \n");
    vars_set_show(dfa_vars.final_set);
  }


  //n = cfg_from_ast(tree,opts);
  //dfa_find_cfg_wr_vars(n,keep_only_previous_defined_vars);

  return dfa_vars.final_set;

}

set(vars) dfa_find_rw_vars(aststmt tree,int keep_only_previous_defined_vars) {
  //cfgnode n;

  dfa_base_t dfa_vars;
  dfa_vars.decl_vars = set_new(vars);
  dfa_vars.final_set = set_new(vars);
  dfa_vars.read_set = set_new(vars);
  dfa_vars.write_set = set_new(vars);
  dfa_vars.func_ptr = find_rw_vars;
  dfa_vars.decl_flag = 0;
  dfa_vars.param_flag = 0;
  dfa_vars.l.nelems = 0;

  dfa(tree,&dfa_vars);

  
  if(keep_only_previous_defined_vars){
    dfa_vars.final_set = set_subtract(dfa_vars.final_set,dfa_vars.decl_vars);
  }

  //print the set
  if(enableDFAdebug){
    fprintf(stderr,"rw_set:  \n");
    vars_set_show(dfa_vars.final_set);
  }


  //n = cfg_from_ast(tree,opts);
  //dfa_find_cfg_rw_vars(n,keep_only_previous_defined_vars);

  return dfa_vars.final_set;
}

set(vars) dfa_find_written_firstprivate_vars(aststmt tree) {
  //cfgnode n;

  dfa_base_t dfa_vars;
  dfa_vars.decl_vars = set_new(vars);
  dfa_vars.final_set = set_new(vars);
  dfa_vars.clause_vars_set = set_new(vars);
  dfa_vars.acclist_vars_set = set_new(accvar);
  dfa_vars.func_ptr = find_written_firstprivate_vars;
  dfa_vars.decl_flag = 0;
  dfa_vars.param_flag = 0;
  dfa_vars.dinit_flag = 0;
  dfa_vars.l.nelems = 0;

  find_no_clause_vars(tree,&dfa_vars);

  /*we need xc_ompcon_get_vars() for constructs with no implicitDefault function.*/
  if(tree->type == OMPSTMT){
    xc_ompcon_get_vars(tree->u.omp, OCFIRSTPRIVATE, OC_DontCare, dfa_vars.clause_vars_set);
  }

  dfa_vars.clause_vars_set = set_union(dfa_vars.clause_vars_set, dfa_vars.usedvars[DCT_BYVALUE]);

  dfa(tree,&dfa_vars);

  // print the set
  if(enableDFAdebug){
    fprintf(stderr,"writen_firstprivate_vars_set:  \n");
    vars_set_show(dfa_vars.final_set);
  }
  
  //n = cfg_from_ast(tree,opts);
  //dfa_find_cfg_written_firstprivate_vars(n);

  // stentry elem;
  // for (elem = stab->top; elem; elem = elem->stacknext){
  //   fprintf(stderr,"symbol is: %s\n",elem->key->name);
  //   fprintf(stderr,"elem->scopelevel:  %d\n",elem->scopelevel);
  // }
  return dfa_vars.final_set;
}

set(vars) dfa_find_read_private_vars(aststmt tree) {
  //cfgnode n;

  dfa_base_t dfa_vars;
  dfa_vars.decl_vars = set_new(vars);
  dfa_vars.final_set = set_new(vars);
  dfa_vars.clause_vars_set = set_new(vars);
  dfa_vars.acclist_vars_set = set_new(accvar);
  dfa_vars.func_ptr = find_read_private_vars;
  dfa_vars.decl_flag = 0;
  dfa_vars.param_flag = 0;
  dfa_vars.dinit_flag = 0;
  dfa_vars.l.nelems = 0;

  find_no_clause_vars(tree,&dfa_vars);

  /*we need xc_ompcon_get_vars() for constructs with no implicitDefault function.*/
  if(tree->type == OMPSTMT){
    xc_ompcon_get_vars(tree->u.omp, OCPRIVATE, OC_DontCare, dfa_vars.clause_vars_set);
  }
  


  dfa_vars.clause_vars_set = set_union(dfa_vars.clause_vars_set,dfa_vars.usedvars[DCT_PRIVATE]);

  // fprintf(stderr,"00000000000000  \n");
  // vars_set_show(dfa_vars.clause_vars_set);

  dfa(tree,&dfa_vars);

  //print the set
  if(enableDFAdebug){
    fprintf(stderr,"read_private_vars_set:  \n");
    vars_set_show(dfa_vars.final_set);
  }
  
  // fprintf(stderr,"1111111111111111111111111111111111  \n");
  // vars_set_show(dfa_vars.usedvars[DCT_PRIVATE]);
  
  
  //n = cfg_from_ast(tree,opts);
  //dfa_find_cfg_read_private_vars(n);

  return dfa_vars.final_set;

}




/////////////////////////  CFG  /////////////////////////////////////////////

int param_flag = 0;
int decl_flag = 0;
int dinit_flag = 0;
acclist_t l;
set(vars) decl_vars;
set(vars) final_set;
set(vars) read_set;
set(vars) write_set;
set(vars) read_private_vars_set;
set(vars) private_vars_set;
set(accvar) acclist_vars_set;
set(vars) *usedvars;
set(vars) writen_firstprivate_vars_set;
set(vars) firstprivate_set;
void (*func_ptr)(acclist_t *);


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                   *
 *    CFG FUNCTIONS                                                  *
 *                                                                   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void find_no_clause_vars_cfg(cfgnode n){
  setelem(vars) e;
  usedvars = analyze_used_vars(n->succs[0]->stmt);
  for (e = usedvars[DCT_UNSPECIFIED]->first; e; e = e->next){
    switch (n->succs[0]->stmt->u.omp->type) 
    {
      case DCPARALLEL:
        set_put(usedvars[xp_implicitDefault(e)], e->key)->value = e->value;
        break;
      case DCTASK:
        set_put(usedvars[xt_implicitDefault(e)], e->key)->value = e->value;
        break;
      case DCTARGET:
        set_put(usedvars[xtarget_implicitDefault(e)], e->key)->value = e->value;
        break;
      case DCTEAMS:
        set_put(usedvars[xtm_implicitDefault(e)], e->key)->value = e->value;
        break;    
      default:
        break;
    }
  }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                   *
 *    DFA OPERATIONS                                                 *
 *                                                                   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void find_cfg_used_vars(acclist_t *l){
  int i=0;
  while(i < l->nelems){
    set_put_unique(final_set,l->list[i].var);
    i++;
  }
  l->nelems = 0;
}

void find_cfg_read_only_vars(acclist_t *l){
  int i=0;
  while(i < l->nelems){
    if(l->list[i].way == ACC_READ){
      set_put_unique(read_set,l->list[i].var);
    }
    else if(l->list[i].way == ACC_WRITE){
      set_put_unique(write_set,l->list[i].var);
    }
    i++;
  }
  final_set = set_subtract(read_set, write_set);
  l->nelems = 0;
}

void find_cfg_write_only_vars(acclist_t *l){
  int i=0;
  while(i < l->nelems){
    if(l->list[i].way == ACC_READ){
      set_put_unique(read_set,l->list[i].var);
    }
    else if(l->list[i].way == ACC_WRITE){
      set_put_unique(write_set,l->list[i].var);
    }
    i++;
  }
  final_set = set_subtract(write_set,read_set);
  l->nelems = 0;
}

void find_cfg_wr_vars(acclist_t *l){
  int i=0;
  while(i < l->nelems){
    if(l->list[i].way == ACC_WRITE){
      set_put_unique(write_set,l->list[i].var);
    }
    else if(l->list[i].way == ACC_READ){
      if (set_get(write_set, l->list[i].var) && !set_get(read_set, l->list[i].var)) {
        set_put_unique(final_set,l->list[i].var);          
      } 
      else {
        set_put_unique(read_set,l->list[i].var);
      }
    }
    i++;
  }
  l->nelems = 0;
}

void find_cfg_rw_vars(acclist_t *l){
  int i=0;
  while(i < l->nelems){
    if(l->list[i].way == ACC_READ){
      set_put_unique(read_set,l->list[i].var);
    }
    else if(l->list[i].way == ACC_WRITE){
      if (set_get(read_set, l->list[i].var) && !set_get(write_set, l->list[i].var)) {
        set_put_unique(final_set,l->list[i].var);          
      } 
      else {
        set_put_unique(write_set,l->list[i].var);
      }
    }
    i++;
  }
  l->nelems = 0;
}

void find_cfg_read_private_vars(acclist_t *l){
  stentry elem;
  int thislevel = stab->scopelevel;
  setelem(vars) e;
  setelem(accvar) e1;
  

  acclist_vars_set = acclist2vars(l); 

  for (e = private_vars_set->first; e; e = e->next){
    for(e1 = acclist_vars_set->first; e1; e1 = e1->next){
      if(e1->key == e->key){  
        elem = symtab_get(stab,e1->key,IDNAME); 
        if(elem->scopelevel < thislevel){
          if(e1->value == xR || e1->value == xRW){  // check for xR or xRW
            set_put_unique(read_private_vars_set,e1->key);
          }  
        }        
      }  
    } 
  } 
}

void find_cfg_written_firstprivate_vars(acclist_t *l){
  stentry elem;
  int thislevel = stab->scopelevel;
  setelem(vars) e;
  setelem(accvar) e1;

  acclist_vars_set = acclist2vars(l); 

  for (e = firstprivate_set->first; e; e = e->next){
    for(e1 = acclist_vars_set->first; e1; e1 = e1->next){
      if(e1->key == e->key){
        elem = symtab_get(stab,e1->key,IDNAME); 
        if(elem->scopelevel < thislevel){
          if(e1->value == xW || e1->value == xWR){  // check for Wr or WR
            set_put_unique(writen_firstprivate_vars_set,e1->key);
          }  
        }        
      }  
    } 
  }  
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                   *
 *    CALLBACKS FOR OMPCLAUSE NODES                                  *
 *                                                                   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static void vars_ompclvars(ompclause t, void *user_data, int vistime){
  decl_flag=1;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                   *
 *    CALLBACKS FOR DECLARATION NODES                                *
 *                                                                   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void dinit_cfg(astdecl t, void *user_data, int vistime){
  expr2acclist(t->u.expr,&l);

  if(l.nelems > 0){ // need to check for the initializer part
    dinit_flag = 1;
  }
}

void dparam_cfg(astdecl t, void *user_data, int vistime){
  if(t->decl){
    if(t->decl->type == DECLARATOR){ // make sure dparam refers to an argument
      param_flag = 1;
    } 
  }
}

void dident_cfg(astdecl t, void *user_data, int vistime){
  if(param_flag == 0 ){ // check to avoid arguments
    if (t->u.id){
      if(decl_flag == 0){ // take only the new variables inside the construct
        symtab_put(stab,t->u.id,IDNAME);
        set_put(decl_vars,t->u.id);
        if(dinit_flag == 1){
          func_ptr(&l);
          dinit_flag = 0;
        }
      }
    }
  }  
  param_flag = 0; 
  decl_flag=0;
}

void cfg_c(cfgnode n){
  aststmt stmt = n->stmt;
  if(n->type==CFG_PLAIN){
    if(n->astnodetype == EXPRNODE)
    {
      if(n->expr){
        expr2acclist(n->expr,&l);
        func_ptr(&l);
      } 
    } 
    if(n->astnodetype == STMTNODE){
      if(stmt){  
        if(stmt->type == DECLARATION){
          
          travopts_t asopts;
        
          travopts_init_noop(&asopts);

          asopts.declc.dinit_c = dinit_cfg;
          asopts.declc.dparam_c = dparam_cfg;
          asopts.declc.dident_c = dident_cfg;

          ast_stmt_traverse(stmt, &asopts);
        }
        if(stmt->type == OMPSTMT){
          travopts_t asopts;
        
          travopts_init_noop(&asopts);
          asopts.ompclausec.ompclvars_c = vars_ompclvars;
          asopts.declc.dident_c = dident_cfg;

          ast_stmt_traverse(stmt, &asopts);
        }
      }
    }  
  }
  else{
    return;
  }
   
} 

void dfa_cfg(cfgnode n, void (*ptr_arg)(acclist_t *))
{
  func_ptr = ptr_arg;
  cfg_traverse_fwd(n,cfg_c,vis_preord);
}

void dfa_find_cfg_used_vars(cfgnode n,int keep_only_previous_defined_vars) {
  
  final_set = set_new(vars);
  decl_vars = set_new(vars);
  l.nelems = 0;
  setelem(vars) e;

  dfa_cfg(n, find_cfg_used_vars);

  if(keep_only_previous_defined_vars){
    final_set = set_subtract(final_set, decl_vars);
  }

  for (e = decl_vars->first; e; e = e->next){
    symtab_remove(stab,e->key,IDNAME);
  }
  
  //print the set
  if(enableDFAdebug){
    fprintf(stderr,"cfg_used_vars_set:  \n");
    vars_set_show(final_set);
  }

}

void dfa_find_cfg_read_only_vars(cfgnode n,int keep_only_previous_defined_vars) {
  
  final_set = set_new(vars);
  read_set = set_new(vars);
  write_set = set_new(vars);
  decl_vars = set_new(vars);
  l.nelems = 0;
  setelem(vars) e;

  dfa_cfg(n, find_cfg_read_only_vars);

  if(keep_only_previous_defined_vars){
    final_set = set_subtract(final_set, decl_vars);
  }

  for (e = decl_vars->first; e; e = e->next){
    symtab_remove(stab,e->key,IDNAME);
  }
  
  //print the set
  if(enableDFAdebug){
    fprintf(stderr,"cfg_read_only_set:  \n");
    vars_set_show(final_set);
  }
  

}

void dfa_find_cfg_write_only_vars(cfgnode n,int keep_only_previous_defined_vars) {
  
  final_set = set_new(vars);
  read_set = set_new(vars);
  write_set = set_new(vars);
  decl_vars = set_new(vars);
  l.nelems = 0;
  setelem(vars) e;

  dfa_cfg(n, find_cfg_write_only_vars);

  if(keep_only_previous_defined_vars){
    final_set = set_subtract(final_set, decl_vars);
  }

  for (e = decl_vars->first; e; e = e->next){
    symtab_remove(stab,e->key,IDNAME);
  }

  //print the set
  if(enableDFAdebug){
    fprintf(stderr,"cfg_write_only_set:  \n");
    vars_set_show(final_set);
  }
  
}

void dfa_find_cfg_wr_vars(cfgnode n,int keep_only_previous_defined_vars) {

  final_set = set_new(vars);
  read_set = set_new(vars);
  write_set = set_new(vars);
  decl_vars = set_new(vars);
  l.nelems = 0;
  setelem(vars) e;

  dfa_cfg(n, find_cfg_wr_vars);

  if(keep_only_previous_defined_vars){
    final_set = set_subtract(final_set, decl_vars);
  }

  for (e = decl_vars->first; e; e = e->next){
    symtab_remove(stab,e->key,IDNAME);
  }

  //print the set
  if(enableDFAdebug){
    fprintf(stderr,"cfg_wr_set:  \n");
    vars_set_show(final_set);
  }
}

void dfa_find_cfg_rw_vars(cfgnode n, int keep_only_previous_defined_vars) {

  final_set = set_new(vars);
  read_set = set_new(vars);
  write_set = set_new(vars);
  decl_vars = set_new(vars);
  l.nelems = 0;
  setelem(vars) e;

  dfa_cfg(n, find_cfg_rw_vars);

  if(keep_only_previous_defined_vars){
    final_set = set_subtract(final_set, decl_vars);
  }

  for (e = decl_vars->first; e; e = e->next){
    symtab_remove(stab,e->key,IDNAME);
  }

  // print the set
  if(enableDFAdebug){
    fprintf(stderr,"cfg_rw_set:  \n");
    vars_set_show(final_set);
  }
  
}

void dfa_find_cfg_read_private_vars(cfgnode n){
  read_private_vars_set = set_new(vars);
  private_vars_set = set_new(vars);
  acclist_vars_set = set_new(accvar);
  l.nelems = 0;
  decl_vars = set_new(vars);
  setelem(vars) e;

  find_no_clause_vars_cfg(n);

  /*we need xc_ompcon_get_vars() for constructs with no implicitDefault function.*/
  
  if(n->succs[0]->stmt->type == OMPSTMT){
    xc_ompcon_get_vars(n->succs[0]->stmt->u.omp, OCPRIVATE, OC_DontCare, private_vars_set);
  }
  private_vars_set = set_union(private_vars_set, usedvars[DCT_PRIVATE]);

  scope_start(stab);
  dfa_cfg(n, find_cfg_read_private_vars);
  for (e = decl_vars->first; e; e = e->next){
    symtab_remove(stab,e->key,IDNAME);
  }
  scope_end(stab);

  //print the set
  if(enableDFAdebug){
    fprintf(stderr,"cfg_read_private_vars_set:  \n");
    vars_set_show(read_private_vars_set);
  }
  
}

void dfa_find_cfg_written_firstprivate_vars(cfgnode n) {
  writen_firstprivate_vars_set = set_new(vars);
  firstprivate_set = set_new(vars);
  acclist_vars_set = set_new(accvar);
  l.nelems = 0;
  setelem(vars) e;
  decl_vars = set_new(vars);
  
  find_no_clause_vars_cfg(n);

  
  /*we need xc_ompcon_get_vars() for constructs with no implicitDefault function.*/
  if(n->succs[0]->stmt->type == OMPSTMT){
    xc_ompcon_get_vars(n->succs[0]->stmt->u.omp, OCFIRSTPRIVATE, OC_DontCare, firstprivate_set);
  }
  
  firstprivate_set = set_union(firstprivate_set,usedvars[DCT_BYVALUE]);

  scope_start(stab);
  dfa_cfg(n,find_cfg_written_firstprivate_vars);
  for (e = decl_vars->first; e; e = e->next){
    symtab_remove(stab,e->key,IDNAME);
  }
  scope_end(stab);
  
  // print the set
  if(enableDFAdebug){
    fprintf(stderr,"cfg_writen_firstprivate_vars_set:  \n");
    vars_set_show(writen_firstprivate_vars_set);
  }
  
}

