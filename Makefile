all: remote-control data-export web-test backend

remote-control:
	gcc -Wall -o build/remote-control src/remote-control/main.c src/remote-control/tftp.c src/common/logger.c src/common/communication.c -Isrc/common -lcrypto -lbsd -lm

data-export:
	gcc -Wall -o build/data-export src/data-export/main.c src/common/logger.c src/common/communication.c -Isrc/common -lcrypto -lbsd -lm

web-test:
	gcc -Wall -o build/web-test src/web-test/main.c src/common/logger.c src/common/communication.c src/web-test/cJSON/cJSON.c -Isrc/common -Isrc/web-test/cJSON -lcrypto -lbsd -lmicrohttpd -lm

backend:
	gcc -Wall -o build/backend src/backend/main.c src/backend/data_acquisition.c src/backend/power.c src/backend/energy.c src/backend/http.c src/backend/http_auth.c src/backend/auth.c src/backend/config.c src/backend/http_config.c src/backend/device_events.c src/common/logger.c src/common/communication.c -Isrc/common -pthread -lcrypto -lbsd -lmicrohttpd -lm -ljson-c -lsqlite3 -luuid
