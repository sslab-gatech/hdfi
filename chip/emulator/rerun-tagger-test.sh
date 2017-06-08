
rm -f output/dhrystone.riscv.out

./emulator-Top-DefaultL2CPPConfig +dramsim +max-cycles=100000000 +verbose +loadmem=output/dhrystone.riscv.hex none
#./emulator-Top-DefaultCPPConfig +dramsim +max-cycles=100000000 +verbose +loadmem=output/dhrystone.riscv.hex none
exit

rm -f output/tagger-test.riscv.out
rm -rf output/rv64ui-pt-dfi-allmatch.out
rm -rf output/rv64ui-pt-dfi-mismatch.out
echo "running tagger-test"
./emulator-Top-DefaultCPPConfig +dramsim +max-cycles=100000000 +verbose +loadmem=output/tagger-test.riscv.hex none 2> output/tagger-test.riscv.out && [ $PIPESTATUS -eq 0 ] 
echo "running allmatch"
./emulator-Top-DefaultCPPConfig +dramsim +max-cycles=100000000 +verbose +loadmem=output/rv64ui-pt-dfi-allmatch.hex none 2> output/rv64ui-pt-dfi-allmatch.out && [ $PIPESTATUS -eq 0 ]
echo "running mismatch"
./emulator-Top-DefaultCPPConfig +dramsim +max-cycles=100000000 +verbose +loadmem=output/rv64ui-pt-dfi-mismatch.hex none 2> output/rv64ui-pt-dfi-mismatch.out && [ $PIPESTATUS -eq 0 ]


exit

rm -f output/vvadd.riscv.out
rm -f output/mt-vvadd.riscv.out
rm -f output/qsort.riscv.out
rm -f output/mm.riscv.out
rm -f output/mt-matmul.riscv.out
rm -f output/median.riscv.out
rm -f output/multiply.riscv.out
rm -f output/spmv.riscv.out
rm -f output/towers.riscv.out

./emulator-Top-DefaultCPPConfig +dramsim +max-cycles=100000000 +verbose +loadmem=output/vvadd.riscv.hex none 
./emulator-Top-DefaultCPPConfig +dramsim +max-cycles=100000000 +verbose +loadmem=output/mt-vvadd.riscv.hex none 
./emulator-Top-DefaultCPPConfig +dramsim +max-cycles=100000000 +verbose +loadmem=output/qsort.riscv.hex none 
./emulator-Top-DefaultCPPConfig +dramsim +max-cycles=100000000 +verbose +loadmem=output/mm.riscv.hex none 
./emulator-Top-DefaultCPPConfig +dramsim +max-cycles=100000000 +verbose +loadmem=output/mt-matmul.riscv.hex none 
./emulator-Top-DefaultCPPConfig +dramsim +max-cycles=100000000 +verbose +loadmem=output/median.riscv.hex none 
./emulator-Top-DefaultCPPConfig +dramsim +max-cycles=100000000 +verbose +loadmem=output/multiply.riscv.hex none 
./emulator-Top-DefaultCPPConfig +dramsim +max-cycles=100000000 +verbose +loadmem=output/spmv.riscv.hex none 
./emulator-Top-DefaultCPPConfig +dramsim +max-cycles=100000000 +verbose +loadmem=output/towers.riscv.hex none 


