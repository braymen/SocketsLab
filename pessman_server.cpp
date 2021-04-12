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
#include <mutex>
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
int currentWindow[500];
bool packetSending = false;
mutex threadLocker;

bool situationalErrors = false;

string shouldDropPackets = "n";
int whichDropPackets[100];
int totalDropPackets = 0;

string shouldFailCRC = "n";
int whichFailCRC[100];
int totalFailCRC = 0;

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

        threadLocker.lock();

        // Find frame to acknowledge
        for (int i = 0; i < windowSize; i++)
        {
            if (sequenceNumber == currentWindow[i])
            {
                windowAck[i] = true;
                i = windowSize;
            }
        }

        threadLocker.unlock();
    }
}

int main()
{
    // Client Settings
    struct sockaddr_in client_addr;

    // Initialize variables
    char ip[20] = "10.34.40.33";
    char port[20] = "9353";
    char sendFile[20];

    // Packet Tracking variables
    totalPackets = 0;
    int leftOverPacket = 0;
    int numPackets = 0;

    // Window Settings
    int packetSize = 10;
    windowSize = 8;
    string mode = "sr";
    long timeout = 100;

    // // IP
    // cout << "Connect to IP address: ";
    // cin >> ip;

    // // Port
    // cout << "Port #: ";
    // cin >> port;

    // Protocol
    cout << "Protocol Type ('GBN' or 'SR'): ";
    cin >> mode;

    // Packet Size
    cout << "Size of Packet: ";
    cin >> packetSize;

    // Timeout Interval
    cout << "Timeout Interval in Milliseconds (User-specified): ";
    cin >> timeout;

    // Size of Window
    cout << "Size of Sliding Window: ";
    cin >> windowSize;

    // Sequence Number Range
    cout << "Range of Sequence Numbers: ";
    cin >> maxSequenceNumber;

    // Situational Errors
    string t;
    cout << "Shall we run error simulation (y/n): ";
    cin >> t;

    if (!t.compare("y"))
    {
        situationalErrors = true;

        // Drop Packets?
        cout << "Want to drop packets? (y/n): ";
        cin >> shouldDropPackets;

        if (!shouldDropPackets.compare("y"))
        {
            cout << "How many packets to drop (max 100)?: ";
            cin >> totalDropPackets;

            int y = 0;
            for (int i = 0; i < totalDropPackets; i++)
            {
                cout << "\tPacket " << i + 1 << "/" << totalDropPackets << " to drop: ";
                cin >> y;
                whichDropPackets[i] = y;
            }
        }

        // Alter Packet after CRC?
        cout << "Want to alter packets after CRC? (y/n): ";
        cin >> shouldFailCRC;

        if (!shouldFailCRC.compare("y"))
        {
            cout << "How many packets will have bad CRC (max 100)?: ";
            cin >> totalFailCRC;

            int y = 0;
            for (int i = 0; i < totalFailCRC; i++)
            {
                cout << "\tPacket " << i + 1 << "/" << totalFailCRC << " to have bad CRC: ";
                cin >> y;
                whichFailCRC[i] = y;
            }
        }
    }

    // User Input
    cout << "File to Send: ";
    cin >> sendFile;

    // Setup Window
    char window[windowSize][packetSize];
    lar = 0;
    lfs = 0;

    // Get Start Time
    gettimeofday(&start_time, NULL);

    // Initilize Current Sequence Window
    for (int i = 0; i < windowSize; i++)
    {
        currentWindow[i] = i + 1;
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
            threadLocker.lock();

            // cout << "TOTAL: " << lar + 1 << " / " << totalPackets << endl;
            int lastSeq = currentWindow[windowSize - 1];

            // Shift it all
            for (int i = 0; i < windowSize; i++)
            {
                windowAck[i] = windowAck[i + 1];
                memcpy(window[i], window[i + 1], packetSize);
                packetTimes[i] = packetTimes[i + 1];
                currentWindow[i] = currentWindow[i + 1];
            }
            windowAck[windowSize - 1] = false;
            memcpy(window[windowSize - 1], "", packetSize);
            packetTimes[windowSize - 1] = start_time;

            if (lastSeq == maxSequenceNumber)
            {
                currentWindow[windowSize - 1] = 1;
            }
            else
            {
                currentWindow[windowSize - 1] = lastSeq + 1;
            }

            // Print Window
            cout << "Current Window = [";
            for (int i = 0; i < windowSize; i++)
            {
                cout << " " << currentWindow[i];
            }
            cout << " ]" << endl;

            lar++;

            threadLocker.unlock();
        }

        // Check if any packet timedout
        timeval currentTime;
        gettimeofday(&currentTime, NULL);

        for (int i = 0; i < windowSize; i++)
        {
            if (!windowAck[i] && packetTimes[i].tv_usec != start_time.tv_usec)
            {
                seconds = currentTime.tv_sec - packetTimes[i].tv_sec;
                useconds = currentTime.tv_usec - packetTimes[i].tv_usec;
                milli_time = ((seconds)*1000 + useconds / 1000.0);

                if (milli_time > timeout)
                {
                    threadLocker.lock();

                    packetSending = true;
                    cout << "Timed out packet" << endl;

                    char newPacket[packetSize];
                    for (int q = 0; q < packetSize; q++)
                    {
                        newPacket[q] = window[i][q];
                    }

                    // Write Packet
                    write(sockfd, newPacket, packetSize);

                    // Set the Sequence Number
                    int tmpSequenceNumber = ((lar + i) % maxSequenceNumber) + 1;

                    // Timed Out Packet Message
                    cout << "Packet " << tmpSequenceNumber << " *****Timed Out *****" << endl;

                    // Write Sequence Packet
                    sprintf(stringSequenceNumber, "%d", tmpSequenceNumber);
                    cout << "Packet " << stringSequenceNumber << " sent" << endl;
                    write(sockfd, stringSequenceNumber, 128);

                    // Get CRC and Send it
                    string data(newPacket);
                    boost::crc_32_type crc;
                    crc.process_bytes(data.data(), data.size());
                    unsigned int numNum = crc.checksum();
                    string s = to_string(numNum); // Max 10 size
                    char crcToSend[20];
                    strcpy(crcToSend, s.c_str());
                    write(sockfd, crcToSend, 20);

                    // Save current time for packet and add to timeout buffer
                    gettimeofday(&packetTimes[i], NULL);
                    errorPackets++;

                    // Retransmit Message
                    cout << "Packet " << tmpSequenceNumber << " Re-transmitted." << endl;

                    threadLocker.unlock();
                }
            }
        }

        // Put new packet in buffer
        bool sendPacket = false;
        int t;
        if (lfs - lar < windowSize && lfs < totalPackets)
        {
            threadLocker.lock();

            packetSending = true;
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

            threadLocker.unlock();
        }

        // Send out new packet
        if (sendPacket == true)
        {
            threadLocker.lock();

            // Dropping this Packet Check for Simulation
            bool dropThisPacket = false;
            if (!shouldDropPackets.compare("y"))
            {
                for (int z = 0; z < totalDropPackets; z++)
                {
                    if (lfs == whichDropPackets[z])
                    {
                        dropThisPacket = true;
                    }
                }
            }

            // Dropping this Packet Check for Simulation
            bool alterThisPacket = false;
            if (!shouldFailCRC.compare("y"))
            {
                for (int z = 0; z < totalFailCRC; z++)
                {
                    if (lfs == whichFailCRC[z])
                    {
                        alterThisPacket = true;
                    }
                }
            }

            if (!dropThisPacket)
            {
                // Write Packet
                write(sockfd, packet, packetSize);

                // Write Sequence Packet
                sprintf(stringSequenceNumber, "%d", currentSequenceNumber);
                cout << "Packet " << stringSequenceNumber << " sent" << endl;
                write(sockfd, stringSequenceNumber, 128);

                if (alterThisPacket)
                {
                    cout << "Alter packet" << endl;
                    write(sockfd, "badcrc", 20);
                }
                else
                {
                    // Get CRC and Send it
                    string data(packet);
                    boost::crc_32_type crc;
                    crc.process_bytes(data.data(), data.size());
                    unsigned int numNum = crc.checksum();
                    string s = to_string(numNum); // Max 10 size
                    char crcToSend[20];
                    strcpy(crcToSend, s.c_str());
                    write(sockfd, crcToSend, 20);
                }
            }

            // Save current time for packet and add to timeout buffer
            gettimeofday(&packetTimes[lfs - lar - 1], NULL);

            // Save Packet to Buffer
            memcpy(window[lfs - lar - 1], packet, packetSize);

            currentSequenceNumber++;
            if (currentSequenceNumber > maxSequenceNumber)
            {
                currentSequenceNumber = 1;
            }

            // Increments
            numPackets++;
            bzero(packet, packetSize);
            bzero(stringSequenceNumber, 128);
            packetSending = false;

            threadLocker.unlock();
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
    cout << "Total throughput: "
         << ((double)(totalPackets + errorPackets) / (double)(milli_time)) << endl;
    cout << "Effective throughput: "
         << ((double)(totalPackets) / (double)(milli_time)) << endl;

    // MD5 Hash
    cout << "MD5: " << endl;
    char sys[200] = "md5sum ";
    system(strcat(sys, sendFile));

    // Close the Socket
    close(sockfd);

    return 0;
}