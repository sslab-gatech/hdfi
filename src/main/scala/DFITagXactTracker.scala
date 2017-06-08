
package rocketchip

import Chisel._
//import junctions._
import uncore._
import rocket._
//import rocket.Util._

class DFITaggedXactTracker extends DFITModule
{
  val io = new Bundle {
    val inner = (new ClientUncachedTileLinkIO).flip
    val outer = new ClientUncachedTileLinkIO

    val config = (new TaggerConfigIO).flip
    val tagWBEnable = Bool(INPUT)

    val count_tag_cache_read_access = UInt(OUTPUT)
    val count_tag_cache_read_hit = UInt(OUTPUT)
    val count_tag_cache_write_access = UInt(OUTPUT)
    val count_tag_cache_write_hit = UInt(OUTPUT)
    val count_tag_entry_evicted = UInt(OUTPUT)
    val count_mtt_write_skip = UInt(OUTPUT)
    val count_mtt_read_skip = UInt(OUTPUT)
    
    val mttBase = UInt(INPUT)
  }
  /*
    defs to make the code shorter
  */
  def iacq = io.inner.acquire
  def oacq = io.outer.acquire
  def ignt = io.inner.grant
  def ognt = io.outer.grant




  val tagBase = io.config.tagBase
  /*
    plain table
    one beat = 128bits = 16 bytes = 2^4 bytes
    addr_block_base = tagBase(31,5)
    addr_block_tag = (addr(31,9) = addr_block(26,5) + addr_block_base
    addr(31,0)
    addr(2,0): in-word = tlByteAddrBits
    addr(10,3): in-entry idx = tagEntryAddrBitss
    addr(31,11) = addr_block(25,5): table idx
    table idx: 21bits

  */

  def tag_base_addr_block = tagBase(31,6)


  //def addr_block_tag(addr_block: UInt): UInt = addr_block_2_table_idx(addr_block) + tag_base_addr_block
  //def addr_block_mtt(addr_block:UInt): UInt = addr_block_2_mtt_idx(addr_block)
  //def addr_idx_tag(idx: UInt): UInt = idx + tag_base_addr_block

  def addr_block_base(addr_block: UInt): UInt = Cat(addr_block, UInt(0,width=6))
  
/*
  Input handling
*/

  val 	sh_ready ::		sh_check_cache_pre ::	sh_check_cache ::	sh_fetch_valid ::	sh_fetch_wait :: 	sh_putBlock :: 		sh_getBlock :: 		sh_getBlock_valid :: 	sh_getBlock_wait :: 	sh_wb_cache_only ::  	sh_wb_valid :: 		sh_wb_wait :: 	sh_done :: 		sh_getBlock_early_valid :: sh_meta_tag_match :: sh_wb_cache_valid :: 	sh_wait_input :: 	sh_fetch_mtt_valid :: 	sh_fetch_mtt_wait :: 	sh_wb_mtt_wait :: 	sh_wb_mtt_valid :: Nil = Enum(UInt(),21)
  val state_handle = Reg(init=sh_ready)
  val sin_ready :: sin_putblock_busy ::  sin_putblock_done :: sin_single_done :: Nil = Enum(UInt(), 4)
  val state_in = Reg(init=sin_ready)

  val mode_putBlock :: mode_getBlock :: Nil = Enum(UInt(),2)
  val handle_mode = Reg(init=mode_putBlock)


  val q_iacq_bits = Module(new Queue(iacq.bits,tlDataBeats))(params)
  q_iacq_bits.io.enq.bits := iacq.bits
  q_iacq_bits.io.enq.valid := (iacq.valid & ((state_in === sin_ready & iacq.bits.a_type === Acquire.putBlockType) | state_in === sin_putblock_busy))

  val reg_iacq_bits = Reg(iacq.bits)

  val cnt_iacq = Reg(UInt(width=tlBeatAddrBits))

  val q_iacq_bypass = Module(new Queue(iacq.bits,tlDataBeats))(params)
  q_iacq_bypass.io.enq.bits := iacq.bits
  q_iacq_bypass.io.enq.valid := (iacq.valid & ((state_in === sin_ready & iacq.bits.a_type === Acquire.putBlockType) | state_in === sin_putblock_busy))

  val client_xact_id = Reg(iacq.bits.client_xact_id)
  val addr_block = Reg(iacq.bits.addr_block)
  val full_addr = Reg(iacq.bits.full_addr()) 


  val metaTagDir = Reg(init=Bits(0,width=mtdBits))
  val metaTagDirIdx = addr_block_2_mtt_idx(addr_block)
  //printf("mtd:\t%x\n",metaTagDir)

  val isTagged = Reg(init=Bool(false))

  when(state_in === sin_ready) {
    when(iacq.fire()){
      client_xact_id := iacq.bits.client_xact_id
      addr_block := iacq.bits.addr_block
      full_addr := iacq.bits.full_addr()
      when( iacq.bits.a_type === Acquire.putBlockType) {
        isTagged := iacq.bits.data(129) | iacq.bits.data(128) 
        state_in := sin_putblock_busy
	cnt_iacq := UInt(1)
      }.otherwise{
	reg_iacq_bits := iacq.bits
        state_in := sin_single_done
      }
    }
  }.elsewhen(state_in === sin_putblock_busy) {
    when(iacq.fire()) {
      isTagged := iacq.bits.data(129) | iacq.bits.data(128) | isTagged 
      cnt_iacq := cnt_iacq + UInt(1)
      when(cnt_iacq === UInt(3)) {
        state_in := sin_putblock_done

      }
    }
  }.elsewhen(state_in === sin_single_done || state_in === sin_putblock_done ) {
    when(state_handle === sh_done && !q_iacq_bypass.io.deq.valid) {
      cnt_iacq := UInt(0)
      isTagged := Bool(false)
      state_in := sin_ready
    }
  }

/*
   A
  A A
 A   A
   A        Input Handling
   A
   A
*/

  val matched_entry_plain = Reg(Bits(width=512))
  val matched_entry_valid = Reg(init=Bool(false))
  val matched_meta_tag_entry = Reg(Bits())
  val matched_meta_tag_entry_valid = Reg(init=Bool(false))
  val reg_bits_handling = Reg(iacq.bits)
  val reg_bits_handling_valid = Reg(init=Bool(false))
  q_iacq_bits.io.deq.ready := (!reg_bits_handling_valid & state_handle === sh_putBlock) || (state_handle === sh_meta_tag_match)
  val cnt_handled = Reg(UInt(width=4))


  println(f"tlBlockAddrBits:\t$tlBlockAddrBits%d\n")
  println(f"tableIdxBits:\t$tableIdxBits%d\n")

  val fetcher = Module(new DFITagFetcher)(params)
  fetcher.io.req.bits.client_xact_id := UInt(0,width=tlClientXactIdBits-1)//client_xact_id
  fetcher.io.req.bits.addr := addr_block_2_table_idx(addr_block) + tagBase(31,6)
  fetcher.io.req.valid := state_handle === sh_fetch_valid
  fetcher.io.resp.ready := state_handle === sh_fetch_wait

  when(fetcher.io.req.fire()){
    printf("fetched invokded with:\t%x\n",addr_block_2_table_idx(addr_block))
    printf("tagBase(31,6):\t%x\n",tagBase(31,6))
    printf("sum:\t%x\n",tagBase(31,6) + addr_block_2_table_idx(addr_block))
  }

  val mtt_fetcher = Module(new DFITagFetcher)(params)
  mtt_fetcher.io.req.bits.client_xact_id := UInt(2,width=tlClientXactIdBits-1)//client_xact_id
  mtt_fetcher.io.req.bits.addr := addr_block_2_mtt_idx(addr_block) + io.mttBase(31,6)
  mtt_fetcher.io.req.valid := state_handle === sh_fetch_mtt_valid
  mtt_fetcher.io.resp.ready := state_handle === sh_fetch_mtt_wait


  val evictDataSep = Reg(Bits(width=512))
  val evictTableIdxSep = Reg(Bits(width= tableIdxBits))
  val evictValid = Reg(Bool())
  val evictMTT = Reg(Bits(width=512))
  val evictMTTIdx = Reg(Bits())
  val evictMTTValid = Reg(Bool())

  val writer = Module(new DFITagWriter)(params)
  writer.io.req.bits.client_xact_id := UInt(0,width=tlClientXactIdBits-1)//client_xact_id
  writer.io.req.bits.addr := evictTableIdxSep + tagBase(31,6)//addr_idx_tag(evictTableIdxSep)// addr_block_tag(addr_block)
  writer.io.req.bits.entry := evictDataSep //matched_entry_plain
  writer.io.req.valid := state_handle === sh_wb_valid
  writer.io.resp.ready := state_handle === sh_wb_wait

  when(writer.io.req.fire()){
    printf("writer invokded with:\t%x\n",evictTableIdxSep)
    printf("tagBase(31,6):\t%x\n",tagBase(31,6))
    printf("sum:\t%x\n",tagBase(31,6) + evictTableIdxSep)
  }



  val mtt_writer = Module(new DFITagWriter)(params)
  mtt_writer.io.req.bits.client_xact_id := UInt(2,width=tlClientXactIdBits-1)//client_xact_id
  mtt_writer.io.req.bits.addr := evictMTTIdx + io.mttBase(31,6)// addr_block_tag(addr_block)
  mtt_writer.io.req.bits.entry := evictMTT //matched_entry_plain
  mtt_writer.io.req.valid := state_handle === sh_wb_mtt_valid
  mtt_writer.io.resp.ready := state_handle === sh_wb_mtt_wait




  val tag_base_for_granter = Cat(addr_block(4,0),UInt(7,width=3))
  val granter = Module(new DFIGranter)(params)
  granter.io.req.bits.client_xact_id :=  client_xact_id
  granter.io.req.bits.addr_block := addr_block
  //want to fix this to pass necessary bits only...
  granter.io.req_tag.bits.tags := matched_entry_plain
  granter.io.req.valid := state_handle === sh_getBlock_early_valid
  granter.io.req_tag.valid := state_handle === sh_getBlock_valid
  granter.io.resp.ready := state_handle === sh_getBlock_wait

  /*
    Separated TagCache
   */

  val tagCache = Module(new DFITagCache)(params)
  tagCache.io.tagBase := tagBase
  tagCache.io.req.bits.addr_block := addr_block
  tagCache.io.req.bits.write := state_handle === sh_wb_cache_valid
  tagCache.io.req.bits.entry := matched_entry_plain
  tagCache.io.req.bits.mtt_entry := matched_meta_tag_entry
  tagCache.io.req.bits.mtt_valid := matched_meta_tag_entry_valid
  tagCache.io.req.valid := state_handle === sh_check_cache_pre || state_handle === sh_wb_cache_valid
  tagCache.io.resp.ready := state_handle === sh_check_cache || state_handle === sh_wb_cache_only




/*

  main FSM begins here
  
*/


  val needMTTFetched = Reg(init=Bool(false))
  val needTagEntryFetched = Reg(init=Bool(false))

  when(state_handle === sh_ready) {

    when(state_in === sin_putblock_done) { 
    //when(state_in === sin_putblock_busy) { 
      //state_handle := sh_fetch_valid
      state_handle := sh_check_cache_pre
      cnt_handled := UInt(0)
      handle_mode := mode_putBlock
    }
    when(state_in === sin_single_done) {
      handle_mode := mode_getBlock
      state_handle := sh_getBlock_early_valid
      //state_handle := sh_check_cache_pre
      //state_handle := sh_fetch_valid
      cnt_handled := UInt(0)
    }
  }.elsewhen(state_handle === sh_getBlock_early_valid) {
    when(granter.io.req.fire()) {

      //state_handle := sh_getBlock_valid
      state_handle := sh_check_cache_pre
    }
  }.elsewhen(state_handle === sh_check_cache_pre) {
    when(tagCache.io.req.fire()){
      state_handle := sh_check_cache

      needTagEntryFetched := Bool(true)
    }
  }.elsewhen(state_handle === sh_check_cache) {
    
    when(tagCache.io.resp.fire()){
      when(tagCache.io.resp.bits.matched){ //temp.
        matched_entry_plain := tagCache.io.resp.bits.entry
	matched_entry_valid := Bool(true) 
	when(tagCache.io.resp.bits.mtt_matched || !Bool(enableMetaTag)){
          matched_meta_tag_entry := tagCache.io.resp.bits.mtt_entry
	  matched_meta_tag_entry_valid := Bool(true)
          when(handle_mode === mode_putBlock){
            state_handle := sh_putBlock
          }.otherwise{ //getBlock
            state_handle := sh_getBlock_valid
            //state_handle := sh_getBlock_early_valid
          }
	  /*
	  printf("tagCache.io.resp.fire() in sh_check_cache\n")
	  printf("matched entry:\t%x\n",tagCache.io.resp.bits.entry)
	  */

	}.otherwise{
	  printf("touched!!!\n")
	  matched_meta_tag_entry_valid := Bool(false)
	  needTagEntryFetched := Bool(false)
	  state_handle := sh_fetch_mtt_valid
	  needMTTFetched := Bool(true)
	}
        
      }.otherwise{ //need to fetch..
        //state_handle := sh_fetch_valid
        when(!Bool(enableMetaTag)){
	    state_handle := sh_fetch_valid
	}.otherwise{
	  when( metaTagDir(addr_block_2_mtt_idx(addr_block)) === UInt(0) /*Bool(false) */){ // all the entry bits should be 0
		  printf("matched mtt\n")
		  printf("mtd:\t%x\n",metaTagDir(addr_block_2_mtt_idx(addr_block)))
		  printf("mtte valid:\t%x\n",tagCache.io.resp.bits.mtt_matched)
		  printf("mtte:\t%x\n",tagCache.io.resp.bits.mtt_entry)
		  matched_entry_plain := UInt(0,width=512)
		  matched_entry_valid := Bool(true)
		  matched_meta_tag_entry := UInt(0,width=512)
		  matched_meta_tag_entry_valid := Bool(true)
		  when(handle_mode === mode_getBlock){
		    state_handle := sh_getBlock_valid
		  }.otherwise{
		    state_handle := sh_wait_input //no need to do anything...
		  }
          }.elsewhen( 	//Bool(false)
	  		tagCache.io.resp.bits.mtt_matched && 
	    		tagCache.io.resp.bits.mtt_entry(addr_block_2_mtt_entry_idx(addr_block)) === UInt(0) 
			){
		  printf("mtte fetched:\t%x\n",tagCache.io.resp.bits.mtt_entry)
		  matched_entry_plain := UInt(0,width=512)
		  matched_entry_valid := Bool(true)
		  matched_meta_tag_entry := tagCache.io.resp.bits.mtt_entry
		  matched_meta_tag_entry_valid := Bool(true)
		  when(handle_mode === mode_getBlock){
		    state_handle := sh_getBlock_valid
		  }.otherwise{
		    state_handle := sh_wait_input //no need to do anything...
		  }

	  }.otherwise{ //need the entries
	    when(tagCache.io.resp.bits.mtt_matched){
	      matched_meta_tag_entry := tagCache.io.resp.bits.mtt_entry
 	      matched_meta_tag_entry_valid := Bool(true)
	      state_handle := sh_fetch_valid
	      needMTTFetched := Bool(false)
	    }.otherwise{
	      state_handle := sh_fetch_mtt_valid
	      needMTTFetched := Bool(true)
	    }
	  }
        }
      }
    }
  }.elsewhen(state_handle === sh_wait_input){
    when(state_in === sin_putblock_done){
      printf("isTagged in wati_input:\t%x\n",isTagged)
      when(isTagged){
        state_handle := sh_putBlock
      }.otherwise{
        state_handle := sh_meta_tag_match
      }
    }
  }.elsewhen(state_handle === sh_meta_tag_match){
    when(!q_iacq_bits.io.deq.valid){
      state_handle := sh_done
    }
  }.elsewhen(state_handle === sh_fetch_mtt_valid){
    when(mtt_fetcher.io.req.fire()){
      when(needTagEntryFetched){
        state_handle := sh_fetch_valid
      }.otherwise{
        state_handle := sh_fetch_mtt_wait
      }
      needMTTFetched := Bool(true)
    }
  }.elsewhen(state_handle === sh_fetch_valid) {
    when(fetcher.io.req.fire()){
      when(needMTTFetched && Bool(enableMetaTag)){
        state_handle := sh_fetch_mtt_wait
      }.otherwise{
        state_handle := sh_fetch_wait
      }
    } 
  }.elsewhen(state_handle === sh_fetch_mtt_wait){
    when(mtt_fetcher.io.resp.fire()){
      matched_meta_tag_entry := mtt_fetcher.io.resp.bits.entry
      matched_meta_tag_entry_valid := Bool(true)
      when(needTagEntryFetched){
        state_handle := sh_fetch_wait
      }.otherwise{
        when(handle_mode === mode_putBlock){
          state_handle := sh_putBlock
        }.otherwise{ //getBlock
          state_handle := sh_getBlock_valid
          //state_handle := sh_getBlock_early_valid
        }
      }
    }
  }.elsewhen(state_handle === sh_fetch_wait) {
    when(fetcher.io.resp.fire()) {
      printf("matched_entry_plain fetched:\t%x\n",fetcher.io.resp.bits.entry)

      when(handle_mode === mode_putBlock){
        state_handle := sh_putBlock
      }.otherwise{ //getBlock
        state_handle := sh_getBlock_valid
        //state_handle := sh_getBlock_early_valid
      }
      matched_entry_plain := fetcher.io.resp.bits.entry
      matched_entry_valid := Bool(true)
    }
  }.elsewhen(state_handle === sh_putBlock) {

      printf("matched_entry_putblock:\t%x\n",matched_entry_plain)
      printf("matched_meta_tag_entry_putblock:\t%x\n",matched_meta_tag_entry)
      printf("matched_meta_tag_field:\t%x\n",matched_meta_tag_entry(addr_block_2_mtt_entry_idx(addr_block)))
      printf("matched_meta_tag_entry_valid_putblock:\t%x\n",matched_meta_tag_entry_valid)
    when(q_iacq_bits.io.deq.fire()) {
      reg_bits_handling := q_iacq_bits.io.deq.bits
      reg_bits_handling_valid := Bool(true)
    }
    when(reg_bits_handling_valid) {

        when(reg_bits_handling.data(129) === UInt(0)) {
          matched_entry_plain(addr_2_entry_addr_upper(reg_bits_handling.full_addr())) := UInt(0)


	}
	when(reg_bits_handling.data(128) === UInt(0)) {
	  matched_entry_plain(addr_2_entry_addr_lower(reg_bits_handling.full_addr())) := UInt(0)

	}
        when(reg_bits_handling.data(129) === UInt(1)) {
          matched_entry_plain(addr_2_entry_addr_upper(reg_bits_handling.full_addr())) := UInt(1)
	  matched_meta_tag_entry(addr_block_2_mtt_entry_idx(addr_block)) := UInt(1)
	  metaTagDir(addr_block_2_mtt_idx(addr_block)) := UInt(1)

	  printf("write to mtt_idx:\t%x\n",addr_block_2_mtt_idx(addr_block))

	}
	when(reg_bits_handling.data(128) === UInt(1)) {
	  matched_entry_plain(addr_2_entry_addr_lower(reg_bits_handling.full_addr())) := UInt(1)
	  matched_meta_tag_entry(addr_block_2_mtt_entry_idx(addr_block)) := UInt(1)
	  metaTagDir(addr_block_2_mtt_idx(addr_block)) := UInt(1)
	  printf("write to mtte_idx:\t%x\n",addr_block_2_mtt_entry_idx(addr_block))
	  printf("write to mtte_idx:\t%x\n",addr_block_2_mtt_entry_idx(addr_block))

	}
        reg_bits_handling_valid := Bool(false) 
	cnt_handled := cnt_handled + UInt(1)
    }
    when(cnt_handled === UInt(io.inner.tlDataBeats)) {

      when(io.tagWBEnable) {
        state_handle := sh_wb_cache_valid
      }.otherwise{
        state_handle := sh_done
      }
    }
  }.elsewhen(state_handle === sh_wb_cache_valid) {
  /*
      printf("tagCache.io.resp.fire() at sh_wb_cache_valid\n")

      printf("\thandle_mode:\t%x\n",handle_mode)
      */
      when(tagCache.io.req.fire()){

        printf("\twb to cache matched_entry_plain:\t%x\n",matched_entry_plain)
        printf("\twb to cache matched_meta_tag_entry:\t%x\n",matched_meta_tag_entry)

        state_handle := sh_wb_cache_only
      }
  }.elsewhen(state_handle === sh_wb_cache_only) {

    when(tagCache.io.resp.fire()){
      when(matched_meta_tag_entry_valid && matched_meta_tag_entry.orR === Bool(false) && Bool(enableMetaTag)){
        metaTagDir(addr_block_2_mtt_idx(addr_block)) := Bool(false)
      }
      when(matched_entry_valid && matched_entry_plain.orR === Bool(false)){
        matched_meta_tag_entry(addr_block_2_mtt_entry_idx(addr_block)) := Bool(false)
      }



      evictMTT := tagCache.io.resp.bits.evict_mtt
      evictMTTValid := tagCache.io.resp.bits.evict_mtt_valid
      evictMTTIdx := tagCache.io.resp.bits.evict_mtt_idx

      when(tagCache.io.resp.bits.evict_valid && io.tagWBEnable){
        state_handle := sh_wb_valid
        evictDataSep := tagCache.io.resp.bits.evict_data
	evictTableIdxSep := tagCache.io.resp.bits.evict_table_idx
	evictValid := Bool(true)
        printf("evictIdx:\t%x\n",tagCache.io.resp.bits.evict_table_idx)
        printf("evictData:\t%x\n",tagCache.io.resp.bits.evict_data)
      }.elsewhen(tagCache.io.resp.bits.evict_mtt_valid && Bool(enableMetaTag)){
        state_handle := sh_wb_mtt_valid
	evictValid := Bool(false)
      }.otherwise{
        when(handle_mode === mode_putBlock){
	  state_handle := sh_done
	}.otherwise{
	  state_handle := sh_getBlock_wait
	}
      }
    }
  }.elsewhen(state_handle === sh_getBlock_valid) {
    when(granter.io.req_tag.fire()) {
      state_handle := sh_wb_cache_valid
    }
  }.elsewhen(state_handle === sh_getBlock_wait) {
    when(granter.io.resp.fire()) {
      state_handle := sh_done
    }
  }.elsewhen(state_handle === sh_wb_valid) {
    
    when(writer.io.req.fire()) {
      when(evictMTTValid && Bool(enableMetaTag)){
        state_handle := sh_wb_mtt_valid
      }.otherwise{
        state_handle := sh_wb_wait
      }
      

    }
  }.elsewhen(state_handle === sh_wb_mtt_valid) {
    when(mtt_writer.io.req.fire()){
      state_handle := sh_wb_mtt_wait
    }
  }.elsewhen(state_handle === sh_wb_mtt_wait){
    when(mtt_writer.io.resp.fire()){
      when(evictValid){
        state_handle := sh_wb_wait
      }.otherwise{
        when(handle_mode === mode_putBlock){
          state_handle := sh_done
        }.otherwise{
          state_handle := sh_getBlock_wait
        }
      }
    }
  }.elsewhen(state_handle === sh_wb_wait){
    when(writer.io.resp.fire()){
      when(handle_mode === mode_putBlock){
        state_handle := sh_done
      }.otherwise{
        state_handle := sh_getBlock_wait
      }
    }
  }.elsewhen(state_handle === sh_done) {
    matched_entry_valid := Bool(false)
    matched_meta_tag_entry_valid := Bool(false)
        state_handle := sh_ready
  }
















  val oacq_arb = Module(new LockingArbiter(new Acquire, 6, tlDataBeats, (a: Acquire) => a.hasMultibeatData()))(params)
  oacq.bits := oacq_arb.io.out.bits
  oacq.valid := oacq_arb.io.out.valid
  oacq_arb.io.out.ready := oacq.ready

  oacq_arb.io.in(0) <> fetcher.io.tl.acquire
  oacq_arb.io.in(1) <> writer.io.tl.acquire
  oacq_arb.io.in(2) <> granter.io.outer.acquire

  oacq_arb.io.in(3).bits := q_iacq_bypass.io.deq.bits
  oacq_arb.io.in(3).valid := state_in === sin_putblock_done && q_iacq_bypass.io.deq.valid
  q_iacq_bypass.io.deq.ready := state_in === sin_putblock_done && oacq_arb.io.in(3).ready
 
  iacq.ready := q_iacq_bypass.io.enq.ready  & ( (state_in === sin_ready | state_in === sin_putblock_busy) & q_iacq_bits.io.enq.ready & cnt_iacq != UInt(4))
  oacq_arb.io.in(4) <> mtt_fetcher.io.tl.acquire
  oacq_arb.io.in(5) <> mtt_writer.io.tl.acquire
/*
  handling grants, only the granter sends somtheing through ignt
*/


  mtt_writer.io.tl.grant.valid := ognt.valid && ognt.bits.client_xact_id === UInt(10)
  mtt_writer.io.tl.grant.bits := ognt.bits
  mtt_fetcher.io.tl.grant.valid := ognt.valid && ognt.bits.client_xact_id === UInt(10)
  mtt_fetcher.io.tl.grant.bits := ognt.bits 
  fetcher.io.tl.grant.valid := ognt.valid && ognt.bits.client_xact_id === UInt(8)
  fetcher.io.tl.grant.bits := ognt.bits 
  writer.io.tl.grant.valid := ognt.valid && ognt.bits.client_xact_id === UInt(8)
  writer.io.tl.grant.bits := ognt.bits
  granter.io.outer.grant.valid := ognt.valid && ognt.bits.client_xact_id === UInt(9)
  granter.io.outer.grant.bits := ognt.bits
  ognt.ready := (/*state_handle === sh_fetch_wait &*/ fetcher.io.tl.grant.ready & ognt.bits.client_xact_id === UInt(8)) |
                (/*state_handle === sh_fetch_mtt_wait &*/ mtt_fetcher.io.tl.grant.ready & ognt.bits.client_xact_id === UInt(10)) |
               (/*state_handle === sh_wb_wait &*/ writer.io.tl.grant.ready & ognt.bits.client_xact_id === UInt(8)) |
               (/*state_handle === sh_wb_mtt_wait &*/ mtt_writer.io.tl.grant.ready & ognt.bits.client_xact_id === UInt(10)) |
	       (/*state_handle === sh_getBlock_wait &*/ognt.bits.client_xact_id === UInt(9) && granter.io.outer.grant.ready)
  //ignt <> granter.io.inner.grant

  ignt.valid := granter.io.inner.grant.valid    //Mux(state_handle === sh_getBlock_wait, granter.io.inner.grant.valid, Bool(false))
  ignt.bits := granter.io.inner.grant.bits
  granter.io.inner.grant.ready := ignt.ready //Mux(state_handle === sh_getBlock_wait, ignt.ready, Bool(false))

  /*
    debug prints
  */
 when(iacq.fire()) {
    printf("hgmoon-debug:\tincoming traffic: in tracker\n");
    printf("hgmoon-debug:\tDFITracker: client_xact_id:\t%x\n",iacq.bits.client_xact_id)
    printf("hgmoon-debug:\tDFITracker: addr_block:\t%x\n",iacq.bits.addr_block)
    printf("hgmoon-debug:\tDFITracker: addr_beat:\t%x\n",iacq.bits.addr_beat)
    printf("hgmoon-debug:\tDFITracker: data:\t%x\n",iacq.bits.data)
    printf("hgmoon-debug:\tDFITracker: a_type:\t%x\n",iacq.bits.a_type)
    printf("hgmoon-debug:\tDFITracker: union:\t%x\n",iacq.bits.union)
  }
when(oacq.fire()) {

    printf("hgmoon-debug:\tleaving traffic: in tracker\n");
    printf("hgmoon-debug:\tDFITracker: client_xact_id:\t%x\n",oacq.bits.client_xact_id)
    printf("hgmoon-debug:\tDFITracker: addr_block:\t%x\n",oacq.bits.addr_block)
    printf("hgmoon-debug:\tDFITracker: addr_beat:\t%x\n",oacq.bits.addr_beat)
    printf("hgmoon-debug:\tDFITracker: data:\t%x\n",oacq.bits.data)
    printf("hgmoon-debug:\tDFITracker: a_type:\t%x\n",oacq.bits.a_type)
    printf("hgmoon-debug:\tDFITracker: union:\t%x\n",oacq.bits.union)
  }

/*
  val num_iacq = Reg(init=UInt(0,width=32))
when(Bool(true)) {
  when(iacq.fire()) {
    num_iacq := num_iacq + UInt(1)
  }
  when(oacq.fire()){
    printf("num_iacq_cache\t%d\n",num_iacq)
    printf("hgmoon-debug:\tleaving traffic from DFITagger:\n");


    printf("hgmoon-debug:\tDFITagger: built_in_type:\t%x\n",oacq.bits.is_builtin_type)
    printf("hgmoon-debug:\tDFITagger: client_xact_id:\t%x\n",oacq.bits.client_xact_id)
    printf("hgmoon-debug:\tDFITagger: addr_block:\t%x\n",oacq.bits.addr_block)
    printf("hgmoon-debug:\tDFITagger: addr_beat:\t%x\n",oacq.bits.addr_beat)
    printf("hgmoon-debug:\tDFITagger: data:\t%x\n",oacq.bits.data)
    printf("hgmoon-debug:\tDFITagger: a_type:\t%x\n",oacq.bits.a_type)
    printf("hgmoon-debug:\tDFITagger: union:\t%x\n",oacq.bits.union)
  }
}
*/

when(ognt.fire()){
    printf("hgmoon-debug:\tognt.fire() in TagCache\n")

    printf("hgmoon-debug:\tognt.g_type:\t%x\n",ognt.bits.g_type)

    printf("hgmoon-debug:\tognt.addr_beat:\t%x\n",ognt.bits.addr_beat)
    printf("hgmoon-debug:\tognt.data:\t%x\n",ognt.bits.data)
    printf("hgmoon-debug:\tognt.client_xact_id:\t%x\n",ognt.bits.client_xact_id)
    printf("hgmoon-debug:\tognt.manager_xact_id:\t%x\n",ognt.bits.manager_xact_id)
    printf("hgmoon-debug:\tognt.data:\t%x\n",ognt.bits.data)
  }

  when(ignt.fire()){
    printf("hgmoon-debug:\tignt.fire() in TagCache\n")

    printf("hgmoon-debug:\tignt.g_type:\t%x\n",ignt.bits.g_type)

    printf("hgmoon-debug:\tignt.addr_beat:\t%x\n",ignt.bits.addr_beat)
    printf("hgmoon-debug:\tignt.data:\t%x\n",ignt.bits.data)
    printf("hgmoon-debug:\tignt.client_xact_id:\t%x\n",ignt.bits.client_xact_id)
    printf("hgmoon-debug:\tignt.manager_xact_id:\t%x\n",ignt.bits.manager_xact_id)
    printf("hgmoon-debug:\tignt.data:\t%x\n",ignt.bits.data)
  }



  /*
    counters
   */

  val count_tag_cache_read_access = Reg(init=UInt(0,width=64))
  val count_tag_cache_read_hit = Reg(init=UInt(0,width=64))
  val count_tag_cache_write_access = Reg(init=UInt(0,width=64))
  val count_tag_cache_write_hit = Reg(init=UInt(0,width=64))
  val count_tag_entry_evicted = Reg(init=UInt(0,width=64))
  val count_mtt_write_skip = Reg(init=UInt(0,width=64))
  val count_mtt_read_skip = Reg(init=UInt(0,width=64))

  when(handle_mode === mode_putBlock){
    when(tagCache.io.resp.fire() && state_handle === sh_check_cache ){
      count_tag_cache_write_access := count_tag_cache_write_access + UInt(1)
      when(tagCache.io.resp.bits.matched){
        count_tag_cache_write_hit := count_tag_cache_write_hit + UInt(1)
      }
    }
    when(state_handle === sh_meta_tag_match) {
      when(!q_iacq_bits.io.deq.valid){
        count_mtt_write_skip := count_mtt_write_skip + UInt(1)
      }
    }  
  }
  when(handle_mode === mode_getBlock){
    when(tagCache.io.resp.fire() && state_handle === sh_check_cache) {
      count_tag_cache_read_access := count_tag_cache_read_access + UInt(1)
      when(tagCache.io.resp.bits.matched) {
        count_tag_cache_read_hit := count_tag_cache_read_hit + UInt(1)
      }
    }
    when(state_handle === sh_check_cache && !tagCache.io.resp.bits.matched &&
 	 				(tagCache.io.resp.bits.mtt_matched && 
	    				tagCache.io.resp.bits.mtt_entry(addr_block_2_mtt_entry_idx(addr_block)) === UInt(0) 
			) ||
			metaTagDir(addr_block_2_mtt_idx(addr_block)) === UInt(0)
	    	){ // all the entry bits should be 0

      count_mtt_read_skip := count_mtt_read_skip + UInt(1)
    }
  }
  when(tagCache.io.resp.fire() && state_handle === sh_wb_cache_only) {
    when(tagCache.io.resp.bits.evict_valid){
      count_tag_entry_evicted := count_tag_entry_evicted + UInt(1)
    }
  }
/*
  printf("count_tag_cache_read_access:\t%x\n",count_tag_cache_read_access)
  printf("count_tag_cache_read_hit:\t%x\n",count_tag_cache_read_hit)
  printf("count_tag_cache_write_access:\t%x\n",count_tag_cache_write_access)
  printf("count_tag_cache_write_hit:\t%x\n",count_tag_cache_write_hit)
  printf("count_tag_entry_evicted:\t%x\n",count_tag_entry_evicted)
*/
  
  io.count_tag_cache_read_access := count_tag_cache_read_access
  io.count_tag_cache_read_hit := count_tag_cache_read_hit
  io.count_tag_cache_write_access := count_tag_cache_write_access
  io.count_tag_cache_write_hit := count_tag_cache_write_hit
  io.count_tag_entry_evicted := count_tag_entry_evicted
  io.count_mtt_write_skip := count_mtt_write_skip
  io.count_mtt_read_skip := count_mtt_read_skip

}



