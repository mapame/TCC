all: remote-control data-export web-test backend

bearssl:
	make -C lib/bearssl lib

remote-control: bearssl
	gcc -Wall -o build/remote-control src/remote-control/main.c src/remote-control/tftp.c src/common/logger.c src/common/communication.c lib/bearssl/build/libbearssl.a -Isrc/common -Ilib/bearssl/inc -lbsd -lm

data-export: bearssl
	gcc -Wall -o build/data-export src/data-export/main.c src/common/logger.c src/common/communication.c lib/bearssl/build/libbearssl.a -Isrc/common -Ilib/bearssl/inc -lbsd -lm

web-test: bearssl
	gcc -Wall -o build/web-test src/web-test/main.c src/common/logger.c src/common/communication.c lib/bearssl/build/libbearssl.a lib/cJSON/cJSON.c -Isrc/common -Ilib/bearssl/inc -Ilib/cJSON -lbsd -lmicrohttpd -lm


backend: bearssl
	gcc -Wall -o build/backend src/backend/main.c src/backend/data_acquisition.c src/common/logger.c src/common/communication.c lib/bearssl/build/libbearssl.a lib/cJSON/cJSON.c -Isrc/common -Ilib/bearssl/inc -Ilib/cJSON -pthread -lbsd -lmicrohttpd -lm
