/**CFile****************************************************************

  FileName    [node_retention.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Node retention manager.]

  Synopsis    [Tracks original node IDs for transformed nodes.]

  Author      Advay Singh

  Affiliation Silimate

  Date        [Jan 2026]

  Revision    

***********************************************************************/

#ifndef ABC__base__abc__node_retention_h
#define ABC__base__abc__node_retention_h

////////////////////////////////////////////////////////////////////////
///                          INCLUDES                                ///
////////////////////////////////////////////////////////////////////////

#include "misc/vec/vec.h"
ABC_NAMESPACE_HEADER_START

// Forward declaration for Extra_MmFlex_t (defined in misc/extra/extra.h)
typedef struct Extra_MmFlex_t_ Extra_MmFlex_t;

typedef struct Abc_Ntk_t_ Abc_Ntk_t;
typedef struct Gia_Man_t_ Gia_Man_t;
typedef struct Aig_Man_t_ Aig_Man_t;

////////////////////////////////////////////////////////////////////////
///                         BASIC TYPES                              ///
////////////////////////////////////////////////////////////////////////

typedef struct Nr_Entry_t_ Nr_Entry_t;
struct Nr_Entry_t_
{
    int              NodeId;         // current node ID
    Vec_Int_t *      vOrigins;       // list of origin node IDs (uint32_t indices)
    Nr_Entry_t *     pNext;          // next entry in hash table
};

typedef struct Nr_Man_t_ Nr_Man_t;
struct Nr_Man_t_
{
    Nr_Entry_t **    pBins;          // hash table bins
    int              nBins;          // number of bins
    int              nEntries;       // number of entries
    int              nSizeFactor;    // determines when to resize (default: 2)
    int              fEnabled;       // whether node retention tracking is enabled
};

////////////////////////////////////////////////////////////////////////
///                      MACRO DEFINITIONS                           ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                    FUNCTION DECLARATIONS                          ///
////////////////////////////////////////////////////////////////////////

/*=== node_retention.c ================================================*/
extern Nr_Man_t *    Nr_ManCreate( int nSize );
extern void          Nr_ManFree( Nr_Man_t * p );
extern void          Nr_ManSetEnabled( Nr_Man_t * p, int fEnabled );
extern void          Nr_ManAddOrigin( Nr_Man_t * p, int NodeId, int OriginId );
extern void          Nr_ManCopyOrigins( Nr_Man_t * pNew, Nr_Man_t * pOld, int NewId, int OldId );
extern Vec_Int_t *   Nr_ManGetOrigins( Nr_Man_t * p, int NodeId );
extern int           Nr_ManHasEntry( Nr_Man_t * p, int NodeId );
extern int           Nr_ManNumEntries( Nr_Man_t * p );
extern void          Nr_ManClear( Nr_Man_t * p );
extern void          Nr_ManProfile( Nr_Man_t * p );
extern void          Nr_ManPrintOrigins( Nr_Man_t * p, int NodeId );
extern void          Nr_ManPrintRetentionMap( FILE * pFile, Abc_Ntk_t * pNtk, Nr_Man_t * p );
extern int           Nr_ManTotalOriginCount( Nr_Man_t * p );
extern Nr_Man_t *    Nr_ManPrune( Nr_Man_t * p );
extern void          Nr_ManPrintShape( Nr_Man_t * p, const char * pLabel );

ABC_NAMESPACE_HEADER_END

#endif

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
