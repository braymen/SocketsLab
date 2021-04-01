pessman_server: pessman_server.cpp
	g++ pessman_server.cpp -o pessman_server -pthread

pessman_client: pessman_client.cpp
	g++ pessman_client.cpp -o pessman_client

clean:
	rm pessman_client pessman_server