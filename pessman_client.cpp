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
int currentSequenceNumber = 1;
int maxSequenceNumber = 8;

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

    int windowSize = 5;
    char mode[3] = "sw";
    int currentWindow[windowSize];

    // User Input
    cout << "Save file to: ";
    cin >> saveFile;

    // Initilize Current Sequence Window
    for (int i = 0; i < windowSize; i++)
    {
        currentWindow[i] = i + 1;
    }

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

    // Setup Packet Buffer using Max Packet Size
    char packetBuffer[windowSize][maxPacketSize];

    // Get total Packets
    char data_totalPackets[1024];
    read(client_sock, data_totalPackets, 1024);
    totalPackets = atoi(data_totalPackets);

    char packet[maxPacketSize];
    bzero(packet, maxPacketSize);

    while (lfr < totalPackets)
    {
        bool isBuffered = false;
        // Check if next packet is one saved in buffer
        // Shift things

        // If next packet is buffered, write that packet

        // If next packet is not buffered, read for next
        // If out of frame, disregard packet
        // If out of order, add packet to buffer and send ack
        if (isBuffered == false)
        {
            // Read Next Packet
            valread = read(client_sock, packet, maxPacketSize);

            // Read Sequence Number
            int sequenceNumber = 999;
            char stringSequenceNumber[128];
            read(client_sock, stringSequenceNumber, 128);
            sequenceNumber = atoi(stringSequenceNumber);

            // Print Packet Sent Message
            cout << "Packet " << sequenceNumber << " recieved" << endl;

            // Make sure Sequence Number is within Frame
            bool isGood = false;
            for (int i = 0; i < windowSize; i++)
            {
                if (sequenceNumber == currentWindow[i])
                {
                    isGood = true;

                    // Cut Loop
                    i = windowSize;
                }
            }

            if (isGood)
            {
                // Check to see if it's out of order
                if (sequenceNumber != currentSequenceNumber)
                {
                    // Add packet to the buffer
                    memcpy(packetBuffer[sequenceNumber - currentSequenceNumber], packet, maxPacketSize);

                    // Send Ack
                    write(client_sock, stringSequenceNumber, 64);
                    cout << "Ack " << sequenceNumber << " sent" << endl;
                }
                else
                {
                    // Packet is completely good, lets do the magic!

                    // Send Ack
                    write(client_sock, stringSequenceNumber, 64);
                    cout << "Ack " << sequenceNumber << " sent" << endl;

                    // Print Window
                    cout << "Current Window = [";
                    for (int i = 0; i < windowSize; i++)
                    {
                        cout << " " << currentWindow[i];
                    }
                    cout << " ]" << endl;

                    // Read Packet and Write
                    char packetWrite[maxPacketSize];
                    bool extraStuff = false;
                    int extraStuffIndex = 0;
                    for (int i = 0; i < maxPacketSize; i++)
                    {
                        packetWrite[i] = packet[i];
                        if (packet[i] == '\0' && extraStuff == false)
                        {
                            extraStuff = true;
                            extraStuffIndex = i;
                        }
                    }

                    // Remove null characters
                    if (extraStuff == true)
                    {
                        char newPacketWrite[extraStuffIndex];
                        memcpy(newPacketWrite, packetWrite, extraStuffIndex);
                        fwrite(newPacketWrite, 1, extraStuffIndex, pFile);
                    }
                    else
                    {
                        fwrite(packetWrite, 1, maxPacketSize, pFile);
                    }

                    // Moving everything up
                    lfr++;
                    laf++;
                    numPackets++;
                    currentSequenceNumber++;
                    if (currentSequenceNumber > maxSequenceNumber)
                    {
                        currentSequenceNumber = 1;
                    }
                    bzero(packet, maxPacketSize);
                    bzero(stringSequenceNumber, 64);

                    // Shift Buffer and Sequence Numbers
                    for (int i = 0; i < windowSize; i++)
                    {
                        currentWindow[i] = currentWindow[i + 1];
                        memcpy(packetBuffer[i], packetBuffer[i + 1], maxPacketSize);
                    }

                    memcpy(packetBuffer[windowSize - 1], "\0", maxPacketSize);

                    if (currentSequenceNumber + windowSize - 1 > maxSequenceNumber)
                    {
                        currentWindow[windowSize - 1] = currentSequenceNumber + windowSize - 1 - maxSequenceNumber;
                    }
                    else
                    {
                        currentWindow[windowSize - 1] = currentSequenceNumber + windowSize - 1;
                    }
                }
            }
            else
            {
                cout << "Packet " << sequenceNumber << " is not within frame!" << endl;
            }
        }
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

// // Read all the packets
// while ((valread = read(client_sock, packet, maxPacketSize)) > 0)
// {
//     // Read Sequence Number
//     int sequenceNumber = 999;
//     char stringSequenceNumber[128];
//     read(client_sock, stringSequenceNumber, 128);
//     sequenceNumber = atoi(stringSequenceNumber);

//     // Check if out-of-order and buffer if so
//     if (sequenceNumber != currentSequenceNumber) {

//     }

//     // Print Packet Sent Message
//     cout << "Packet "
//          << sequenceNumber
//          << " recieved" << endl;

//     // Send Ack
//     write(client_sock, stringSequenceNumber, 64);
//     cout << "Ack "
//          << sequenceNumber
//          << " sent" << endl;

//     // Read Packet and Write
//     char packetWrite[maxPacketSize];
//     bool extraStuff = false;
//     int extraStuffIndex = 0;
//     for (int i = 0; i < maxPacketSize; i++)
//     {
//         packetWrite[i] = packet[i];
//         if (packet[i] == '\0' && extraStuff == false)
//         {
//             extraStuff = true;
//             extraStuffIndex = i;
//         }
//     }

//     // Remove null characters
//     if (extraStuff == true)
//     {
//         char newPacketWrite[extraStuffIndex];
//         memcpy(newPacketWrite, packetWrite, extraStuffIndex);
//         fwrite(newPacketWrite, 1, extraStuffIndex, pFile);
//     }
//     else
//     {
//         fwrite(packetWrite, 1, maxPacketSize, pFile);
//     }

//     numPackets++;
//     bzero(packet, maxPacketSize);
//     bzero(stringSequenceNumber, 64);
// }

// // Recieve Success
// cout << "Recieve Success!" << endl;

// // Close file
// fclose(pFile);

// // MD5 Hash
// cout << "MD5: " << endl;
// char sys[200] = "md5sum ";
// system(strcat(sys, saveFile));

// // Close socket
// close(client_sock);

// return 0;
// }