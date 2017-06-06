INSTALL_DIR=install
ATTACK_DIR=$(INSTALL_DIR)/attack

all: attack openssl

openssl:
	./build.py

attack:
	cd $(ATTACK_DIR) && make

clean: clean_attack
	rm -rf openssl-1.0.1*

clean_attack: 
	cd $(ATTACK_DIR) && make clean
