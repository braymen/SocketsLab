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
#include <sys/time.h>
#include "crc.hpp"

using namespace std;

int sockfd;
bool windowAck[128] = {false};
int lar = 0;
int lfs = 0;
int currentSequenceNumber = 1;
int maxSequenceNumber = 32;
int totalPackets;
int errorPackets = 0;
struct timeval start_time, end_time;
long milli_time, seconds, useconds;
int windowSize;
int seqNumArray[512];

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

        // Sequence Number we need
        int larRelative = (lar % (maxSequenceNumber + 1));

        // SOMETHING ISNT WORKING HERE COMON
        bool isGood = false;
        for (int i = 0; i < windowSize; i++)
        {
            if (sequenceNumber == seqNumArray[i])
            {
                // For Error Messages
                isGood = true;

                // Setting Ack
                windowAck[i] = true;

                // Cut Loop
                i = windowSize;
            }
        }
    }
}

int main()
{
    // Client Settings
    struct sockaddr_in client_addr;

    // Initialize variables
    char ip[20] = "10.35.195.236";
    char port[20] = "9353";
    char sendFile[20];
    int packetSize = 2;
    totalPackets = 0;
    int leftOverPacket = 0;
    int numPackets = 0;
    long timeout = 5000;

    // Window Settings
    windowSize = 5;
    char mode[3] = "sw";
    char window[windowSize][packetSize];
    lar = 0;
    lfs = 0;

    // User Input
    cout << "File to be sent: ";
    cin >> sendFile;

    // Get Start Time
    gettimeofday(&start_time, NULL);

    // Setup Sequence Window
    for (int i = 0; i < windowSize; i++)
    {
        seqNumArray[i] = i + 1;
    }

    // Setup packet times array
    timeval packetTimes[windowSize];
    for (int i = 0; i < windowSize; i++)
    {
        packetTimes[i] = start_time;
    }

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
        while (windowAck[0] == true)
        {
            // Shift it all
            for (int i = 0; i < windowSize; i++)
            {
                windowAck[i] = windowAck[i + 1];
                memcpy(window[i], window[i + 1], packetSize);
                packetTimes[i] = packetTimes[i + 1];
                seqNumArray[i] = seqNumArray[i + 1];
            }
            windowAck[windowSize - 1] = false;
            memcpy(window[windowSize - 1], "\0", packetSize);
            packetTimes[windowSize - 1] = start_time;

            if (currentSequenceNumber + windowSize - 1 > maxSequenceNumber)
            {
                seqNumArray[windowSize - 1] = currentSequenceNumber + windowSize - 1 - maxSequenceNumber;
            }
            else
            {
                seqNumArray[windowSize - 1] = currentSequenceNumber + windowSize - 1;
            }

            cout << "Total: " << lar << " / " << totalPackets << endl;

            lar++;
        }

        // Check if any packet timedout
        timeval currentTime;
        gettimeofday(&currentTime, NULL);

        for (int i = 0; i < windowSize; i++)
        {
            if (packetTimes[i].tv_usec != start_time.tv_usec)
            {
                seconds = currentTime.tv_sec - packetTimes[i].tv_sec;
                useconds = currentTime.tv_usec - packetTimes[i].tv_usec;
                milli_time = ((seconds)*1000 + useconds / 1000.0);

                if (milli_time > timeout)
                {
                    cout << "Timed out packet" << endl;
                }
            }
        }

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

            // Get CRC and Send it
            string data(packet);
            boost::crc_32_type crc;
            crc.process_bytes(data.data(), data.size());
            unsigned int numNum = crc.checksum();
            string s = to_string(numNum); // Max 10 size
            char crcToSend[20];
            strcpy(crcToSend, s.c_str());
            write(sockfd, crcToSend, 20);

            // Save current time for packet and add to timeout buffer
            gettimeofday(&packetTimes[lfs - lar - 1], NULL);

            // Save Packet to Buffer
            memcpy(window[lfs - lar - 1], packet, packetSize);

            // Increments
            numPackets++;
            bzero(packet, packetSize);
            bzero(stringSequenceNumber, 128);
        }
    }

    // Shutdown Read thread
    ackThread.detach();

    // Get End Time
    gettimeofday(&end_time, NULL);

    // Getting Completion Times
    seconds = end_time.tv_sec - start_time.tv_sec;    //seconds
    useconds = end_time.tv_usec - start_time.tv_usec; //milliseconds
    milli_time = ((seconds)*1000 + useconds / 1000.0);

    // Sent Success
    cout << "Session successfully terminated" << endl;
    cout << endl;

    // Additional Information
    cout << "Number of original packets sent: " << totalPackets << endl;
    cout << "Number of retransmitted packets: " << errorPackets << endl;
    cout << "Total elapsed time: " << milli_time << "ms" << endl;
    cout << "Total throughput (Mbps): "
         << ((double)(fileSize * 1000.0) / (double)(milli_time * 1000.0)) << endl;
    cout << "Effective throughput: "
         << "N/A" << endl;

    // MD5 Hash
    cout << "MD5: " << endl;
    char sys[200] = "md5sum ";
    system(strcat(sys, sendFile));

    // Close the Socket
    close(sockfd);

    return 0;
}