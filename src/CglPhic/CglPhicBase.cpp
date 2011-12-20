/*
  Copyright (C) 2011, Lou Hafer
  All Rights Reserved.

  This code is licensed under the terms of the Eclipse Public License (EPL).

  $Id$
*/
/*
  This file contains class boilerplate, setup, initialisation, and reporting
  methods.

  TODO: Arguably the content of CglPhicConInfo should be captured as part of
	the original row lhs information. This would allow restoration of
	row lhs information from the change record. Currently we need to
	recalculate because we've lost the norm and gaps (they are
	recalculated along with the lhs bounds at the recalculation interval,
	but are not saved in the change record).
*/

#if defined(_MSC_VER)
// Turn off compiler warning about long names
#  pragma warning(disable:4786)
#endif

#include "CglConfig.h"

#include "CoinFinite.hpp"
#include "CoinHelperFunctions.hpp"
#include "CoinPackedVector.hpp"

#include "CglPhic.hpp"

namespace {

/*! \brief Default zero tolerance

  This value is used for coefficients, bounds, variable values, etc.
*/
const double dfltZeroTol = 1.0e-11 ;

/* \brief Default feasibility tolerance

  This value is used when comparing constraint and variable bounds in
  feasibility tests.
*/
const double dfltFeasTol = 1.0e-7 ;

// Default value for infinity
const double dfltInfinity = COIN_DBL_MAX ;

/*! \brief Default column types to propagate

  This value is used to determine which variable types will be propagated.
*/
const int dfltColPropType = CglPhic::PropGenInt|CglPhic::PropBinary ;

/*! \brief Default column propagation tolerance

  This value is used to determine if the change to a column bound is worth
  propagating.
*/
const double dfltColPropTol = 1.0e-3 ;

/*! \brief Default row propagation tolerance

  This value is used to determine if the change to a lhs bound is worth
  propagating.
*/
const double dfltRowPropTol = 1.0e-3 ;

/*! \brief Default revision limit for constraint lhs bounds

  The value comes from previous experience in bonsaiG.
*/
const int dfltRevLimit = 10 ;

}    // end file-local namespace


/*
  Default constructor
*/
CglPhic::CglPhic ()
  : zeroTol_(dfltZeroTol),
    feasTol_(dfltFeasTol),
    propType_(dfltColPropType),
    colPropTol_(dfltColPropTol),
    rowPropTol_(dfltRowPropTol),
    infty_(dfltInfinity),
    revLimit_(dfltRevLimit),
    verbosity_(0),
    paranoia_(0),
    m_(0),
    n_(0),
    ourRowMtx_(false),
    rowMtx_(0),
    rm_rowStarts_(0),
    rm_rowLens_(0),
    rm_colIndices_(0),
    rm_coeffs_(0),
    ourColMtx_(0),
    colMtx_(0),
    cm_colStarts_(0),
    cm_colLens_(0),
    cm_rowIndices_(0),
    cm_coeffs_(0),
    rhsL_(0),
    rhsU_(0),
    ourColL_(false),
    colL_(0),
    ourColU_(false),
    colU_(0),
    intVar_(0),
    lhsL_(0),
    lhsU_(0),
    info_(0),
    candInfo_(0),
    heapCmpObj_(0),
    inProcess_(0),
    pending_(0),
    rebuildHeap_(false),
    szeLhsBndChgs_(0),
    numLhsBndChgs_(0),
    lhsBndChgs_(0),
    lhsHasChanged_(0),
    szeVarBndChgs_(0),
    numVarBndChgs_(0),
    varBndChgs_(0),
    varHasChanged_(0)
{ }

/*
  Constructor with constraint system.
*/
CglPhic::CglPhic (const CoinPackedMatrix *const rowMtx,
		  const CoinPackedMatrix *const colMtx,
		  const double *const rhsLower, const double *const rhsUpper)
  : zeroTol_(dfltZeroTol),
    feasTol_(dfltFeasTol),
    propType_(dfltColPropType),
    colPropTol_(dfltColPropTol),
    rowPropTol_(dfltRowPropTol),
    infty_(dfltInfinity),
    revLimit_(dfltRevLimit),
    verbosity_(0),
    paranoia_(0),
    m_(0),
    n_(0),
    ourRowMtx_(false),
    rowMtx_(0),
    rm_rowStarts_(0),
    rm_rowLens_(0),
    rm_colIndices_(0),
    rm_coeffs_(0),
    ourColMtx_(0),
    colMtx_(0),
    cm_colStarts_(0),
    cm_colLens_(0),
    cm_rowIndices_(0),
    cm_coeffs_(0),
    rhsL_(0),
    rhsU_(0),
    ourColL_(false),
    colL_(0),
    ourColU_(false),
    colU_(0),
    intVar_(0),
    lhsL_(0),
    lhsU_(0),
    info_(0),
    candInfo_(0),
    heapCmpObj_(0),
    inProcess_(0),
    rebuildHeap_(false),
    szeLhsBndChgs_(0),
    numLhsBndChgs_(0),
    lhsBndChgs_(0),
    lhsHasChanged_(0),
    szeVarBndChgs_(0),
    numVarBndChgs_(0),
    varBndChgs_(0),
    varHasChanged_(0)
{
  loanSystem(rowMtx,colMtx,rhsLower,rhsUpper) ;
}
  

/*
  Destructor
*/
CglPhic::~CglPhic ()
{
 if (ourRowMtx_) delete rowMtx_ ;
 if (ourColMtx_) delete colMtx_ ;
 delete[] lhsL_ ;
 delete[] lhsU_ ;
 delete[] info_ ;
 if (ourColL_) delete[] colL_ ;
 if (ourColU_) delete[] colU_ ;
 delete[] candInfo_ ;
 delete[] varBndChgs_ ;
 delete[] varHasChanged_ ;
 delete[] lhsBndChgs_ ;
 delete[] lhsHasChanged_ ;
}



/*
  Install constraint system on loan from the client.
*/
bool CglPhic::loanSystem (const CoinPackedMatrix *const rowMtx,
				 const CoinPackedMatrix *const colMtx,
				 const double *const rhsLower,
				 const double *const rhsUpper)
{
  assert(rhsLower != 0 && rhsUpper != 0) ;
  assert(rowMtx != 0 || colMtx != 0) ;
/*
  Install the lower and upper rhs row bounds.
*/
  rhsL_ = rhsLower ;
  rhsU_ = rhsUpper ;
/*
  Install row- and column-major copies of the constraint matrix.
*/
  if (rowMtx != 0 && colMtx != 0) {
    rowMtx_ = rowMtx ;
    ourRowMtx_ = false ;
    colMtx_ = colMtx ;
    ourColMtx_ = false ;
  } else if (colMtx != 0) {
    colMtx_ = colMtx ;
    ourColMtx_ = false ;
    CoinPackedMatrix *tmp = new CoinPackedMatrix() ;
    tmp->reverseOrderedCopyOf(*colMtx_) ;
    rowMtx_ = tmp ;
    ourRowMtx_ = true ;
  } else if (rowMtx != 0) {
    rowMtx_ = rowMtx ;
    ourRowMtx_ = false ;
    CoinPackedMatrix *tmp = new CoinPackedMatrix() ;
    tmp->reverseOrderedCopyOf(*colMtx_) ;
    colMtx_ = tmp ;
    ourColMtx_ = true ;
  }
/*
  If the lhs bound arrays no longer fit, remove them.
*/
  if (colMtx_->getNumRows() > m_) {
    delete[] lhsL_ ;
    lhsL_ = 0 ;
    delete[] lhsU_ ;
    lhsU_ = 0 ;
  }
  m_ = colMtx_->getNumRows() ;
  n_ = colMtx_->getNumCols() ;
/*
  Unpack the matrix structure vectors --- we'll use these a lot, no sense
  doing it every time.
*/
  rm_rowStarts_ = rowMtx_->getVectorStarts() ;
  rm_rowLens_ = rowMtx_->getVectorLengths() ;
  rm_colIndices_ = rowMtx_->getIndices() ;
  rm_coeffs_ = rowMtx_->getElements() ;
  cm_colStarts_ = colMtx_->getVectorStarts() ;
  cm_colLens_ = colMtx_->getVectorLengths() ;
  cm_rowIndices_ = colMtx_->getIndices() ;
  cm_coeffs_ = colMtx_->getElements() ;

  return (true) ;
}


/*
  Install column bounds on loan from client.
*/
void CglPhic::loanColBnds (double *const colLower,
				  double *const colUpper)
{
  assert(colLower && colUpper) ;
  colL_ = colLower ;
  ourColL_ = false ;
  colU_ = colUpper ;
  ourColU_ = false ;
}


/*
  Copy column bounds provided by client.
*/
void CglPhic::setColBnds (const double *const colLower,
			  const double *const colUpper)
{
  assert(colLower && colUpper) ;
  CoinMemcpyN(colLower,n_,colL_) ;
  ourColL_ = true ;
  CoinMemcpyN(colUpper,n_,colU_) ;
  ourColU_ = true ;
}

/*
  Return copies of the row lhs bounds arrays.

  Takes a little bit of work because the internal structure is a bit more
  complicated.
*/
void CglPhic::getRowLhsBnds (double *&lhsLower, double *&lhsUpper) const
{
  assert(lhsL_ && lhsU_) ;
  if (!lhsLower) lhsLower = new double [m_] ;
  for (int i = 0 ; i < m_ ; i++) {
    if (lhsL_[i].infCnt_ == 0)
      lhsLower[i] = lhsL_[i].bnd_ ;
    else
      lhsLower[i] = -infty_ ;
  }
  if (!lhsUpper) lhsUpper = new double [m_] ;
  for (int i = 0 ; i < m_ ; i++) {
    if (lhsU_[i].infCnt_ == 0)
      lhsUpper[i] = lhsU_[i].bnd_ ;
    else
      lhsUpper[i] = infty_ ;
  }
}

/*
  Calculate upper and lower lhs bounds for a given row, and the row measures
  (gaps and norm). The gaps are calculated over variables with both bounds
  finite. An infinite gap causes problems elsewhere, and there are explicit
  mechanisms for converting infinite bounds to finite bounds where possible.

  For a <= constraint, we will only ever use L(i) during propagation.
  Similarly, for a >= constraint, only U(i). The coefficient norm won't
  change, and the gaps have limited utility. Arguably this method should
  pay attention. The question is whether the additional tests to avoid
  calculation would disrupt the execution pipeline and slow down execution
  more than the additional work.  -- lh, 110326 --
*/
void CglPhic::calcLhsBnds (int i)
{
  assert(0 <= i && i < m_) ;
  assert(colL_ && colU_ && rowMtx_) ;
  assert(lhsL_ && lhsU_ && info_) ;

  double l1norm = 0.0 ;
  double posGap = 0.0 ;
  double negGap = 0.0 ;
  int infU = 0 ;
  int ndxU = -1 ;
  double bndU = 0.0 ;
  int infL = 0 ;
  int ndxL = -1 ;
  double bndL = 0.0 ;
/*
  Walk the row, accumulating the bound values.
*/
  const CoinBigIndex firstjj = rm_rowStarts_[i] ;
  const CoinBigIndex lastjj = firstjj+rm_rowLens_[i] ;
  for (CoinBigIndex jj = firstjj ; jj < lastjj ; jj++) {
    const int j = rm_colIndices_[jj] ;
    const double aij = rm_coeffs_[jj] ;
    const double lj = colL_[j] ;
    const double uj = colU_[j] ;
    if (aij > zeroTol_) {
      l1norm += aij ;
      if (uj >= infty_) {
        infU++ ;
	ndxU = j ;
      } else {
	bndU += aij*uj ;
	if (lj > -infty_) posGap = CoinMax(posGap,aij*(uj-lj)) ;
      }
      if (lj <= -infty_) {
        infL++ ;
	ndxL = j ;
      } else {
	bndL += aij*lj ;
	if (uj < infty_) posGap = CoinMax(posGap,aij*(uj-lj)) ;
      }
    } else if (aij < -zeroTol_) {
      l1norm -= aij ;
      if (uj >= infty_) {
        infL++ ;
	ndxL = j ;
      } else {
	bndL += aij*uj ;
	if (lj > -infty_) negGap = CoinMin(negGap,aij*(uj-lj)) ;
      }
      if (lj <= -infty_) {
        infU++ ;
	ndxU = j ;
      } else {
	bndU += aij*lj ;
	if (uj < infty_) negGap = CoinMin(negGap,aij*(uj-lj)) ;
      }
    }
  }
/*
  Clean up and fill in the bound and info entries.
*/
  CglPhicLhsBnd &Li = lhsL_[i] ;
  CglPhicLhsBnd &Ui = lhsU_[i] ;
  CglPhicConInfo &infoi = info_[i] ;

  Li.bnd_ = bndL ;
  if (infL == 1)
    Li.infCnt_ = -(ndxL+1) ;
  else
    Li.infCnt_ = infL ;
  Li.revs_ = 0 ;

  Ui.bnd_ = bndU ;
  if (infU == 1)
    Ui.infCnt_ = -(ndxU+1) ;
  else
    Ui.infCnt_ = infU ;
  Ui.revs_ = 0 ;

  infoi.l1norm_ = l1norm ;
  infoi.posGap_ = posGap ;
  infoi.negGap_ = negGap ;
/*
  And a bit of debug printing
*/
  if (verbosity_ >= 4) {
    std::cout
      << "        " << "init " << rhsL_[i] << " < " << Li << " <= r(" << i
      << ") <= " << Ui << " < " << rhsU_[i] << ", l1 " << l1norm << ", pGap "
      << posGap << ", nGap " << negGap << std::endl ;
  }
}

/*
  Calculate upper and lower row lhs bounds.
*/
void CglPhic::initLhsBnds ()
{
  if (verbosity_ >= 3)
    std::cout << "    " << "Initialising row info and lhs bounds ... " ;
  assert(colL_ && colU_ && rowMtx_) ;
/*
  Allocate fresh bound and info arrays if we need them.
*/
  if (!lhsL_) lhsL_ = new CglPhicLhsBnd [m_] ;
  if (!lhsU_) lhsU_ = new CglPhicLhsBnd [m_] ;
  if (!info_) info_ = new CglPhicConInfo [m_] ;
/*
  Step through the major vectors and calculate measures and bounds.
*/
  if (verbosity_ >= 5) std::cout << std::endl ;
  for (int i = 0 ; i < m_ ; i++) {
    calcLhsBnds(i) ;
  }
  if (verbosity_ >= 3) {
    if (verbosity_ >= 5) std::cout << "    " ;
    std::cout << "done." << std::endl ;
  }
}

/*
  Initialise the propagation data structures
*/
void CglPhic::initPropagation ()
{
  assert(colMtx_) ;
/*
  Allocate and initialise the data structures that record variable bound
  changes.
*/
  int newSize = CoinMin(n_/4+10,n_) ;
  if (newSize > szeVarBndChgs_) {
    szeVarBndChgs_ = newSize ;
    delete[] varBndChgs_ ;
    varBndChgs_ = new CglPhicVarBndChg [szeVarBndChgs_] ;
  }
  if (!varHasChanged_) varHasChanged_ = new int [n_] ;
  CoinZeroN(varHasChanged_,n_) ;
  numVarBndChgs_ = 0 ;
/*
  And again, for the data structures that record row lhs bound changes.
*/
  newSize = CoinMin(m_/4+10,m_) ;
  if (newSize > szeLhsBndChgs_) {
    szeLhsBndChgs_ = newSize ;
    delete[] lhsBndChgs_ ;
    lhsBndChgs_ = new CglPhicLhsBndChg [szeLhsBndChgs_] ;
  }
  if (!lhsHasChanged_) lhsHasChanged_ = new int [m_] ;
  CoinZeroN(lhsHasChanged_,m_) ;
  numLhsBndChgs_ = 0 ;
/*
  And again, for the data structures that control bound propagation: The vector
  of metrics and the heap of candidates for propagation.
*/
  int szePending = static_cast<int>(pending_.capacity()) ;
  if (newSize > szePending) pending_.reserve(newSize) ;
  inProcess_ = -1 ;
  rebuildHeap_ = false ;
  if (!candInfo_ ) candInfo_ = new CglPhicCand [m_] ;
  for (int i = 0 ; i < m_ ; i++) {
    candInfo_[i].isPending_ = false ;
  }
  heapCmpObj_.candVec_ = candInfo_ ;
}


/*
  Clear propagation data structures.
*/
void CglPhic::clearPropagation ()
{
  if (verbosity_ >= 2)
    std::cout << "    clearing pending set and change records." << std::endl ;

  pending_.clear() ;
  for (int i = 0 ; i < m_ ; i++) {
    candInfo_[i].isPending_ = false ;
  }

  numVarBndChgs_ = 0 ;
  CoinZeroN(varHasChanged_,n_) ;
  numLhsBndChgs_ = 0 ;
  CoinZeroN(lhsHasChanged_,m_) ;
}


/*
  Create a variable bound change record.

  First find the existing change record, or make one. Then (after the logging
  print) update the bounds arrays and change record.
*/
void CglPhic::recordVarBndChg (int j, char bnd, double nbndj)
{
  assert(0 <= j && j < n_) ;
  assert(bnd == 'l' || bnd == 'u') ;

  const bool deltal = (bnd == 'l') ;

  int chgNdx = varHasChanged_[j] ;
/*
  If there's no existing change record, create one, initialising it with the
  current bounds.
*/
  if (!chgNdx) {
    if (numVarBndChgs_ >= szeVarBndChgs_) {
      int newSze = szeVarBndChgs_+(szeVarBndChgs_/2)+10 ;
      newSze = CoinMax(newSze,n_) ;
      CglPhicVarBndChg *newVarBndChgs = new CglPhicVarBndChg [newSze] ;
      CoinMemcpyN(varBndChgs_,szeVarBndChgs_,newVarBndChgs) ;
      delete[] varBndChgs_ ;
      varBndChgs_ = newVarBndChgs ;
      szeVarBndChgs_ = newSze ;
    }
    chgNdx = numVarBndChgs_ ;
    numVarBndChgs_++ ;
    varHasChanged_[j] = chgNdx+1 ;
    varBndChgs_[chgNdx].ndx_ = j ;
    varBndChgs_[chgNdx].type_ = intVar_[j] ;
    varBndChgs_[chgNdx].revl_ = 0 ;
    varBndChgs_[chgNdx].ol_ = colL_[j] ;
    varBndChgs_[chgNdx].nl_ = colL_[j] ;
    varBndChgs_[chgNdx].revu_ = 0 ;
    varBndChgs_[chgNdx].ou_ = colU_[j] ;
    varBndChgs_[chgNdx].nu_ = colU_[j] ;
    varHasChanged_[j] = chgNdx+1 ;
  } else {
    chgNdx-- ;
  }
  CglPhicVarBndChg &chg = varBndChgs_[chgNdx] ;

  if (verbosity_ >= 5) {
    char vartypelet[3] = {'c','b','g'} ;
    int vartype = static_cast<int>(intVar_[j]) ;
    double delta = 0.0 ;
    if (deltal) {
      if (chg.ol_ > -infty_)
	delta = nbndj-chg.ol_ ;
      else
	delta = nbndj ;
    } else {
      if (chg.ou_ < infty_)
	delta = nbndj-chg.ou_ ;
      else
	delta = nbndj ;
    }
    std::cout
      << "          " << "x<" << j << "> " << vartypelet[vartype]
      << " [" << chg.ol_ << "," << chg.ou_ << "] " ;
    if (deltal)
      std::cout
	<< "lb #" << chg.revl_+1 << ": " << colL_[j] << " -> " << nbndj ;
    else
      std::cout
	<< "ub #" << chg.revu_+1 << ": " << colU_[j] << " -> " << nbndj ;
    std::cout << "  delta " << delta << std::endl ;
  }

  if (deltal) {
    colL_[j] = nbndj ;
    chg.revl_++ ;
    chg.nl_ = nbndj ;
  } else {
    colU_[j] = nbndj ;
    chg.revu_++ ;
    chg.nu_ = nbndj ;
  }
}

/*
  Report out variable bound changes as a pair of CoinPackedVectors. Very
  convenient for constructing a column cut.
*/
void CglPhic::getColBndChgs (CoinPackedVector &lbs, CoinPackedVector &ubs)
{
  lbs.clear() ;
  ubs.clear() ;
  for (int ndx = 0 ; ndx < numVarBndChgs_ ; ndx++) {
    const CglPhicVarBndChg &chgRec = varBndChgs_[ndx] ;
    if (chgRec.revl_ > 0) lbs.insert(chgRec.ndx_,chgRec.nl_) ;
    if (chgRec.revu_ > 0) ubs.insert(chgRec.ndx_,chgRec.nu_) ;
  }
}

/*
  Report out column bound changes as a vector of CglPhicBndPair entries. This
  happens to be convenient for some tasks. It's dead easy to construct this
  vector from inside, but difficult once the changes have been separated into
  a pair of packed vectors. Turns out that it's also handy on occasion to have
  a correlated array with the original bounds. And then there's the matter of
  controlling what bound changes you want to see. The arrays that are reported
  out may well be longer than necessary (there seems little point in
  reallocating them, as the length is generally small to begin with).
*/
int CglPhic::getColBndChgs (CglPhic::CglPhicBndPair **newBnds,
			    CglPhic::CglPhicBndPair **oldBnds,
			    bool binVar, bool intVar, bool conVar)
{
  assert(newBnds || oldBnds) ;
  if (newBnds) (*newBnds) = new CglPhicBndPair [numVarBndChgs_] ;
  if (oldBnds) (*oldBnds) = new CglPhicBndPair [numVarBndChgs_] ;
  int outNdx = 0 ;
  for (int chgNdx = 0 ; chgNdx < numVarBndChgs_ ; chgNdx++) {
    const CglPhicVarBndChg &chgRec = varBndChgs_[chgNdx] ;
    const int &j = chgRec.ndx_ ;
    const char &vartype = intVar_[j] ;
    if (!((binVar && vartype == 1) ||
    	  (intVar && vartype == 2) || (conVar && vartype == 0))) continue ;
    if (newBnds) {
      CglPhicBndPair &newBnd = (*newBnds)[outNdx] ;
      newBnd.ndx_ = chgRec.ndx_ ;
      newBnd.lb_ = chgRec.nl_ ;
      newBnd.changed_ = 0 ;
      if (chgRec.revl_ > 0) newBnd.changed_ |= 0x01 ;
      newBnd.ub_ = chgRec.nu_ ;
      if (chgRec.revu_ > 0) newBnd.changed_ |= 0x02 ;
    }
    if (oldBnds) {
      CglPhicBndPair &oldBnd = (*oldBnds)[outNdx] ;
      oldBnd.ndx_ = chgRec.ndx_ ;
      oldBnd.changed_ = 0 ;
      oldBnd.lb_ = chgRec.ol_ ;
      oldBnd.ub_ = chgRec.ou_ ;
    }
    outNdx++ ;
  }
  return (outNdx) ;
}

/*
  Create a row lhs bound change record.

  First find the existing change record, or make one. Then (after the logging
  print) update the bounds arrays and change record.

  The code 'B' triggers a full recalculation of both lhs bounds.
*/
void CglPhic::recordLhsBndChg (int i, bool fullRecalc, char bnd,
			       CglPhicLhsBnd &nbndi)
{
  assert(0 <= i && i < m_) ;
  assert(bnd == 'L' || bnd == 'U' || bnd == 'B') ;

  const bool deltaL = (bnd == 'L') ;

  int chgNdx = lhsHasChanged_[i] ;
/*
  If there's no existing change record, create one, initialising it with the
  current bounds.
*/
  if (!chgNdx) {
    if (numLhsBndChgs_ >= szeLhsBndChgs_) {
      int newSze = szeLhsBndChgs_+(szeLhsBndChgs_/2)+10 ;
      newSze = CoinMax(newSze,m_) ;
      CglPhicLhsBndChg *newLhsBndChgs = new CglPhicLhsBndChg [newSze] ;
      CoinMemcpyN(lhsBndChgs_,szeLhsBndChgs_,newLhsBndChgs) ;
      delete[] lhsBndChgs_ ;
      lhsBndChgs_ = newLhsBndChgs ;
      szeLhsBndChgs_ = newSze ;
    }
    chgNdx = numLhsBndChgs_ ;
    numLhsBndChgs_++ ;
    lhsHasChanged_[i] = chgNdx+1 ;
    lhsBndChgs_[chgNdx].ndx_ = i ;
    lhsBndChgs_[chgNdx].revL_ = 0 ;
    lhsBndChgs_[chgNdx].oL_ = lhsL_[i] ;
    lhsBndChgs_[chgNdx].nL_ = lhsL_[i] ;
    lhsBndChgs_[chgNdx].revU_ = 0 ;
    lhsBndChgs_[chgNdx].oU_ = lhsU_[i] ;
    lhsBndChgs_[chgNdx].nU_ = lhsU_[i] ;
    lhsHasChanged_[i] = chgNdx+1 ;
  } else {
    chgNdx-- ;
  }
  CglPhicLhsBndChg &chg = lhsBndChgs_[chgNdx] ;

  if (verbosity_ >= 5) {
    std::cout
      << "          " <<  "r(" << i << ") {" << chg.oL_ << "," << chg.oU_
      << "} " ;
    if (fullRecalc) std::cout << "*" ;
    if (deltaL) {
      std::cout
	<< "L #" << chg.revL_+1 << ": " << lhsL_[i] << " -> " << nbndi ;
      if (!fullRecalc && nbndi.infCnt_ == 0)
        std::cout << "  gap " << (nbndi.bnd_-rhsL_[i]) ;
    } else {
      std::cout
	<< "U #" << chg.revU_+1 << ": " << lhsU_[i] << " -> " << nbndi ;
      if (!fullRecalc && nbndi.infCnt_ == 0)
        std::cout << "  gap " << (rhsU_[i]-nbndi.bnd_) ;
    }
    std::cout << std::endl ;
  }
  if (fullRecalc) {
    calcLhsBnds(i) ;
  } else if (deltaL) {
    lhsL_[i] = nbndi ;
  } else {
    lhsU_[i] = nbndi ;
  }
  if (deltaL) {
    chg.revL_++ ;
    chg.nL_ = nbndi ;
  } else {
    chg.revU_++ ;
    chg.nU_ = nbndi ;
  }
}


/*
  Report out row lhs bound changes as a pair of CoinPackedVectors. There's a
  loss of information here: any amount of infinity translates into an infinite
  bound.
*/
void CglPhic::getRowLhsBndChgs (CoinPackedVector &lhsLChgs,
				CoinPackedVector &lhsUChgs)
{
  lhsLChgs.clear() ;
  lhsUChgs.clear() ;
  for (int ndx = 0 ; ndx < numLhsBndChgs_ ; ndx++) {
    const CglPhicLhsBndChg &chgRec = lhsBndChgs_[ndx] ;
    const int &i = chgRec.ndx_ ;
    if (chgRec.revL_ > 0) {
      if (chgRec.nL_.infCnt_ != 0)
        lhsLChgs.insert(i,-infty_) ;
      else
        lhsLChgs.insert(i,chgRec.nL_.bnd_) ;
    }
    if (chgRec.revU_ > 0) {
      if (chgRec.nU_.infCnt_ != 0)
        lhsUChgs.insert(i,infty_) ;
      else
        lhsUChgs.insert(i,chgRec.nU_.bnd_) ;
    }
  }
}

/*
  Report out row lhs bound changes as a vector of CglPhicBndPair entries.
  There's a loss of information here: any amount of infinity translates into
  an infinite bound. Just as for getColBndChgs, this is often a convenient
  form and it's easy to do from inside.
*/
int CglPhic::getRowLhsBndChgs (CglPhic::CglPhicBndPair **newBnds,
			       CglPhic::CglPhicBndPair **oldBnds)
{
  assert(newBnds || oldBnds) ;
  if (newBnds) (*newBnds) = new CglPhicBndPair [numLhsBndChgs_] ;
  if (oldBnds) (*oldBnds) = new CglPhicBndPair [numLhsBndChgs_] ;
  for (int ndx = 0 ; ndx < numLhsBndChgs_ ; ndx++) {
    const CglPhicLhsBndChg &chgRec = lhsBndChgs_[ndx] ;
    if (newBnds) {
      CglPhicBndPair &newBnd = (*newBnds)[ndx] ;
      newBnd.ndx_ = chgRec.ndx_ ;
      newBnd.changed_ = 0 ;
      if (chgRec.nL_.infCnt_ != 0)
	newBnd.lb_ = -infty_ ;
      else
	newBnd.lb_ = chgRec.nL_.bnd_ ;
      if (chgRec.revL_ > 0) newBnd.changed_ |= 0x01 ; 
      if (chgRec.nU_.infCnt_ != 0)
	newBnd.ub_ = infty_ ;
      else
	newBnd.ub_ = chgRec.nU_.bnd_ ;
      if (chgRec.revU_ > 0) newBnd.changed_ |= 0x02 ;
    }
    if (oldBnds) {
      CglPhicBndPair &oldBnd = (*oldBnds)[ndx] ;
      oldBnd.ndx_ = chgRec.ndx_ ;
      oldBnd.changed_ = 0 ;
      if (chgRec.oL_.infCnt_ != 0)
	oldBnd.lb_ = -infty_ ;
      else
	oldBnd.lb_ = chgRec.oL_.bnd_ ;
      if (chgRec.oU_.infCnt_ != 0)
	oldBnd.ub_ = -infty_ ;
      else
	oldBnd.ub_ = chgRec.oU_.bnd_ ;
    }
  }
  return (numLhsBndChgs_) ;
}

/*
  Revert the current set of bound changes (column, row, or both).

  The general notion here is that it's more efficient to back out the current
  set of bounds changes from within CglPhic, where we have complete access to
  the variable and constraint bound change records. There are two common
  reasons for this:
    * Revert the propagator state to an original state. Unless the bounds
      changes are particularly sweeping, it's more efficient to back out
      individual changes.
    * Revert loaned column bounds to an original state before reclaiming them.
  It's considerably more work to back out row lhs bounds, because the
  propagator doesn't keep complete information. A complete scan of each row is
  required.
*/
void CglPhic::revert (bool revertColBnds, bool revertRowBnds)
{
  if (verbosity_ >= 3) {
    std::cout << "          "
      << "reverting " << numVarBndChgs_ << " var bnds." << std::endl ;
  }
  if (revertColBnds) {
    for (int k = (numVarBndChgs_-1) ; k >= 0 ; k--) {
      CglPhicVarBndChg &chg = varBndChgs_[k] ;
      if (verbosity_ >= 4) {
        std::cout << "            " << chg << std::endl ;
      }
      int j = chg.ndx_ ;
      assert(0 <= j && j < n_) ;
      colL_[j] = chg.ol_ ;
      colU_[j] = chg.ou_ ;
      varHasChanged_[j] = 0 ;
    }
    numVarBndChgs_ = 0 ;
  }
  if (verbosity_ >= 3) {
    std::cout << "          "
      << "reverting " << numLhsBndChgs_ << " lhs bnds." << std::endl ;
  }
  if (revertRowBnds) {
    for (int k = (numLhsBndChgs_-1) ; k >= 0 ; k--) {
      CglPhicLhsBndChg &chg = lhsBndChgs_[k] ;
      if (verbosity_ >= 4) {
        std::cout << "            " << chg << std::endl ;
      }
      int i = chg.ndx_ ;
      assert(0 <= i && i < m_) ;
#     if CGLPHIC_RECALC_ON_REVERT
      calcLhsBnds(i) ;
#     else
      lhsL_[i] = chg.oL_ ;
      lhsU_[i] = chg.oU_ ;
#     endif
      lhsHasChanged_[i] = 0 ;
    }
    numLhsBndChgs_ = 0 ;
  }
}


/*
  Edit in a set of changes to column bounds. The result is once again
  considered to be the original bounds.  Valid only if there are no
  current changes. This avoids answering the question `What happens if an edit
  and a change collide?'
*/
void CglPhic::editColBnds (const CoinPackedVector *const lbs,
			   const CoinPackedVector *const ubs)
{
  assert(lbs || ubs) ;
  assert(numVarBndChgs_ == 0) ;

  if (lbs) {
    const int *const indices = lbs->getIndices() ;
    const double *const bnds = lbs->getElements() ;
    const int numBnds = lbs->getNumElements() ;
    for (int k = 0 ; k < numBnds ; k++) {
      const int &j = indices[k] ;
      colL_[j] = bnds[k] ;
    }
  }
  if (ubs) {
    const int *const indices = ubs->getIndices() ;
    const double *const bnds = ubs->getElements() ;
    const int numBnds = ubs->getNumElements() ;
    for (int k = 0 ; k < numBnds ; k++) {
      const int &j = indices[k] ;
      colU_[j] = bnds[k] ;
    }
  }
}

void CglPhic::editColBnds (int len,
			   const CglPhic::CglPhicBndPair *const newBnds)
{
  assert(newBnds) ;
  assert(numVarBndChgs_ == 0) ;

  for (int k = 0 ; k < len ; k++) {
    const CglPhicBndPair &newBnd = newBnds[k] ;
    const int &j = newBnd.ndx_ ;
    colL_[j] = newBnd.lb_ ;
    colU_[j] = newBnd.ub_ ;
  }
}

/*
  Print method for a constraint bound.
*/
std::ostream& operator<< (std::ostream &strm, CglPhic::CglPhicLhsBnd lhs)
{
  strm << "(" ;
  if (lhs.infCnt_ < 0)
    strm << "x(" << (-lhs.infCnt_)-1 << ")" ;
  else if (lhs.infCnt_ == 0 || lhs.infCnt_ >= 2)
    strm << lhs.infCnt_ ;
  else
    strm << "invalid!" ;
  strm << "," << lhs.bnd_ << ")" ;

  return (strm) ;
}

/*
  Print method for a variable bound change record.
*/
std::ostream& operator<< (std::ostream &strm, CglPhic::CglPhicVarBndChg chg)
{
  char typlet[3] = {'c','b','g'} ;
  strm
    << "x<" << chg.ndx_ << "> " << typlet[chg.type_]
    << " [" << chg.ol_ << "," << chg.ou_ << "] --#"
    << chg.revl_ << "," << chg.revu_
    << "#-> [" << chg.nl_ << "," << chg.nu_ << "]" ;

  return (strm) ;
}

/*
  Print method for a constraint lhs bound change record.
*/
std::ostream& operator<< (std::ostream &strm, CglPhic::CglPhicLhsBndChg chg)
{
  strm
    << "r(" << chg.ndx_ << ") {" << chg.oL_ << "," << chg.oU_
    << "} --#" << chg.revL_ << "," << chg.revU_
    << "#-> {" << chg.nL_ << "," << chg.nU_ << "}" ;

  return (strm) ;
}