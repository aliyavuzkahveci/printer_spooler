// main.cpp : Defines the entry point for the console application.
//
#include <csignal>
#include <algorithm>

#include "SpoolerTester.h"

constexpr auto HELP = "-h";

constexpr auto UC_Q = 0x51;
constexpr auto LC_Q = 0x71;

bool terminationReceived = false;

void signalHandler(int sigNum)
{
	if (sigNum == SIGINT)
		std::cout << "Interactive Attention Signal received!" << std::endl;
	else if (sigNum == SIGILL)
		std::cout << "Illegal Instruction Signal received!" << std::endl;
	else if (sigNum == SIGFPE)
		std::cout << "Erroneous Arithmetic Operation Signal received!" << std::endl;
	else if (sigNum == SIGSEGV)
		std::cout << "Invalid Access to Storage Signal received!" << std::endl;
	else if (sigNum == SIGTERM)
		std::cout << "Termination Request Signal received!" << std::endl;
	else if (sigNum == SIGBREAK)
		std::cout << "Ctrl-Break Sequence Signal received!" << std::endl;
	else if (sigNum == SIGABRT)
		std::cout << "Abnormal Termination Signal received!" << std::endl;
	else if (sigNum == SIGABRT_COMPAT)
		std::cout << "Abnormal Termination Signal (compatible with other platforms) received!" << std::endl;
	else
	{
		std::cout << "Unknown signal received!" << std::endl;
		return; //does not enter to termination state!
	}

	terminationReceived = true;
}

using namespace SpoolerTest;

int main(int argc, char* argv[])
{
	int returnValue = 0;
	std::string printerName;
	SpoolerTester_Ptr spoolerTester = SpoolerTester_Ptr(new SpoolerTester());

	/*register termination signals to gracefully shut down*/
	//std::cout << "Registering Signals to catch when occured!" << std::endl;
	signal(SIGINT, signalHandler);			// SIGINT -> Receipt of an interactive attention signal.
	signal(SIGILL, signalHandler);			// SIGILL -> Detection of an illegal instruction.
	signal(SIGFPE, signalHandler);			// SIGFPE -> An erroneous arithmetic operation, such as a divide by zero or an operation resulting in overflow.
	signal(SIGSEGV, signalHandler);			// SIGSEGV -> An invalid access to storage.
	signal(SIGTERM, signalHandler);			//SIGTERM -> A termination request sent to the program.
	signal(SIGBREAK, signalHandler);		// SIGBREAK -> Ctrl-Break sequence
	signal(SIGABRT, signalHandler);			// SIGABRT -> Abnormal termination of the program, such as a call to abort.
	signal(SIGABRT_COMPAT, signalHandler);	// SIGABRT_COMPAT -> SIGABRT compatible with other platforms, same as SIGABRT
	/*register termination signals to gracefully shut down*/

	if (argc == 1)
	{
		std::vector<std::string> spooledPrinters = spoolerTester->getSpooledPrinters();
		std::cout << "Spooled Printer List:" << std::endl;
		//std::for_each(spooledPrinters.begin(), spooledPrinters.end(), [](std::string& printerName) { std::cout << printerName << std::endl; });
		for (auto iter : spooledPrinters)
		{
			std::cout << iter << std::endl;
		}
		std::cout << "Please select one of the above printers: " << std::endl;
		std::getline(std::cin, printerName);

		if (std::find(spooledPrinters.begin(), spooledPrinters.end(), printerName) == spooledPrinters.end())
		{
			std::cout << "Non-existent printer name is given! Closing..." << std::endl;
			return -1;
		}
	}
	else if (argc == 2 && std::strcmp(argv[1], HELP) == 0)
	{
		returnValue = 1; //HELP requested!
	}
	else
	{
		std::cout << "You have entered wrong inputs..." << std::endl;
		returnValue = -1;
	}
	
	if (returnValue != 0)
	{
		std::cout << "No argument is required to start the program" << std::endl;
		std::cout << "   The printers available on the system will be listed" << std::endl;
		std::cout << "   You are expected to select one from the list" << std::endl;
	}
	else //Starting the execution of the real program!
	{
		spoolerTester->connectToPrinter(printerName);

		std::cout
			<< "Please write \"Q\" to quit application..." << std::endl
			<< "File Path: " << std::endl;

		std::string filePath;

		while (!terminationReceived && std::getline(std::cin, filePath))
		{
			if (terminationReceived)
				break;
			if (filePath.length() == 1 && (filePath.at(0) == UC_Q || filePath.at(0) == LC_Q))
				break;

			spoolerTester->startPrinting(filePath);

			std::cout
				<< "Please write \"Q\" to quit application..." << std::endl
				<< "File Path: " << std::endl;
		}

		spoolerTester->disconnectFromPrinter();
		spoolerTester.reset();
	}
	
	return returnValue;
}

