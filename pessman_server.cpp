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
#include <thread>
#include <chrono>

#define PACKET_MAX_SIZE 10
#define TOTAL_PACKET_MAX_SIZE 10

int sockfd;
bool windowAck[128];
// milliseconds windowTimeouts[128];
int lar = 0;
int lfs = 0;
int currentSequenceNumber = 1;
int maxSequenceNumber = 32;

using namespace std;
using namespace std::chrono;

/*
 * p0 is 10.35.195.251
 * p1 is 10.34.40.33
 * p2 is 10.35.195.250
 * p3 is 10.35.195.236
 */

void listenForClient()
{
    int sequenceNumber = 999;
    while (true)
    {
        // Wait for Acknowledge
        char stringSequenceNumber[64];
        read(sockfd, stringSequenceNumber, 64);
        sequenceNumber = atoi(stringSequenceNumber);
        cout << "Ack " << stringSequenceNumber << " recieved" << endl;
        lar++;
    }
}

/*

GBN: 
    Server: Many
    Reciever: 1

SR:
    Server: Many
    Reciever: Many

(Stop and Wait):
    Server: 1
    Reciever: 1

*/

/*
 LAR - START
 LFS - END
*/

int main()
{
    // Client Settings
    struct sockaddr_in client_addr;

    // Initialize variables
    char ip[20] = "10.35.195.236";
    char port[20] = "9372";
    char sendFile[20];
    int packetSize = 20;
    int totalPackets = 0;
    int leftOverPacket = 0;
    int numPackets = 0;

    int windowSize = 1;
    char mode[3] = "sw";
    int sequenceNumbers = 2;

    // User Input
    cout << "File to be sent: ";
    cin >> sendFile;

    // Sliding Window Setup
    char window[windowSize][packetSize + PACKET_MAX_SIZE];
    lar = 1; // last acknowledgement recieved
    lfs = 1; // last frame sent

    // Client address initialization
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(atoi(port));
    client_addr.sin_addr.s_addr = inet_addr(ip);
    inet_pton(AF_INET, ip, &client_addr.sin_addr);

    // Open the socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("Socket Creation");
        return 0;
    }

    // Connect socket
    if (connect(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0)
    {
        perror("Connect");
        return 0;
    }

    // Get File
    FILE *pFile = fopen(sendFile, "r");
    fseek(pFile, 0, SEEK_END);
    long fileSize = ftell(pFile);
    rewind(pFile);

    // Calculate Packets from file size
    while (fileSize >= packetSize)
    {
        fileSize -= packetSize;
        totalPackets++;
    }

    // Adds an extra packet if there is less than a packet
    if (fileSize != 0)
    {
        leftOverPacket = fileSize;
        totalPackets++;
    }

    // Init Packet with max packet byte header
    char packet[packetSize + PACKET_MAX_SIZE];

    // Convert t to max packet byte number and add to packet
    char packetSizeToSend[PACKET_MAX_SIZE + sizeof(char)];
    sprintf(packetSizeToSend, "%d", packetSize);

    // Send Size
    write(sockfd, packetSizeToSend, PACKET_MAX_SIZE);

    // Send Total
    char totalPacketSizeToSend[TOTAL_PACKET_MAX_SIZE + sizeof(char)];
    sprintf(totalPacketSizeToSend, "%d", totalPackets);
    write(sockfd, totalPacketSizeToSend, TOTAL_PACKET_MAX_SIZE);

    // Create a thread for listening for Acks
    thread ackThread(listenForClient);

    char stringSequenceNumber[64 + sizeof(char)];

    while (lar < totalPackets)
    {
        // Check if lar is good and shift everything
        if (windowAck[lar] == true)
        {
            lar += 1;
            for (int i = 0; i < windowSize; i++)
            {
                windowAck[i] = windowAck[i + 1];
            }
            windowAck[windowSize - 1] = false;
        }

        // Check if any packet timedout

        // Put new packet in buffer
        bool sendPacket = false;
        int t;
        if (lfs - lar < windowSize)
        {
            t = packetSize;
            if (numPackets == totalPackets - 1 && leftOverPacket != 0)
            {
                t = leftOverPacket;
            }
            for (int j = 0; j < t; j++)
            {
                packet[j + PACKET_MAX_SIZE] = (char)fgetc(pFile);
            }

            // Convert t to max packet byte number and add to packet
            char sizeToSend[PACKET_MAX_SIZE + sizeof(char)];
            sprintf(sizeToSend, "%d", t);

            for (int i = 0; i < PACKET_MAX_SIZE; i++)
            {
                packet[i] = sizeToSend[i];
            }
            lfs++;
            sendPacket = true;
        }

        // Send out packet and/or timedout packets
        if (sendPacket == true)
        {
            // Write Packet
            write(sockfd, packet, t + PACKET_MAX_SIZE);

            // Write Sequence Packet
            sprintf(stringSequenceNumber, "%d", currentSequenceNumber);
            cout << "Packet " << stringSequenceNumber << " sent" << endl;
            write(sockfd, stringSequenceNumber, 64);
            currentSequenceNumber++;
            if (currentSequenceNumber > maxSequenceNumber)
            {
                currentSequenceNumber = 1;
            }

            // Add packet to buffer
            // window[(lfs - 1) - lar] = packet;

            // Increments
            numPackets++;
            bzero(packet, packetSize + PACKET_MAX_SIZE);
            bzero(stringSequenceNumber, 64 + sizeof(char));
        }
    }

    /*
    for (int i = 0; i < totalPackets; i++)
    {
        int t = packetSize;
        if (i == totalPackets - 1 && leftOverPacket != 0)
        {
            t = leftOverPacket;
        }
        for (int j = 0; j < t; j++)
        {
            packet[j + PACKET_MAX_SIZE] = (char)fgetc(pFile);
        }

        // Convert t to max packet byte number and add to packet
        char sizeToSend[PACKET_MAX_SIZE + sizeof(char)];
        sprintf(sizeToSend, "%d", t);

        for (int i = 0; i < PACKET_MAX_SIZE; i++)
        {
            packet[i] = sizeToSend[i];
        }

        // Write Packet
        write(sockfd, packet, t + PACKET_MAX_SIZE);

        // Write Sequence Packet
        sprintf(stringSequenceNumber, "%d", currentSequenceNumber);
        cout << "Packet " << stringSequenceNumber << " sent" << endl;
        write(sockfd, stringSequenceNumber, 64);
        currentSequenceNumber++;
        if (currentSequenceNumber > maxSequenceNumber)
        {
            currentSequenceNumber = 1;
        }

        // Increments
        numPackets++;
        bzero(packet, packetSize + PACKET_MAX_SIZE);
        bzero(stringSequenceNumber, 64 + sizeof(char));
    }
    */

    // Shutdown Read thread
    ackThread.detach();

    cout << "Send Success!" << endl;
    //write(sockfd, "", 0);

    // MD5 Hash
    cout << "MD5: " << endl;
    char sys[200] = "md5sum ";
    system(strcat(sys, sendFile));

    // Close the Socket
    close(sockfd);

    return 0;
}