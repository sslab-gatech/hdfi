
package rocketchip

import Chisel._
//import junctions._
import uncore._
import rocket._
//import rocket.Util._

/*
  This file currently contains three submodules:
    Granter, Fetcher and Writer
*/

class DFIGranter extends DFITModule {
  val io = new Bundle {
    val req = Decoupled(
      new Bundle {
        val addr_block = UInt(OUTPUT)
	val client_xact_id = UInt(OUTPUT)
	//val tags = Bits(OUTPUT)
      }
    ).flip
    val req_tag = Decoupled(
      new Bundle {
        val tags = Bits(OUTPUT)
      }
    ).flip
    val resp = Decoupled(
      new Bundle {
        val dummy = Bool()
      }
    )
    val inner = (new ClientUncachedTileLinkIO).flip
    val outer = new ClientUncachedTileLinkIO
  }

/*
  debug counters
*/
   val q_gnt = Module(new Queue(io.inner.grant.bits,tlDataBeats))(params) 
   

  if(enableDebugCounters) {
    val count_accessed = Reg(init=UInt(0,width=32))
    when(io.req.fire()) {
      count_accessed := count_accessed + UInt(1)
      printf("count_accessed in Granter:\t%d\n",count_accessed + UInt(1))
    }
  }

  val ready :: valid :: wait_tag :: forward :: fin ::  done :: Nil = Enum(UInt(),6)
  val state = Reg(init=ready)
  val cnt_fwd = Reg(UInt(width=3))
  val tags = Reg(Bits())
  val addr_block = Reg(UInt())
  val client_xact_id = Reg(UInt())
  val manager_xact_id = Reg(UInt())



  when(state === ready) {
    when(io.req.fire()) {
      state := valid
      //tags := io.req.bits.tags
      addr_block := io.req.bits.addr_block
      client_xact_id := io.req.bits.client_xact_id
    }
  }
  when(state === valid) {
    when(io.outer.acquire.fire()) {
      state := wait_tag
      cnt_fwd := UInt(0)
    }
  }
  when(state === wait_tag) {
    when(io.req_tag.fire()){
      tags := io.req_tag.bits.tags 
      state := forward
    }
  }
  when(state === forward) {
    when(io.inner.grant.fire()) {
      cnt_fwd := cnt_fwd + UInt(1)
      when(cnt_fwd === UInt(3)){
        state := done
	manager_xact_id := io.outer.grant.bits.manager_xact_id
      }
    }
  }
/*
  when(state === fin) {
    when(io.inner.finish.fire()){
      io.inner.finish.bits.manager_xact_id === manager_xact_id
      state := done
    }
  }
*/
  when(state === done) {
    when(io.resp.fire()){
      state := ready
    }
  }

  io.req.ready := state === ready
  io.req_tag.ready := state === wait_tag
  io.resp.valid := state === done

  io.outer.acquire.valid := state === valid
  io.outer.acquire.bits := Bundle(GetBlock (
                                      client_xact_id = Cat(UInt(1),UInt(1,width=tlClientXactIdBits-1)),//client_xact_id(tlClientXactIdBits-2,0)),
				      addr_block = addr_block,
				      alloc = Bool(true)
				      ))(params)
		
  io.inner.grant <> q_gnt.io.deq
  io.inner.grant.bits.client_xact_id := client_xact_id
  io.inner.grant.valid := q_gnt.io.deq.valid & state === forward
  q_gnt.io.deq.ready := io.inner.grant.ready & state === forward


  q_gnt.io.enq <> io.outer.grant

/*
  val upper_tag = tags(Cat(io.outer.grant.bits.addr_beat,UInt(1,width=1)))
  val lower_tag = tags(Cat(io.outer.grant.bits.addr_beat,UInt(0,width=1)))
*/
  val addr = Cat(addr_block,q_gnt.io.deq.bits.addr_beat,UInt(0,width=4))
  val upper_tag = tags(addr_2_entry_addr_upper(addr))
  val lower_tag= tags(addr_2_entry_addr_lower(addr))

  //io.inner.grant.bits.data := Cat(upper_tag,lower_tag,io.outer.grant.bits.data(127,0))

  io.inner.grant.bits.data := Cat(upper_tag,lower_tag,q_gnt.io.deq.bits.data(127,0))
  when(io.inner.grant.fire()){
    printf("forwarding in Granter:\n")
    printf("tags:\t%x\n",tags)
    printf("addr_beat:\t%x\n",io.outer.grant.bits.addr_beat)
    printf("upper_tag:\t%x\n",upper_tag)
    printf("lower_tag:\t%x\n",lower_tag)
  }

}

class DFITagWriter extends DFITModule {

  val io = new Bundle {
    val req = Decoupled(
      new Bundle {
        val addr = UInt(OUTPUT)
	val client_xact_id = UInt(OUTPUT)
	val entry = UInt(OUTPUT)
      }
    ).flip
    val resp =  Decoupled(
      new Bundle {
        val dummy = Bool(OUTPUT)
      }
    )
    val tl = new ClientUncachedTileLinkIO

  }

/*
  debug counters
*/
  if(enableDebugCounters) {
    val count_accessed = Reg(init=UInt(0,width=32))
    when(io.req.fire()) {
      count_accessed := count_accessed + UInt(1)
      printf("count_accessed in Writer:\t%d\n",count_accessed + UInt(1))
    }
  }
  val w_ready :: w_valid :: w_wait :: w_done :: Nil = Enum(UInt(),4)
  val state_write = Reg(init=w_ready)

  val client_xact_id = Reg(UInt())



  val cnt_acq = Reg(UInt(width=2))
  val vec_entry = Reg(Vec.fill(tlDataBeats){Bits()})
  val addr_block = Reg(UInt())
  when(state_write === w_ready) {
    when(io.req.fire()) {
      state_write := w_valid
      client_xact_id := Cat(UInt(1),io.req.bits.client_xact_id(tlClientXactIdBits-2,0))
      addr_block := io.req.bits.addr
      vec_entry(UInt(3)) := io.req.bits.entry(511,384)
      vec_entry(UInt(2)) := io.req.bits.entry(383,256)
      vec_entry(UInt(1)) := io.req.bits.entry(255,128)
      vec_entry(UInt(0)) := io.req.bits.entry(127,0)
      cnt_acq := UInt(0)
    }
  }
  when(state_write === w_valid) {
    when(io.tl.acquire.fire()){
      cnt_acq := cnt_acq + UInt(1)
      when(cnt_acq === UInt(3)) {
        state_write := w_wait
      }
    }
  }
  when(state_write === w_wait) {
    when(io.tl.grant.fire() && io.tl.grant.bits.client_xact_id === client_xact_id){
      state_write := w_done
    }
  }
  when(state_write === w_done) {
    when(io.resp.fire()){
      state_write := w_ready
    }
  }

  io.req.ready := state_write === w_ready
  io.resp.valid := state_write === w_done

  io.tl.acquire.valid := state_write === w_valid
  io.tl.acquire.bits := Bundle(PutBlock(
                                        client_xact_id = client_xact_id,
					addr_block = addr_block,
					addr_beat = cnt_acq,
					data = vec_entry(cnt_acq)
					))(params)

  io.tl.grant.ready := state_write === w_wait
/*
  when(io.req.fire()) {
    printf("io.req.fire() in Writer\n")
    printf("addr:\t%x\n",io.req.bits.addr)
    printf("client_xact_id:\t%x\n",io.req.bits.client_xact_id)
    printf("entry:\t%x\n",io.req.bits.entry)
  }
*/

}

class DFITagFetcher extends DFITModule {
 
  val io = new Bundle {
    val req = Decoupled(
      new Bundle {
        val addr = UInt(OUTPUT)
	val client_xact_id = UInt(OUTPUT)
      }
    ).flip
    val resp = Decoupled(
      new Bundle {
        val entry = UInt(OUTPUT)
      }
    )

    val tl = new ClientUncachedTileLinkIO

  }

/*
  debug counters
*/

  if(enableDebugCounters) {
    val count_accessed = Reg(init=UInt(0,width=32))
    when(io.req.fire()) {
      count_accessed := count_accessed + UInt(1)
      printf("count_accessed in Fetcher:\t%d\n",count_accessed + UInt(1))
    }
  }


  val f_ready :: f_valid :: f_wait :: f_done :: Nil = Enum(UInt(),4)
  val state_fetch = Reg(init=f_ready)

  val client_xact_id = Reg(UInt())

  val cnt_gnt = Reg(UInt(width = 2))
  val vec_entry = Reg(Vec.fill(tlDataBeats){Bits()})
  val addr_block = Reg(UInt())
  when(state_fetch === f_ready) {
    when(io.req.fire()) {
      state_fetch := f_valid
      addr_block := io.req.bits.addr
      client_xact_id := Cat(UInt(1),io.req.bits.client_xact_id(tlClientXactIdBits-2,0))
    }
  }
  when(state_fetch === f_valid) {
    when(io.tl.acquire.fire()) {
      state_fetch := f_wait
      cnt_gnt := UInt(0)
    }
  }
  when(state_fetch === f_wait) {
    when(io.tl.grant.fire() && io.tl.grant.bits.client_xact_id === client_xact_id) {
      cnt_gnt := cnt_gnt + UInt(1)
      vec_entry(cnt_gnt) := io.tl.grant.bits.data
      when(cnt_gnt === UInt(tlDataBeats-1)) {
        state_fetch := f_done
      }
    }
  }
  when(state_fetch === f_done) {
    when(io.resp.fire()) {
      state_fetch := f_ready
    }
  }

  
  io.req.ready := state_fetch === f_ready
  io.resp.bits.entry := Cat(vec_entry(UInt(3))(127,0),vec_entry(UInt(2))(127,0),vec_entry(UInt(1))(127,0),vec_entry(UInt(0))(127,0))
  io.resp.valid := state_fetch === f_done

  io.tl.acquire.valid := state_fetch === f_valid
  io.tl.acquire.bits := Bundle(GetBlock (
                                      client_xact_id = client_xact_id,
				      addr_block = addr_block,
				      alloc = Bool(true)
				      ))(params)
				     
  io.tl.grant.ready := state_fetch === f_wait

/*
  when(io.req.fire()) {
    printf("io.req.fire() in Fetcher\n")
    printf("addr:\t%x\n",io.req.bits.addr)
    printf("client_xact_id:\t%x\n",io.req.bits.client_xact_id)
  }
*/


}


