pessman_server: pessman_server.cpp crc.hpp
	g++ pessman_server.cpp -o pessman_server -pthread

pessman_client: pessman_client.cpp crc.hpp
	g++ pessman_client.cpp -o pessman_client -pthread

clean:
	rm pessman_client pessman_server