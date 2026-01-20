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
Nr_Man_t * Nr_ManCreate( int nSize, char * calling_cmd )
{
    Nr_Man_t * p;
    p = ABC_ALLOC( Nr_Man_t, 1 );
    memset( p, 0, sizeof(Nr_Man_t) );
    p->nSizeFactor = 2;
    p->nBins = Abc_PrimeCudd( nSize > 0 ? nSize : 100 );
    p->pBins = ABC_ALLOC( Nr_Entry_t *, p->nBins );
    memset( p->pBins, 0, sizeof(Nr_Entry_t *) * p->nBins );
    p->pMem = Extra_MmFlexStart();
    p->calling_cmd = calling_cmd ? Extra_UtilStrsav( calling_cmd ) : NULL;
    return p;
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
    Nr_Origin_t * pOrigin;
    int i, j;
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
            {
                Vec_PtrForEachEntry( Nr_Origin_t *, pEntry->vOrigins, pOrigin, j )
                {
                    // names are stored in pMem, will be freed with pMem
                    ABC_FREE( pOrigin );
                }
                Vec_PtrFree( pEntry->vOrigins );
            }
            ABC_FREE( pEntry );
            pEntry = pNext;
        }
    }
    if ( p->pMem )
        Extra_MmFlexStop( p->pMem );
    if ( p->calling_cmd )
        ABC_FREE( p->calling_cmd );
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
    Nr_Origin_t * pOrigin;
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
    {
        Vec_PtrForEachEntry( Nr_Origin_t *, pEntryToDelete->vOrigins, pOrigin, i )
            ABC_FREE( pOrigin );
        Vec_PtrFree( pEntryToDelete->vOrigins );
    }
    ABC_FREE( pEntryToDelete );
    p->nEntries--;
}

/**Function*************************************************************

  Synopsis    [Adds an origin (name) to a node's origin list.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Nr_ManAddOrigin( Nr_Man_t * p, int NodeId, char * pOriginName )
{
    Nr_Entry_t * pEntry;
    Nr_Origin_t * pOrigin;
    char * pNameCopy;
    // find or create entry
    pEntry = Nr_ManTableLookup( p, NodeId );
    if ( pEntry == NULL )
    {
        pEntry = ABC_ALLOC( Nr_Entry_t, 1 );
        pEntry->NodeId = NodeId;
        pEntry->vOrigins = Vec_PtrAlloc( 4 );
        pEntry->pNext = NULL;
        Nr_ManTableAdd( p, pEntry );
    }
    // create origin entry
    pOrigin = ABC_ALLOC( Nr_Origin_t, 1 );
    // copy name if provided
    if ( pOriginName && p->pMem )
    {
        pNameCopy = (char *)Extra_MmFlexEntryFetch( p->pMem, strlen(pOriginName) + 1 );
        strcpy( pNameCopy, pOriginName );
        pOrigin->pName = pNameCopy;
    }
    else
    {
        pOrigin->pName = NULL;
    }
    Vec_PtrPush( pEntry->vOrigins, pOrigin );
    // output the calling command
    if ( p->calling_cmd )
        printf( "Nr_ManAddOrigin: called by %s\n", p->calling_cmd );
}

/**Function*************************************************************

  Synopsis    [Gets the list of origins for a node.]

  Description [Returns Vec_Ptr_t of Nr_Origin_t* entries. Returns NULL if node not found.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Vec_Ptr_t * Nr_ManGetOrigins( Nr_Man_t * p, int NodeId )
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
    Nr_Origin_t * pOrigin;
    Vec_Ptr_t * vUniqueNames;
    int i, j, nUnique = 0;
    char * pName;
    if ( p == NULL || p->nEntries == 0 )
        return 0;
    // collect all unique origin names
    vUniqueNames = Vec_PtrAlloc( 100 );
    for ( i = 0; i < p->nBins; i++ )
    {
        pEntry = p->pBins[i];
        while ( pEntry )
        {
            if ( pEntry->vOrigins )
            {
                Vec_PtrForEachEntry( Nr_Origin_t *, pEntry->vOrigins, pOrigin, j )
                {
                    pName = pOrigin->pName;
                    if ( pName == NULL )
                        continue;
                    // check if already in vector
                    for ( int k = 0; k < Vec_PtrSize(vUniqueNames); k++ )
                    {
                        if ( strcmp( (char *)Vec_PtrEntry(vUniqueNames, k), pName ) == 0 )
                        {
                            pName = NULL; // mark as found
                            break;
                        }
                    }
                    if ( pName != NULL )
                        Vec_PtrPush( vUniqueNames, pName );
                }
            }
            pEntry = pEntry->pNext;
        }
    }
    nUnique = Vec_PtrSize(vUniqueNames);
    Vec_PtrFree( vUniqueNames );
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
    Nr_Origin_t * pOrigin;
    int i, j;
    if ( p == NULL )
        return;
    for ( i = 0; i < p->nBins; i++ )
    {
        pEntry = p->pBins[i];
        while ( pEntry )
        {
            pNext = pEntry->pNext;
            if ( pEntry->vOrigins )
            {
                Vec_PtrForEachEntry( Nr_Origin_t *, pEntry->vOrigins, pOrigin, j )
                    ABC_FREE( pOrigin );
                Vec_PtrFree( pEntry->vOrigins );
            }
            ABC_FREE( pEntry );
            pEntry = pNext;
        }
        p->pBins[i] = NULL;
    }
    if ( p->pMem )
    {
        Extra_MmFlexStop( p->pMem );
        p->pMem = Extra_MmFlexStart();
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
    Nr_Origin_t * pOrigin;
    Vec_Ptr_t * vOrigins;
    int i;
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
    if ( vOrigins == NULL || Vec_PtrSize(vOrigins) == 0 )
    {
        printf( "Node %d: No origins.\n", NodeId );
        return;
    }
    printf( "Node %d has %d origin(s):\n", NodeId, Vec_PtrSize(vOrigins) );
    Vec_PtrForEachEntry( Nr_Origin_t *, vOrigins, pOrigin, i )
    {
        if ( pOrigin->pName )
            printf( "  [%d] Name: %s\n", i, pOrigin->pName );
        else
            printf( "  [%d] Name: (none)\n", i );
    }
}

/**Function*************************************************************

  Synopsis    [Prints origins for all nodes in the manager.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Nr_ManPrintAllOrigins( Nr_Man_t * p )
{
    Nr_Entry_t * pEntry;
    int i;
    if ( p == NULL )
    {
        printf( "Nr_ManPrintAllOrigins: Manager is NULL.\n" );
        return;
    }
    if ( p->nEntries == 0 )
    {
        printf( "Node retention manager is empty.\n" );
        return;
    }
    printf( "Printing all origins (%d entries):\n", p->nEntries );
    for ( i = 0; i < p->nBins; i++ )
    {
        pEntry = p->pBins[i];
        while ( pEntry )
        {
            Nr_ManPrintOrigins( p, pEntry->NodeId );
            pEntry = pEntry->pNext;
        }
    }
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
    printf( "DEBUG: Number of entries: %d\n", Nr_ManNumEntries( p ) );
    printf( "DEBUG: Number of original nodes mapped: %d\n", Nr_ManNumOriginalNodes( p ) );
}

ABC_NAMESPACE_IMPL_END

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
