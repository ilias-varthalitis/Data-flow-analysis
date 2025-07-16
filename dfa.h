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

/* dfa.h */

#ifndef __DFA_H__
#define __DFA_H__

#include "ast.h"
#include "ast_vars.h"
#include "tac.h"
#include <set.h>
#include "cfg.h"

extern void vars_set_show(set(vars) s);


// for ast
extern set(vars) dfa_find_used_vars(aststmt tree,int keep_only_previous_defined_vars);
extern set(vars) dfa_find_read_only_vars(aststmt tree,int keep_only_previous_defined_vars);
extern set(vars) dfa_find_write_only_vars(aststmt tree,int keep_only_previous_defined_vars);
extern set(vars) dfa_find_wr_vars(aststmt tree,int keep_only_previous_defined_vars);
extern set(vars) dfa_find_rw_vars(aststmt tree,int keep_only_previous_defined_vars);

extern set(vars) dfa_find_written_firstprivate_vars(aststmt tree);
extern set(vars) dfa_find_read_private_vars(aststmt tree);

// for cfg
extern void dfa_find_cfg_used_vars(cfgnode n,int keep_only_previous_defined_vars);
extern void dfa_find_cfg_read_only_vars(cfgnode n,int keep_only_previous_defined_vars);	
extern void dfa_find_cfg_write_only_vars(cfgnode n,int keep_only_previous_defined_vars);	
extern void dfa_find_cfg_wr_vars(cfgnode n,int keep_only_previous_defined_vars);	
extern void dfa_find_cfg_rw_vars(cfgnode n,int keep_only_previous_defined_vars);	
extern void dfa_find_cfg_read_private_vars(cfgnode n);
extern void dfa_find_cfg_written_firstprivate_vars(cfgnode n);

typedef struct {  // holds the global shared variables among the different structs
    void (*func_ptr)(void*); 
	int param_flag;
	int decl_flag;
	int dinit_flag;
	acclist_t l;
	set(vars) decl_vars;   // holds the new declarations inside a block
	set(vars) *usedvars;  // used for find_no_clause vars()
	set(vars) final_set; // a set that holds the result for every dfa operation
	set(vars) read_set;  // for W/R
	set(vars) write_set; // for W/R
	set(vars) clause_vars_set; // a set that holds variables in clauses
	set(accvar) acclist_vars_set; // holds the used variables inside a block of code
} dfa_base_t;


#endif /* __DFA_H__ */