/* e1_dll.cpp
Created from caller.cpp to provide function API to be used in python
EYafuso 2019 */

#include <iostream>
#include "stdio.h"
#include "windows.h"
#include "edl.h"

#define MINIMUM_DATA_PACKETS_TO_READ 10

extern "C" __declspec(dllexport) int setSampleRate(int nSampleRate)
{
    EdlCommandStruct_t commandStruct;
    EdlErrorCode_t res;

    if (nSampleRate < 1 || nSampleRate > 6) {return -1;}
    commandStruct.radioId = nSampleRate;
    res = setCommand(EdlCommandSamplingRate, commandStruct, true);
    if (res != 0) {return res;}

    return 0;
}

extern "C" __declspec(dllexport) int setRange(int nRange)
{
    EdlCommandStruct_t commandStruct;
    EdlErrorCode_t res;

    if (nRange < 0 || nRange > 1) {return -1;}
    commandStruct.radioId = nRange;
    res = setCommand(EdlCommandRange, commandStruct, false);
    if (res != 0) {return res;}

    return 0;
}

extern "C" __declspec(dllexport) int setBandwidth(int nBandwidth)
{
    EdlCommandStruct_t commandStruct;
    EdlErrorCode_t res;

    if (nBandwidth < 0 || nBandwidth > 3) {return -1;}

    commandStruct.radioId = nBandwidth;
    res = setCommand(EdlCommandFinalBandwidth, commandStruct, true);
    if (res != 0) {return res;}

    return 0;
}


extern "C" __declspec(dllexport) int compensateDigitalOffset()
{
    EdlCommandStruct_t commandStruct;
    EdlErrorCode_t res;

	/*! Select the constant protocol: protocol 0. */
    commandStruct.value = 0.0;
    res = setCommand(EdlCommandMainTrial, commandStruct, false);
    if (res != 0) {return res;}

	/*! Set the vHold to 0mV. */
    commandStruct.value = 0.0;
    res = setCommand(EdlCommandVhold, commandStruct, false);
    if (res != 0) {return res;}

	/*! Apply the protocol. */
    res = setCommand(EdlCommandApplyProtocol, commandStruct, true);
    if (res != 0) {return res;}

	/*! Start the digital compensation. */
    commandStruct.checkboxChecked = EDL_CHECKBOX_CHECKED;
    res = setCommand(EdlCommandDigitalCompensation, commandStruct, true);
    if (res != 0) {return res;}

	/*! Wait for some seconds. */
	Sleep(5000);

	/*! Stop the digital compensation. */
    commandStruct.checkboxChecked = EDL_CHECKBOX_UNCHECKED;
    res = setCommand(EdlCommandDigitalCompensation, commandStruct, true);
    if (res != 0) {return res;}

    return 0;
}


void setSealTestProtocol()
{
    EdlCommandStruct_t commandStruct;

    /*! Select the seal test protocol: protocol 1. */
    commandStruct.value = 2.0;
    setCommand(EdlCommandMainTrial, commandStruct, false);

    /*! Set the vHold to 0mV. */
    commandStruct.value = 0.0;
    setCommand(EdlCommandVhold, commandStruct, false);

    /*! Set the pulse amplitude to 50mV: 100mV positive to negative delta voltage. */
    commandStruct.value = 50.0;
    setCommand(EdlCommandVstep, commandStruct, false);

    /*! Set the pulse period to 20ms. */
    commandStruct.value = 20.0;
    setCommand(EdlCommandTpu, commandStruct, false);

    /*! Set the command period to 50ms. */
    commandStruct.value = 50.0;
    setCommand(EdlCommandTpe, commandStruct, false);

    /*! Apply the protocol. */
    setCommand(EdlCommandApplyProtocol, commandStruct, true);
}

EdlErrorCode_t readAndSaveSomeData(FILE * f)
{
    EdlErrorCode_t res;

	/*! Declare an #EdlDeviceStatus_t variable to collect the device status. */
    EdlDeviceStatus_t status;

	/*! Declare a variable to collect the number of read data packets. */
    unsigned int readPacketsNum;

	/*! Declare a vector to collect the read data packets. */
    std::vector <float> data;

    Sleep(500);

    std::cout << "purge old data" << std::endl;
	/*! Get rid of data acquired during the device configuration */
	res = purgeData();

	/*! If the purgeData returns an error code output an error and return. */
    if (res != EdlSuccess) {
        std::cout << "failed to purge data" << std::endl;
        return res;
    }

    std::cout << "collecting data... ";
	unsigned int c;
    for (c = 0; c < 1e3; c++) {
        res = getDeviceStatus(status);
        if (res != EdlSuccess) {
            std::cout << "failed to get device status" << std::endl;
            return res;
        }

		if (status.bufferOverflowFlag) {
			std::cout << std::endl << "lost some data due to buffer overflow; increase MINIMUM_DATA_PACKETS_TO_READ to improve performance" << std::endl;
		}

		if (status.lostDataFlag) {
			std::cout << std::endl << "lost some data from the device; decrease sampling frequency or close unused applications to improve performance" << std::endl;
			std::cout << "data loss may also occur immediately after sending a command to the device" << std::endl;
		}

        if (status.availableDataPackets >= MINIMUM_DATA_PACKETS_TO_READ) {
		    /*! If at least MINIMUM_DATA_PACKETS_TO_READ data packet are available read them. */
			res = readData(status.availableDataPackets, readPacketsNum, data);

	        /*! If the device is not connected output an error, close the file for data storage and return. */
            if (res == EdlDeviceNotConnectedError) {
				std::cout << "the device is not connected" << std::endl;
                fclose(f);
				return res;

			} else {
	            /*! If the number of available data packets is lower than the number of required packets output an error, but the read is performed nonetheless
				 * with the available data. */
				if (res == EdlNotEnoughAvailableDataError) {
					std::cout << "not enough available data, only "  << readPacketsNum << " packets have been read" << std::endl;
				}

	            /*! The output vector consists of \a readPacketsNum data packets of #EDL_CHANNEL_NUM floating point data each.
				 * The first item in each data packet is the value voltage channel [mV];
				 * the following items are the values of the current channels either in pA or nA, depending on value assigned to #EdlCommandSamplingRate. */
                for (unsigned int readPacketsIdx = 0; readPacketsIdx < readPacketsNum; readPacketsIdx++) {
                    for (unsigned int channelIdx = 0; channelIdx < EDL_CHANNEL_NUM; channelIdx++) {
                        fwrite((unsigned char *)&data.at(readPacketsIdx*EDL_CHANNEL_NUM+channelIdx), sizeof(float), 1, f);
                    }
                }
			}
        } else {
		    /*! If the read was not performed wait 1 ms before trying to read again. */
            Sleep(1);
		}
    }
	std::cout << "done" << std::endl;

    return res;
}

void configureWorkingModality() {
	/*! Declare an #EdlCommandStruct_t to be used as configuration for the commands. */
    EdlCommandStruct_t commandStruct;

	/*! Set the sampling rate to 5kHz. Stack the command (do not apply). */
    commandStruct.radioId = EDL_RADIO_SAMPLING_RATE_5_KHZ;
    setCommand(EdlCommandSamplingRate, commandStruct, false);

	/*! Set the current range to 200pA. Stack the command (do not apply). */
    commandStruct.radioId = EDL_RADIO_RANGE_200_PA;
    setCommand(EdlCommandRange, commandStruct, false);

	/*! Disable current filters (final bandwidth equal to half sampling rate). Apply all of the stacked commands. */
    commandStruct.radioId = EDL_RADIO_FINAL_BANDWIDTH_SR_2;
    setCommand(EdlCommandFinalBandwidth, commandStruct, true);
}

extern "C" __declspec(dllexport) int initEDL()
{
    init();

    EdlErrorCode_t res;

    std::vector <std::string> devices;
    std::ios::sync_with_stdio(true);

    res = detectDevices(devices);
    if (res != EdlSuccess) {return res;}

    res = connectDevice(devices.at(0));
    if (res != EdlSuccess) {return res;}

    configureWorkingModality();

    compensateDigitalOffset();

    return 0;
}

extern "C" __declspec(dllexport) int closeEDL()
{
    EdlErrorCode_t res;

    unsigned int c = 0;
    while (c++ < 1e3)
    {
		res = disconnectDevice();
		if (res == EdlSuccess) {break;}
        Sleep(1);
    }

    if (res != EdlSuccess) {return -1;}

    return 0;
}
