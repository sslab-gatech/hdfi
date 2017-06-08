// See LICENSE for license details.

package uncore
package constants

import Chisel._

object MemoryOpConstants extends MemoryOpConstants
trait MemoryOpConstants {
  val MT_SZ = 4
  val MT_X  = BitPat("b????")
  val MT_B  = UInt("b0000")
  val MT_H  = UInt("b0001")
  val MT_W  = UInt("b0010")
  val MT_D  = UInt("b0011")
  val MT_BU = UInt("b0100")
  val MT_HU = UInt("b0101")
  val MT_WU = UInt("b0110")
  val MT_Q  = UInt("b0111")
  val MT_T  = UInt("b1111") // dfi


  val NUM_XA_OPS = 9
  val M_SZ      = 6
  val M_X       = BitPat("b??????");
  val M_XRD     = UInt("b000000"); // int load
  val M_XWR     = UInt("b000001"); // int store
  val M_PFR     = UInt("b000010"); // prefetch with intent to read
  val M_PFW     = UInt("b000011"); // prefetch with intent to write
  val M_XA_SWAP = UInt("b000100");
  val M_NOP     = UInt("b000101");
  val M_XLR     = UInt("b000110");
  val M_XSC     = UInt("b000111");
  val M_XA_ADD  = UInt("b001000");
  val M_XA_XOR  = UInt("b001001");
  val M_XA_OR   = UInt("b001010");
  val M_XA_AND  = UInt("b001011");
  val M_XA_MIN  = UInt("b001100");
  val M_XA_MAX  = UInt("b001101");
  val M_XA_MINU = UInt("b001110");
  val M_XA_MAXU = UInt("b001111");
  val M_FLUSH   = UInt("b010000") // write back dirty data and cede R/W permissions
  val M_PRODUCE = UInt("b010001") // write back dirty data and cede W permissions
  val M_CLEAN   = UInt("b010011") // write back dirty data and retain R/W permissions
  
  //tagged memory.
  //avoid setting cmd(3) = 1 since it is implicitly reserved 
  val M_XRD_T   = UInt("b010100"); // int load with flag
  val M_XWR_T   = UInt("b010101"); // int store
  val M_PFR_T   = UInt("b010110"); // prefetch with intent to read with flag
  val M_PFW_T   = UInt("b010111"); // prefetch with intent to write with flag
  val M_XCP_T   = UInt("b100000")  // int memcpy while preserving flag
  val M_XWP_T   = UInt("b100001")
 
  
  def isAMO(cmd: UInt) = cmd(3) || cmd === M_XA_SWAP
  def isPrefetch(cmd: UInt) = cmd === M_PFR || cmd === M_PFW || cmd === M_PFR_T || cmd === M_PFW_T
  def isRead(cmd: UInt) = cmd === M_XRD || cmd === M_XLR || isAMO(cmd) || cmd === M_XRD_T
  def isWrite(cmd: UInt) = cmd === M_XWR || cmd === M_XSC || isAMO(cmd) || cmd === M_XWR_T
  def isWriteIntent(cmd: UInt) = isWrite(cmd) || cmd === M_PFW || cmd === M_XLR || cmd === M_PFW_T
  def isTagged(cmd: UInt) = cmd === M_XRD_T || cmd === M_PFR_T || cmd === M_PFW_T || cmd === M_XWR_T
}
