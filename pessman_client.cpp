/*
 * p0 is 10.35.195.251
 * p1 is 10.34.40.33
 * p2 is 10.35.195.250
 * p3 is 10.35.195.236
 */

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

using namespace std;

int lfr = 0; // START
int laf = 0; // LAST

int main()
{
    // Server Settings
    struct sockaddr_in serv_addr, client_addr;
    int addrlen = sizeof(client_addr);

    // Initialize variables
    char ip[20] = "10.35.195.250";
    char port[20] = "9373";
    char saveFile[100];
    int maxPacketSize;
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
    char data_packetSize[1024];
    read(client_sock, data_packetSize, 1024);
    maxPacketSize = atoi(data_packetSize);

    // Get total Packets
    char data_totalPackets[1024];
    read(client_sock, data_totalPackets, 1024);
    totalPackets = atoi(data_totalPackets);

    char packet[maxPacketSize];
    bzero(packet, maxPacketSize);

    // Read all the packets
    while ((valread = read(client_sock, packet, maxPacketSize)) > 0)
    {
        // Read Sequence Number
        int sequenceNumber = 999;
        char stringSequenceNumber[128];
        read(client_sock, stringSequenceNumber, 128);
        sequenceNumber = atoi(stringSequenceNumber);

        // Print Packet Sent Message
        cout << "Packet "
             << sequenceNumber
             << " recieved" << endl;

        // Send Ack
        write(client_sock, stringSequenceNumber, 64);
        cout << "Ack "
             << sequenceNumber
             << " sent" << endl;

        // Read Packet and Write
        char packetWrite[maxPacketSize];
        for (int i = 0; i < maxPacketSize; i++)
        {
            packetWrite[i] = packet[i];
        }
        fwrite(packetWrite, 1, maxPacketSize, pFile);

        numPackets++;
        bzero(packet, maxPacketSize);
        bzero(stringSequenceNumber, 64);
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