#include "gtest/gtest.h"

#include "aig/gia/gia.h"
#include "base/abc/node_retention.h"

ABC_NAMESPACE_IMPL_START

TEST(GiaTest, CanAllocateGiaManager) {
  Gia_Man_t* aig_manager =  Gia_ManStart(100);

  EXPECT_TRUE(aig_manager != nullptr);
  Gia_ManStop(aig_manager);
}

TEST(GiaTest, CanAddACi) {
  Gia_Man_t* aig_manager =  Gia_ManStart(100);
  Gia_ManAppendCi(aig_manager);

  EXPECT_EQ(Gia_ManCiNum(aig_manager), 1);
  Gia_ManStop(aig_manager);
}

TEST(GiaTest, CanAddACo) {
  Gia_Man_t* aig_manager =  Gia_ManStart(100);
  int input1 = Gia_ManAppendCi(aig_manager);
  Gia_ManAppendCo(aig_manager, input1);

  EXPECT_EQ(Gia_ManCiNum(aig_manager), 1);
  EXPECT_EQ(Gia_ManCoNum(aig_manager), 1);
  Gia_ManStop(aig_manager);
}

TEST(GiaTest, CanAddAnAndGate) {
  Gia_Man_t* aig_manager =  Gia_ManStart(100);

  int input1 = Gia_ManAppendCi(aig_manager);
  int input2 = Gia_ManAppendCi(aig_manager);

  int and_output = Gia_ManAppendAnd(aig_manager, input1, input2);
  Gia_ManAppendCo(aig_manager, and_output);

  Vec_Wrd_t* stimulus = Vec_WrdAlloc(2);
  Vec_WrdPush(stimulus, /*A*/1);
  Vec_WrdPush(stimulus, /*B*/1);
  Vec_Wrd_t* output = Gia_ManSimPatSimOut(aig_manager, stimulus, /*fouts*/1);

  EXPECT_EQ(Gia_ManCiNum(aig_manager), 2);
  EXPECT_EQ(Gia_ManCoNum(aig_manager), 1);
  // A = 1, B = 1 -> A & B == 1
  EXPECT_EQ(Vec_WrdGetEntry(output, 0), 1);
  Vec_WrdFree(output);
  Gia_ManStop(aig_manager);
}

TEST(GiaRetentionTest, NewGiaHasRetentionManager) {
  Gia_Man_t* p = Gia_ManStart(100);
  EXPECT_NE(p->pNodeRetention, nullptr);
  Gia_ManStop(p);
}

TEST(GiaRetentionTest, SeedAndRetrieveOrigins) {
  Gia_Man_t* p = Gia_ManStart(100);
  int ci_lit = Gia_ManAppendCi(p);
  int ci_id = Abc_Lit2Var(ci_lit);

  // Seed CI with self-origin
  Nr_ManAddOrigin(p->pNodeRetention, ci_id, ci_id);

  Vec_Int_t* origins = Nr_ManGetOrigins(p->pNodeRetention, ci_id);
  ASSERT_NE(origins, nullptr);
  EXPECT_EQ(Vec_IntSize(origins), 1);
  EXPECT_EQ(Vec_IntEntry(origins, 0), ci_id);

  Gia_ManStop(p);
}

TEST(GiaRetentionTest, RetentionPropagatesToDup) {
  // Build a simple GIA: two CIs -> AND -> CO
  Gia_Man_t* p = Gia_ManStart(100);
  p->pName = Abc_UtilStrsav((char*)"test");
  int ci1_lit = Gia_ManAppendCi(p);
  int ci2_lit = Gia_ManAppendCi(p);
  int ci1_id = Abc_Lit2Var(ci1_lit);
  int ci2_id = Abc_Lit2Var(ci2_lit);

  // Seed CIs with self-origins
  Nr_ManAddOrigin(p->pNodeRetention, ci1_id, ci1_id);
  Nr_ManAddOrigin(p->pNodeRetention, ci2_id, ci2_id);

  int and_lit = Gia_ManAppendAnd(p, ci1_lit, ci2_lit);
  int and_id = Abc_Lit2Var(and_lit);
  Nr_ManAddOrigin(p->pNodeRetention, and_id, and_id);

  Gia_ManAppendCo(p, and_lit);

  // Rehash (creates a new GIA with retention propagated)
  Gia_Man_t* pNew = Gia_ManRehash(p, 0);

  // Check that the new GIA has retention entries
  ASSERT_NE(pNew->pNodeRetention, nullptr);
  EXPECT_GT(Nr_ManNumEntries(pNew->pNodeRetention), 0);

  // The new CI nodes should have origins pointing back to original CI IDs
  Gia_Obj_t* pObj;
  int i;
  Gia_ManForEachCi(pNew, pObj, i) {
    int newId = Gia_ObjId(pNew, pObj);
    Vec_Int_t* origins = Nr_ManGetOrigins(pNew->pNodeRetention, newId);
    ASSERT_NE(origins, nullptr);
    EXPECT_GT(Vec_IntSize(origins), 0);
  }

  Gia_ManStop(pNew);
  Gia_ManStop(p);
}

TEST(GiaRetentionTest, PrintRetentionMapGiaFormat) {
  // Build a simple GIA and write retention map
  Gia_Man_t* p = Gia_ManStart(100);
  p->pName = Abc_UtilStrsav((char*)"test");
  int ci1_lit = Gia_ManAppendCi(p);
  int ci2_lit = Gia_ManAppendCi(p);

  Nr_ManAddOrigin(p->pNodeRetention, Abc_Lit2Var(ci1_lit), Abc_Lit2Var(ci1_lit));
  Nr_ManAddOrigin(p->pNodeRetention, Abc_Lit2Var(ci2_lit), Abc_Lit2Var(ci2_lit));

  int and_lit = Gia_ManAppendAnd(p, ci1_lit, ci2_lit);
  Nr_ManAddOrigin(p->pNodeRetention, Abc_Lit2Var(and_lit), Abc_Lit2Var(ci1_lit));
  Nr_ManAddOrigin(p->pNodeRetention, Abc_Lit2Var(and_lit), Abc_Lit2Var(ci2_lit));

  Gia_ManAppendCo(p, and_lit);

  // Write to a temp file
  FILE* f = tmpfile();
  ASSERT_NE(f, nullptr);
  Nr_ManPrintRetentionMapGia(f, p, p->pNodeRetention);

  // Read back and verify format
  rewind(f);
  char buf[256];
  // First line: .gia_retention_begin
  ASSERT_NE(fgets(buf, sizeof(buf), f), nullptr);
  EXPECT_STREQ(buf, ".gia_retention_begin\n");

  // CI lines
  ASSERT_NE(fgets(buf, sizeof(buf), f), nullptr);
  EXPECT_TRUE(strncmp(buf, "CI ", 3) == 0);
  ASSERT_NE(fgets(buf, sizeof(buf), f), nullptr);
  EXPECT_TRUE(strncmp(buf, "CI ", 3) == 0);

  // AND node SRC line
  ASSERT_NE(fgets(buf, sizeof(buf), f), nullptr);
  EXPECT_TRUE(strstr(buf, " SRC ") != nullptr);

  fclose(f);
  Gia_ManStop(p);
}

ABC_NAMESPACE_IMPL_END
