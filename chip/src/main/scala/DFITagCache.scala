
package rocketchip

import Chisel._
//import junctions._
import uncore._
import rocket._
//import rocket.Util._

class DFITagCache extends DFITModule
{

  val io = new Bundle {
    val req = Decoupled(
      new Bundle{
        val addr_block = UInt(OUTPUT)
        val write = Bool(OUTPUT)
        val entry = UInt(OUTPUT,width=512)
	val mtt_entry = UInt(OUTPUT)
	val mtt_valid = Bool(OUTPUT)
      }
    ).flip
    val resp = Decoupled(
      new Bundle{
        val matched = Bool(OUTPUT)
        val entry = UInt(OUTPUT,width=512)
	val mtt_matched = Bool(OUTPUT)
        val mtt_entry = UInt(OUTPUT,width=512)
        val evict_valid = Bool(OUTPUT)
        val evict_data = UInt(OUTPUT,width=512)
        val evict_table_idx = UInt(OUTPUT)
	val evict_mtt = UInt(OUTPUT)
	val evict_mtt_idx = UInt(OUTPUT)
	val evict_mtt_valid = Bool(OUTPUT)
      }
    )
    val tagBase = UInt(INPUT)
  }

  val tagBase = io.tagBase
  /*
    plain table
    one beat = 128bits = 16 bytes = 2^4 bytes
    addr_block_base = tagBase(31,5)
    addr_block_tag = (addr(31,9) = addr_block(26,5) + addr_block_base
    addr(31,0)
    addr(2,0): in-word = tlByteAddrBits
    addr(10,3): in-entry idx = tagEntryAddrBitss
    addr(31,11) = addr_block(25,5): table idx
  def tableIdxBits = 21
    table idx: 21bits

  */
  def tag_base_addr_block = tagBase(31,6)

  //def addr_block_tag(addr_block: UInt): UInt = addr_block_2_tabl_eidx(addr_block) + tag_base_addr_block
  //def addr_idx_tag(idx: UInt): UInt = idx + tag_base_addr_block

  //def addr_block_tag(addr_block: UInt): UInt = tag_base_addr_block




  println(f"tlBlockAddrBits:\t$tlBlockAddrBits%d\n")
  println(f"tableIdxBits:\t$tableIdxBits%d\n")



  val s_ready :: s_match :: s_fetch :: s_wb :: s_wb_done :: Nil = Enum(UInt(),5)
  val s = Reg(init=s_ready)

  val addr_block = io.req.bits.addr_block//Reg(io.req.bits.addr_block)
  val addr_block_reg = Reg(io.req.bits.addr_block)
  val entry = Reg(io.req.bits.entry)
  val mtt_entry = Reg(io.req.bits.mtt_entry)
  val write = Reg(io.req.bits.write)
  val write_mtt = Reg(Bool())
  when(s === s_ready){
    when(io.req.fire()){
      addr_block_reg := io.req.bits.addr_block
      entry := io.req.bits.entry
      mtt_entry := io.req.bits.mtt_entry
      write := io.req.bits.write
      write_mtt := io.req.bits.write && io.req.bits.mtt_valid
      s := s_match
    }
  }
  when(s === s_match){
      when(!write){
        s := s_fetch
      }.otherwise{
        s := s_wb
      }
  }
  when(s === s_fetch){
    when(io.resp.fire()){
      s := s_ready
    }
  }
  when( s === s_wb ) {

      s := s_wb_done

  }

  when( s === s_wb_done ){
    when(io.resp.fire()){
      s := s_ready
    }
  }


 
  val flagDataArray = Mem((Bits(width=512)),flagCacheLines,false)
  val flagMetaDataArray = Reg(init=Vec.fill(flagCacheLines)(Bits(0,width=(tableIdxBits + 1))))
  val metaTagDataArray = Mem((Bits(width=512)),flagCacheLines,false)
  val metaTagMetaDataArray = Reg(init=Vec.fill(flagCacheLines)(Bits(0,width=(mttIdxBits + 1))))



//  val tagMatchIdx = Reg(Bits())


  val tagMatchIdx = Reg(Bits())   
  val tagMatchValid = Reg(Bool())
  val tagMatchEntry = Reg(Bits())// flagDataArray(tagMatchIdx)
  val metaTagMatchEntry = Reg(Bits())
  val metaTagMatchValid = Reg(Bool())
  val metaTagMatchIdx = Reg(Bits())
  when(s === s_ready && io.req.fire()) {
   tagMatchIdx := flagMetaDataArray.indexWhere((idx: Bits) => idx(tableIdxBits-1,0) === addr_block_2_table_idx(addr_block) && idx(tableIdxBits) === Bits(1) ) 
   tagMatchValid := flagMetaDataArray.exists((idx: Bits) => idx(tableIdxBits-1,0) === addr_block_2_table_idx(addr_block) && idx(tableIdxBits) === Bits(1) )
   metaTagMatchIdx := metaTagMetaDataArray.indexWhere((idx: Bits) => idx(mttIdxBits-1,0) === addr_block_2_mtt_idx(addr_block) && idx(mttIdxBits) === Bits(1) ) 
   metaTagMatchValid := metaTagMetaDataArray.exists((idx: Bits) => idx(mttIdxBits-1,0) === addr_block_2_mtt_idx(addr_block) && idx(mttIdxBits) === Bits(1) )


    //tagMatchValid := Bool(true)
  }
  when(s === s_match){
     tagMatchEntry := flagDataArray(tagMatchIdx)
     metaTagMatchEntry := metaTagDataArray(metaTagMatchIdx)
  }

  io.resp.bits.matched := tagMatchValid
  io.resp.bits.entry := tagMatchEntry
  io.resp.bits.mtt_matched := metaTagMatchValid
  io.resp.bits.mtt_entry := metaTagMatchEntry
/*
  when(s === s_fetch){
    printf("\ttagMatchValid:\t%x\n",tagMatchValid)
    printf("\ttagMatchIdx:\t%x\n",tagMatchIdx)
    printf("\ttagMatchEntry\t%x\n",tagMatchEntry)
  }
*/

  val flagRefillIdx = Reg(init=UInt(0,width=log2Up(flagCacheLines)))
  val mttRefillIdx = Reg(init=UInt(0,width=log2Up(flagCacheLines)))
  //val flagRefillIdx = UInt(0)

  val updateFlagRefillIdx = s === s_wb_done && io.resp.fire() && io.resp.bits.evict_valid // when evictValid becomes true
  val updateMTTRefillIdx = s === s_wb_done && io.resp.fire() && io.resp.bits.evict_valid // when evictValid becomes true



  val plru = new PseudoLRU(flagCacheLines)
  val mtt_plru = new PseudoLRU(flagCacheLines)

  val update_lfsr = updateFlagRefillIdx

  val lfsr = LFSR16(update_lfsr)
  val update_mtt_lfsr = updateMTTRefillIdx
  val mtt_lfsr = LFSR16(update_mtt_lfsr)

  val flagRefillIdxFIFO = Reg(init=UInt(0,width = log2Up(flagCacheLines)))

  when(updateFlagRefillIdx){
    when(flagRefillIdxFIFO != UInt(flagCacheLines-1)) {
      flagRefillIdxFIFO := flagRefillIdxFIFO + UInt(1)
    }.otherwise{
      flagRefillIdxFIFO :=UInt(0)
    }
  }

  val mttRefillIdxFIFO = Reg(init=UInt(0,width = log2Up(flagCacheLines)))

  when(updateMTTRefillIdx){
    when(mttRefillIdxFIFO != UInt(flagCacheLines-1)) {
      mttRefillIdxFIFO := mttRefillIdxFIFO + UInt(1)
    }.otherwise{
      mttRefillIdxFIFO :=UInt(0)
    }
  }



  when(s === s_fetch){
    when(tagMatchValid){
      plru.access(tagMatchIdx)
    }
    when(metaTagMatchValid){
      mtt_plru.access(metaTagMatchIdx)
    }
  }


  val tagCacheFull = Reg(init=Bool(false))
  val mttCacheFull = Reg(init=Bool(false))
  when(flagMetaDataArray.forall((idx:Bits) => idx(tableIdxBits) === Bits(1)) ){
    tagCacheFull := Bool(true)
  }
  when(metaTagMetaDataArray.forall((idx:Bits) => idx(mttIdxBits) === Bits(1)) ){
    mttCacheFull := Bool(true)
  }


  when(updateFlagRefillIdx){
    //flagRefillIdx := plru.replace
    printf("flagRefillIdx:\t%x\n",flagRefillIdx)
    flagRefillIdx := Mux(!tagCacheFull, flagRefillIdxFIFO,
        Mux( replacementPolicy === rp_random, lfsr(log2Up(flagCacheLines)-1,0)  ,
	Mux( replacementPolicy === rp_plru, plru.replace, flagRefillIdxFIFO))
	)
    mttRefillIdx := Mux(!mttCacheFull, mttRefillIdxFIFO,
        Mux( replacementPolicy === rp_random, mtt_lfsr(log2Up(flagCacheLines)-1,0)  ,
	Mux( replacementPolicy === rp_plru, mtt_plru.replace, mttRefillIdxFIFO))
	)
  }


 
  val evictData = Reg(Bits())
  val evictTableIdx = Reg(Bits())
  val evictValid = Reg(init=Bool(false)) 

  val evictMTT = Reg(Bits())
  val evictMTTIdx = Reg(Bits())
  val evictMTTValid = Reg(Bool())

/*
  for( i <- 0 until 15){

    printf("flagDataArray(%x):\t%x\n",UInt(i),flagDataArray(UInt(i)))
    printf("flagMetaDataArray(%x):\t%x\n",UInt(i),flagMetaDataArray(UInt(i)))
  }
*/

  when(s === s_wb) {
  
    printf("wb in tagCache\n")
    printf("\ttagMatchValid:\t%x\n",tagMatchValid)
    printf("\tentry:\t%x\n",entry)
    printf("\ttagMatchIdx:\t%x\n",tagMatchIdx)
    printf("\tflagRefillIdx:\t%x\n",flagRefillIdx)
    printf("\ttableIdx:\t%x\n",addr_block_2_table_idx(addr_block_reg))

    printf("\tmetaTagMatchValid:\t%x\n",metaTagMatchValid)
    printf("\tmtt_entry:\t%x\n",mtt_entry)
    printf("\tmetaTagMatchIdx:\t%x\n",metaTagMatchIdx)
    printf("\tmetaTagRefillIdx:\t%x\n",mttRefillIdx)


    
    
    when(tagMatchValid){
      flagDataArray(tagMatchIdx) := entry
      flagMetaDataArray(tagMatchIdx) := Cat(UInt(1),addr_block_2_table_idx(addr_block_reg))
    }.otherwise{
      when(flagMetaDataArray(flagRefillIdx)(tableIdxBits) === UInt(1)){
        printf("need to evict\t%x\n",flagDataArray(flagRefillIdx))
        printf("with tableIdx\t%x\n",flagMetaDataArray(flagRefillIdx)(tableIdxBits-1,0))
      
        evictData := flagDataArray(flagRefillIdx)
        evictTableIdx := flagMetaDataArray(flagRefillIdx)(tableIdxBits-1,0)
        evictValid := Bool(true)
      }
      flagDataArray(flagRefillIdx) := entry
      flagMetaDataArray(flagRefillIdx) := Cat(UInt(1),addr_block_2_table_idx(addr_block_reg))
    }
    when(metaTagMatchValid && write_mtt){
      metaTagDataArray(metaTagMatchIdx) := mtt_entry
      metaTagMetaDataArray(metaTagMatchIdx) := Cat(UInt(1),addr_block_2_mtt_idx(addr_block_reg))
    }.otherwise{
      when(metaTagMetaDataArray(mttRefillIdx)(mttIdxBits) === UInt(1)){
      printf("need to evict\t%x\n",metaTagDataArray(mttRefillIdx))
      printf("with mttIdx\t%x\n",metaTagMetaDataArray(mttRefillIdx)(mttIdxBits-1,0))
      
        evictMTT := metaTagDataArray(mttRefillIdx)
        evictMTTIdx := metaTagMetaDataArray(mttRefillIdx)(mttIdxBits-1,0)
        evictMTTValid := Bool(true)
      }
      metaTagDataArray(mttRefillIdx) := mtt_entry
      metaTagMetaDataArray(mttRefillIdx) := Cat(UInt(1),addr_block_2_mtt_idx(addr_block_reg))
    }


    /*
    printf("\tflagCacheRefill!\n")
    printf("\tflagRefillIdx:\t%x\n",flagRefillIdx)
    printf("\ttableIdx:\t%x\n",addr_block_idx(addr_block))
*/
  }.elsewhen( s === s_ready) {
    evictValid := Bool(false)
    evictMTTValid := Bool(false)
  }

  io.resp.bits.evict_valid := evictValid
  io.resp.bits.evict_data := evictData
  io.resp.bits.evict_table_idx := evictTableIdx

  io.resp.bits.evict_mtt_valid := evictMTTValid
  io.resp.bits.evict_mtt := evictMTT
  io.resp.bits.evict_mtt_idx := evictMTTIdx

  


  io.resp.valid := s === s_wb_done || s === s_fetch
  io.req.ready := s === s_ready
 

}
