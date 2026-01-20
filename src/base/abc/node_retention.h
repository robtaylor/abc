/**CFile****************************************************************

  FileName    [node_retention.h]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Node retention manager.]

  Synopsis    [Tracks original node IDs for transformed nodes.]

  Author      Silimate

  Affiliation Silimate

  Date        

  Revision    

***********************************************************************/

#ifndef ABC__base__abc__node_retention_h
#define ABC__base__abc__node_retention_h

////////////////////////////////////////////////////////////////////////
///                          INCLUDES                                ///
////////////////////////////////////////////////////////////////////////

#include "misc/vec/vec.h"
#include "misc/extra/extra.h"

ABC_NAMESPACE_HEADER_START

////////////////////////////////////////////////////////////////////////
///                         BASIC TYPES                              ///
////////////////////////////////////////////////////////////////////////

typedef struct Nr_Origin_t_ Nr_Origin_t;
struct Nr_Origin_t_
{
    char *           pName;          // original node name (can be NULL)
};

typedef struct Nr_Entry_t_ Nr_Entry_t;
struct Nr_Entry_t_
{
    int              NodeId;         // current node ID
    Vec_Ptr_t *      vOrigins;       // list of Nr_Origin_t* (name pairs)
    Nr_Entry_t *     pNext;          // next entry in hash table
};

typedef struct Nr_Man_t_ Nr_Man_t;
struct Nr_Man_t_
{
    Nr_Entry_t **    pBins;          // hash table bins
    int              nBins;          // number of bins
    int              nEntries;       // number of entries
    int              nSizeFactor;    // determines when to resize (default: 2)
    Extra_MmFlex_t * pMem;           // memory manager for origin names
    char *           calling_cmd;     // command that created this manager
};

////////////////////////////////////////////////////////////////////////
///                      MACRO DEFINITIONS                           ///
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
///                    FUNCTION DECLARATIONS                          ///
////////////////////////////////////////////////////////////////////////

/*=== node_retention.c ================================================*/
extern Nr_Man_t *    Nr_ManCreate( int nSize, char * calling_cmd );
extern void          Nr_ManFree( Nr_Man_t * p );
extern void          Nr_ManAddOrigin( Nr_Man_t * p, int NodeId, char * pOriginName );
extern Vec_Ptr_t *   Nr_ManGetOrigins( Nr_Man_t * p, int NodeId );
extern int           Nr_ManHasEntry( Nr_Man_t * p, int NodeId );
extern int           Nr_ManNumEntries( Nr_Man_t * p );
extern int           Nr_ManNumOriginalNodes( Nr_Man_t * p );
extern void          Nr_ManClear( Nr_Man_t * p );
extern void          Nr_ManProfile( Nr_Man_t * p );
extern void          Nr_ManPrintOrigins( Nr_Man_t * p, int NodeId );
extern void          Nr_ManPrintAllOrigins( Nr_Man_t * p );
extern void          Nr_ManPrintDebug( Nr_Man_t * p, char * pFuncName );

ABC_NAMESPACE_HEADER_END

#endif

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
