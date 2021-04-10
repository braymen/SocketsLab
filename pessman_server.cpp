/*
 * p0 is 10.35.195.251
 * p1 is 10.34.40.33
 * p2 is 10.35.195.250
 * p3 is 10.35.195.236.
 * 
 * 
 * GBN
 *      Server: Many
 *      Reciever: 1
 * 
 * SR
 *      Server: Many
 *      Reciever: Many
 * 
 * Stop and Wait
 *      Server: 1
 *      Reciever: 1
 * 
 * 
 * LAR = Start
 * LFS = End
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
#include <thread>

using namespace std;

int sockfd;
bool windowAck[128];
int lar = 0;
int lfs = 0;
int currentSequenceNumber = 1;
int maxSequenceNumber = 32;
int totalPackets;

void listenForClient()
{
    int sequenceNumber = 999;
    while (lar < totalPackets)
    {
        // Wait for Acknowledge
        char stringSequenceNumber[64];
        read(sockfd, stringSequenceNumber, 64);
        sequenceNumber = atoi(stringSequenceNumber);
        cout << "Ack " << stringSequenceNumber << " recieved" << endl;
        lar++;
    }
}

int main()
{
    // Client Settings
    struct sockaddr_in client_addr;

    // Initialize variables
    char ip[20] = "10.35.195.236";
    char port[20] = "9373";
    char sendFile[20];
    int packetSize = 10;
    totalPackets = 0;
    int leftOverPacket = 0;
    int numPackets = 0;

    // Window Settings
    int windowSize = 8;
    char mode[3] = "sw";
    char window[windowSize][packetSize];
    lar = 0;
    lfs = 0;

    // User Input
    cout << "File to be sent: ";
    cin >> sendFile;

    // Cleaning Address
    memset(&client_addr, 0, sizeof(client_addr));

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
    char packet[packetSize];

    // Convert t to max packet byte number and add to packet
    char packetSizeToSend[packetSize];
    sprintf(packetSizeToSend, "%d", packetSize);

    // Send Size
    write(sockfd, packetSizeToSend, 1024);

    // Send Total
    char totalPacketSizeToSend[1024];
    sprintf(totalPacketSizeToSend, "%d", totalPackets);
    write(sockfd, totalPacketSizeToSend, 1024);

    // Create a thread for listening for Acks
    thread ackThread(listenForClient);

    char stringSequenceNumber[128];

    while (lar < totalPackets)
    {
        // Check if lar is good and shift everything
        // if (windowAck[lar] == true)
        // {
        //     lar += 1;
        //     for (int i = 0; i < windowSize; i++)
        //     {
        //         windowAck[i] = windowAck[i + 1];
        //     }
        //     windowAck[windowSize - 1] = false;
        // }

        // Check if any packet timedout

        // Put new packet in buffer
        bool sendPacket = false;
        int t;
        if (lfs - lar < windowSize && lfs < totalPackets)
        {
            t = packetSize;
            if (numPackets == totalPackets - 1 && leftOverPacket != 0)
            {
                t = leftOverPacket;
            }
            for (int j = 0; j < t; j++)
            {
                packet[j] = (char)fgetc(pFile);
            }

            lfs++;
            sendPacket = true;
        }

        // Send out packet and/or timedout packets
        if (sendPacket == true)
        {
            // Write Packet
            write(sockfd, packet, packetSize);

            // Write Sequence Packet
            sprintf(stringSequenceNumber, "%d", currentSequenceNumber);
            cout << "Packet " << stringSequenceNumber << " sent" << endl;
            write(sockfd, stringSequenceNumber, 128);
            currentSequenceNumber++;
            if (currentSequenceNumber > maxSequenceNumber)
            {
                currentSequenceNumber = 1;
            }

            // Add packet to buffer
            // window[(lfs - 1) - lar] = packet;

            // Increments
            numPackets++;
            bzero(packet, packetSize);
            bzero(stringSequenceNumber, 128);
        }
    }

    // Shutdown Read thread
    ackThread.detach();

    // Sent Success
    cout << "Send Success!" << endl;

    // MD5 Hash
    cout << "MD5: " << endl;
    char sys[200] = "md5sum ";
    system(strcat(sys, sendFile));

    // Close the Socket
    close(sockfd);

    return 0;
}