// Copyright (C) 2002, International Business Machines
// Corporation and others.  All Rights Reserved.
#if defined(_MSC_VER)
// Turn off compiler warning about long names
#  pragma warning(disable:4786)
#endif
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cfloat>
#include <cassert>
#include <iostream>
#include "CoinHelperFunctions.hpp"
#include "CoinPackedVector.hpp"
#include "CoinPackedMatrix.hpp"
#include "CoinIndexedVector.hpp"
#include "OsiRowCutDebugger.hpp"
#include "CoinFactorization.hpp"
#include "CoinWarmStartBasis.hpp"
#include "CoinFinite.hpp"
#include "CoinPackedMatrix.hpp"
#include "CoinFactorization.hpp"
#include "OsiRowCutDebugger.hpp"
#include "CoinWarmStartBasis.hpp"
#include "CglTwomir.hpp"
class CoinWarmStartBasis;

#define DGG_DEBUG_DGG 1
#define CGL_DEBUG
#define  q_max  data->cparams.q_max
#define  q_min  data->cparams.q_min
#define  t_max  data->cparams.t_max
#define  t_min  data->cparams.t_min
#define  a_max  data->cparams.a_max

#define talk true


const OsiSolverInterface * six;

void write_cut( DGG_constraint_t *cut){ //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
       printf("2mir_test: cut: !!!!!!!!!!!!!!!!!!!!!!!***********************************\n");
      for (int i=0; i<cut->nz; i++)
	{ printf(" %12.10f x[%d] ", cut->coeff[i],cut->index[i]);}
      printf(" >= %12.10f \n", cut->rhs); 
}

void testus( DGG_constraint_t *cut){ //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  //  const OsiRowCutDebugger debugg(*six, "flugpl.mps"); 
  const OsiRowCutDebugger debugg(*six, "egout.mps"); 
  const OsiRowCutDebugger *debugger = &debugg;
    if (debugger&&!debugger->onOptimalPath(*six))
      return;
   
    OsiRowCut rowcut;

    rowcut.setRow(cut->nz, cut->index, cut->coeff);
    rowcut.setUb(DBL_MAX);
    rowcut.setLb(cut->rhs);
   
    if(debugger->invalidCut(rowcut)){
      write_cut(cut);
      //assert(0); 
    }
}


//-------------------------------------------------------------------
// Generate  cuts
//------------------------------------------------------------------- 
void CglTwomir::generateCuts(const OsiSolverInterface & si, OsiCuts & cs, 
			     const CglTreeInfo info ) const
{

  //!!!!!!!!!!!!!!!!!!
  six = &si;
  
  DGG_list_t cut_list;
  DGG_list_init (&cut_list);

  DGG_data_t* data = DGG_getData((const void *) &si);

  // Note that the lhs variables are hash defines to data->cparams.*
  q_max = q_max_;
  q_min = q_min_;
  t_max = t_max_;
  t_min = t_min_;
  a_max = a_max_;

  if (!do_mir_) t_max = t_min - 1;
  if (!do_2mir_) q_max = q_min - 1;

  if (do_tab_ && info.level < 1 && info.pass < 6)
      DGG_generateTabRowCuts( &cut_list, data, (const void *) &si );
  
  if (do_form_)
    DGG_generateFormulationCuts( &cut_list, data, (const void *) &si,
				 info.formulation_rows );
  
#ifdef CGL_DEBUG
  const OsiRowCutDebugger debugg(si, probname_);// = si.getRowCutDebugger();
  const OsiRowCutDebugger *debugger = &debugg;
  if (debugger&&!debugger->onOptimalPath(si))
    debugger = NULL;
  else
    {if(talk) printf ("2mir_test: debug success\n");}
#endif
  
  for (int i=0; i<cut_list.n; i++){
    DGG_constraint_t *cut = cut_list.c[i];
    OsiRowCut rowcut;
    
    rowcut.setRow(cut->nz, cut->index, cut->coeff);
    rowcut.setUb(DBL_MAX);
    rowcut.setLb(cut->rhs);
    cs.insert(rowcut);
    
#ifdef CGL_DEBUG
    if (debugger) {
      if (debugger->invalidCut(rowcut)) {
	write_cut(cut);
	printf ("2mir_test: debug failed, mayday, mayday **********************************\n");} 
	//assert(0);
      }
    //assert(!debugger->invalidCut(rowcut));
#endif
    
  }
  
  for (int i=0; i<cut_list.n; i++)
    DGG_freeConstraint (cut_list.c[i]);
  DGG_list_free (&cut_list);
  DGG_freeData (data);
}


//-------------------------------------------------------------------
// Default Constructor 
//-------------------------------------------------------------------
CglTwomir::CglTwomir () :
  CglCutGenerator(),
  do_mir_(true), do_2mir_(true), do_tab_(true), do_form_(true),
  t_min_(1), t_max_(1), q_min_(1), q_max_(1), a_max_(2),
  form_nrows_(0) {}

//-------------------------------------------------------------------
// Copy constructor 
//-------------------------------------------------------------------
CglTwomir::CglTwomir (const CglTwomir & source) :
  CglCutGenerator(source),
  t_min_(source.t_min_),
  t_max_(source.t_max_),
  q_min_(source.q_min_),
  q_max_(source.q_max_)
{  
}

//-------------------------------------------------------------------
// Clone
//-------------------------------------------------------------------
CglCutGenerator *
CglTwomir::clone() const
{
  return new CglTwomir(*this);
}

//-------------------------------------------------------------------
// Destructor 
//-------------------------------------------------------------------
CglTwomir::~CglTwomir ()
{
}

//----------------------------------------------------------------
// Assignment operator 
//-------------------------------------------------------------------
CglTwomir &
CglTwomir::operator=(const CglTwomir& rhs)
{
  if (this != &rhs) {
    CglCutGenerator::operator=(rhs);
    t_min_=rhs.t_min_;
    t_max_=rhs.t_max_;
    q_min_=rhs.q_min_;
    q_max_=rhs.q_max_;
  }
  return *this;
}

int DGG_freeData( DGG_data_t *data )
{
  free(data->info);
  free(data->lb);
  free(data->ub);
  free(data->x);
  free(data->rc);

  free(data);
  return 0;
}

DGG_data_t* DGG_getData(const void *osi_ptr )
{
  DGG_data_t *data = NULL;
  const OsiSolverInterface *si = (const OsiSolverInterface *) osi_ptr;

  data = (DGG_data_t*) malloc( sizeof(DGG_data_t) );

  /* retrieve basis information */
  CoinWarmStart *startbasis = si->getWarmStart();
  const CoinWarmStartBasis *basis = dynamic_cast<const CoinWarmStartBasis*>(startbasis);

  /* retrieve bounds information */
  const double *colUpper = si->getColUpper();
  const double *colLower = si->getColLower();
  const double *rowUpper = si->getRowUpper();
  const double *rowLower = si->getRowLower();
  const double *redCost  = si->getReducedCost();
  const double *dualVal  = si->getRowPrice();

  /* retrieve current optimal solution */
  const double *colSolut = si->getColSolution();

  /* retrieve the matrix in row format */
  const CoinPackedMatrix *rowMatrixPtr = si->getMatrixByRow();
  const int *rowBeg = 0, *rowCnt = 0, *rowInd = 0;
  const double *rowMat;
    
  rowBeg = rowMatrixPtr->getVectorStarts();
  rowCnt = rowMatrixPtr->getVectorLengths();
  rowMat = rowMatrixPtr->getElements();
  rowInd = rowMatrixPtr->getIndices();

  /* set number of columns and number of rows */
  data->ncol = si->getNumCols();
  data->nrow = si->getNumRows();

  /* set ninteger */
  data->ninteger = 0;

  /* allocate memory for the arrays in 'data' */
  data->info = (int*) malloc( sizeof(int)*(data->ncol+data->nrow) );
  data->lb = (double*) malloc( sizeof(double)*(data->ncol+data->nrow) );
  data->ub = (double*) malloc( sizeof(double)*(data->ncol+data->nrow) );
  data->x  = (double*) malloc( sizeof(double)*(data->ncol+data->nrow) );
  data->rc = (double*) malloc( sizeof(double)*(data->ncol+data->nrow) );

  memset(data->info, 0, sizeof(int)*(data->ncol+data->nrow));

  /* set parameters for column variables */
  data->nbasic_col = 0;

  for(int i=0; i < data->ncol; i++){
    /* is variable basic */
    if ( basis->getStructStatus(i) == CoinWarmStartBasis::basic ){
      data->nbasic_col++;
      DGG_setIsBasic(data,i);
    }

#if DGG_DEBUG_DGG
    {
      int error = 0;

      if ( basis->getStructStatus(i) != CoinWarmStartBasis::basic )
	if ( fabs(colSolut[i] - colUpper[i]) > DGG_BOUND_THRESH )
	  if ( fabs(colSolut[i] - colLower[i]) > DGG_BOUND_THRESH ){
	    fprintf(stdout, "WARNING!!!! : ");
	    fprintf(stdout, "variable %d non-basic, lb = %lf, ub = %lf, x = %lf\n",
		    i, colLower[i], colUpper[i], colSolut[i]);
	    error+=1;
	  }
	
      if (error)
	fprintf(stdout, "\nFOUND %d errors. BYE.\n", error);
    }
#endif

    /* set variable bounds*/
    data->lb[i] = colLower[i];
    data->ub[i] = colUpper[i];


    /* is variable integer */
    if ( si->isInteger(i) ){
      data->ninteger++;
      DGG_setIsInteger(data,i);
      /* tighten variable bounds*/
      data->lb[i] = ceil(colLower[i]);
      data->ub[i] = floor(colUpper[i]);
    }
 
    /* set x value */
    data->x[i] = colSolut[i];

    /* WARNING: remember to set rc!! Its not set!! */
    data->rc[i] = redCost[i];
  }

  /* set parameters for row variables */

  /* slack variables (row variables) work as follows:
     for a ranged constraint, b_dw < ax < b_up, define a variable s so that
     1) if b_up is not infinity:
        ax + s = b_up,   0 < s < b_up - b_dw
     2) if b_up is infinity:
        ax - s = b_dw,   0 < s < b_up - b_dw
  */
  {
    int i,j;
    double activity;

    data->nbasic_row = 0;

    for(i=0, j=data->ncol; i < data->nrow; i++, j++){

      /* check if the row is an equality constraint */ 
      if ( fabs( rowUpper[i] - rowLower[i] ) <= DGG_BOUND_THRESH )
        DGG_setEqualityConstraint(data,j);
      
      /* check if the row is bounded above/below and define variable bounds */
      if ( rowUpper[i] < DBL_MAX )
        DGG_setIsConstraintBoundedAbove(data,j);
      if ( rowLower[i] > -1*DBL_MAX )
        DGG_setIsConstraintBoundedBelow(data,j);

      data->lb[j] = 0.0;
      if (DGG_isConstraintBoundedAbove(data,j) && DGG_isConstraintBoundedBelow(data,j)) 
          data->ub[j] = rowUpper[i] - rowLower[i];
      else
          data->ub[j] = UB_MAX;

      /* compute row activity. for this we need to go to the row in question,
         and multiply all the coefficients times their respective variables.
         For the moment, we will store the inverse of this value in 
         the 'x' field (since it is in fact a partial computation of it)  */
      {
        int k;
 
        activity = 0.0;
	for(k=rowBeg[i]; k < rowBeg[i]+rowCnt[i]; k++)
          activity += rowMat[k]*colSolut[rowInd[k]];
      }
  
      /* compute x value */
      if ( DGG_isConstraintBoundedAbove(data,j) )
        data->x[j] = rowUpper[i] - activity;
      else
        data->x[j] = activity - rowLower[i];

      if ( data->x[j] < -DGG_NULL_SLACK ){
	int k;
	double norm = 0.0, min = DBL_MAX, amin = DBL_MAX, max = DBL_MIN;
        printf("** warning: row %d has negative slack!\n", i);
        for(k=rowBeg[i]; k < rowBeg[i]+rowCnt[i]; k++){
	  norm += rowMat[k]*rowMat[k];
	  if ( fabs(rowMat[k]) < amin ) amin = fabs(rowMat[k]);
	  if ( rowMat[k] < min ) min = rowMat[k];
	  if ( rowMat[k] > max ) max = rowMat[k];
	}
	norm = sqrt(norm);
	printf("min = %lf  amin = %lf  max = %lf\n", min, amin, max);
	printf("rlower = %lf activity = %lf\n", rowLower[i], activity);
	printf("norm = %lf   (b-ax) = %lf\n", norm, (rowLower[i] - activity));
	printf("steepn = %lf\n", (rowLower[i] - activity)/norm);
      }

      data->rc[j] = dualVal[i];
#if DGG_DEBUG_SOLVER
      DGG_IF_EXIT( !DGG_isConstraintBoundedAbove(data,j) && !DGG_isConstraintBoundedBelow(data,j),
                   1, "some row is not bounded above or below");
#endif

      /* is variable basic */
      if ( basis->getArtifStatus(i) == CoinWarmStartBasis::basic ){
        data->nbasic_row++;
        DGG_setIsBasic(data,j);
      }

      /* is variable integer. For this we need to go to the row in question,
         and check that the rhs is integer, and that all of the coefficients
         and variables participating in the constraint are also integer.    */
      {
        int k;
     
        if( DGG_isConstraintBoundedAbove(data,j)) {
          if ( frac_part(rowUpper[i]) > DGG_INTEGRALITY_THRESH )
            goto DONE_ROW; 
        }
        else
          if ( frac_part(rowLower[i]) > DGG_INTEGRALITY_THRESH )
            goto DONE_ROW;
 
        for(k=rowBeg[i]; k < rowBeg[i]+rowCnt[i]; k++)
          if ( frac_part(rowMat[k]) > DGG_INTEGRALITY_THRESH || !DGG_isInteger(data, rowInd[k]))
            goto DONE_ROW;
        
        DGG_setIsInteger(data, j); 
        data->ninteger++;
      }

    DONE_ROW:;
 
      /* set variable bounds: careful!! Later, remember to adjust 
         the INFINITY to a DGG standard (to deal with neq solvers). */
      /* WARNING: remember to set rc!! Its not set!! */
    }
  }

  /* CLEANUP */
  delete basis;
  return data;
}

DGG_constraint_t*
DGG_getSlackExpression(const void *osi_ptr, DGG_data_t* data, int row_index)
{
  DGG_constraint_t *row = 0;
  int i,j;

  /* retrieve the matrix in row format */
  const OsiSolverInterface *si = (const OsiSolverInterface *) osi_ptr;
  const CoinPackedMatrix *rowMatrixPtr = si->getMatrixByRow();
  const int *rowBeg = 0, *rowCnt = 0, *rowInd = 0;
  const double *rowMat;
  const double *rowUpper;
  const double *rowLower;
  const char *rowSense;
    
  row = DGG_newConstraint(data->ncol);

  rowBeg = rowMatrixPtr->getVectorStarts();
  rowCnt = rowMatrixPtr->getVectorLengths();
  rowMat = rowMatrixPtr->getElements();
  rowInd = rowMatrixPtr->getIndices();

  rowUpper = si->getRowUpper();
  rowLower = si->getRowLower();
  rowSense = si->getRowSense();

#if DGG_DEBUG_DGG
  if ( row_index < 0 || row_index > data->nrow )
    DGG_THROW(0, "bad row index"); 
#endif

  /* copy the information into the row ADT */
  row->nz = rowCnt[row_index];
  for(j=0, i=rowBeg[row_index]; i < rowBeg[row_index]+rowCnt[row_index]; i++, j++){
    row->coeff[j] = rowMat[i];
    row->index[j] = rowInd[i];
    if (DGG_isConstraintBoundedAbove (data, data->ncol + row_index))
      row->coeff[j] = -row->coeff[j];
  }
  
  row->sense = '?';
  if ( DGG_isConstraintBoundedAbove(data, data->ncol + row_index) )
    row->rhs = rowUpper[row_index];
  else 
    row->rhs = -rowLower[row_index];

  return row;
}

int
DGG_getTableauConstraint( int index,  const void *osi_ptr, DGG_data_t *data,
				  DGG_constraint_t* tabrow, int mode )
{
  int rval = 0;

#if DGG_DEBUG_DGG
  /* ensure that the index corresponds to a basic variable */
  if ( !DGG_isBasic(data, index) )
     DGG_THROW(1, "index is non-basic");

  /* ensure that the index corresponds to a column variable */
  if ( index < 0 || index > (data->ncol - 1) )
    DGG_THROW(1, "index not a column variable");
#endif

  /* obtain pointer to solver interface */
  const OsiSolverInterface *si = (const OsiSolverInterface *) osi_ptr;
  DGG_TEST(!si, 1, "null OsiSolverInterfave");

  /* obtain address of the LP matrix */
  const CoinPackedMatrix *colMatrixPtr = si->getMatrixByCol();
  const int* colBeg = colMatrixPtr->getVectorStarts();
  const int* colCnt = colMatrixPtr->getVectorLengths();
  const int* colInd = colMatrixPtr->getIndices();
  const double* colMat = colMatrixPtr->getElements();

  /* obtain row right-hand-sides */
  const double *rowUpper = si->getRowUpper();
  const double *rowLower = si->getRowLower();

  /* allocate memory for constraint in non-sparse form */
  int nz = 0;
  double *value = NULL, rhs = 0.0;
  value = (double*)malloc(sizeof(double)*(data->nrow+data->ncol));
  memset(value, 0, sizeof(double)*(data->nrow+data->ncol));

  /* allocate memory for basic column/row indicators */
  int *rowIsBasic = 0, *colIsBasic = 0;
  rowIsBasic = (int*)malloc(sizeof(int)*data->nrow);
  colIsBasic = (int*)malloc(sizeof(int)*data->ncol);
    
  /* initialize the IsBasic arrays with -1 / 1 values indicating 
     where the basic rows and columns are. NOTE: WE could do this 
     only once and keep it in osi_data at the expense of space!! */

  for(int i=0; i<data->ncol; i++){
    if ( DGG_isBasic(data,i) ) colIsBasic[i] = 1;
    else                       colIsBasic[i] = -1;
  }
  for(int i=0; i<data->nrow; i++){
    if ( DGG_isBasic(data,i+data->ncol) ) rowIsBasic[i] = 1;
    else                                  rowIsBasic[i] = -1;
  }

  /* obtain factorization */
  CoinFactorization factorization;
  factorization.increasingRows(2); /* ??? */
  rval = factorization.factorize(*colMatrixPtr, rowIsBasic, colIsBasic); 
  /* 0 = okay. -1 = singular. -2 = too many in basis. -99 = memory. */
  DGG_TEST(rval, 1, "factorization error = %d", rval);

  /* obtain the tableau row coefficients for all variables */
  /* note: we could speed this up by only computing non-basic variables */
  {
    int i, j, cnt = 0;
    double one = 1.0;
    CoinIndexedVector work;
    CoinIndexedVector array;

    work.reserve(data->nrow);
    array.reserve(data->nrow);

    array.setVector(1,&colIsBasic[index],&one);
 
    factorization.updateColumnTranspose ( &work, &array );

    int * arrayRows = array.getIndices();
    double *arrayElements = array.denseVector();
    cnt = array.getNumElements();

    /* compute column (structural) variable coefficients */
    for(j = 0; j < data->ncol; j++) {
      value[j] = 0.0;
      for(i=colBeg[j]; i < colBeg[j]+colCnt[j]; i++)
        value[j] += colMat[i]*arrayElements[ colInd[i] ];
    }

#if DGG_DEBUG_SOLVER
    /* check pivot */ 
    if ( fabs(value[index] - 1.0) > DGG_INTEGRALITY_THRESH )
       DGG_THROW(1, "pivot is not one"); 
#endif

    /* compute row variable (slack/logical) variable coefficients */

    for(j = 0; j < cnt; j++){
      if ( DGG_isEqualityConstraint(data,data->ncol + arrayRows[j]) && !mode )
        value[ data->ncol + arrayRows[j] ] = 0.0;
      else if ( DGG_isConstraintBoundedAbove(data, data->ncol + arrayRows[j]) )
        value[ data->ncol + arrayRows[j] ] = arrayElements[ arrayRows[j] ];
      else
        value[ data->ncol + arrayRows[j] ] = -1*arrayElements[ arrayRows[j] ];
    }

    /* compute rhs */
    rhs = 0.0;
    for(i=0; i < cnt; i++){
      if ( DGG_isConstraintBoundedAbove(data,data->ncol + arrayRows[i]) )
        rhs += arrayElements[arrayRows[i]]*rowUpper[arrayRows[i]];
      else
        rhs += arrayElements[arrayRows[i]]*rowLower[arrayRows[i]];
    }

   /* free 'work' and 'array' ?? do the empty functions do it?? they are not 
      cleared in CglGomory. Is that due to a mistake? Is it done on purpose? */ 
   /*
   work.empty();
   array.empty();
   */
  }

  /* count non-zeroes */
  nz = 0; 
  for(int j=0; j<data->ncol+data->nrow; j++){
    if ( fabs(value[j]) > DGG_MIN_TABLEAU_COEFFICIENT )
      nz += 1;
  }

  /* put in sparse constraint format */
  /* technical issue: should we let max_nz == nz or should we instead 
     set max_nz == (nrow+ncol). The advantage of the latter approach
     is that later, when we substitute the slacks, the denser column 
     will not require us to re-allocate memory */

  tabrow->max_nz = nz;

  if (tabrow->coeff)
    free(tabrow->coeff);
  if (tabrow->index)
    free(tabrow->index);
  
  tabrow->coeff = (double*) malloc(sizeof(double)*nz);
  tabrow->index = (int*) malloc(sizeof(int)*nz);
 
  tabrow->nz = 0;
  for(int j = 0; j < data->ncol + data->nrow; j++)
    if ( fabs(value[j]) > DGG_MIN_TABLEAU_COEFFICIENT ){
      tabrow->coeff[tabrow->nz] = value[j];
      tabrow->index[tabrow->nz] = j;
      tabrow->nz += 1;    
    }

  tabrow->sense = 'E';
  tabrow->rhs = rhs;

  /* CLEANUP */
  free(value);

  /* put down here to test */
  free(rowIsBasic);
  free(colIsBasic);

  return 0;
}

int
DGG_getFormulaConstraint( int da_row,  
                                  const void *osi_ptr,   
				  DGG_data_t *data, 
                                  DGG_constraint_t* form_row )
{
  /* ensure that the da_row corresponds to a row */
  if ( data->nrow <= da_row || 0> da_row)      DGG_THROW(1, "row out of range...");
  
  /* obtain pointer to solver interface */
  const OsiSolverInterface *si = (const OsiSolverInterface *) osi_ptr;
  //DGG_TEST(!si, 1, "null OsiSolverInterfave");
  
  /* obtain address of the LP matrix */
  const CoinPackedMatrix *rowMatrixPtr = si->getMatrixByRow();
  const int* rowBeg = rowMatrixPtr->getVectorStarts();
  const int* rowCnt = rowMatrixPtr->getVectorLengths();
  const int* rowInd = rowMatrixPtr->getIndices();
  const double* rowMat = rowMatrixPtr->getElements();

  /* obtain row right-hand-sides */
  const double *rowUpper = si->getRowUpper();
  const double *rowLower = si->getRowLower();
 
  int nz = rowCnt[da_row]; 

  form_row->nz = nz; 
  form_row->max_nz = nz+1;
 
  for(int i=0; i < nz; i++) form_row->coeff[i] = rowMat[rowBeg[da_row]+i];
  for(int i=0; i < nz; i++) form_row->index[i] = rowInd[rowBeg[da_row]+i];

  if ( DGG_isConstraintBoundedAbove(data,data->ncol + da_row) ){
    form_row->rhs = rowUpper[da_row];
    form_row->sense = 'L';
  }
  else{
    form_row->rhs = rowLower[da_row];
    form_row->sense = 'G';
  }
  if ( DGG_isEqualityConstraint(data,data->ncol + da_row)  )
    form_row->sense = 'E';

  /* now add slack/surplus if there is one */
  if ( DGG_isEqualityConstraint(data,data->ncol + da_row) == 0 ){
    form_row->index[nz] =  data->ncol + da_row; 
    if ( DGG_isConstraintBoundedAbove(data, data->ncol + da_row) )
      form_row->coeff[nz] = 1; 
    else
      form_row->coeff[nz] = -1; 
    form_row->nz +=1;
  }

  return 0;

}
//---------------------------------------------------------------
//---------------------------------------------------------------
//---------------------------------------------------------------
//---------------------------------------------------------------
//---------------------------------------------------------------

/******************** CONSTRAINT ADTs *****************************************/
DGG_constraint_t* DGG_newConstraint(int max_arrays)
{
   DGG_constraint_t *c = NULL;
   
   if (max_arrays <= 0) return NULL;
   c = (DGG_constraint_t*) malloc(sizeof(DGG_constraint_t));
   c->nz = 0;
   c->max_nz = max_arrays;
   c->rhs = 0.0;
   c->sense = '?';

   c->coeff = NULL;
   c->index = NULL;
   c->coeff = (double*)malloc(sizeof(double)*max_arrays);
   c->index = (int*)malloc(sizeof(int)*max_arrays);
   return c;
}

void DGG_freeConstraint(DGG_constraint_t *c)
{
  if (c == NULL) return;
  if (c->coeff) free(c->coeff);
  if (c->index) free(c->index);
  free(c);
}

DGG_constraint_t *DGG_copyConstraint(DGG_constraint_t* c)
{
  DGG_constraint_t *nc = NULL;

  if (!c || c->max_nz <= 0) return nc;
  nc = DGG_newConstraint(c->max_nz);
  if (nc == NULL) return nc;

  nc->nz = c->nz;
  nc->rhs = c->rhs;
  nc->sense = c->sense;

  memcpy(nc->coeff, c->coeff, sizeof(double)*nc->nz);
  memcpy(nc->index, c->index, sizeof(int)*nc->nz);

  return nc;
}

void DGG_scaleConstraint(DGG_constraint_t *c, int t)
{
  int i;

  c->rhs *= t;
  if (t < 0){
    if (c->sense == 'G') c->sense = 'L';
    else if (c->sense == 'L') c->sense = 'G';
  }
  for(i=0; i<c->nz; i++)  c->coeff[i] *= t;
}

void DGG_list_init (DGG_list_t *l)
{
  l->n = 0;
  l->c = NULL;
  l->ctype = NULL;
  l->alpha = NULL;
}

void DGG_list_free(DGG_list_t *l)
{
  if (l->c != NULL) free (l->c);
  if (l->ctype != NULL) free (l->ctype);
  if (l->alpha != NULL) free (l->alpha);
}

int DGG_list_addcut (DGG_list_t *l, DGG_constraint_t *cut, int ctype, double alpha)
{
  l->n ++;
  l->c = (DGG_constraint_t **)realloc (l->c, l->n * sizeof(DGG_constraint_t *));
  l->ctype = (int *)realloc (l->ctype, l->n * sizeof (int));
  l->alpha = (double *)realloc (l->alpha, l->n * sizeof (double));

  if (l->c == NULL || l->ctype == NULL || l->alpha == NULL){
    printf ("No memory, bailing out\n");
    return -1;
  }

  l->c[l->n - 1] = cut;
  l->ctype[l->n - 1] = ctype;
  l->alpha[l->n - 1] = alpha;
  return 0;
}

void DGG_list_delcut (DGG_list_t *l, int i)
{
  if (i >= l->n && i < 0) return;

  DGG_freeConstraint (l->c[i]);
  l->c[i] = l->c[l->n - 1];
  l->ctype[i] = l->ctype[l->n - 1];
  l->alpha[i] = l->alpha[l->n - 1];
  l->n --;
}

/******************* CONSTRAINT MANIPULATION **********************************/
/* VARIABLES CLOSE TO UPPER BOUNDS:
 we will substitute: x' = (u - x); hence the constraint will change from ax ~ b
 to -ax' ~ b - au note: the new bounds of x' will be, 0 <= x' <= u - l
 VARIABLES  CLOSE TO LOWER BOUNDS:
 we will substitute: x' = (x - l); hence, the constraint will change from
 ax ~ b to ax' ~ b - al. note: some variable lower bounds may have changed
 when doing the complement  in the previous stage - this must be taken into
 account. note: the new bounds  of x' will be, 0 <= x'  <= u - l */

int DGG_transformConstraint( DGG_data_t *data,
                             double **x_out,
			     double **rc_out,
                             char **isint_out,
                             DGG_constraint_t *constraint )
{
  double half;
  double *px = (double*) malloc( sizeof(double)*constraint->max_nz );
  double *rc = (double*) malloc( sizeof(double)*constraint->max_nz );
  char   *pi = (char*)   malloc( sizeof(char)  *constraint->max_nz );

  {
    int i, idx;

    for(i=0; i < constraint->nz; i++){
      idx = constraint->index[i];

      px[i] = data->x[idx];
      rc[i] = data->rc[idx];
      pi[i] = DGG_isInteger(data, idx); 
      half = (data->ub[idx] - data->lb[idx]) / 2;

      if ( data->ub[idx] - data->x[idx] < half ){
	px[i] = data->ub[idx] - data->x[idx];
	if (fabs(px[i]) <= DGG_BOUND_THRESH)
	  px[i] = 0.0;
	constraint->rhs -= constraint->coeff[i]*data->ub[idx];
	constraint->coeff[i] *= -1;
      }
      else {
	px[i] = data->x[idx] - data->lb[idx];
	if (fabs(px[i]) <= DGG_BOUND_THRESH)
	  px[i] = 0.0;
	constraint->rhs -= constraint->coeff[i]*data->lb[idx];
      }
    }
  }

  *x_out = px;
  *rc_out = rc;
  *isint_out = pi;

#if DGG_DEBUG_DGG
  DGG_TEST(DGG_isConstraintViolated(data, constraint), 1, "bad transformation");
#endif
 
  return 0;
}

int DGG_unTransformConstraint( DGG_data_t *data, 
                               DGG_constraint_t *constraint )
{
  int i, idx;
  double half;
  
  for(i=0; i < constraint->nz; i++){
    idx = constraint->index[i];
    half = (data->ub[idx] - data->lb[idx]) / 2;
    
    if ( data->ub[idx] - data->x[idx] < half ){
      constraint->rhs -= constraint->coeff[i]*data->ub[idx];	
      constraint->coeff[i] *= -1;
    }
    else 
      constraint->rhs += constraint->coeff[i]*data->lb[idx];
  }
  return 0;
}

int
DGG_substituteSlacks( const void *solver_ptr, 
                          DGG_data_t *data, 
                          DGG_constraint_t *cut )
{
  int i,j, lnz;
  double *lcut, lrhs;
  DGG_constraint_t *row=NULL;
 
  /* lcut will store all the column coefficients. allocate space and init. */
  lcut = (double*)malloc(sizeof(double)*data->ncol); 
  memset(lcut, 0, sizeof(double)*data->ncol);
 
  /* initialize lrhs */
  lrhs = cut->rhs;

  /* set coefficients in lcut */
  /* technical: we could speed this up by re-using allocated memory 
     for row->coeff and row->index                                  */
  for(i=0; i < cut->nz; i++){
    if ( cut->index[i] < data->ncol )
      lcut[ cut->index[i] ] += cut->coeff[i];
    else{
      row = DGG_getSlackExpression(solver_ptr, data, (cut->index[i] - data->ncol));
      
      for(j=0; j < row->nz; j++)
	lcut[ row->index[j] ] += row->coeff[j]*cut->coeff[i];
      lrhs -= row->rhs*cut->coeff[i];
      DGG_freeConstraint(row);
    }
  }

  /* count nz in new constraint */
  lnz = 0;
  for(i=0; i < data->ncol; i++)
    if ( fabs(lcut[i]) > DGG_MIN_TABLEAU_COEFFICIENT )
      lnz += 1;

  /* free row->coeff and row->index, and re-allocate */
  free(cut->coeff); cut->coeff = 0;
  free(cut->index); cut->index = 0;

  cut->nz = lnz;
  cut->max_nz = lnz;
  if (lnz)
    {
      cut->coeff = (double*) malloc( sizeof(double)*lnz );
      cut->index = (int*) malloc( sizeof(int)*lnz );
    }

  /* set new constraint */
  lnz = 0;
  for(i=0; i < data->ncol; i++){
    if ( fabs(lcut[i]) > DGG_MIN_TABLEAU_COEFFICIENT ){
      cut->coeff[lnz] = lcut[i];
      cut->index[lnz] = i;
      lnz += 1;
    }
  }
  cut->rhs = lrhs;

  free(lcut);
  return 0; 
}

int DGG_nicefyConstraint( const void *solver_ptr, 
                          DGG_data_t *data,
			  DGG_constraint_t *cut)
													
{
  
  double min_coef = DBL_MAX, max_coef = DBL_MIN;
  
  DGG_TEST(cut->sense == 'L', 1, "can't nicefy an L constraint");
  
  for(int i=0; i<cut->nz; i++) // first clean out noise
    if( fabs(cut->coeff[i]) < DGG_NICEFY_MIN_ABSVALUE)
      cut->coeff[i] = 0;

  for(int i=0; i<cut->nz; i++){
    
    if( DGG_isInteger(data, cut->index[i])){// look at integral vars.

      double aht = ABOV(cut->coeff[i]);
      double ub  = data->ub[ cut->index[i]];

      if(aht <  DGG_NICEFY_MIN_FIX){// coefficient = integer + epsylon
	
	cut->coeff[i] = floor( cut->coeff[i]);
	double ahtu = aht * ub;
	
	if(ahtu<DGG_NICEFY_MAX_PADDING)
	  cut->rhs -= ahtu;// safely remove the fractional part
       	else 
	  cut->coeff[i] += DGG_NICEFY_MIN_FIX; // inflate the fractional part
      }
      else 
	if (1-aht <  DGG_NICEFY_MIN_FIX) // coefficient = integer - epsylon
	  cut->coeff[i] = ceil( cut->coeff[i]);
      
    }// done with integers
    else // now look at continuous variables
      if ( cut->coeff[i] < DGG_NICEFY_MIN_ABSVALUE) // delete all negative and noise
	cut->coeff[i] = 0.0;
      else 
	if(cut->coeff[i] <  DGG_NICEFY_MIN_FIX) {// coefficient = epsylon
	  double au = cut->coeff[i] * data->ub[ cut->index[i]];
	
	  if(au<DGG_NICEFY_MAX_PADDING){ // safely remove the variable
	    cut->coeff[i] = 0.0;
	    cut->rhs -= au;
	  }
	  else 
	    cut->coeff[i] = DGG_NICEFY_MIN_FIX; // inflate the coefficient
	}// done with continuous variables too

    double abs_coef = fabs(cut->coeff[i]);
    min_coef = DGG_MIN(min_coef, abs_coef);
    max_coef = DGG_MAX(max_coef, abs_coef);
  }

  cut->sense = 'G';
  /*
  if ( max_coef > DGG_NICEFY_MAX_RATIO*min_coef ) // kill the cut if numbers are all over the place
    cut->nz = 0;
  */
  return 0;

}

/******************* CUT GENERATION *******************************************/
int
DGG_generateTabRowCuts( DGG_list_t *cut_list,
			    DGG_data_t *data,
			    const void *solver_ptr )

{
  int k, rval = 0;
  DGG_constraint_t *base = NULL;
  int nc = cut_list->n;

  base = DGG_newConstraint(data->ncol + data->nrow);

  if(talk) printf ("2mir_test: generating tab row cuts\n");
  for(k=0; k<data->ncol; k++){
    if (!(DGG_isBasic(data, k) && DGG_isInteger(data,k))) continue;

    double frac = frac_part (data->x[k]);
    if (frac < DGG_GOMORY_THRESH || frac > 1-DGG_GOMORY_THRESH) continue;

    base->nz = 0;
    rval = DGG_getTableauConstraint(k, solver_ptr, data, base, 0);
    DGG_CHECKRVAL(rval, rval);

    if (base->nz == 0){
      printf ("2mir_test: why does constraint not exist ?\n");
      continue;
    }

    if (base->nz > 500) continue;
    rval = DGG_generateCutsFromBase(base, cut_list, data, solver_ptr);
    DGG_CHECKRVAL(rval, rval);
  }

   if(talk) printf ("2mir_test: generated %d tab cuts\n", cut_list->n - nc); fflush (stdout);
  DGG_freeConstraint(base);
  return rval;
}

int DGG_generateFormulationCuts( DGG_list_t *cut_list,
				 DGG_data_t *data,
				 const void *solver_ptr,
				 int nrows)
{
  int k, rval = 0;
  DGG_constraint_t *base = NULL;
  int num_rows = (data->nrow < nrows) ? data->nrow : nrows;
  int nc = cut_list->n;

  base = DGG_newConstraint(data->ncol + data->nrow);

  if(talk) printf ("2mir_test: generating form row cuts %d\n", num_rows);
  for(k=0; k<num_rows; k++) {
    base->nz = 0;

    rval = DGG_getFormulaConstraint(k, solver_ptr, data, base);
    DGG_CHECKRVAL1(rval, rval);

    //printf ("generating formulation for row %d\n", k);
    rval = DGG_generateFormulationCutsFromBase(base, data->x[data->ncol+k],
					       cut_list, data, solver_ptr);
    DGG_CHECKRVAL1(rval, rval);
    if (base->nz == 0){
      printf ("why does constraint not exist ?\n");
      continue;
    }
  }

 CLEANUP:
  if(talk) printf ("2mir_test: generated %d form cuts\n", cut_list->n - nc); fflush (stdout);
  DGG_freeConstraint(base);
  return rval;
}


double random_01(long unsigned int *next){// uniform [0,1] 
  unsigned int x;  double dx,r = 0;
  
  while (r<1e-18){
    *next=(*next)*1103515245 + 12345;
    x=(*next/65536) % 32768;
    dx=x;    r=dx/32768;  }
  return (double) (r);
}	

int DGG_generateFormulationCutsFromBase( DGG_constraint_t *base,
					 double slack,
					 DGG_list_t *cut_list,
					 DGG_data_t *data,
					 const void *solver_ptr )
{
  int i, p, rval;
  int int_skala;
  double skala;
  int num_inlist = 0;
  int* skala_list = (int*) malloc( sizeof(int)*base->nz );
  char *isint = NULL;
  double *xout = NULL, *rcout = NULL;
  DGG_constraint_t *scaled_base = NULL;
  static long unsigned int rand_seed = 1983747;
  int tot_int = 0;
  double prob_choose = 0.0;
    
  rval = DGG_transformConstraint(data, &xout, &rcout, &isint, base);
  DGG_CHECKRVAL1(rval, rval);

  for(p = 0; p < base->nz; p++)  if(isint[p]) tot_int ++;
  if (tot_int == 0) goto CLEANUP;

  prob_choose = 5.0/tot_int;

  for(p = 0; p < base->nz; p++) {
    if(isint[p]) if(random_01(&rand_seed) < prob_choose){
      if(xout[p]<0.01) continue;

      skala =fabs(base->coeff[p]);
      if(skala<0.01)  continue;

      // check if slack is too large
      if (fabs(slack/skala) > 0.5) continue;

      scaled_base = DGG_copyConstraint(base);
      DGG_CHECKRVAL1((scaled_base == NULL),-1);

      if(base->sense == 'L') {
	skala = -skala; 
	scaled_base->sense = 'G';
      }
      
      int_skala = int(100*skala);

      for(i = 0; i< num_inlist; i++) 
	if(int_skala == skala_list[i]) 
	  goto END_LOOP;

      skala_list[num_inlist++] = int_skala;

      scaled_base->rhs = base->rhs/skala;
      for(i = 0; i<base->nz; i++) 
	scaled_base->coeff[i] = base->coeff[i] / skala;
 
      rval = DGG_unTransformConstraint(data, scaled_base);
      DGG_CHECKRVAL1(rval, rval);

      rval = DGG_generateCutsFromBase(scaled_base, cut_list,
				      data, solver_ptr);
      DGG_CHECKRVAL1(rval, rval);
      
    END_LOOP:
      DGG_freeConstraint(scaled_base);
      scaled_base = NULL;
    }      
  }

 CLEANUP:
  if (isint) free(isint);    
  if (xout)  free(xout);   
  if (rcout)  free(rcout);   
  if (skala_list) free(skala_list);
  if (scaled_base != NULL) DGG_freeConstraint (scaled_base);
  return rval;
}

int
DGG_generateCutsFromBase( DGG_constraint_t *orig_base,
			  DGG_list_t *cut_list,
			  DGG_data_t *data,
			  const void *solver_ptr )
{
  int rval = 0;
  int t;
  double *x = NULL, *rc = NULL;
  char *isint = NULL;
  DGG_constraint_t *base = NULL;
  bool   not_nicefied = true;
  int new_pos = cut_list->n;

  //  DGG_constraint_t *keep_origbase = DGG_copyConstraint(orig_base); //for debug only ------

  if (orig_base->sense == 'L') return 0;
  if (orig_base->nz == 0) return 0;

  rval = DGG_transformConstraint(data, &x, &rc, &isint, orig_base);
  double frac = frac_part(orig_base->rhs);
  //printf ("frac = %.7f, r %.7f, fr %.7f\n", frac, orig_base->rhs, floor(orig_base->rhs));
  if (rval || frac < DGG_RHS_THRESH || frac > 1-DGG_RHS_THRESH){
    free (x); free (rc); free (isint);
    return 0;
  }

  int min_t = t_min;
  int min_q = q_min;
  if (orig_base->sense == 'G' && min_t < 1) min_t = 1;
  if (orig_base->sense == 'G' && min_q < 1) min_q = 1;
  
  if (min_q > 0 &&  min_t > 0 ) {
    not_nicefied = false;
    rval = DGG_nicefyConstraint(solver_ptr, data, orig_base);
    DGG_CHECKRVAL(rval, rval);

    if (orig_base->nz == 0){
      if(talk) printf ("2mir_test: Nicefy returns empty constraint\n"); rval = 0; goto CLEANUP;
    }
  }

  for(t = min_t; t <= t_max ; t++){
    if (t == 0) continue;

    base = DGG_copyConstraint(orig_base);
    DGG_TEST(!base, 1, "error making copy of base");

    DGG_scaleConstraint (base, t);

    if(not_nicefied){
      rval = DGG_nicefyConstraint(solver_ptr, data, base);
      DGG_CHECKRVAL(rval, rval);
      if (base->nz == 0){
	 if(talk) printf ("2mir_test: Nicefy returns empty constraint\n"); goto MIR_DONE;
      }
    }
    
    if ( DGG_isBaseTrivial(data, base) ) goto MIR_DONE;

    rval = DGG_addMirToList(base, isint, x, cut_list, data, orig_base);
    DGG_CHECKRVAL(rval, rval);

  MIR_DONE:
    DGG_freeConstraint(base);
  }

  for( t = min_q; t <= q_max; t++ ){
    if (t == 0) continue;

    base = DGG_copyConstraint(orig_base);
    DGG_TEST(!base, 1, "error making copy of base");

    DGG_scaleConstraint (base, t);

    if(not_nicefied){
      rval = DGG_nicefyConstraint(solver_ptr, data, base);
      DGG_CHECKRVAL(rval, rval);
      if (base->nz == 0){
	 if(talk) printf ("2mir_test: Nicefy returns empty constraint\n"); goto TWOMIR_DONE;
      }
    }
    
    if ( DGG_isBaseTrivial(data, base) ) goto TWOMIR_DONE;

    rval = DGG_add2stepToList(base, isint, x, rc, cut_list, data, orig_base);
    DGG_CHECKRVAL(rval, rval);
    
  TWOMIR_DONE:
    DGG_freeConstraint(base);
  }

  for (int i = cut_list->n-1; i>=new_pos; i--){
    DGG_constraint_t *lcut = cut_list->c[i];

    rval = DGG_unTransformConstraint(data, lcut);
    DGG_CHECKRVAL(rval, rval);
    
    rval = DGG_substituteSlacks(solver_ptr, data, lcut);
    DGG_CHECKRVAL(rval, rval);

 

    if ( !DGG_isCutDesirable(lcut, data) ){
      DGG_list_delcut (cut_list, i);
      continue;
    }
    //else  testus(lcut);//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    /*
    if ( data->opt_x && DGG_cutsOffPoint(data->opt_x, lcut) ){
      //DGG_cutDisplay_sparse(data, data->opt_x, lcut, stdout);
      DGG_TEST(1,1, "new cut is infeasible for optimal solution\n");
    }
    */
  }

 CLEANUP:
  if (x) free(x);
  if (rc) free (rc);
  if (isint) free(isint);
  return 0;
}

int
DGG_addMirToList ( DGG_constraint_t *base, char *isint, double *x,
			      DGG_list_t *list, DGG_data_t *data,
			      DGG_constraint_t *orig_base )
{
  int rval = 0;
  DGG_constraint_t *cut = NULL;

  rval = DGG_buildMir(isint, base, &cut); 
  DGG_CHECKRVAL(rval, rval);

  DGG_list_addcut(list, cut, DGG_TMIR_CUT, 0.0);

  return 0;
}

int
DGG_add2stepToList ( DGG_constraint_t *base, char *isint, double *x,
				double *rc, DGG_list_t *list, DGG_data_t *data,
				DGG_constraint_t *orig_base )
{
  int rval;
  DGG_constraint_t *cut = NULL;
  int i;
  double norm_val, best_norm_val, best_norm_alpha=-1.0;
  double rc_val, best_rc_val,  best_rc_alpha=-1.0;
  double vht, bht, alpha;
  
  best_rc_val = best_norm_val = DBL_MAX;
  
  bht = ABOV(base->rhs);

  double best_rc = 0;
  for(i=0; i<base->nz; i++) if (isint[i]) best_rc = max(best_rc, fabs(rc[i]));
  double  rc_cutoff = best_rc / 10;

  for(i=0; i<base->nz; i++){
    if (!isint[i]) continue;
    if (fabs(rc[i]) <= rc_cutoff) continue; //too unimportant

    vht = ABOV(base->coeff[i]);
    if(vht >= bht)  continue;  // too big
    if(vht < bht/a_max) continue; // too small
    
    alpha = vht;
    int kk = 1;
    while ( !DGG_is2stepValid(alpha, bht) &&  bht/alpha <= a_max) {
      alpha = vht/kk; 
      kk++;
    }
    if ( !DGG_is2stepValid(alpha, bht) )    continue;
      
    rval = DGG_build2step(alpha, isint, base, &cut);
    DGG_CHECKRVAL(rval, rval);

    rc_val = DBL_MAX; // this gives a lower bound on obj. fn. improvement

    for(i=0; i<cut->nz; i++) if(cut->coeff[i]> 1E-6){
      rc_val = min(rc_val, fabs(rc[i])/cut->coeff[i]);
    }
    rc_val *= cut->rhs;

    norm_val = 0; // this is the square of the L2 norm
 
    for(i=0; i<cut->nz; i++) if(cut->coeff[i]> 1E-6){
      norm_val += (cut->coeff[i]*cut->coeff[i]);
    }

    norm_val /= cut->rhs * cut->rhs;
         
    if (rc_val < best_rc_val )  {	
      best_rc_val = rc_val; best_rc_alpha = alpha;  }

    if (norm_val < best_norm_val ) {	
      best_norm_val = norm_val;  best_norm_alpha = alpha;  }

    DGG_freeConstraint(cut);
  }
 
  if( best_rc_val> 1E-6 && best_rc_alpha != -1.0){
    rval = DGG_build2step(best_rc_alpha, isint, base, &cut);
    DGG_CHECKRVAL(rval, rval);
    DGG_list_addcut(list, cut, DGG_2STEP_CUT, best_rc_alpha);
  }
  else if (best_norm_alpha != -1.0){
    rval = DGG_build2step(best_norm_alpha, isint, base, &cut);
    DGG_CHECKRVAL(rval, rval);
    DGG_list_addcut(list, cut, DGG_2STEP_CUT, best_norm_alpha);
  }

  return 0;
}

int DGG_buildMir( char *isint,
		  DGG_constraint_t *base,
		  DGG_constraint_t **cut_out )
{
  int i, lnz = 0;
  double b   = (base->rhs);
  double bht = ABOV(b);
  double bup = ceil(b);
  DGG_constraint_t *tmir = NULL;

  DGG_TEST( base->sense == 'L', 1, "this form not valid for L");
  DGG_TEST( base->nz == 0, 1, "base must have some coefficients\n");

  tmir = DGG_newConstraint( base->nz );
  tmir->sense = 'G';
  tmir->rhs = bht * bup;

  for(i=0; i<base->nz; i++){
    double v   = base->coeff[i];

    if (!isint[i]) {
      if (v > 0.0) tmir->coeff[lnz] = v;
      else         tmir->coeff[lnz] = 0.0;
    }
    else {
      double vht = ABOV(v); 
      DGG_IF_EXIT( vht<0, 1, "negative vht");
      tmir->coeff[lnz]  = bht * floor(v) + DGG_MIN(bht,vht);
    }

    tmir->index[lnz] = base->index[i];
    lnz += 1;
  }

  tmir->nz = lnz;
  *cut_out = tmir;

  return 0;
}

int DGG_build2step( double alpha,
		    char *isint,
		    DGG_constraint_t *base,
		    DGG_constraint_t **cut_out )

{
  DGG_constraint_t *tmir = 0;

  int i,  lnz = 0;
  double vht, bht, bup, rho, tau, k;
  double b = (base->rhs);

  DGG_TEST( base->sense == 'L', 1, "this form not valid for L");
  DGG_TEST( base->nz == 0, 1, "base must have some coefficients\n");

  bht = ABOV(b);  
  bup = ceil(b); 
  tau = ceil(bht/alpha); 
  rho = bht - alpha*floor(bht/alpha);

  /* ensure bht > alpha > 0 */
  DGG_TEST( (bht <= alpha) || (alpha <= 0.0), 1, "bad alpha (%lf) / bht (%lf) pair", alpha, bht);
  /* ensure that we are not in a limiting case */
  DGG_TEST( DGG_is_a_multiple_of_b(alpha, bht), 1, "can't generate simple 2mir in limiting case");
  /* ensure that rho is not zero */
  DGG_TEST( rho < DGG_MIN_RHO, 1, "rho (%lf) too small", rho);

  /* initialize constraint */
  tmir = DGG_newConstraint( base->nz );

  tmir->rhs = bup*tau*rho;
  tmir->sense = 'G';

  /* compute cut coefficients */
  for(i=0; i<base->nz; i++){
    double v   = base->coeff[i];

    if (!isint[i]) {
      if (v > 0.0) tmir->coeff[lnz] = v;
      else         tmir->coeff[lnz] = 0.0;
    }
    else {
      vht = v - floor(v);
      DGG_IF_EXIT( vht < 0.0, 1, "negative vht");
      k   = DGG_MIN(tau-1,floor(vht/alpha));
      tmir->coeff[lnz]  = floor(v)*tau*rho +  k*rho + DGG_MIN(rho,vht-k*alpha);
    }

    tmir->index[lnz] = base->index[i];
    lnz += 1;
  }

  tmir->nz = lnz;
  *cut_out = tmir;

  return 0;
}

/******************* TEST / DEBUGGING ROUTINES ********************************/

/* DGG_is2stepValid:
   checks that:

   bht > alpha > 0
   (1/alpha) >= tau > (bht/alpha)
*/
int DGG_is2stepValid(double alpha, double bht)
{

  /* d */
  double tau;
  /* d-bar */
  double tau_lim;

  /* ensure that alpha is not null or negative */
  if ( alpha < DGG_MIN_ALPHA )
    return 0;

  /* compute tau and tau_lim */
  tau = ceil( bht / alpha );
  tau_lim = ceil( 1 / (1 - bht) ) - 1;

  /* make sure alpha is not a divisor of bht */
  if ( DGG_is_a_multiple_of_b(alpha, bht) )
    return 0;

  /* page 15, definition 12 */
  /* check if alpha is admissible for simple-2-step-tmir */

  if ( (bht > alpha) && (alpha > 0.0) )
    if ( (1/alpha) >= tau )
      return 1;

  /* not admissible */
  return 0;
}

/* checks that its worth doing a 1MIR on the constraint. More precisely,

- Is the RHS null? 
- Are there any integer variables set at fractional values?           */

int DGG_isBaseTrivial(DGG_data_t *d, DGG_constraint_t* c)
{

  /* is rhs sufficiently fractional */
  if ( frac_part(ABOV(c->rhs)) < DGG_GOMORY_THRESH )
    return 1;

  if ( (1.0 - frac_part(ABOV(c->rhs))) < DGG_GOMORY_THRESH )
    return 1;

  return 0;
}


/* tests lhs vs rhs of a constraint */
int DGG_isConstraintViolated(DGG_data_t *d, DGG_constraint_t *c)
{
  double lhs = DGG_cutLHS(c, d->x);
  double rhs = c->rhs;

  /* compare LHS and RHS */
  if (c->sense == 'G')
    if ( lhs > (rhs - DGG_NULL_SLACK) )
      return 0;
  if (c->sense == 'L')
    if ( lhs < (rhs + DGG_NULL_SLACK) )
      return 0;
  if (c->sense == 'E')
    if ( fabs(lhs - rhs) < DGG_NULL_SLACK )
      return 0;

  return 0;

}

double DGG_cutLHS(DGG_constraint_t *c, double *x)
{

  int i;
  double lhs = 0.0;

  for(i=0; i < c->nz; i++)
    lhs += c->coeff[i]*x[c->index[i]];

  return lhs;
}

int DGG_isCutDesirable(DGG_constraint_t *c, DGG_data_t *d)
{
  double lhs, rhs;

  lhs = DGG_cutLHS(c, d->x);
  rhs = c->rhs;

  if (c->nz > 500) return 0;

  /* if the cut is not violated, return 0 */
  if (c->sense == 'G')
    if ( lhs > (rhs - DGG_NULL_SLACK) )
      return 0;
  if (c->sense == 'L')
    if ( lhs < (rhs + DGG_NULL_SLACK) )
      return 0;
  if (c->sense == 'E')
    if ( fabs(lhs - rhs) < DGG_NULL_SLACK )
      return 0;

  return 1;
}

/******************** SIMPLE MACROS AND FUNCTIONS *****************************/

int DGG_is_even(double vht, double bht, int tau, int q)
{

  double v2 = V2I(bht, tau, q);

  if ( vht > v2 )
    return 1;

  return 0;
}

double frac_part(double value) 
{
  return value-floor(value);
}

int DGG_is_a_multiple_of_b(double a, double b)
{
  double c = b/a;
  
  if ( (b - a*floor(c)) < DGG_MIN_RHO )
    return 1;

  return 0;
}

int DGG_cutsOffPoint(double *x, DGG_constraint_t *cut)
{

  int i;
  double LHS = 0.0;

	for(i=0; i < cut->nz; i++)
	  LHS += cut->coeff[i]*(x[ cut->index[i] ]);

  //fprintf(stdout, "LHS = %lf, SENSE = %c, RHS = %lf\n", LHS, cut->sense, cut->rhs);
  if ( cut->sense == 'E' )
    if ( fabs(LHS - cut->rhs) > DGG_NULL_SLACK )
      goto BAD;
  if (cut->sense == 'G' )
    if ( (cut->rhs - LHS) > DGG_NULL_SLACK )
      goto BAD;
  if (cut->sense == 'L' )
    if ( (LHS - cut->rhs) > DGG_NULL_SLACK )
      goto BAD;

  return 0;

  BAD:

  fprintf(stdout, "LHS = %lf, SENSE = %c, RHS = %lf\n", LHS, cut->sense, cut->rhs);
  DGG_TEST(1, 1, "found a bad cut!");

}
