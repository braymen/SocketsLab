pessmabm_server: pessmabm_server.cpp pessmabm_crc.hpp
	g++ pessmabm_server.cpp -o pessmabm_server -pthread

pessmabm_client: pessmabm_client.cpp pessmabm_crc.hpp
	g++ pessmabm_client.cpp -o pessmabm_client -pthread

clean:
	rm pessmabm_client pessmabm_server