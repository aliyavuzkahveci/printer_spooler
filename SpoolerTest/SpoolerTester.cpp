#include "SpoolerTester.h"

#include <fstream>

namespace SpoolerTest
{
	SpoolerTester::SpoolerTester()
	{
	}


	SpoolerTester::~SpoolerTester()
	{
		m_spooler.reset();
	}

	std::vector<std::string> SpoolerTester::getSpooledPrinters()
	{
		return PrinterSpooler::GetSpooledPrinters();
	}

	void SpoolerTester::connectToPrinter(const std::string& printerName)
	{
		m_spooler = PrinterSpooler_Ptr(new PrinterSpooler(shared_from_this(), printerName));
		m_spooler->initializePrinterSpooler();
	}

	void SpoolerTester::disconnectFromPrinter()
	{
		m_printDocInfoMap.clear();
		m_cancelledDocumentNames.clear();
		std::swap(m_waitingForJobIdDocs, std::queue<PrintDocumentInfo>()); //clear the contents of the queue!

		if(m_spooler.get())
			m_spooler->closePrinterSpooler();
	}

	void SpoolerTester::startPrinting(const std::string& filePath)
	{
		std::vector<std::string> documents;
		char* docData = nullptr;
		unsigned int dataSize = readPNGFiles(filePath, &docData);
		documents.push_back(std::string(docData, dataSize));
		std::string documentName = filePath;
		unsigned int index = filePath.find_last_of("/\\");
		if (index != std::string::npos)
			documentName = filePath.substr(index + 1);

		if ((index = documentName.find_last_of(".")) != std::string::npos)
			documentName = documentName.substr(0, index);

		PrintRequest_Map prMap;
		prMap.insert(PrintRequest_Pair(documentName, documents));

		m_waitingForJobIdDocs.push(PrintDocumentInfo(documentName));

		if (m_spooler.get())
			m_spooler->printDocuments(prMap);

		free(docData);
	}

	void SpoolerTester::on_printerStatusChange(PrinterStatus newStatus)
	{
		if (m_currentStatus == newStatus) //the same status might be sent twice!!!
			return; //we need to return NOT to cause unnecessary logs!

		m_currentStatus = newStatus;
		std::cout << "SpoolerTester::on_printerStatusChange() -> Printer Status changed: " << m_currentStatus << std::endl;
	}

	void SpoolerTester::on_printDocumentStatusChange(unsigned long jobId, SpooledJobStatus status)
	{
		auto iter = m_printDocInfoMap.find(jobId);
		if (iter != m_printDocInfoMap.end())
		{	//the corresponding jobId should be in the map!
			if (iter->second.m_status != status)
			{
				iter->second.m_status = status;

				if (status == SpooledJobStatus::Job_Deleted) //print job is cancelled by the user!
					m_cancelledDocumentNames.push_back(iter->second.m_documentName);

				std::cout << "Document (" << iter->second.m_documentName << ") Status -> " << iter->second.m_status << std::endl;

				if (iter->second.m_status != SpooledJobStatus::Job_Printing)
					m_printDocInfoMap.erase(iter);

				if (m_printDocInfoMap.empty() && //if all the documents in the spooler sent to the printer!
					m_currentStatus == PrinterStatus::Printer_Ready) //AND iff the device is online!
					std::cout << "SpoolerTester::on_printDocumentStatusChange() -> All the documents have been printed out!" << std::endl;
			}
		}
		else
		{
			std::cout << "SpoolerTester::on_printDocumentStatusChange() -> given jobId does not exist in the map!" << std::endl;
		}
	}

	void SpoolerTester::on_feedBackJobId(const unsigned long jobId)
	{
		if (jobId > 0) //jobId > 0 => sending to spooler is successful!
			m_printDocInfoMap.insert(PrintedDocStatus_Pair(jobId, m_waitingForJobIdDocs.front()));

		m_waitingForJobIdDocs.pop(); //remove
	}

	void SpoolerTester::on_printResponseReceived(const PrintDocResult_Map& printResponseMap)
	{
		std::vector<std::string> documents;
		bool printStarted = true;

		for (auto iter = printResponseMap.begin(); iter != printResponseMap.end(); iter++)
		{
			documents.push_back(iter->first);

			switch (iter->second)
			{
			case PrintDocumentResult::Print_OK:
				break;
			case PrintDocumentResult::NotReady:
				printStarted = false;
				break;
			case PrintDocumentResult::PaperJam:
				printStarted = false;
				break;
			case PrintDocumentResult::PaperOut:
				printStarted = false;
				break;
			case PrintDocumentResult::Cancelled:
			default:
				printStarted = false;
				break;
			}
		}

		if (printStarted)
			std::cout << "SpoolerTester::on_printResponseReceived() -> Print in progress..." << std::endl;
		else
			std::cout << "SpoolerTester::on_printResponseReceived() -> Print did NOT started!" << std::endl;
	}

	//param 1 -> filePath, param 2 -> the content of the file given in param 1
	//return value -> the size of the file data
	unsigned int SpoolerTester::readPNGFiles(const std::string& filePath, char** docData)
	{
		std::ifstream is(filePath, std::ifstream::binary);
		if (is)
		{
			is.seekg(0, is.end);
			unsigned int dataLength = is.tellg();
			is.seekg(0, is.beg);

			char* imageData = (char*)malloc(dataLength * sizeof(char));

			is.read(imageData, dataLength);

			is.close();

			*docData = imageData;
			return dataLength;
		}
		return 0;
	}

}
