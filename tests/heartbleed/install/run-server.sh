#!/bin/ash
ifconfig lo 127.0.0.1
./openssl_hb s_server -key ./assets/server.key -cert ./assets/server.cert5 -accept 443 -www&
./openssl_hdfi s_server -key ./assets/server.key -cert ./assets/server.cert5 -accept 444 -www&
./openssl_hb_hdfi s_server -key ./assets/server.key -cert ./assets/server.cert5 -accept 445 -www&
