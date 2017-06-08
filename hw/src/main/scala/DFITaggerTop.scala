
package rocketchip

import Chisel._
//import junctions._
import uncore._
import rocket._
//import rocket.Util._


case object TaggerEnable extends Field[Boolean]
case object TaggerWBEnable extends Field[Boolean]
case object TagCacheLines extends Field[Int]
case object EnableMetaTag extends Field[Boolean]
case object TagCacheReplPolicy extends Field[Int]
case object FPGA extends Field[Boolean]
abstract trait DFITParameters extends TileLinkParameters 
{
  val physAddrBits=32
  val physMemSizeInMB=512
  val tagEntryBits=512
  val validPhysAddrBits=log2Up(physMemSizeInMB)+20
  val enableDebugCounters = true
  val flagCacheLines = params(TagCacheLines)
  val missWithTVB = params(HasTagValidBitsInL1)
  val forceTaggerEnable = params(TaggerEnable)
  val forceTaggerWBEnable = params(TaggerWBEnable)
  val wordIdxBits = 3
  val entryIdxBits = log2Up(tagEntryBits) // 9
  val tableIdxBits = physAddrBits-entryIdxBits-wordIdxBits//32 - 9 - 3 = 20
  val rp_plru :: rp_random :: rp_fifo :: Nil = Enum(UInt(),3)
  val replacementPolicy =  UInt(params(TagCacheReplPolicy),width=2)
  val hasDFITagger = params(HasDFITagger)

  def addr_block_2_table_idx(addr_block: UInt): UInt = Cat(addr_block,UInt(0,width=6))(physAddrBits-1,physAddrBits-tableIdxBits) 
  //entry_addr_lower
  def addr_2_entry_addr_upper(addr: UInt): UInt = Cat(addr(entryIdxBits+wordIdxBits-1,wordIdxBits+1),UInt(1))
  def addr_2_entry_addr_lower(addr: UInt): UInt = Cat(addr(entryIdxBits+wordIdxBits-1,wordIdxBits+1),UInt(0))
  def table_idx_2_base_addr(idx: UInt): UInt = Cat(idx, Bits(0,width=32-tableIdxBits))
  def addr_block_2_base_addr(addr_block: UInt): UInt = Cat(addr_block, UInt(0,width=6)) 


  //MTT
  val mttEntryIdxBits = log2Up(512) // 9
  //metaTagTableIdxBits
  val mttIdxBits = tableIdxBits-mttEntryIdxBits // 20 - 9 = 11
  //metaTagDirIdxBits
  val mtdIdxBits = mttIdxBits//physAddrBits-metaTagTableIdxBits-tableIdxBits-entryIdxBits-wordIdxBits 12
  //val metaTagUnitInMB = 64*512*512/1024/1024 //=16
  //metaTagDirBits
  val mtdBits = scala.math.pow(2,mtdIdxBits).toInt//512//pow(//512/metaTagUnitInMB
  val enableMetaTag = params(EnableMetaTag)

  //addr_block_mtt_idx
  def addr_block_2_mtt_idx(addr_block: UInt): UInt = Cat(addr_block,UInt(0,width=6))(physAddrBits-1,physAddrBits-mttIdxBits)
  //addr_block_mtt_entry_idx
  def addr_block_2_mtt_entry_idx(addr_block: UInt): UInt = Cat(addr_block,UInt(0,width=6))(physAddrBits-mttIdxBits-1,physAddrBits-mttIdxBits-9)

}



abstract class DFITBundle extends Bundle 
with DFITParameters
with CoreParameters 

abstract class DFITModule extends Module 
with DFITParameters
with CoreParameters 



class TaggerConfigIO extends Bundle {
  val tagBase = UInt(OUTPUT)
  val tagEnable = Bool(OUTPUT)
}



class DFITaggerTop extends DFITModule
{
  val io = new Bundle {
    val inner = (new ClientUncachedTileLinkIO).flip
    val outer = new ClientUncachedTileLinkIO

    val scr = (new SCRIO).flip
  }

  println(f"\ttlClientXactIdBits in Tagger:\t$tlClientXactIdBits%x")
  println(f"\tentryIdxBits:\t$entryIdxBits%x")
  println(f"\ttableIdxBits:\t$tableIdxBits%x")


  //consts

  def iacq = io.inner.acquire
  def oacq = io.outer.acquire
  def ignt = io.inner.grant
  def ognt = io.outer.grant



/*
  control regitsters
  starts at 9
 */
  
  val control_2 = Reg(init=Bits(0x0000000000700000L,width=64))

  val control = Reg(init=Bits(0x0000100000400000L,width=64))
  //val control = Reg(init=Bits(0x2DF1100300400000L,width=64))

  val tagBase = control(31,0)
  val tagMask = Cat(Bits(0x3F,width=6),UInt(0,width=26))
  val tagEnable = control(32) || !Bool(params(FPGA))
  val tagWBEnable = control(33) || !Bool(params(FPGA))
  val device_id = Bits(0x2DF1,width=16)
  val resetDebugCounters = Bool(false)
  val mttBase = control_2(31,0)


  val control_rd = Cat(
  			device_id, //16 (63,48)
			UInt(0,width=1), //47
			Bool(params(HasL2Cache)), //46
			UInt(flagCacheLines,width=6), //8bits (45,40)
			Bool(enableDebugCounters), //(39)
			replacementPolicy, //(38,37)
			Bool(params(HasTagValidBitsInL1)), // (36)
			Bool(params(HasDFITagger)), //(35)
			Bool(enableMetaTag), //(34)
			control(33,0)
			)

  io.scr.rdata(32) := control_2
  io.scr.rdata(8) := control_rd
  when(io.scr.wen){
    when(io.scr.waddr === UInt(8)) {
      control := io.scr.wdata
    }
    when(io.scr.waddr === UInt(32)) {
      control_2 := io.scr.wdata
    }
  }


if(hasDFITagger){

/*
  iacq distribution 
*/

  val tagXactTracker = Module(new DFITaggedXactTracker)(params)
  tagXactTracker.io.config.tagBase := tagBase
  tagXactTracker.io.config.tagEnable := tagEnable
  tagXactTracker.io.tagWBEnable := tagWBEnable
  tagXactTracker.io.mttBase := mttBase
  def tc_iacq = tagXactTracker.io.inner.acquire
  def tc_oacq = tagXactTracker.io.outer.acquire

  val buf_iacq_bits = Reg(iacq.bits)
  val buf_iacq_valid = Reg(init=Bool(false))
  val buf_iacq_bypass = Reg(init=Bool(false))

  val oacq_arb = Module(new LockingArbiter(new Acquire, 2, tlDataBeats, (a: Acquire) => a.hasMultibeatData()))

  val buf_client_xact_id = Reg(iacq.bits.client_xact_id)
  val buf_handling = !tc_iacq.ready
  val fence = Reg(init=Bool(false))
 
  val bypass_iacq_val = 
    iacq.bits.full_addr() >= UInt(0x40000000) || //bypass mmio access
  Mux(Bool(missWithTVB), ((iacq.bits.a_type === Acquire.getBlockType) && !isTagged(iacq.bits.union(M_SZ,1))), Bool(false))  //bypass load without tags
  
  
  when(iacq.fire()) {
    buf_iacq_bits := iacq.bits
    when(tagEnable){
      buf_iacq_bypass := bypass_iacq_val 
    }.otherwise {
      buf_iacq_bypass := Bool(true)
    }

    when(iacq.bits.a_type === Acquire.putBlockType) {
      buf_client_xact_id := iacq.bits.client_xact_id
    }
    fence := Bool(false)
  }

  buf_iacq_valid := iacq.fire() | (buf_iacq_valid & (tc_oacq.valid | (buf_iacq_bypass & !oacq_arb.io.in(1).fire()) | (!buf_iacq_bypass & !tc_iacq.fire())))

  iacq.ready := (!buf_iacq_valid | (buf_iacq_bypass & oacq_arb.io.in(1).fire() ) | (!buf_iacq_bypass & tc_iacq.fire()))

  tc_iacq.bits := buf_iacq_bits
  tc_iacq.valid := (buf_iacq_valid & !buf_iacq_bypass)

  oacq_arb.io.in(0) <> tc_oacq
  oacq_arb.io.in(1).valid := buf_iacq_valid & buf_iacq_bypass & (!fence | !buf_handling)
  oacq_arb.io.in(1).bits := buf_iacq_bits
  oacq.bits := oacq_arb.io.out.bits
  oacq.valid := oacq_arb.io.out.valid
  oacq_arb.io.out.ready := oacq.ready
  /*
    bypass or forward to the tagcache the grants
  */

  val ignt_arb = Module(new LockingArbiter(new Grant, 2, tlDataBeats, (g: Grant) => g.hasMultibeatData()))(params)
  def tc_ignt = tagXactTracker.io.inner.grant
  def tc_ognt = tagXactTracker.io.outer.grant

  ignt <> ignt_arb.io.out
  ignt_arb.io.in(0) <> tc_ignt

  val wire_ognt_bypass = ognt.bits.client_xact_id(tlClientXactIdBits-1) != UInt(1)
  ignt_arb.io.in(1).bits := ognt.bits
  ignt_arb.io.in(1).valid := ognt.valid & wire_ognt_bypass


  tc_ognt.bits := ognt.bits
  tc_ognt.valid := ognt.valid & !wire_ognt_bypass


  ognt.ready := (ignt_arb.io.in(1).ready & wire_ognt_bypass) | (tc_ognt.ready & !wire_ognt_bypass)

/*
  debug prints
*/


when(iacq.fire()) {
    printf("hgmoon-debug:\tincoming traffic:\n");
    printf("hgmoon-debug:\tDFITagger: client_xact_id:\t%x\n",iacq.bits.client_xact_id)
    printf("hgmoon-debug:\tDFITagger: addr_block:\t%x\n",iacq.bits.addr_block)
    printf("hgmoon-debug:\tDFITagger: full_addr:\t%x\n",iacq.bits.full_addr())
    printf("hgmoon-debug:\tDFITagger: addr_beat:\t%x\n",iacq.bits.addr_beat)
    printf("hgmoon-debug:\tDFITagger: data:\t%x\n",iacq.bits.data)
    printf("hgmoon-debug:\tDFITagger: a_type:\t%x\n",iacq.bits.a_type)
    printf("hgmoon-debug:\tDFITagger: union:\t%x\n",iacq.bits.union)

  }


when(oacq.fire()) {

    printf("hgmoon-debug:\tleaving traffic:\n");
    printf("hgmoon-debug:\tDFITagger: client_xact_id:\t%x\n",oacq.bits.client_xact_id)
    printf("hgmoon-debug:\tDFITagger: addr_block:\t%x\n",oacq.bits.addr_block)
    printf("hgmoon-debug:\tDFITagger: full_addr:\t%x\n",oacq.bits.full_addr())
    printf("hgmoon-debug:\tDFITagger: addr_beat:\t%x\n",oacq.bits.addr_beat)
    printf("hgmoon-debug:\tDFITagger: data:\t%x\n",oacq.bits.data)
    printf("hgmoon-debug:\tDFITagger: a_type:\t%x\n",oacq.bits.a_type)
    printf("hgmoon-debug:\tDFITagger: union:\t%x\n",oacq.bits.union)
  }
/*
  debug prints for acq
*/
/*
  val bypassed = Reg(init=UInt(0,width=32))
  val num_iacq = Reg(init=UInt(0,width=32))
  val num_tc_iacq = Reg(init=UInt(0,width=32))
  val num_tc_oacq = Reg(init=UInt(0,width=32))
  val num_oacq = Reg(init=UInt(0,width=32))
when(Bool(true)) {
  when(iacq.fire()) {
    num_iacq := num_iacq + UInt(1)
  }
  when(buf_iacq_bypass === Bool(true) & oacq.fire() & !tc_oacq.valid) {
    bypassed := bypassed + UInt(1)
  }

  when(tc_iacq.fire()) {
    num_tc_iacq := num_tc_iacq + UInt(1)
  }
  
  when(tc_oacq.fire()) {
    num_tc_oacq := num_tc_oacq + UInt(1)
  }
  when(oacq.fire()){
    num_oacq := num_oacq + UInt(1)
    printf("bypassed:\t%d\n",bypassed)
    printf("num_iacq:\t%d\n",num_iacq)
    printf("num_tc_iacq:\t%d\n",num_tc_iacq)
    printf("num_tc_oacq:\t%d\n",num_tc_oacq)
    printf("num_oacq:\t%d\n",num_oacq)
  }
}
*/
/*
  when(ognt.fire()){
    printf("hgmoon-debug:\tognt.fire() in Top\n")

    printf("hgmoon-debug:\tognt.g_type:\t%x\n",ognt.bits.g_type)

    printf("hgmoon-debug:\tognt.addr_beat:\t%x\n",ognt.bits.addr_beat)
    printf("hgmoon-debug:\tognt.data:\t%x\n",ognt.bits.data)
    printf("hgmoon-debug:\tognt.client_xact_id:\t%x\n",ognt.bits.client_xact_id)
    printf("hgmoon-debug:\tognt.manager_xact_id:\t%x\n",ognt.bits.manager_xact_id)
  }

  when(ignt.fire()){
    printf("hgmoon-debug:\tignt.fire() in Top\n")

    printf("hgmoon-debug:\tignt.g_type:\t%x\n",ignt.bits.g_type)

    printf("hgmoon-debug:\tignt.addr_beat:\t%x\n",ignt.bits.addr_beat)
    printf("hgmoon-debug:\tignt.data:\t%x\n",ignt.bits.data)
    printf("hgmoon-debug:\tignt.client_xact_id:\t%x\n",ignt.bits.client_xact_id)
    printf("hgmoon-debug:\tignt.manager_xact_id:\t%x\n",ignt.bits.manager_xact_id)
  }

*/


    io.scr.rdata(13) := tagXactTracker.io.count_tag_cache_read_access
    io.scr.rdata(14) := tagXactTracker.io.count_tag_cache_read_hit
    io.scr.rdata(15) := tagXactTracker.io.count_tag_cache_write_access
    io.scr.rdata(16) := tagXactTracker.io.count_tag_cache_write_hit
    io.scr.rdata(17) := tagXactTracker.io.count_tag_entry_evicted
    io.scr.rdata(18) := tagXactTracker.io.count_mtt_write_skip
    io.scr.rdata(19) := tagXactTracker.io.count_mtt_read_skip
    


}else{
  iacq <> oacq
  ignt <> ognt

}


/*
  debug counters
*/
 
  if(enableDebugCounters) {

    val count_read_miss = Reg(init=UInt(0,width=64))
    val count_write_miss = Reg(init=UInt(0,width=64))
    val count_read_miss_tagged = Reg(init=UInt(0,width=64)) //cmd = XRD_T
    val count_write_miss_tagged = Reg(init=UInt(0,width=64)) //contains a tagged dword
    val count_mem_read = Reg(init=UInt(0,width=64))
    val count_mem_write = Reg(init=UInt(0,width=64))


    io.scr.rdata(9) := count_read_miss
    io.scr.rdata(10) := count_write_miss
    io.scr.rdata(11) := count_read_miss_tagged
    io.scr.rdata(12) := count_write_miss_tagged
    io.scr.rdata(20) := count_mem_read
    io.scr.rdata(21) := count_mem_write

    when(oacq.fire()){
      when(oacq.bits.a_type === Acquire.putBlockType){
        count_mem_write := count_mem_write + UInt(1)
      }
      when(oacq.bits.a_type === Acquire.getBlockType){
        count_mem_read := count_mem_read + UInt(1)
      }
    }


/*
    io.scr.rdata(16) := count_read_miss
    io.scr.rdata(17) := count_write_miss
    io.scr.rdata(24) := count_read_miss_tagged
    io.scr.rdata(25) := count_write_miss_tagged
*/

    


    val recognized_tag = Reg(init=Bool(false))

    when(resetDebugCounters === UInt(1)){
      count_read_miss := UInt(0)
      count_write_miss := UInt(0)
      count_read_miss_tagged := UInt(0)
      count_write_miss_tagged := UInt(0)
    }.elsewhen(iacq.fire()){
      when(iacq.bits.a_type === Acquire.getBlockType) {
        count_read_miss := count_read_miss + UInt(1)
	when(isTagged(iacq.bits.union(M_SZ,1))){
	  count_read_miss_tagged := count_read_miss_tagged + UInt(1)
	}
      }
      when(iacq.bits.a_type === Acquire.putBlockType) {
        when(iacq.bits.addr_beat === UInt(0)){
          count_write_miss := count_write_miss + UInt(1)
	}
	when(iacq.bits.addr_beat != UInt(3)){
  	  when(iacq.bits.data(129) | iacq.bits.data(128)){
	    when(!recognized_tag){
	      count_write_miss_tagged := count_write_miss_tagged + UInt(1)
	    }
	    recognized_tag := Bool(true)
	  }
	}.otherwise{
          when(iacq.bits.data(129) | iacq.bits.data(128)){
	    when(!recognized_tag){
	      count_write_miss_tagged := count_write_miss_tagged + UInt(1)
	    }
	  }
	  recognized_tag := Bool(false)
	}
      }
    }
/*
    printf("count_read_miss:\t%d\n",count_read_miss)
    printf("count_read_miss_tagged:\t%d\n",count_read_miss_tagged)
    printf("count_write_miss:\t%d\n",count_write_miss)
    printf("count_write_miss_tagged:\t%d\n",count_write_miss_tagged)
*/
  }



} // DFITaggerTop

