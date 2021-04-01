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

#define PACKET_MAX_SIZE 10
#define TOTAL_PACKET_MAX_SIZE 10

int sockfd;

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

void listenForClient()
{
    int i = 0;
    while (true)
    {
        // Wait for Acknowledge
        char packetStatus[10];
        read(sockfd, packetStatus, 10);
        cout << "Ack " << i << " recieved" << endl;
        i++;
    }
}

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

    // User Input
    cout << "File to be sent: ";
    cin >> sendFile;

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

        // Encrypt Packet (CRC Packet)

        //xorPacket(packet, encryptKey, t);

        // Print Packet
        // if (numPackets == 0 || numPackets == 1 || numPackets == totalPackets - 2 || numPackets == totalPackets - 1)
        // {
        //     printPacket(packet, numPackets, 's', t);
        // }

        // Write Packet
        write(sockfd, packet, t + PACKET_MAX_SIZE);

        // Print Packet Sent Message
        cout << "Packet " << i << " sent" << endl;

        numPackets++;
        bzero(packet, packetSize + PACKET_MAX_SIZE);
    }

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