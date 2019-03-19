/*! \file caller.cpp
 * \brief Sample program to connect to an eONE-HS device, set a working configuration and read some data.
 */
#include <iostream>
#include "stdio.h"
#include "windows.h"
#include "edl.h"


#define MINIMUM_DATA_PACKETS_TO_READ 10


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

/*! \fn compensateDigitalOffset
 * \brief Compensate digital offset due to electrical load.
 */
void compensateDigitalOffset() {
	/*! Declare an #EdlCommandStruct_t to be used as configuration for the commands. */
    EdlCommandStruct_t commandStruct;

	/*! Select the constant protocol: protocol 0. */
    commandStruct.value = 0.0;
    setCommand(EdlCommandMainTrial, commandStruct, false);

	/*! Set the vHold to 0mV. */
    commandStruct.value = 0.0;
    setCommand(EdlCommandVhold, commandStruct, false);

	/*! Apply the protocol. */
    setCommand(EdlCommandApplyProtocol, commandStruct, true);

	/*! Start the digital compensation. */
    commandStruct.checkboxChecked = EDL_CHECKBOX_CHECKED;
    setCommand(EdlCommandDigitalCompensation, commandStruct, true);

	/*! Wait for some seconds. */
	Sleep(5000);

	/*! Stop the digital compensation. */
    commandStruct.checkboxChecked = EDL_CHECKBOX_UNCHECKED;
    setCommand(EdlCommandDigitalCompensation, commandStruct, true);
}

/*! \fn setSealTestProtocol
 * \brief Set the parameters and start a seal test protocol.
 */
void setSealTestProtocol() {
    /*! Declare an #EdlCommandStruct_t to be used as configuration for the commands. */
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

/*! \fn readAndSaveSomeData
 * \brief Reads data from the EDL device and writes them on an open file.
 */
EdlErrorCode_t readAndSaveSomeData(FILE * f) {
    /*! Declare an #EdlErrorCode_t to be returned from #EDL methods. */
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

	/*! Start collecting data. */

    std::cout << "collecting data... ";
	unsigned int c;
    for (c = 0; c < 1e3; c++) {
		/*! Get current status to know the number of available data packets EdlDeviceStatus_t::availableDataPackets. */
        res = getDeviceStatus(status);

        /*! If the getDeviceStatus returns an error code output an error and return. */
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

/*! \fn main
 * \brief Application entry point.
 */
int main() {
	/*! Initialization. */
    init();

	/*! Declare an #EdlErrorCode_t to be returned from #EDL methods. */
    EdlErrorCode_t res;

	/*! Initialize a vector of strings to collect the detected devices. */
    std::vector <std::string> devices;

    std::ios::sync_with_stdio(true);

	/*! Detect plugged in devices. */
    res = detectDevices(devices);

	/*! If none is found output an error and return. */
    if (res != EdlSuccess) {
        std::cout << "could not detect devices" << std::endl;
        return -1;
    }

    std::cout << "first device found " << devices.at(0) << std::endl;

	/*! If at list one device is found connect to the first one. */
    res = connectDevice(devices.at(0));

    std::cout << "connecting... ";
	/*! If the connectDevice returns an error code output an error and return. */
    if (res != EdlSuccess) {
        std::cout << "connection error" << std::endl;
        return -1;
    }
	std::cout << "done" << std::endl;

	/*! Configure the device working modality. */
    std::cout << "configuring working modality" << std::endl;
    configureWorkingModality();

	/*! Compensate for digital offset. */
	std::cout << "performing digital offset compensation... ";
    compensateDigitalOffset();
	std::cout << "done" << std::endl;

    std::cout << "applying seal test protocol" << std::endl;
    /*! Apply a seal test protocol. */
    setSealTestProtocol();

	/*! Initialize a file descriptor to store the read data packets. */
    FILE * f;
    f = fopen("data.dat", "wb+");

    res = readAndSaveSomeData(f);
    if (res != EdlSuccess) {
        std::cout << "failed to read data" << std::endl;
        return -1;
    }

	/*! Close the file for data storage. */
    fclose(f);

	/*! Try to disconnect the device.
	 * \note Data reading is performed in a separate thread started by connectDevice.
	 * The while loop may be useful in case few operations are performed between before calling disconnectDevice,
	 * to ensure that the connection is fully established before trying to disconnect. */

	std::cout << "disconnecting... ";
    unsigned int c = 0;
    while (c++ < 1e3) {
		res = disconnectDevice();
		if (res == EdlSuccess) {
            std::cout << "done" << std::endl;
			break;
		}

		/*! If the disconnection was unsuccessful wait 1 ms before trying to disconnect again. */
        Sleep(1);
    }

	/*! If the disconnectDevice returns an error code after trying for 1 second (1e3 * 1ms) output an error and return. */
    if (res != EdlSuccess) {
        std::cout << "disconnection error" << std::endl;
        return -1;
    }

    return 0;
}
/*! [caller_snippet] */
