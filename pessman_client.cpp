#include <iostream>
#include <fstream>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>

#define PACKET_MAX_SIZE 10
#define TOTAL_PACKET_MAX_SIZE 10

using namespace std;

/*
 * p0 is 10.35.195.251
 * p1 is 10.34.40.33
 * p2 is 10.35.195.250
 * p3 is 10.35.195.236
 */

void printPacket(char packet[], int index, char type, int packetSize)
{
    // Sent packet
    if (type == 's')
    {
        cout << "Sent encrypted packet #" << index << " - encrypted as ";
    }

    // Recieve packet
    if (type == 'r')
    {
        cout << "Rec encrypted packet #" << index << " - encrypted as ";
    }

    // Printing
    printf("%02X\0", packet[0 + PACKET_MAX_SIZE]);
    printf("%02X\0", packet[1 + PACKET_MAX_SIZE]);
    cout << " ... ";
    printf("%02X\0", packet[packetSize + PACKET_MAX_SIZE - 2]);
    printf("%02X\0", packet[packetSize + PACKET_MAX_SIZE - 1]);
    cout << endl;
}

int main()
{
    // Server Settings
    struct sockaddr_in serv_addr, client_addr;
    int addrlen = sizeof(client_addr);

    // Initialize variables
    char ip[20] = "10.35.195.250";
    char port[20] = "9372";
    char saveFile[100];
    int maxPacketSize = 1000;
    int leftOverPacket = 0;
    int valread;
    int numPackets = 0;
    int totalPackets = 0;

    int windowSize = 1;
    char mode[3] = "sw";
    int sequenceNumbers = 2;

    // User Input
    cout << "Save file to: ";
    cin >> saveFile;

    // Server address initialization
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(port));

    // Open the socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // Bind the socket
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in)) < 0)
    {
        perror("Bind Failed");
        return 0;
    }

    // Set listen to 5 queued connections
    if (listen(sockfd, 5) < 0)
    {
        perror("Listen Failed");
        return 0;
    }

    // Accept a client connection
    int client_sock = accept(sockfd, (struct sockaddr *)&client_addr, (socklen_t *)&addrlen);
    if (client_sock < 0)
    {
        perror("Accept Failed");
        return 0;
    }

    // Open a file
    FILE *pFile;
    pFile = fopen(saveFile, "w");

    // Get Packet Size
    char data_packetSize[PACKET_MAX_SIZE];
    read(client_sock, data_packetSize, PACKET_MAX_SIZE);
    maxPacketSize = atoi(data_packetSize);

    // Get total Packets
    char data_totalPackets[TOTAL_PACKET_MAX_SIZE];
    read(client_sock, data_totalPackets, TOTAL_PACKET_MAX_SIZE);
    totalPackets = atoi(data_totalPackets);

    char packet[maxPacketSize + PACKET_MAX_SIZE];
    bzero(packet, maxPacketSize + PACKET_MAX_SIZE);

    // Read all the packets
    while ((valread = read(client_sock, packet, maxPacketSize + PACKET_MAX_SIZE)) > 0)
    {
        // Read Sequence Number
        char *stringSequenceNumber;
        int sequenceNumber;
        read(client_sock, stringSequenceNumber, TOTAL_PACKET_MAX_SIZE);
        // sequenceNumber = atoi(stringSequenceNumber);

        // Print Packet Sent Message
        cout << "Packet "
             << stringSequenceNumber
             << " recieved" << endl;

        // Send Ack
        write(client_sock, stringSequenceNumber, 64);
        cout << "Ack "
             << "1"
             << " sent" << endl;

        // Get Size from Header
        char packetWriteSize[PACKET_MAX_SIZE];
        for (int i = 0; i < PACKET_MAX_SIZE; i++)
        {
            packetWriteSize[i] = packet[i];
        }
        int sz = atoi(packetWriteSize);

        char packetWrite[sz];
        for (int i = 0; i < sz; i++)
        {
            packetWrite[i] = packet[i + PACKET_MAX_SIZE];
        }
        fwrite(packetWrite, 1, sz, pFile);

        numPackets++;
        bzero(packet, maxPacketSize + PACKET_MAX_SIZE);
    }

    // Recieve Success
    cout << "Recieve Success!" << endl;

    // Close file
    fclose(pFile);

    // MD5 Hash
    cout << "MD5: " << endl;
    char sys[200] = "md5sum ";
    system(strcat(sys, saveFile));

    // Close socket
    close(client_sock);

    return 0;
}