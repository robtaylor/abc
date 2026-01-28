/**CFile****************************************************************

  FileName    [node_retention.c]

  SystemName  [ABC: Logic synthesis and verification system.]

  PackageName [Node retention manager.]

  Synopsis    [Tracks original node IDs for transformed nodes.]

  Author      Silimate

  Affiliation Silimate

  Date        

  Revision    

***********************************************************************/

#include "base/abc/abc.h"
#include "base/abc/node_retention.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"

ABC_NAMESPACE_IMPL_START

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

static Nr_Entry_t * Nr_ManTableLookup( Nr_Man_t * p, int NodeId );
static void         Nr_ManTableAdd( Nr_Man_t * p, Nr_Entry_t * pEntry );
static void         Nr_ManTableDelete( Nr_Man_t * p, int NodeId );
static void         Nr_ManTableResize( Nr_Man_t * p );

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Creates the node retention manager.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Nr_Man_t * Nr_ManCreate( int nSize, int fCanModify, int fCanCopyFromOld )
{
    Nr_Man_t * p;
    p = ABC_ALLOC( Nr_Man_t, 1 );
    memset( p, 0, sizeof(Nr_Man_t) );
    p->nSizeFactor = 2;
    p->nBins = Abc_PrimeCudd( nSize > 0 ? nSize : 100 );
    p->pBins = ABC_ALLOC( Nr_Entry_t *, p->nBins );
    memset( p->pBins, 0, sizeof(Nr_Entry_t *) * p->nBins );
    p->fCanModify = fCanModify;
    p->fCanCopyFromOld = fCanCopyFromOld;
    return p;
}

/**Function*************************************************************

  Synopsis    [Sets the fCanModify flag.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Nr_ManSetCanModify( Nr_Man_t * p, int fCanModify )
{
    if ( p )
        p->fCanModify = fCanModify;
}

/**Function*************************************************************

  Synopsis    [Sets the fCanCopyFromOld flag.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Nr_ManSetCanCopyFromOld( Nr_Man_t * p, int fCanCopyFromOld )
{
    if ( p )
        p->fCanCopyFromOld = fCanCopyFromOld;
}

/**Function*************************************************************

  Synopsis    [Frees the node retention manager.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Nr_ManFree( Nr_Man_t * p )
{
    Nr_Entry_t * pEntry, * pNext;
    int i;
    if ( p == NULL )
        return;
    // free all entries and their origins
    for ( i = 0; i < p->nBins; i++ )
    {
        pEntry = p->pBins[i];
        while ( pEntry )
        {
            pNext = pEntry->pNext;
            if ( pEntry->vOrigins )
                Vec_IntFree( pEntry->vOrigins );
            ABC_FREE( pEntry );
            pEntry = pNext;
        }
    }
    ABC_FREE( p->pBins );
    ABC_FREE( p );
}

/**Function*************************************************************

  Synopsis    [Looks up an entry by node ID.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static Nr_Entry_t * Nr_ManTableLookup( Nr_Man_t * p, int NodeId )
{
    Nr_Entry_t * pEntry;
    int i = NodeId % p->nBins;
    pEntry = p->pBins[i];
    while ( pEntry != NULL && pEntry->NodeId != NodeId )
    {
        pEntry = pEntry->pNext;
    }
    return pEntry;
}

/**Function*************************************************************

  Synopsis    [Resizes the hash table when too many collisions occur.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static void Nr_ManTableResize( Nr_Man_t * p )
{
    Nr_Entry_t ** pBinsOld;
    Nr_Entry_t * pEntry, * pNext;
    int nBinsOld, i, iNew;
    // check if resize is needed
    if ( p->nEntries < p->nSizeFactor * p->nBins )
        return;
    // save old table
    pBinsOld = p->pBins;
    nBinsOld = p->nBins;
    // create new larger table
    p->nBins = Abc_PrimeCudd( 3 * p->nBins );
    p->pBins = ABC_ALLOC( Nr_Entry_t *, p->nBins );
    memset( p->pBins, 0, sizeof(Nr_Entry_t *) * p->nBins );
    // rehash all entries
    for ( i = 0; i < nBinsOld; i++ )
    {
        pEntry = pBinsOld[i];
        while ( pEntry )
        {
            pNext = pEntry->pNext;
            iNew = pEntry->NodeId % p->nBins;
            pEntry->pNext = p->pBins[iNew];
            p->pBins[iNew] = pEntry;
            pEntry = pNext;
        }
    }
    ABC_FREE( pBinsOld );
}

/**Function*************************************************************

  Synopsis    [Adds an entry to the hash table.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static void Nr_ManTableAdd( Nr_Man_t * p, Nr_Entry_t * pEntry )
{
    int i = pEntry->NodeId % p->nBins;
    pEntry->pNext = p->pBins[i];
    p->pBins[i] = pEntry;
    p->nEntries++;
    // check if resize is needed
    Nr_ManTableResize( p );
}

/**Function*************************************************************

  Synopsis    [Deletes an entry from the hash table.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static void Nr_ManTableDelete( Nr_Man_t * p, int NodeId )
{
    Nr_Entry_t * pEntry, * pPrev, * pEntryToDelete;
    int i;
    // use lookup to find the entry
    pEntryToDelete = Nr_ManTableLookup( p, NodeId );
    if ( pEntryToDelete == NULL )
        return;
    // find predecessor in hash table chain
    i = NodeId % p->nBins;
    pPrev = NULL;
    pEntry = p->pBins[i];
    while ( pEntry && pEntry != pEntryToDelete )
    {
        pPrev = pEntry;
        pEntry = pEntry->pNext;
    }
    // remove from hash table
    if ( pPrev )
        pPrev->pNext = pEntryToDelete->pNext;
    else
        p->pBins[i] = pEntryToDelete->pNext;
    // free origins
    if ( pEntryToDelete->vOrigins )
        Vec_IntFree( pEntryToDelete->vOrigins );
    ABC_FREE( pEntryToDelete );
    p->nEntries--;
}

/**Function*************************************************************

  Synopsis    [Adds an origin (name) to a node's origin list.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Nr_ManAddOrigin( Nr_Man_t * p, int NodeId, int OriginId )
{
    Nr_Entry_t * pEntry;
    // check if modification is allowed
    if ( !p || !p->fCanModify )
        return;
    // find or create entry
    pEntry = Nr_ManTableLookup( p, NodeId );
    if ( pEntry == NULL )
    {
        pEntry = ABC_ALLOC( Nr_Entry_t, 1 );
        pEntry->NodeId = NodeId;
        pEntry->vOrigins = Vec_IntAlloc( 4 );
        pEntry->pNext = NULL;
        Nr_ManTableAdd( p, pEntry );
    }
    Vec_IntPush( pEntry->vOrigins, OriginId );
}

/**Function*************************************************************

  Synopsis    [Copies origins from old manager to new manager.]

  Description [Copies all origins associated with OldId in pOld to NewId in pNew.
               Only copies if pNew->fCanCopyFromOld is true.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Nr_ManCopyOrigins( Nr_Man_t * pNew, Nr_Man_t * pOld, int NewId, int OldId )
{
    Vec_Int_t * vOrigins;
    int j, OriginId;
    // check if copying is allowed
    if ( !pNew || !pNew->fCanCopyFromOld || !pOld )
        return;
    // get origins from old manager
    vOrigins = Nr_ManGetOrigins( pOld, OldId );
    if ( vOrigins && Vec_IntSize(vOrigins) > 0 )
    {
        Vec_IntForEachEntry( vOrigins, OriginId, j )
            Nr_ManAddOrigin( pNew, NewId, OriginId );
    } else {
        printf( "Nr_ManCopyOrigins: No origins found for node %d\n", OldId );
    }
}

/**Function*************************************************************

  Synopsis    [Gets the list of origins for a node.]

  Description [Returns Vec_Int_t of origin IDs. Returns NULL if node not found.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Vec_Int_t * Nr_ManGetOrigins( Nr_Man_t * p, int NodeId )
{
    Nr_Entry_t * pEntry;
    pEntry = Nr_ManTableLookup( p, NodeId );
    return pEntry ? pEntry->vOrigins : NULL;
}

/**Function*************************************************************

  Synopsis    [Checks if an entry exists for a node ID.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Nr_ManHasEntry( Nr_Man_t * p, int NodeId )
{
    return Nr_ManTableLookup( p, NodeId ) != NULL;
}

/**Function*************************************************************

  Synopsis    [Returns the number of entries.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Nr_ManNumEntries( Nr_Man_t * p )
{
    return p ? p->nEntries : 0;
}

/**Function*************************************************************
 *
 *  Synopsis    [Returns the number of unique original nodes mapped.]
 *
 *  Description [Counts the number of unique origin names across all entries.]
 *               
 *  SideEffects []
 *
 *  SeeAlso     []
 *
***********************************************************************/
int Nr_ManNumOriginalNodes( Nr_Man_t * p )
{
    Nr_Entry_t * pEntry;
    Vec_Int_t * vUniqueIds;
    int i, j, OriginId, k, nUnique = 0;
    if ( p == NULL || p->nEntries == 0 )
        return 0;
    // collect all unique origin IDs
    vUniqueIds = Vec_IntAlloc( 100 );
    for ( i = 0; i < p->nBins; i++ )
    {
        pEntry = p->pBins[i];
        while ( pEntry )
        {
            if ( pEntry->vOrigins )
            {
                Vec_IntForEachEntry( pEntry->vOrigins, OriginId, j )
                {
                    // check if already in vector
                    Vec_IntForEachEntry( vUniqueIds, k, k )
                        if ( k == OriginId )
                            goto skip;
                    Vec_IntPush( vUniqueIds, OriginId );
                    skip:;
                }
            }
            pEntry = pEntry->pNext;
        }
    }
    nUnique = Vec_IntSize(vUniqueIds);
    Vec_IntFree( vUniqueIds );
    return nUnique;
}

/**Function*************************************************************

  Synopsis    [Clears all entries but keeps the structure.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Nr_ManClear( Nr_Man_t * p )
{
    Nr_Entry_t * pEntry, * pNext;
    int i;
    if ( p == NULL )
        return;
    for ( i = 0; i < p->nBins; i++ )
    {
        pEntry = p->pBins[i];
        while ( pEntry )
        {
            pNext = pEntry->pNext;
            if ( pEntry->vOrigins )
                Vec_IntFree( pEntry->vOrigins );
            ABC_FREE( pEntry );
            pEntry = pNext;
        }
        p->pBins[i] = NULL;
    }
    p->nEntries = 0;
}

/**Function*************************************************************

  Synopsis    [Profiles the hash table distribution.]

  Description [Prints statistics about hash table usage and collisions.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Nr_ManProfile( Nr_Man_t * p )
{
    Nr_Entry_t * pEntry;
    int i, nEmpty, nMaxChain, nChain, nTotalChains;
    if ( p == NULL )
    {
        printf( "Nr_ManProfile: Manager is NULL.\n" );
        return;
    }
    nEmpty = 0;
    nMaxChain = 0;
    nTotalChains = 0;
    for ( i = 0; i < p->nBins; i++ )
    {
        nChain = 0;
        pEntry = p->pBins[i];
        while ( pEntry )
        {
            nChain++;
            pEntry = pEntry->pNext;
        }
        if ( nChain == 0 )
            nEmpty++;
        else
            nTotalChains += nChain;
        if ( nChain > nMaxChain )
            nMaxChain = nChain;
    }
    printf( "Hash table profile:\n" );
    printf( "  Bins:        %d\n", p->nBins );
    printf( "  Entries:     %d\n", p->nEntries );
    printf( "  Empty bins:  %d (%.2f%%)\n", nEmpty, 100.0 * nEmpty / p->nBins );
    printf( "  Max chain:   %d\n", nMaxChain );
    if ( p->nEntries > 0 )
        printf( "  Avg chain:   %.2f\n", (double)nTotalChains / (p->nBins - nEmpty) );
    printf( "  Load factor: %.2f\n", (double)p->nEntries / p->nBins );
}

/**Function*************************************************************

  Synopsis    [Prints the origins for a node in a clean format.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Nr_ManPrintOrigins( Nr_Man_t * p, int NodeId )
{
    Nr_Entry_t * pEntry;
    Vec_Int_t * vOrigins;
    int i, OriginId;
    if ( p == NULL )
    {
        printf( "Nr_ManPrintOrigins: Manager is NULL.\n" );
        return;
    }
    pEntry = Nr_ManTableLookup( p, NodeId );
    if ( pEntry == NULL )
    {
        printf( "Node %d: No entry found.\n", NodeId );
        return;
    }
    vOrigins = pEntry->vOrigins;
    if ( vOrigins == NULL || Vec_IntSize(vOrigins) == 0 )
    {
        printf( "Node %d: No origins.\n", NodeId );
        return;
    }
    printf( "Node %d has %d origin(s):\n", NodeId, Vec_IntSize(vOrigins) );
    Vec_IntForEachEntry( vOrigins, OriginId, i )
        printf( "  [%d] OriginID: %d\n", i, OriginId );
}

/**Function*************************************************************

  Synopsis    [Prints origins for all nodes in the manager.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Nr_ManPrintAllOrigins( Nr_Man_t * p )
{
    // Nr_Entry_t * pEntry;
    // int i;
    // if ( p == NULL )
    // {
    //     printf( "Nr_ManPrintAllOrigins: Manager is NULL.\n" );
    //     return;
    // }
    // if ( p->nEntries == 0 )
    // {
    //     printf( "Node retention manager is empty.\n" );
    //     return;
    // }
    // printf( "Printing all origins (%d entries):\n", p->nEntries );
    // for ( i = 0; i < p->nBins; i++ )
    // {
    //     pEntry = p->pBins[i];
    //     while ( pEntry )
    //     {
    //         Nr_ManPrintOrigins( p, pEntry->NodeId );
    //         pEntry = pEntry->pNext;
    //     }
    // }
}

/**Function*************************************************************

  Synopsis    [Prints debug information about node retention manager.]

  Description [Takes a function name and prints node retention info after that function.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Nr_ManPrintDebug( Nr_Man_t * p, char * pFuncName )
{
    if ( p == NULL )
        return;
    printf( "DEBUG: Node retention after %s:\n", pFuncName );
    Nr_ManPrintAllOrigins( p );
    // printf( "DEBUG: Number of entries: %d\n", Nr_ManNumEntries( p ) );
    // printf( "DEBUG: Number of original nodes mapped: %d\n", Nr_ManNumOriginalNodes( p ) );
}

/**Function*************************************************************

  Synopsis    [Prints node retention map for nets to a file.]

  Description [Iterates through all nets in the netlist and prints
               their retention information in the format:
               # net_name SRC origin1 origin2 ...]
               
  SideEffects []
  
  SeeAlso     []

***********************************************************************/
void Nr_ManPrintRetentionMap( FILE * pFile, Abc_Ntk_t * pNtk, Nr_Man_t * p )
{
    Abc_Obj_t * pNet;
    Vec_Int_t * vOrigins;
    Abc_Frame_t * pAbc;
    int i, j;
    
    if ( pFile == NULL || pNtk == NULL || p == NULL )
        return;
    pAbc = Abc_FrameGetGlobalFrame();
    fprintf( pFile, ".node_retention_begin\n" );
    Abc_NtkForEachNet( pNtk, pNet, i )
    {
        int NetId = Abc_ObjId(pNet);
        vOrigins = Nr_ManGetOrigins( p, NetId );
        if ( vOrigins && Vec_IntSize(vOrigins) > 0 )
        {
            int OriginId;
            char * pName;
            fprintf( pFile, "%s SRC", Abc_ObjName(pNet) );
            Vec_IntForEachEntry( vOrigins, OriginId, j )
            {
                pName = (char *)Vec_PtrEntry( pAbc->vNodeRetention, OriginId );
                if ( pName )
                    fprintf( pFile, " %s", pName );
            }
            fprintf( pFile, "\n" );
        }
    }
    fprintf( pFile, ".node_retention_end\n" );
}

int Nr_ManTotalOriginCount( Nr_Man_t * p )
{
    Nr_Entry_t * pEntry;
    int i, nTotal = 0;
    if ( p == NULL ) return 0;
    for ( i = 0; i < p->nBins; i++ )
        for ( pEntry = p->pBins[i]; pEntry; pEntry = pEntry->pNext )
            if ( pEntry->vOrigins )
                nTotal += Vec_IntSize(pEntry->vOrigins);
    return nTotal;
}

ABC_NAMESPACE_IMPL_END

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
