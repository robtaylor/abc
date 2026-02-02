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

#include "base/abc/abc.h"
#include "base/abc/node_retention.h"
#include "base/main/main.h"
#include "base/main/mainInt.h"
#include "aig/gia/gia.h"
#include "aig/aig/aig.h"

ABC_NAMESPACE_IMPL_START

////////////////////////////////////////////////////////////////////////
///                        DECLARATIONS                              ///
////////////////////////////////////////////////////////////////////////

static Nr_Entry_t * Nr_ManTableLookup( Nr_Man_t * p, int NodeId );
static void         Nr_ManTableAdd( Nr_Man_t * p, Nr_Entry_t * pEntry );
static void         Nr_ManTableResize( Nr_Man_t * p );

// Origin hash set helpers
static Nr_OriginSet_t * Nr_EntryOriginLookup( Nr_Entry_t * pEntry, int OriginId, int * pBin );
static void         Nr_EntryOriginAdd( Nr_Entry_t * pEntry, int OriginId );
static void         Nr_EntryOriginsFree( Nr_Entry_t * pEntry );

////////////////////////////////////////////////////////////////////////
///                     FUNCTION DEFINITIONS                         ///
////////////////////////////////////////////////////////////////////////

/**Function*************************************************************

  Synopsis    [Creates the node retention manager.]

  Description [Allocates and initializes a hash table to track node origins.
               nSize hints initial capacity. Reads fNodeRetention from global
               frame to determine if tracking is enabled.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
Nr_Man_t * Nr_ManCreate( int nSize )
{
    Abc_Frame_t * pAbc = Abc_FrameGetGlobalFrame();
    Nr_Man_t * p;
    p = ABC_ALLOC( Nr_Man_t, 1 );
    memset( p, 0, sizeof(Nr_Man_t) );
    p->nSizeFactor = 2;                                      // resize threshold multiplier
    p->nBins = Abc_PrimeCudd( nSize > 0 ? nSize : 100 );     // prime-sized hash table
    p->pBins = ABC_ALLOC( Nr_Entry_t *, p->nBins );
    memset( p->pBins, 0, sizeof(Nr_Entry_t *) * p->nBins );
    p->fEnabled = pAbc ? pAbc->fNodeRetention : 0;
    return p;
}

/**Function*************************************************************

  Synopsis    [Sets the fEnabled flag.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Nr_ManSetEnabled( Nr_Man_t * p, int fEnabled )
{
    if ( p )
        p->fEnabled = fEnabled;
}

/**Function*************************************************************

  Synopsis    [Frees the origin hash set for an entry.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static void Nr_EntryOriginsFree( Nr_Entry_t * pEntry )
{
    Nr_OriginSet_t * pOrig, * pNext;
    int i;
    if ( pEntry->pOriginBins == NULL )
        return;
    for ( i = 0; i < pEntry->nOriginBins; i++ )
    {
        pOrig = pEntry->pOriginBins[i];
        while ( pOrig )
        {
            pNext = pOrig->pNext;
            ABC_FREE( pOrig );
            pOrig = pNext;
        }
    }
    ABC_FREE( pEntry->pOriginBins );
    pEntry->pOriginBins = NULL;
    pEntry->nOriginBins = 0;
    pEntry->nOrigins = 0;
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
    // free all entries and their origin hash sets
    for ( i = 0; i < p->nBins; i++ )
    {
        pEntry = p->pBins[i];
        while ( pEntry )
        {
            pNext = pEntry->pNext;
            Nr_EntryOriginsFree( pEntry );
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
    for ( pEntry = p->pBins[i]; pEntry; pEntry = pEntry->pNext )
        if ( pEntry->NodeId == NodeId )
            return pEntry;
    return NULL;
}

/**Function*************************************************************

  Synopsis    [Resizes the hash table when load factor exceeded.]

  Description [Triples table size (to next prime) and rehashes all entries.
               Called automatically when nEntries > nSizeFactor * nBins.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static void Nr_ManTableResize( Nr_Man_t * p )
{
    Nr_Entry_t ** pBinsOld;
    Nr_Entry_t * pEntry, * pNext;
    int nBinsOld, i, iNew;
    // skip if load factor not exceeded
    if ( p->nEntries < p->nSizeFactor * p->nBins )
        return;
    // save old table
    pBinsOld = p->pBins;
    nBinsOld = p->nBins;
    // allocate 3x larger table (prime-sized for better distribution)
    p->nBins = Abc_PrimeCudd( 3 * p->nBins );
    p->pBins = ABC_ALLOC( Nr_Entry_t *, p->nBins );
    memset( p->pBins, 0, sizeof(Nr_Entry_t *) * p->nBins );
    // rehash: move each entry to new bin based on new table size
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

  Synopsis    [Looks up origin in hash set, returns pointer or NULL.]

  Description [Returns pointer to origin entry if found, NULL otherwise.
               Also sets *pBin to the bin index for use by caller.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static Nr_OriginSet_t * Nr_EntryOriginLookup( Nr_Entry_t * pEntry, int OriginId, int * pBin )
{
    Nr_OriginSet_t * pOrig;
    int i;
    if ( pEntry->pOriginBins == NULL )
        return NULL;
    i = OriginId % pEntry->nOriginBins;
    if ( pBin ) *pBin = i;
    for ( pOrig = pEntry->pOriginBins[i]; pOrig; pOrig = pOrig->pNext )
        if ( pOrig->OriginId == OriginId )
            return pOrig;
    return NULL;
}

/**Function*************************************************************

  Synopsis    [Adds an origin to the entry's origin hash set.]

  Description [Creates hash set if needed. Ignores duplicates.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
static void Nr_EntryOriginAdd( Nr_Entry_t * pEntry, int OriginId )
{
    Nr_OriginSet_t * pOrig;
    int iBin;
    // initialize origin hash set if needed
    if ( pEntry->pOriginBins == NULL )
    {
        pEntry->nOriginBins = Abc_PrimeCudd( 8 );
        pEntry->pOriginBins = ABC_ALLOC( Nr_OriginSet_t *, pEntry->nOriginBins );
        memset( pEntry->pOriginBins, 0, sizeof(Nr_OriginSet_t *) * pEntry->nOriginBins );
    }
    // lookup returns existing entry or NULL, and sets iBin
    if ( Nr_EntryOriginLookup( pEntry, OriginId, &iBin ) )
        return;
    // add new origin at iBin (no rehash needed)
    pOrig = ABC_ALLOC( Nr_OriginSet_t, 1 );
    pOrig->OriginId = OriginId;
    pOrig->pNext = pEntry->pOriginBins[iBin];
    pEntry->pOriginBins[iBin] = pOrig;
    pEntry->nOrigins++;
}

/**Function*************************************************************

  Synopsis    [Adds an origin (name) to a node's origin list.]

  Description [Uses hash set to automatically prevent duplicates.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Nr_ManAddOrigin( Nr_Man_t * p, int NodeId, int OriginId )
{
    Nr_Entry_t * pEntry;
    // check if enabled
    if ( !p || !p->fEnabled )
        return;
    // find or create entry
    pEntry = Nr_ManTableLookup( p, NodeId );
    if ( pEntry == NULL )
    {
        pEntry = ABC_ALLOC( Nr_Entry_t, 1 );
        memset( pEntry, 0, sizeof(Nr_Entry_t) );
        pEntry->NodeId = NodeId;
        Nr_ManTableAdd( p, pEntry );
    }
    Nr_EntryOriginAdd( pEntry, OriginId );
}

/**Function*************************************************************

  Synopsis    [Copies origins from old manager to new manager.]

  Description [Copies all origins associated with OldId in pOld to NewId in pNew.
               Only copies if pNew is enabled.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Nr_ManCopyOrigins( Nr_Man_t * pNew, Nr_Man_t * pOld, int NewId, int OldId )
{
    Nr_Entry_t * pEntry;
    Nr_OriginSet_t * pOrig;
    int i;
    // check if enabled
    if ( !pNew || !pNew->fEnabled || !pOld )
        return;
    // get entry from old manager
    pEntry = Nr_ManTableLookup( pOld, OldId );
    if ( pEntry == NULL || pEntry->pOriginBins == NULL )
        return;
    // copy all origins using the iteration macro
    Nr_EntryForEachOrigin( pEntry, pOrig, i )
        Nr_ManAddOrigin( pNew, NewId, pOrig->OriginId );
}

/**Function*************************************************************

  Synopsis    [Gets the list of origins for a node.]

  Description [Returns Vec_Int_t of origin IDs. Returns NULL if node not found.
               Caller must free the returned vector.]
               
  SideEffects [Allocates new vector that caller must free.]

  SeeAlso     []

***********************************************************************/
Vec_Int_t * Nr_ManGetOrigins( Nr_Man_t * p, int NodeId )
{
    Nr_Entry_t * pEntry;
    Nr_OriginSet_t * pOrig;
    Vec_Int_t * vOrigins;
    int i;
    pEntry = Nr_ManTableLookup( p, NodeId );
    if ( pEntry == NULL || pEntry->nOrigins == 0 )
        return NULL;
    // build vector from hash set
    vOrigins = Vec_IntAlloc( pEntry->nOrigins );
    Nr_EntryForEachOrigin( pEntry, pOrig, i )
        Vec_IntPush( vOrigins, pOrig->OriginId );
    return vOrigins;
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

  Synopsis    [Prints shape info: entries, total origins, and ratio.]

  Description [Outputs to node_ret/shape.log with label for identification.]
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
void Nr_ManPrintShape( Nr_Man_t * p, const char * pLabel )
{
    FILE * f;
    int nEntries, nOrigins;
    float ratio;
    if ( p == NULL )
        return;
    f = fopen( "node_ret/shape.log", !strcmp(pLabel, "read_blif") ? "w" : "a" );
    if ( f == NULL )
        return;
    nEntries = Nr_ManNumEntries( p );
    nOrigins = Nr_ManTotalOriginCount( p );
    ratio = nEntries > 0 ? (float)nOrigins / nEntries : 0.0;
    fprintf( f, "%-30s  |  Entries: %8d  |  Origins: %8d  |  Ratio: %6.2f\n", 
             pLabel ? pLabel : "", nEntries, nOrigins, ratio );
    fclose( f );
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
            Nr_EntryOriginsFree( pEntry );
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
        for ( pEntry = p->pBins[i]; pEntry; pEntry = pEntry->pNext )
            nChain++;
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
    Nr_OriginSet_t * pOrig;
    int i, nPrinted;
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
    if ( pEntry->nOrigins == 0 )
    {
        printf( "Node %d: No origins.\n", NodeId );
        return;
    }
    printf( "Node %d has %d origin(s):\n", NodeId, pEntry->nOrigins );
    nPrinted = 0;
    Nr_EntryForEachOrigin( pEntry, pOrig, i )
        printf( "  [%d] OriginID: %d\n", nPrinted++, pOrig->OriginId );
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
    Nr_Entry_t * pEntry;
    Nr_OriginSet_t * pOrig;
    Abc_Frame_t * pAbc;
    int i, j;
    
    if ( pFile == NULL || pNtk == NULL || p == NULL )
        return;
    
    pAbc = Abc_FrameGetGlobalFrame();
    fprintf( pFile, ".node_retention_begin\n" );
    Abc_NtkForEachNet( pNtk, pNet, i )
    {
        int NetId = Abc_ObjId(pNet);
        pEntry = Nr_ManTableLookup( p, NetId );
        if ( pEntry && pEntry->nOrigins > 0 )
        {
            char * pName;
            fprintf( pFile, "%s SRC", Abc_ObjName(pNet) );
            Nr_EntryForEachOrigin( pEntry, pOrig, j )
            {
                pName = (char *)Vec_PtrEntry( pAbc->vNodeRetention, pOrig->OriginId );
                if ( pName )
                    fprintf( pFile, " %s", pName );
            }
            fprintf( pFile, "\n" );
        }
    }
    fprintf( pFile, ".node_retention_end\n" );
}


/**Function*************************************************************

  Synopsis    [Returns total number of origins across all entries.]

  Description []
               
  SideEffects []

  SeeAlso     []

***********************************************************************/
int Nr_ManTotalOriginCount( Nr_Man_t * p )
{
    Nr_Entry_t * pEntry;
    int i, nTotal = 0;
    if ( p == NULL ) 
        return 0;
    Nr_ManForEachEntry( p, pEntry, i )
        nTotal += pEntry->nOrigins;
    return nTotal;
}

ABC_NAMESPACE_IMPL_END

////////////////////////////////////////////////////////////////////////
///                       END OF FILE                                ///
////////////////////////////////////////////////////////////////////////
