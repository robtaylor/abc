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

// Hash set entry for unique origin storage
typedef struct Nr_OriginSet_t_ Nr_OriginSet_t;
struct Nr_OriginSet_t_
{
    int              OriginId;       // the origin node ID
    Nr_OriginSet_t * pNext;          // next entry in hash chain
};

typedef struct Nr_Entry_t_ Nr_Entry_t;
struct Nr_Entry_t_
{
    int              NodeId;         // current node ID
    Nr_OriginSet_t **pOriginBins;    // hash set bins for origins (prevents duplicates)
    int              nOriginBins;    // number of bins in origin hash set
    int              nOrigins;       // number of unique origins
    Nr_Entry_t *     pNext;          // next entry in main hash table
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
extern void          Nr_ManPrintShape( Nr_Man_t * p, const char * pLabel );

// Iterate all entries in the manager's hash table
#define Nr_ManForEachEntry( p, pEntry, i ) \
    for ( i = 0; i < (p)->nBins; i++ ) \
        if ( (p)->pBins[i] == NULL ) {} else \
          for ( pEntry = (p)->pBins[i]; pEntry; pEntry = pEntry->pNext )

// Iterate all origins in an entry's origin hash set
#define Nr_EntryForEachOrigin( pEntry, pOrig, i ) \
    for ( i = 0; i < (pEntry)->nOriginBins; i++ ) \
        if ( (pEntry)->pOriginBins[i] == NULL ) {} else \
          for ( pOrig = (pEntry)->pOriginBins[i]; pOrig; pOrig = pOrig->pNext )

ABC_NAMESPACE_HEADER_END

#endif

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
